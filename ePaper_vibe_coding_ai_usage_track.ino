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

void setup() { app.begin(); }
void loop()  { app.loop(); }
