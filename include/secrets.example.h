// Copy this file to src/secrets.h and fill in the values for your two chosen
// providers. Fields for providers you did NOT enable can stay as placeholders.
// src/secrets.h is excluded from version control via .gitignore.
//
// Providers enabled via -D UM_ENABLE_<X> in platformio.ini. Only the two
// enabled providers need real credentials here; the rest are unused at runtime.

#define UM_WIFI_SSID     "your_wifi_ssid"
#define UM_WIFI_PASSWORD "your_wifi_password"

// POSIX TZ string for the header wall-clock only; quota math is timezone-free.
#define UM_TZ            "UTC0"

// --- Claude (from ~/.claude/.credentials.json -> claudeAiOauth.*) ---
#define UM_CLAUDE_ACCESS_TOKEN  "placeholder"
#define UM_CLAUDE_REFRESH_TOKEN "placeholder"
#define UM_CLAUDE_EXPIRES_AT_MS "0"
#define UM_CLAUDE_SUBSCRIPTION  "pro"

// --- Codex (from ~/.codex/auth.json -> tokens.*) ---
#define UM_CODEX_ACCESS_TOKEN   "placeholder"
#define UM_CODEX_REFRESH_TOKEN  "placeholder"
#define UM_CODEX_ACCOUNT_ID     ""
#define UM_CODEX_LAST_REFRESH   "0"

// --- Copilot (Classic PAT with "copilot" scope from github.com/settings/tokens) ---
#define UM_COPILOT_PAT          "placeholder"

// --- MiniMax (API key from platform.minimax.io; region: 0=intl, 1=china) ---
#define UM_MINIMAX_API_KEY      "placeholder"
#define UM_MINIMAX_REGION       0

// --- Kimi (browser cookie: extract kimi-auth from www.kimi.com; no refresh) ---
#define UM_KIMI_AUTH_TOKEN      "placeholder"

// --- Zai / Zhipu (API key; endpoint: api.z.ai / open.bigmodel.cn / dev.bigmodel.cn) ---
#define UM_ZAI_API_KEY          "placeholder"
#define UM_ZAI_ENDPOINT         "https://api.z.ai"

// Optional computer-side local stats service. Enable the feature in
// platformio.ini with -D UM_ENABLE_LOCAL_STATS before this URL is used.
#define UM_LOCAL_STATS_URL      "http://10.10.50.65:8787"
