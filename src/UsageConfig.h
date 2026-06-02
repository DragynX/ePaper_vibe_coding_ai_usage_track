// UsageConfig.h -- top-level configuration filled in main.cpp from secrets.h.
// 顶层配置,在 main.cpp 用 secrets.h 的宏填充。每个 provider 的字段只在该 provider
// 被 UM_ENABLE_<X> 启用时才有意义;未启用时填空字符串即可。

#ifndef USAGE_MONITOR_USAGE_CONFIG_H
#define USAGE_MONITOR_USAGE_CONFIG_H

#include <Arduino.h>

struct UsageConfig {
  const char* wifiSsid;
  const char* wifiPassword;
  const char* tz;                  // POSIX TZ for the header wall-clock

  // Claude OAuth bootstrap (from ~/.claude/.credentials.json, camelCase).
  const char* claudeAccessToken;
  const char* claudeRefreshToken;
  const char* claudeExpiresAtMs;   // expiresAt in ms epoch, as a string
  const char* claudeSubscription;  // subscriptionType, display only

  // Codex OAuth bootstrap (from ~/.codex/auth.json, snake_case).
  const char* codexAccessToken;
  const char* codexRefreshToken;
  const char* codexAccountId;      // optional
  const char* codexLastRefresh;    // last_refresh ISO8601, or "0"

  // Copilot (Classic PAT with "copilot" scope).
  const char* copilotPat;

  // MiniMax (static API key; region: 0=international, 1=china).
  const char* minimaxApiKey;
  int         minimaxRegion;

  // Kimi (browser cookie token; no refresh, expires -> re-flash).
  const char* kimiAuthToken;

  // Zai / Zhipu (static API key + configurable endpoint).
  const char* zaiApiKey;
  const char* zaiEndpoint;         // "https://api.z.ai" / "https://open.bigmodel.cn"

  const char* localStatsUrl;       // optional computer-side stats service

  uint32_t httpTimeoutMs;
  uint32_t refreshIntervalMs;      // poll cadence (>= 5 min recommended)
};

#endif  // USAGE_MONITOR_USAGE_CONFIG_H
