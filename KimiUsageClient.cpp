#include "KimiUsageClient.h"

#include <ArduinoJson.h>

#include "AppLog.h"
#include "IsoTime.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool KimiUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kKimi;
  if (!oauth_) return false;
  sysLog("[kimi/usage] fetch start");

  // Kimi requires browser-like headers alongside the Bearer token.
  String token = oauth_->state()->accessToken;
  token.trim();
  const HttpHeader extra[] = {
    { "Cookie", String("kimi-auth=") + token },
    { "Accept", "*/*" },
    { "Origin", "https://www.kimi.com" },
    { "Referer", "https://www.kimi.com/code/console" },
    { "connect-protocol-version", "1" },
    { "x-msh-platform", "web" },
  };

  String body = "{\"scope\":[\"FEATURE_CODING\"]}";
  AuthedResult ar = oauth_->post(
      "https://www.kimi.com/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages",
      extra, 6, body, "application/json", now,
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36");

  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    sysLog("[kimi/usage] status %d", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    sysLog("[kimi/usage] json parse error");
    return false;
  }

  // Find the FEATURE_CODING scope entry.
  JsonArray usages = doc["usages"];
  JsonVariant coding;
  for (JsonVariant u : usages) {
    if (String(u["scope"] | "") == "FEATURE_CODING") { coding = u; break; }
  }
  if (coding.isNull()) {
    sysLog("[kimi/usage] no FEATURE_CODING scope");
    return false;
  }

  // Weekly quota from detail.
  auto parseNums = [](JsonVariant d, int& used, int& limit, int& remaining) {
    limit = String(d["limit"] | "0").toInt();
    int rawUsed = String(d["used"] | "").toInt();
    int rawRemaining = String(d["remaining"] | "").toInt();
    if (limit > 0) {
      used = rawUsed;
      remaining = (rawRemaining > 0) ? rawRemaining : (limit - rawUsed);
    } else {
      used = 0; remaining = 0;
    }
  };

  int wUsed = 0, wLimit = 0, wRemaining = 0;
  parseNums(coding["detail"], wUsed, wLimit, wRemaining);
  out.weekly.present = true;
  out.weekly.usedPercent = wLimit > 0 ? (1.0 - static_cast<double>(wRemaining) / wLimit) * 100.0 : 0.0;
  out.weekly.status = umStatusFromUsed(out.weekly.usedPercent);

  const char* resetTime = coding["detail"]["reset_time"];
  if (resetTime) out.weekly.resetEpoch = umParseIso8601(resetTime);

  // 5-hour rate limit from limits array (duration=300, timeUnit=TIME_UNIT_MINUTE).
  JsonArray limits = coding["limits"];
  for (JsonVariant lim : limits) {
    if ((lim["window"]["duration"] | 0) == 300 &&
        String(lim["window"]["time_unit"] | "") == "TIME_UNIT_MINUTE") {
      int sUsed = 0, sLimit = 0, sRemaining = 0;
      parseNums(lim["detail"], sUsed, sLimit, sRemaining);
      out.session.present = true;
      out.session.usedPercent = sLimit > 0 ? (1.0 - static_cast<double>(sRemaining) / sLimit) * 100.0 : 0.0;
      out.session.status = umStatusFromUsed(out.session.usedPercent);
      const char* sReset = lim["detail"]["reset_time"];
      if (sReset) out.session.resetEpoch = umParseIso8601(sReset);
      break;
    }
  }

  // Detect tier from weekly limit.
  if (wLimit == 1024) { out.hasPlan = true; strncpy(out.planType, "Andante", sizeof(out.planType) - 1); }
  else if (wLimit == 2048) { out.hasPlan = true; strncpy(out.planType, "Moderato", sizeof(out.planType) - 1); }
  else if (wLimit == 7168) { out.hasPlan = true; strncpy(out.planType, "Allegretto", sizeof(out.planType) - 1); }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
