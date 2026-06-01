/*
 * UsageMonitor -- an e-paper desk display of AI coding assistant usage quotas.
 *
 * The device connects to WiFi and talks directly to provider APIs (no companion
 * PC), refreshes its own bearer tokens, and renders a two-column dashboard.
 * Which two providers appear on screen is chosen at compile time via
 * UM_ENABLE_<X> / UM_<X>_SIDE build flags (see platformio.ini).
 *
 * Supported providers (pure HTTPS, device-direct):
 *   Claude, Codex, Copilot, MiniMax, Kimi, Zai
 *
 * Fill src/secrets.h (copied from include/secrets.example.h) with your WiFi
 * credentials and the bootstrap tokens for the two providers you chose.
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

  .copilotPat         = UM_COPILOT_PAT,
  .minimaxApiKey      = UM_MINIMAX_API_KEY,
  .minimaxRegion      = UM_MINIMAX_REGION,
  .kimiAuthToken      = UM_KIMI_AUTH_TOKEN,
  .zaiApiKey          = UM_ZAI_API_KEY,
  .zaiEndpoint        = UM_ZAI_ENDPOINT,

  .httpTimeoutMs      = 45000,
  .refreshIntervalMs  = 300000UL,   // 5 minutes
};

UsageApp app(kConfig);

void setup() { app.begin(); }
void loop()  { app.loop(); }
