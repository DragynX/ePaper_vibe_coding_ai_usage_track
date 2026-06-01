/*
 * UsageMonitor -- an e-paper desk display of Claude Code and Codex usage quotas.
 *
 * The device connects to WiFi and talks directly to the Claude and OpenAI
 * OAuth usage APIs (no companion PC), refreshes its own bearer tokens, and
 * renders both providers' rate-limit windows.
 *
 * This file is the PlatformIO entry point and the only file you edit to point
 * the device at your network and accounts. Fill src/secrets.h (copied from
 * include/secrets.example.h) with your WiFi and OAuth bootstrap tokens.
 *
 *   UsageApp.*          orchestrator (boot, WiFi, NTP, poll)
 *   HttpClient.*        HTTPS transport (+ response headers, Retry-After)
 *   OAuthClient.*       authed request with refresh + retry
 *   TokenStore.*        NVS persistence of rotated tokens
 *   ClaudeUsageClient.* / CodexUsageClient.*   per-provider adapters
 *   UsageUI.*           e-paper drawing (Phase 3)
 *   driver.h            device model + screen capability selector
 */

#include <Arduino.h>

#include "UsageApp.h"
#include "secrets.h"

using namespace usage_monitor;

static const UsageConfig kConfig = {
  .wifiSsid           = UM_WIFI_SSID,
  .wifiPassword       = UM_WIFI_PASSWORD,
  .tz                 = UM_TZ,

  .claudeAccessToken  = UM_CLAUDE_ACCESS_TOKEN,
  .claudeRefreshToken = UM_CLAUDE_REFRESH_TOKEN,
  .claudeExpiresAtMs  = UM_CLAUDE_EXPIRES_AT_MS,
  .claudeSubscription = UM_CLAUDE_SUBSCRIPTION,

  .codexAccessToken   = UM_CODEX_ACCESS_TOKEN,
  .codexRefreshToken  = UM_CODEX_REFRESH_TOKEN,
  .codexAccountId     = UM_CODEX_ACCOUNT_ID,
  .codexLastRefresh   = UM_CODEX_LAST_REFRESH,

  .httpTimeoutMs      = 45000,
  .refreshIntervalMs  = 300000UL,   // 5 minutes
};

UsageApp app(kConfig);

void setup() { app.begin(); }
void loop()  { app.loop(); }
