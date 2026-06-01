// CodexUsageClient.h -- Codex usage fetch + OAuth refresh adapter.
// Codex 适配器:拉 chatgpt.com/wham/usage(响应头优先) + 刷 auth.openai.com token。

#ifndef USAGE_MONITOR_CODEX_USAGE_CLIENT_H
#define USAGE_MONITOR_CODEX_USAGE_CLIENT_H

#include "OAuthClient.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

class CodexAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient& http, AuthState& st, long nowEpoch, bool& needsRelogin) override;
};

class CodexUsageClient {
 public:
  void configure(OAuthClient* oauth) { oauth_ = oauth; }
  bool fetch(long nowEpoch, ProviderQuota& out);

 private:
  OAuthClient* oauth_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_CODEX_USAGE_CLIENT_H
