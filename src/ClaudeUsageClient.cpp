#include "ClaudeUsageClient.h"

#include <ArduinoJson.h>

#include "IsoTime.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool ClaudeAuthProvider::refresh(HttpClient& http, AuthState& st, long now,
                                 bool& needsRelogin) {
  needsRelogin = false;
  if (st.refreshToken.length() == 0) { needsRelogin = true; return false; }

  // JSON refresh body with the Claude Code CLI client_id and scope.
  String body = String("{\"grant_type\":\"refresh_token\",\"refresh_token\":\"")
              + st.refreshToken
              + "\",\"client_id\":\"9d1c250a-e61b-44d9-88ed-5944d1962f5e\","
                "\"scope\":\"user:profile user:inference user:sessions:claude_code\"}";
  HttpResult r = http.post("https://platform.claude.com/v1/oauth/token",
                           nullptr, 0, body, "application/json");
  if (r.status != 200) {
    if (r.body.indexOf("invalid_grant") >= 0) needsRelogin = true;
    Serial1.printf("[claude/refresh] failed (%d)\n", r.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, r.body)) return false;
  const char* at = doc["access_token"];
  if (!at) return false;
  st.accessToken = at;
  st.accessToken.trim();
  const char* rt = doc["refresh_token"];
  if (rt) st.refreshToken = rt;            // Anthropic may rotate the refresh token
  const long expiresIn = doc["expires_in"] | 0;
  st.expiryEpoch = now + expiresIn;
  st.usesAbsoluteExpiry = true;
  Serial1.println("[claude/refresh] ok");
  return true;
}

bool ClaudeUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kClaude;
  if (!oauth_) return false;

  static const HttpHeader extra[] = {
    { "Accept", "application/json" },
    { "Content-Type", "application/json" },
    { "anthropic-beta", "oauth-2025-04-20" },
  };
  AuthedResult ar = oauth_->get("https://api.anthropic.com/api/oauth/usage",
                                extra, 3, nullptr, 0, now, "UsageMonitor");
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    Serial1.printf("[claude/usage] status %d\n", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    Serial1.println("[claude/usage] json parse error");
    return false;
  }

  // Each window: utilization (0-100) + ISO8601-Z reset time.
  auto parseWin = [&](JsonVariantConst v, WindowQuota& w) {
    if (v.isNull()) return;
    w.present = true;
    w.usedPercent = v["utilization"] | 0.0;
    const char* ra = v["resets_at"];
    w.resetEpoch = ra ? umParseIso8601(ra) : 0;
    w.status = umStatusFromUsed(w.usedPercent);
  };
  parseWin(doc["five_hour"], out.session);
  parseWin(doc["seven_day"], out.weekly);
  parseWin(doc["seven_day_sonnet"], out.weeklySonnet);
  parseWin(doc["seven_day_opus"], out.weeklyOpus);

  JsonVariantConst ex = doc["extra_usage"];
  if (!ex.isNull()) {
    out.extraEnabled = ex["is_enabled"] | false;
    out.extraUsedCents = ex["used_credits"] | 0.0;
    out.extraLimitCents = ex["monthly_limit"] | 0.0;
  }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
