// UsageApp.h -- application orchestrator (boot, WiFi, NTP, poll, persist).
// 编排器:开机初始化、WiFi、NTP 校时、定时拉取两家用量、刷新并持久化 token。
// Phase 2 is headless: it prints the parsed snapshot to Serial1. The UI is wired
// in Phase 3.

#ifndef USAGE_MONITOR_USAGE_APP_H
#define USAGE_MONITOR_USAGE_APP_H

#include <Arduino.h>

#include "ClaudeUsageClient.h"
#include "CodexUsageClient.h"
#include "HttpClient.h"
#include "OAuthClient.h"
#include "TokenStore.h"
#include "UsageConfig.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

class UsageApp {
 public:
  explicit UsageApp(const UsageConfig& config);
  void begin();
  void loop();

 private:
  bool ensureWiFi(uint32_t timeoutMs);
  void syncTime();
  long now();
  void refreshAll();
  void printSnapshot();

  UsageConfig config_;

  HttpClient http_;
  TokenStore store_;

  AuthState claudeAuth_;
  AuthState codexAuth_;
  ClaudeAuthProvider claudeProvider_;
  CodexAuthProvider codexProvider_;
  OAuthClient claudeOAuth_;
  OAuthClient codexOAuth_;
  ClaudeUsageClient claude_;
  CodexUsageClient codex_;

  UsageSnapshot snapshot_;
  unsigned long lastRefreshMs_ = 0;
  bool timeSynced_ = false;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_USAGE_APP_H
