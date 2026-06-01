// Copy this file to src/secrets.h and fill in your own values.
// src/secrets.h is excluded from version control via .gitignore.
//
// OAuth bootstrap tokens come from your own logged-in CLIs:
//   Claude: ~/.claude/.credentials.json -> claudeAiOauth.{accessToken,refreshToken,expiresAt}
//   Codex:  ~/.codex/auth.json          -> tokens.{access_token,refresh_token,account_id}, last_refresh
// The device refreshes them autonomously and persists rotated tokens to NVS;
// these values only bootstrap the very first run.

#define UM_WIFI_SSID     "your_wifi_ssid"
#define UM_WIFI_PASSWORD "your_wifi_password"

// POSIX TZ string for the header wall-clock only; quota math is timezone-free.
#define UM_TZ            "UTC0"

// --- Claude (camelCase in the credentials file) ---
#define UM_CLAUDE_ACCESS_TOKEN  "your_claude_access_token"
#define UM_CLAUDE_REFRESH_TOKEN "your_claude_refresh_token"
#define UM_CLAUDE_EXPIRES_AT_MS "0"     // expiresAt in ms epoch, as a string; 0 = force refresh
#define UM_CLAUDE_SUBSCRIPTION  "pro"   // subscriptionType, display only

// --- Codex (snake_case in the auth file) ---
#define UM_CODEX_ACCESS_TOKEN   "your_codex_access_token"
#define UM_CODEX_REFRESH_TOKEN  "your_codex_refresh_token"
#define UM_CODEX_ACCOUNT_ID     ""      // tokens.account_id, optional
#define UM_CODEX_LAST_REFRESH   "0"     // last_refresh ISO8601, or 0 = force refresh
