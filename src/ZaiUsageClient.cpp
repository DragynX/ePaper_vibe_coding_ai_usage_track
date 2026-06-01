#include "ZaiUsageClient.h"

#include <ArduinoJson.h>

#include "IsoTime.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool ZaiUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kZai;
  if (!oauth_) return false;

  String url = String(endpoint_) + "/api/monitor/usage/quota/limit";
  static const HttpHeader extra[] = {
    { "Accept-Language", "en-US,en" },
    { "Content-Type", "application/json" },
  };
  AuthedResult ar = oauth_->get(url, extra, 2, nullptr, 0, now, "UsageMonitor");
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    Serial1.printf("[zai/usage] status %d\n", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    Serial1.println("[zai/usage] json parse error");
    return false;
  }

  JsonArray limits = doc["data"]["limits"];
  if (limits.isNull() || limits.size() == 0) {
    Serial1.println("[zai/usage] no limits in response");
    return false;
  }

  for (JsonVariant lim : limits) {
    const char* type = lim["type"];
    const int unit = lim["unit"] | -1;
    const double pctUsed = lim["percentage"] | 0.0;
    const double clamped = umClampPercent(pctUsed);

    // Parse reset time: can be unix ms (number) or ISO8601 string.
    long resetEpoch = 0;
    JsonVariant nrt = lim["next_reset_time"];
    if (nrt.is<long long>()) {
      resetEpoch = static_cast<long>(nrt.as<long long>() / 1000LL);
    } else if (nrt.is<const char*>()) {
      resetEpoch = umParseIso8601(nrt.as<const char*>());
    }

    WindowQuota* target = nullptr;
    if (String(type) == "TOKENS_LIMIT") {
      if (unit == 3)       target = &out.session;     // 5h rolling
      else if (unit == 6)  target = &out.weekly;      // 7d rolling
      else if (unit == -1) target = &out.session;     // legacy (no unit field)
    }
    if (!target) continue;

    target->present = true;
    target->usedPercent = clamped;
    target->resetEpoch = resetEpoch;
    target->status = umStatusFromUsed(clamped);
  }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
