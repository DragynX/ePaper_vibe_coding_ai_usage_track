// ClaudePlatformUsageClient.h -- Anthropic admin-key usage cost API.
// Uses GET /v1/organizations/usage_report/messages with x-api-key header.
// Bypasses OAuthClient (admin key never needs refresh).

#ifndef USAGE_MONITOR_CLAUDE_PLATFORM_USAGE_CLIENT_H
#define USAGE_MONITOR_CLAUDE_PLATFORM_USAGE_CLIENT_H

#include <Arduino.h>

#include "ProjectHttpClient.h"
#include "UsageClientBase.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

class ClaudePlatformUsageClient : public UsageClientBase {
 public:
  void configure(HttpClient* http, const String& adminKey, const String& orgId) {
    http_     = http;
    adminKey_ = adminKey;
    orgId_    = orgId;
  }
  bool fetch(long nowEpoch, ProviderQuota& out) override;

 private:
  HttpClient* http_    = nullptr;
  String      adminKey_;
  String      orgId_;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_CLAUDE_PLATFORM_USAGE_CLIENT_H
