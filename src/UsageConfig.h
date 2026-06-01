// UsageConfig.h -- top-level configuration filled in main.cpp from secrets.h.
// 顶层配置,在 main.cpp 用 secrets.h 的宏填充。

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

  uint32_t httpTimeoutMs;
  uint32_t refreshIntervalMs;      // poll cadence (>= 5 min recommended)
};

#endif  // USAGE_MONITOR_USAGE_CONFIG_H
