#include "CopilotUsageClient.h"

#include <ArduinoJson.h>

#include "AppLog.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool CopilotUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kCopilot;
  if (!oauth_) return false;
  sysLog("[copilot/usage] fetch start");

  static const HttpHeader extra[] = {
    { "Accept", "application/json" },
  };
  AuthedResult ar = oauth_->get("https://api.github.com/copilot_internal/user",
                                extra, 1, nullptr, 0, now, "UsageMonitor");
  out.refreshFailed = ar.refreshFailed;
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    sysLog("[copilot/usage] status %d", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    sysLog("[copilot/usage] json parse error");
    return false;
  }

  const char* plan = doc["copilot_plan"];
  if (plan) {
    out.hasPlan = true;
    strncpy(out.planType, plan, sizeof(out.planType) - 1);
  }

  JsonVariant qi = doc["quota_snapshots"]["premium_interactions"];
  if (qi.isNull()) {
    // No premium_interactions: Free or chat-only plan -> 100% remaining.
    out.session.present = true;
    out.session.usedPercent = 0.0;
    out.session.status = QuotaStatus::kHealthy;
    out.ok = true;
    out.lastSuccessEpoch = now;
    return true;
  }

  if (qi["unlimited"] | false) {
    out.session.present = true;
    out.session.usedPercent = 0.0;
    out.session.status = QuotaStatus::kHealthy;
    out.ok = true;
    out.lastSuccessEpoch = now;
    return true;
  }

  const int entitlement = qi["entitlement"] | 0;
  const int remaining = qi["remaining"] | 0;
  const double pctRemaining = qi["percent_remaining"] | 100.0;
  out.session.present = true;
  out.session.usedPercent = 100.0 - pctRemaining;
  out.session.status = umStatusFromUsed(out.session.usedPercent);

  if (entitlement > 0) {
    out.hasBalance = true;
    out.balance = static_cast<double>(remaining);
  }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
