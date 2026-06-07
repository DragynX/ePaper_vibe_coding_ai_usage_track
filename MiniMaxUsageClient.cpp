#include "MiniMaxUsageClient.h"

#include <ArduinoJson.h>

#include "AppLog.h"
#include "QuotaMath.h"

namespace usage_monitor {

bool MiniMaxUsageClient::fetch(long now, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kMiniMax;
  if (!oauth_) return false;
  sysLog("[minimax/usage] fetch start");

  String url = String(baseUrl_) + "/v1/api/openplatform/coding_plan/remains";
  static const HttpHeader extra[] = {
    { "Accept", "application/json" },
  };
  AuthedResult ar = oauth_->get(url, extra, 1, nullptr, 0, now, "UsageMonitor");
  out.refreshFailed = ar.refreshFailed;
  if (ar.needsRelogin) { out.needsRelogin = true; return false; }
  if (ar.http.status != 200) {
    sysLog("[minimax/usage] status %d", ar.http.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, ar.http.body)) {
    sysLog("[minimax/usage] json parse error");
    return false;
  }
  if ((doc["base_resp"]["status_code"] | -1) != 0) {
    sysLog("[minimax/usage] API error");
    return false;
  }

  JsonArray models = doc["model_remains"];
  if (models.isNull() || models.size() == 0) {
    sysLog("[minimax/usage] no model_remains");
    return false;
  }

  // Use the first model entry as the session window.
  // MiniMax API naming is misleading: current_interval_usage_count is REMAINING,
  // not used. (confirmed in ClaudeBar source, 注释明确指出命名有误导性)
  JsonVariant m = models[0];
  const int total = m["current_interval_total_count"] | 0;
  const int remainCount = m["current_interval_usage_count"] | 0;
  const int clamped = (remainCount > total) ? total : (remainCount < 0 ? 0 : remainCount);

  out.session.present = true;
  if (total > 0) {
    out.session.usedPercent = (1.0 - static_cast<double>(clamped) / total) * 100.0;
  } else {
    out.session.usedPercent = 0.0;
  }
  out.session.status = umStatusFromUsed(out.session.usedPercent);

  // end_time is millisecond timestamp.
  const long long endTimeMs = m["end_time"] | 0LL;
  if (endTimeMs > 0) {
    out.session.resetEpoch = static_cast<long>(endTimeMs / 1000LL);
  }

  // Show remaining/total as "balance" for the UI.
  out.hasBalance = true;
  out.balance = static_cast<double>(clamped);

  const char* modelName = m["model_name"];
  if (modelName) {
    out.hasPlan = true;
    strncpy(out.planType, modelName, sizeof(out.planType) - 1);
  }

  out.ok = true;
  out.lastSuccessEpoch = now;
  return true;
}

}  // namespace usage_monitor
