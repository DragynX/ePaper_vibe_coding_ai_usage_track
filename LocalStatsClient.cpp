#include "LocalStatsClient.h"

#include <ArduinoJson.h>
#include <string.h>

#include "AppLog.h"

namespace usage_monitor {

void LocalStatsClient::configure(HttpClient* http, const char* baseUrl) {
  http_ = http;
  baseUrl_ = baseUrl ? baseUrl : "";
}

void LocalStatsClient::markUnavailable(LocalProviderStats& stats, const char* status) {
  stats = LocalProviderStats();
  stats.enabled = true;
  stats.available = false;
  strncpy(stats.status, status, sizeof(stats.status) - 1);
}

static uint32_t readU32(JsonVariantConst v) {
  if (v.is<uint32_t>()) return v.as<uint32_t>();
  if (v.is<unsigned long>()) return static_cast<uint32_t>(v.as<unsigned long>());
  if (v.is<long>()) {
    const long n = v.as<long>();
    return n > 0 ? static_cast<uint32_t>(n) : 0;
  }
  return 0;
}

void LocalStatsClient::parseProvider(const char* key, JsonVariantConst root,
                                     LocalProviderStats& out) {
  out = LocalProviderStats();
  out.enabled = true;
  JsonVariantConst providers = root["providers"];
  JsonVariantConst p = providers[key];
  if (p.isNull()) {
    markUnavailable(out, "missing");
    return;
  }

  out.available = p["available"] | false;
  const char* status = p["status"] | (out.available ? "ok" : "n/a");
  strncpy(out.status, status, sizeof(out.status) - 1);
  if (!out.available) return;

  out.todayTokens = readU32(p["today_tokens"]);
  out.inputTokens = readU32(p["input_tokens"]);
  out.outputTokens = readU32(p["output_tokens"]);
  out.cacheTokens = readU32(p["cache_tokens"]);
  out.sessionCount = static_cast<uint16_t>(readU32(p["session_count"]));
  out.latestEpoch = p["latest_at"] | 0L;

  JsonArrayConst models = p["top_models"];
  uint8_t idx = 0;
  for (JsonVariantConst model : models) {
    if (idx >= 3) break;
    const char* name = model["name"] | "";
    strncpy(out.models[idx].name, name, sizeof(out.models[idx].name) - 1);
    out.models[idx].tokens = readU32(model["tokens"]);
    out.models[idx].count = static_cast<uint16_t>(readU32(model["count"]));
    idx++;
  }
  out.modelCount = idx;
}

bool LocalStatsClient::fetch(const char* leftKey, const char* rightKey, UsageSnapshot& out) {
  if (!http_ || !baseUrl_ || !baseUrl_[0]) {
    markUnavailable(out.left.local, "no url");
    markUnavailable(out.right.local, "no url");
    return false;
  }

  sysLog("[local/fetch] url=%s", baseUrl_);
  String base(baseUrl_);
  while (base.endsWith("/")) base.remove(base.length() - 1);
  String url = base + "/v1/snapshot?providers=" + leftKey + "," + rightKey;

  HttpResult r = http_->get(url, nullptr, 0, nullptr, 0, "UsageMonitor");
  if (r.status != 200) {
    markUnavailable(out.left.local, "offline");
    markUnavailable(out.right.local, "offline");
    sysLog("[local] status %d", r.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, r.body)) {
    markUnavailable(out.left.local, "bad json");
    markUnavailable(out.right.local, "bad json");
    sysLog("[local] json parse error");
    return false;
  }

  parseProvider(leftKey, doc.as<JsonVariantConst>(), out.left.local);
  parseProvider(rightKey, doc.as<JsonVariantConst>(), out.right.local);
  return true;
}

}  // namespace usage_monitor
