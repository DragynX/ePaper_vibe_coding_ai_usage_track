// UsageApp.h -- application orchestrator (boot, WiFi, NTP, poll, persist).
// 编排器:开机初始化、WiFi、NTP 校时、定时拉取用量、刷新并持久化 token。
// Which two providers to use is resolved at compile time by ProviderSelect.h.

#ifndef USAGE_MONITOR_USAGE_APP_H
#define USAGE_MONITOR_USAGE_APP_H

#include <Arduino.h>

#include "HttpClient.h"
#include "OAuthClient.h"
#include "ProviderSelect.h"
#include "TokenStore.h"
#include "UsageConfig.h"
#include "UsageSnapshot.h"
#include "UsageUI.h"

// Include only the two selected provider headers.
#if UM_LEFT_PROVIDER == UM_PROV_CLAUDE || UM_RIGHT_PROVIDER == UM_PROV_CLAUDE
  #include "ClaudeUsageClient.h"
#endif
#if UM_LEFT_PROVIDER == UM_PROV_CODEX || UM_RIGHT_PROVIDER == UM_PROV_CODEX
  #include "CodexUsageClient.h"
#endif
#if UM_LEFT_PROVIDER == UM_PROV_COPILOT || UM_RIGHT_PROVIDER == UM_PROV_COPILOT
  #include "CopilotUsageClient.h"
#endif
#if UM_LEFT_PROVIDER == UM_PROV_MINIMAX || UM_RIGHT_PROVIDER == UM_PROV_MINIMAX
  #include "MiniMaxUsageClient.h"
#endif
#if UM_LEFT_PROVIDER == UM_PROV_KIMI || UM_RIGHT_PROVIDER == UM_PROV_KIMI
  #include "KimiUsageClient.h"
#endif
#if UM_LEFT_PROVIDER == UM_PROV_ZAI || UM_RIGHT_PROVIDER == UM_PROV_ZAI
  #include "ZaiUsageClient.h"
#endif

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
  UiStatus currentStatus();

  // Fetch one side into the snapshot and persist tokens if refreshed.
  void fetchLeft(long nowEpoch);
  void fetchRight(long nowEpoch);

  UsageConfig config_;
  HttpClient http_;
  TokenStore store_;
  UsageUI ui_;
  UsageSnapshot snapshot_;
  unsigned long lastRefreshMs_ = 0;
  bool timeSynced_ = false;

  // Left provider instances.
  AuthState leftAuth_;
  OAuthClient leftOAuth_;
#if UM_LEFT_PROVIDER == UM_PROV_CLAUDE
  ClaudeAuthProvider leftProvider_;
  ClaudeUsageClient leftClient_;
#elif UM_LEFT_PROVIDER == UM_PROV_CODEX
  CodexAuthProvider leftProvider_;
  CodexUsageClient leftClient_;
#elif UM_LEFT_PROVIDER == UM_PROV_COPILOT
  CopilotAuthProvider leftProvider_;
  CopilotUsageClient leftClient_;
#elif UM_LEFT_PROVIDER == UM_PROV_MINIMAX
  MiniMaxAuthProvider leftProvider_;
  MiniMaxUsageClient leftClient_;
#elif UM_LEFT_PROVIDER == UM_PROV_KIMI
  KimiAuthProvider leftProvider_;
  KimiUsageClient leftClient_;
#elif UM_LEFT_PROVIDER == UM_PROV_ZAI
  ZaiAuthProvider leftProvider_;
  ZaiUsageClient leftClient_;
#endif

  // Right provider instances.
  AuthState rightAuth_;
  OAuthClient rightOAuth_;
#if UM_RIGHT_PROVIDER == UM_PROV_CLAUDE
  ClaudeAuthProvider rightProvider_;
  ClaudeUsageClient rightClient_;
#elif UM_RIGHT_PROVIDER == UM_PROV_CODEX
  CodexAuthProvider rightProvider_;
  CodexUsageClient rightClient_;
#elif UM_RIGHT_PROVIDER == UM_PROV_COPILOT
  CopilotAuthProvider rightProvider_;
  CopilotUsageClient rightClient_;
#elif UM_RIGHT_PROVIDER == UM_PROV_MINIMAX
  MiniMaxAuthProvider rightProvider_;
  MiniMaxUsageClient rightClient_;
#elif UM_RIGHT_PROVIDER == UM_PROV_KIMI
  KimiAuthProvider rightProvider_;
  KimiUsageClient rightClient_;
#elif UM_RIGHT_PROVIDER == UM_PROV_ZAI
  ZaiAuthProvider rightProvider_;
  ZaiUsageClient rightClient_;
#endif
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_USAGE_APP_H
