#pragma once
// Arduino IDE build config for E1001.
// Provider selection is now runtime via the settings web UI — no reflash needed.

#define UM_VERSION "1.1.2"

#ifndef BOARD_HAS_PSRAM
#define BOARD_HAS_PSRAM
#endif

// Device target: E1001 (gray4 screen, BOARD_SCREEN_COMBO 520)
#define UM_DEVICE_E1001

// Runtime provider selection: all provider code compiled in; active providers
// chosen via settings page and persisted in NVS.
#define UM_ALL_PROVIDERS

// Enable local stats companion service (runtime URL configured in settings).
#define UM_ENABLE_LOCAL_STATS
