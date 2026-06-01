// ClaudeUsageClient.h -- Claude usage fetch + OAuth refresh adapter.
// Claude 适配器:拉 api.anthropic.com 用量 + 刷 platform.claude.com token。

#ifndef USAGE_MONITOR_CLAUDE_USAGE_CLIENT_H
#define USAGE_MONITOR_CLAUDE_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

class ClaudeAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient& http, AuthState& st, long nowEpoch, bool& needsRelogin) override;
};

class ClaudeUsageClient {
 public:
  void configure(OAuthClient* oauth) { oauth_ = oauth; }
  bool fetch(long nowEpoch, ProviderQuota& out);

 private:
  OAuthClient* oauth_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_CLAUDE_USAGE_CLIENT_H
