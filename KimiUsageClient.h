// KimiUsageClient.h -- Kimi Coding usage fetch via browser-cookie auth.
// Kimi 适配器:POST kimi.com API,用 kimi-auth cookie token。无 refresh,过期需重填。

#ifndef USAGE_MONITOR_KIMI_USAGE_CLIENT_H
#define USAGE_MONITOR_KIMI_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageClientBase.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// No refresh mechanism for Kimi; on 401 the cookie is dead.
class KimiAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient&, AuthState&, long, bool& needsRelogin) override {
    needsRelogin = true;
    return false;
  }
};

class KimiUsageClient : public UsageClientBase {
 public:
  void configure(OAuthClient* oauth) { oauth_ = oauth; }
  bool fetch(long nowEpoch, ProviderQuota& out) override;

 private:
  OAuthClient* oauth_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_KIMI_USAGE_CLIENT_H
