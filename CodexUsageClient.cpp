#include "CodexUsageClient.h"

#include <ArduinoJson.h>
#include <ctype.h>

#include "AppLog.h"
#include "HeaderField.h"
#include "IsoTime.h"
#include "QuotaMath.h"

namespace usage_monitor {

// Percent-encode a value for an application/x-www-form-urlencoded body.
static String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (isalnum(static_cast<unsigned char>(c)) ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

bool CodexAuthProvider::refresh(HttpClient& http, AuthState& st, long now,
                                bool& needsRelogin) {
  needsRelogin = false;
  if (st.refreshToken.length() == 0) { needsRelogin = true; return false; }

  // form-urlencoded refresh with the Codex client_id.
  String body = String("grant_type=refresh_token&client_id=app_EMoamEEZ73f0CkXaXp7hrann"
                       "&refresh_token=") + urlEncode(st.refreshToken);
  HttpResult r = http.post("https://auth.openai.com/oauth/token",
                           nullptr, 0, body, "application/x-www-form-urlencoded");
  if (r.status != 200) {
    if (r.body.indexOf("refresh_token_expired") >= 0 ||
        r.body.indexOf("refresh_token_reused") >= 0 ||
        r.body.indexOf("refresh_token_invalidated") >= 0) {
      needsRelogin = true;
    }
    sysLog("[codex/refresh] failed (%d)", r.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, r.body)) return false;
  const char* at = doc["access_token"];
  if (!at) return false;
  st.accessToken = at;
  st.accessToken.trim();
  const char* rt = doc["refresh_token"];
  if (rt) st.refreshToken = rt;
  st.expiryEpoch = now;            // last_refresh = now (8-day rolling)
  st.usesAbsoluteExpiry = false;
  sysLog("[codex/refresh] ok");
  return true;
}

bool CodexUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kCodex;
  if (!oauth_) return false;
  sysLog("[codex/usage] fetch start");

  HttpHeader extra[2];
  size_t en = 0;
  extra[en++] = { "Accept", "application/json" };
  AuthState* st = oauth_->state();
  if (st && st->accountId.length()) {
    extra[en++] = { "ChatGPT-Account-Id", st->accountId };
  }

  static const char* hdrKeys[] = {
    "x-codex-primary-used-percent",
    "x-codex-secondary-used-percent",
    "x-codex-credits-balance",
  };
  AuthedResult ar = oauth_->get("https://chatgpt.com/backend-api/wham/usage",
                                extra, en, hdrKeys, 3, now, "OpenUsage");
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    sysLog("[codex/usage] status %d", ar.http.status);
    return false;
  }

  JsonDocument doc;
  const bool haveBody = (deserializeJson(doc, ar.http.body) == DeserializationError::Ok);
  JsonVariantConst rl;
  if (haveBody) rl = doc["rate_limit"];

  // Header value preferred, body rate_limit.<key> as fallback.
  auto win = [&](const String& hdrPct, const char* bodyKey, WindowQuota& w) {
    double bodyUsed = 0.0;
    bool bodyHasUsed = false;
    long resetAt = 0, resetAfter = 0;
    if (!rl.isNull()) {
      JsonVariantConst wv = rl[bodyKey];
      if (!wv.isNull()) {
        if (!wv["used_percent"].isNull()) {
          bodyUsed = wv["used_percent"] | 0.0;
          bodyHasUsed = true;
        }
        resetAt = wv["reset_at"] | 0;
        resetAfter = wv["reset_after_seconds"] | 0;
      }
    }
    bool present = false;
    const double used = umPickNumber(hdrPct.c_str(), bodyUsed, bodyHasUsed, present);
    if (!present) return;
    w.present = true;
    w.usedPercent = used;
    w.resetEpoch = umNormalizeReset(resetAt, resetAfter, now);
    w.status = umStatusFromUsed(used);
  };
  win(ar.http.h0, "primary_window", out.session);
  win(ar.http.h1, "secondary_window", out.weekly);

  // Credits balance: header h2 preferred, body credits.balance as fallback.
  double bodyBal = 0.0;
  bool bodyHasBal = false;
  if (haveBody) {
    JsonVariantConst credits = doc["credits"];
    if (!credits.isNull() && !credits["balance"].isNull()) {
      bodyBal = credits["balance"] | 0.0;
      bodyHasBal = true;
    }
  }
  bool balPresent = false;
  const double bal = umPickNumber(ar.http.h2.c_str(), bodyBal, bodyHasBal, balPresent);
  if (balPresent) {
    out.hasBalance = true;
    out.balance = bal;
  }

  if (haveBody) {
    const char* pt = doc["plan_type"];
    if (pt) {
      out.hasPlan = true;
      strncpy(out.planType, pt, sizeof(out.planType) - 1);
    }
  }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
