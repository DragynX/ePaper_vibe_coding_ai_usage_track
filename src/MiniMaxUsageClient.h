// MiniMaxUsageClient.h -- MiniMax Coding Plan usage fetch.
// MiniMax 适配器:Bearer API key 拉 codingPlanRemains。

#ifndef USAGE_MONITOR_MINIMAX_USAGE_CLIENT_H
#define USAGE_MONITOR_MINIMAX_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// Static API key: never needs refresh.
class MiniMaxAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient&, AuthState&, long, bool& needsRelogin) override {
    needsRelogin = false;
    return true;
  }
};

class MiniMaxUsageClient {
 public:
  void configure(OAuthClient* oauth, const char* baseUrl) {
    oauth_ = oauth;
    baseUrl_ = baseUrl;
  }
  bool fetch(long nowEpoch, ProviderQuota& out);

 private:
  OAuthClient* oauth_ = nullptr;
  const char* baseUrl_ = "https://api.minimax.io";
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_MINIMAX_USAGE_CLIENT_H
