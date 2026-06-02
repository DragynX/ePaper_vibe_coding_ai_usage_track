// LocalStatsClient.h -- optional computer-side stats fetcher.
// 可选本地统计客户端:从电脑端服务读取每日 token 与模型排行。

#ifndef USAGE_MONITOR_LOCAL_STATS_CLIENT_H
#define USAGE_MONITOR_LOCAL_STATS_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "HttpClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

class LocalStatsClient {
 public:
  void configure(HttpClient* http, const char* baseUrl);
  bool fetch(const char* leftKey, const char* rightKey, UsageSnapshot& out);

 private:
  void markUnavailable(LocalProviderStats& stats, const char* status);
  void parseProvider(const char* key, JsonVariantConst root, LocalProviderStats& out);

  HttpClient* http_ = nullptr;
  const char* baseUrl_ = "";
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_LOCAL_STATS_CLIENT_H
