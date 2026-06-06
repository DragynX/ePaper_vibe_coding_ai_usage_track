// CopilotUsageClient.h -- GitHub Copilot usage fetch via Copilot Internal API.
// GitHub Copilot 适配器:用 Classic PAT 拉 api.github.com/copilot_internal/user。

#ifndef USAGE_MONITOR_COPILOT_USAGE_CLIENT_H
#define USAGE_MONITOR_COPILOT_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageClientBase.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// Static PAT: uses the shared StaticKeyAuthProvider from OAuthClient.h.
using CopilotAuthProvider = StaticKeyAuthProvider;

class CopilotUsageClient : public UsageClientBase {
 public:
  void configure(OAuthClient* oauth) { oauth_ = oauth; }
  bool fetch(long nowEpoch, ProviderQuota& out) override;

 private:
  OAuthClient* oauth_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_COPILOT_USAGE_CLIENT_H
