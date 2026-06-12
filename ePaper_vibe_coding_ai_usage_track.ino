/*
 * UsageMonitor -- e-paper desk display of AI coding assistant usage quotas.
 *
 * Connects to WiFi, talks directly to provider APIs (no companion PC),
 * refreshes bearer tokens, and renders a two-column dashboard.
 *
 * Provider selection and all credentials are configured at runtime via
 * the built-in settings web UI at http://usagemonitor.local after boot.
 *
 * Supported providers: Claude OAuth, Codex, Copilot, MiniMax, Kimi, Zai,
 *                      Claude Platform (admin key cost API)
 */

#include <Arduino.h>

#include "UsageApp.h"

using namespace usage_monitor;

UsageApp app;

// The provider fetch (begin()->refreshAll()) runs on the Arduino loop task. A
// TLS handshake plus the 55KB Mozilla CA-bundle verification is very stack-heavy,
// and the Claude 401->reactive-refresh path nests a second handshake — the default
// 8KB loop stack overflows there (crash-reboot loop). Give it room.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() { app.begin(); }
void loop()  { app.loop(); }
