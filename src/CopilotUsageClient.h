// CopilotUsageClient.h -- GitHub Copilot usage fetch via Copilot Internal API.
// GitHub Copilot 适配器:用 Classic PAT 拉 api.github.com/copilot_internal/user。

#ifndef USAGE_MONITOR_COPILOT_USAGE_CLIENT_H
#define USAGE_MONITOR_COPILOT_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// Static PAT: never needs refresh.
class CopilotAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient&, AuthState&, long, bool& needsRelogin) override {
    needsRelogin = false;
    return true;
  }
};

class CopilotUsageClient {
 public:
  void configure(OAuthClient* oauth) { oauth_ = oauth; }
  bool fetch(long nowEpoch, ProviderQuota& out);

 private:
  OAuthClient* oauth_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_COPILOT_USAGE_CLIENT_H
