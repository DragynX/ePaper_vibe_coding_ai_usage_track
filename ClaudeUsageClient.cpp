#include "ClaudeUsageClient.h"

#include <ArduinoJson.h>
#include <string.h>

#include "AppLog.h"
#include "IsoTime.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool ClaudeAuthProvider::refresh(HttpClient& http, AuthState& st, long now,
                                 bool& needsRelogin) {
  needsRelogin = false;
  sysLog("[claude/refresh] rt_len=%d", (int)st.refreshToken.length());
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
    sysLog("[claude/refresh] failed (%d)", r.status);
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
  // Only derive an absolute expiry from a sane clock; epoch 0 keeps the
  // reactive-401 path working instead of poisoning the chain with 1970 dates.
  st.expiryEpoch = (now > 1577836800L) ? now + expiresIn : 0;
  st.usesAbsoluteExpiry = true;
  sysLog("[claude/refresh] ok");
  return true;
}

bool ClaudeUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kClaude;
  if (!oauth_) return false;
  sysLog("[claude/usage] fetch start");

  static const HttpHeader extra[] = {
    { "Accept", "application/json" },
    { "Content-Type", "application/json" },
    { "anthropic-beta", "oauth-2025-04-20" },
  };
  // UA matching the Claude Code CLI — the usage endpoint rate-limits
  // unrecognized user agents (429 with "UsageMonitor").
  AuthedResult ar = oauth_->get("https://api.anthropic.com/api/oauth/usage",
                                extra, 3, nullptr, 0, now,
                                "claude-cli/2.1.174 (external, cli)");
  out.refreshFailed = ar.refreshFailed;
  out.refreshed = ar.refreshed;   // rotated token must be persisted even on a failed usage call
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    sysLog("[claude/usage] status %d", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    sysLog("[claude/usage] json parse error");
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

  // Per-model weekly windows now arrive in limits[] rather than as top-level
  // seven_day_<model> keys (those read null on current accounts). Entries are
  // kind="weekly_scoped" tagged with scope.model.display_name, and carry an
  // integer "percent" instead of the "utilization" double used above.
  auto parseScoped = [&](const char* model, WindowQuota& w) {
    for (JsonVariantConst it : doc["limits"].as<JsonArrayConst>()) {
      const char* kind = it["kind"];
      if (!kind || strcmp(kind, "weekly_scoped") != 0) continue;
      const char* name = it["scope"]["model"]["display_name"];
      if (!name || strcmp(name, model) != 0) continue;
      w.present = true;
      w.usedPercent = it["percent"] | 0.0;
      const char* ra = it["resets_at"];
      w.resetEpoch = ra ? umParseIso8601(ra) : 0;
      w.status = umStatusFromUsed(w.usedPercent);
      return;
    }
  };
  parseScoped("Fable", out.weeklyFable);

  JsonVariantConst ex = doc["extra_usage"];
  if (!ex.isNull()) {
    out.extraEnabled = ex["is_enabled"] | false;
    out.extraUsedCents = ex["used_credits"] | 0.0;
    out.extraLimitCents = ex["monthly_limit"] | 0.0;
  }

  sysLog("[claude/usage] ok 5h=%.0f%% 7d=%.0f%% fable=%.0f%%(%d) opus=%d sonnet=%d extra=%d",
         out.session.usedPercent, out.weekly.usedPercent,
         out.weeklyFable.usedPercent, out.weeklyFable.present ? 1 : 0,
         out.weeklyOpus.present ? 1 : 0, out.weeklySonnet.present ? 1 : 0,
         out.extraEnabled ? 1 : 0);
  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
