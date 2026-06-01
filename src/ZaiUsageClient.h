// ZaiUsageClient.h -- Z.ai / Zhipu GLM Coding Plan usage fetch.
// Zai/智谱 适配器:Bearer API key 拉 /api/monitor/usage/quota/limit。

#ifndef USAGE_MONITOR_ZAI_USAGE_CLIENT_H
#define USAGE_MONITOR_ZAI_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// Static API key: uses the shared StaticKeyAuthProvider from OAuthClient.h.
using ZaiAuthProvider = StaticKeyAuthProvider;

class ZaiUsageClient {
 public:
  void configure(OAuthClient* oauth, const char* endpoint) {
    oauth_ = oauth;
    endpoint_ = endpoint;
  }
  bool fetch(long nowEpoch, ProviderQuota& out);

 private:
  OAuthClient* oauth_ = nullptr;
  const char* endpoint_ = "https://api.z.ai";
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_ZAI_USAGE_CLIENT_H
