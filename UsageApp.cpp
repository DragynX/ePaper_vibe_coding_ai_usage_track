#include "UsageApp.h"

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AppLog.h"
#include "BatteryMath.h"
#include "battery_tracker.h"
#include "DragynESPAsyncWiFiManager.h"
#include "IsoTime.h"
#include "UiLang.h"
#include "driver.h"

// Required symbol for DragynESPAsyncWiFiManager — no WPA3 hardening needed here.
void dragynWifiApplyWpa3Hardening(void) {}

namespace usage_monitor {

static const long kSanityFloorEpoch = 1577836800L;
static const unsigned long kAwakeSampleMs = 60000UL;  // awake battery sample cadence

// Green button (GPIO3) is the EXT1 deep-sleep wake source (active-low).
#define UM_BTN_WAKE GPIO_NUM_3

// Survives deep sleep; diagnostics only.
RTC_DATA_ATTR static uint32_t g_wakeCount = 0;
// Wall-clock epoch of the last cold (power-on) boot. RTC-backed so uptime spans
// deep-sleep wakes; 0 until anchored (set once time is valid after a cold boot).
RTC_DATA_ATTR static long g_coldBootEpoch = 0;

// Anti-flicker: fingerprint of what the dashboard last painted + when. A wake
// whose data fingerprint matches skips the repaint entirely — the panel keeps
// its image for free, and the GRAY4 full-refresh flash sequence never runs.
// RTC-backed: survives deep sleep, zeroes on power loss (0 = invalid -> draw).
RTC_DATA_ATTR static uint32_t g_lastDrawHash  = 0;
RTC_DATA_ATTR static long     g_lastDrawEpoch = 0;
// Repaint at least this often so the (excluded) fetch times / reset countdowns
// on screen never go more than an hour stale.
static const long kMaxNoRepaintSec = 3600;

// WiFi fast-connect cache (RTC, survives deep sleep): the AP BSSID + channel from
// the last good connect let a wake skip the all-channel scan and associate
// directly (~2-4s -> ~1s). Keeps DHCP (no static-IP conflict risk). Slow path on
// any miss re-validates and refreshes the cache.
RTC_DATA_ATTR static uint8_t g_wifiBssid[6]   = {0};
RTC_DATA_ATTR static int32_t g_wifiChannel    = 0;
RTC_DATA_ATTR static bool    g_wifiCacheValid = false;

// Per-side fetch cache, NVS-backed (survives deep sleep AND power loss): the last
// captured values to keep displaying, the gate timestamp (at most one real call
// per provider per 5 min), and the 429 backoff deadline. `prov` invalidates the
// slot on a left/right provider swap. ProviderQuota is POD, so the whole struct
// round-trips through NVS putBytes/getBytes. Loaded once in begin().
struct FetchSlot {
  uint8_t       prov;
  long          lastFetch;    // epoch of the last real network call (rate gate)
  long          retryEpoch;   // 429 backoff deadline (0 = none)
  uint8_t       valid;        // 1 = q holds real last-good data
  ProviderQuota q;
  uint8_t       retryCount;   // consecutive-429 count (drives exponential backoff).
                              // Kept LAST so existing FetchSlot{...} inits value-init
                              // it to 0 -> every success/test path resets the schedule.
};
static FetchSlot g_left{}, g_right{};
static const long kRateLimitBackoffSec   = 35 * 60;  // 35 min cap, per Anthropic's sticky 429

// Seconds to back off after the Nth consecutive 429 (1-based). A server Retry-After
// (>=0) always wins; otherwise exponential 60/120/240/480 capped at 35 min (5+).
static long backoff429Secs(uint8_t count, long retryAfter) {
  if (retryAfter >= 0) return retryAfter;
  return (count >= 1 && count <= 4) ? (60L << (count - 1)) : kRateLimitBackoffSec;
}
static const long kMinFetchIntervalSec   = 300;      // fixed 5-min per-provider rate gate

// Mirror log output to both USB CDC (Serial, visible on the COM port) and the
// external UART (Serial1, pins 43/44). EspAppLog accepts one Stream*.
class TeeStream : public Stream {
 public:
  size_t write(uint8_t c) override {
    Serial.write(c);
    return Serial1.write(c);
  }
  size_t write(const uint8_t* buf, size_t n) override {
    Serial.write(buf, n);
    return Serial1.write(buf, n);
  }
  // availableForWrite drives EspAppLog's skip-line check; report the smaller
  // sink so a full buffer on either side skips cleanly instead of blocking.
  int availableForWrite() override {
    const int a = Serial.availableForWrite();
    const int b = Serial1.availableForWrite();
    return a < b ? a : b;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override { Serial.flush(); Serial1.flush(); }
};
static TeeStream logTee;

// Set from ConfigStore so the static battery reader (a function pointer) can use
// the user-configured 100% level.
static uint16_t g_battFullMv = 4200;
static int      g_battMv     = -1;   // last actual battery voltage (mV), -1 = unread

// Battery: GPIO1 ADC behind a divider (halves the voltage), gated by GPIO21
// (must be HIGH to read — Seeed reTerminal E series wiki).
static int readBatteryPercent() {
  // ~1 s freshness cache: the battery can't move in a second, and several callers
  // sample per wake (currentStatus, the /api/status battPct_ + battDays_ pair).
  // Each real read toggles GPIO21 + a 10 ms ADC settle, so dedup the bursts.
  static uint32_t lastMs = 0;
  static int      lastPct = -1;
  if (lastPct >= 0 && (uint32_t)(millis() - lastMs) < 1000) return lastPct;
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(10);
  uint32_t mv = 0;
  for (int i = 0; i < 16; ++i) mv += analogReadMilliVolts(1);
  digitalWrite(21, LOW);
  const int adcMv = static_cast<int>(mv / 16);
  g_battMv = adcMv * 2;   // hardware halves the battery voltage
  lastMs = millis();
  lastPct = umBatteryPercent(adcMv, g_battFullMv);
  return lastPct;
}

// Actual battery voltage (mV) from the most recent percent read.
static int readBatteryMv() { return g_battMv; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* providerName(uint8_t prov) {
  switch (prov) {
    case UM_PROV_CLAUDE:     return "Claude";
    case UM_PROV_CODEX:      return "Codex";
    case UM_PROV_COPILOT:    return "Copilot";
    case UM_PROV_MINIMAX:    return "MiniMax";
    case UM_PROV_KIMI:       return "Kimi";
    case UM_PROV_ZAI:        return "Zai";
    case UM_PROV_CLAUDEPLAT: return "ClaudePlat";
    default:                 return "—";
  }
}

static const char* providerKey(uint8_t prov) {
  switch (prov) {
    case UM_PROV_CLAUDE:     return "claude";
    case UM_PROV_CODEX:      return "codex";
    case UM_PROV_COPILOT:    return "copilot";
    case UM_PROV_MINIMAX:    return "minimax";
    case UM_PROV_KIMI:       return "kimi";
    case UM_PROV_ZAI:        return "zai";
    case UM_PROV_CLAUDEPLAT: return "claudeplat";
    default:                 return "unknown";
  }
}

// The user-entered credential the provider's token chain derives from; used
// as the TokenStore cache seed (see TokenStore::seedMatches).
static String seedFor(uint8_t prov, const ConfigStore& cfg) {
  switch (prov) {
    case UM_PROV_CLAUDE:  return cfg.claudeRt();
    case UM_PROV_CODEX:   return cfg.codexRt();
    case UM_PROV_COPILOT: return cfg.copilotPat();
    case UM_PROV_MINIMAX: return cfg.minimaxKey();
    case UM_PROV_KIMI:    return cfg.kimiToken();
    case UM_PROV_ZAI:     return cfg.zaiKey();
    default:              return String();
  }
}

static bool isConfigured(uint8_t prov, const ConfigStore& cfg) {
  switch (prov) {
    case UM_PROV_CLAUDE:     return cfg.claudeAt().length() > 0;
    case UM_PROV_CODEX:      return cfg.codexAt().length() > 0;
    case UM_PROV_COPILOT:    return cfg.copilotPat().length() > 0;
    case UM_PROV_MINIMAX:    return cfg.minimaxKey().length() > 0;
    case UM_PROV_KIMI:       return cfg.kimiToken().length() > 0;
    case UM_PROV_ZAI:        return cfg.zaiKey().length() > 0;
    case UM_PROV_CLAUDEPLAT: return cfg.claudePlatKey().length() > 0;
    default:                 return false;
  }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UsageApp::UsageApp() : server_(80) {}

// ---------------------------------------------------------------------------
// configureProviders — wire up one side (left or right) based on prov ID
// ---------------------------------------------------------------------------

// Wire one provider id into the given OAuth client + usage client + auth state,
// using the member auth/client objects. Shared by configureProviders (live
// left/right) and testProvider (scratch wiring for a token test).
void UsageApp::wireProvider(uint8_t prov, OAuthClient& oauth,
                            UsageClientBase*& client, AuthState*& authPtr) {
  // Resume the cached (rotated) token chain only while it derives from the
  // credentials currently in settings; new credentials invalidate the cache.
  auto loadCache = [this](const char* pk, AuthState& auth, const String& seed) {
    if (store_.seedMatches(pk, seed)) {
      store_.load(pk, auth);
      sysLog("[tok] %s: cache resumed", pk);
    } else {
      store_.clearProvider(pk);
      sysLog("[tok] %s: new credentials from settings, cache cleared", pk);
    }
  };

  {
    switch (prov) {
      case UM_PROV_CLAUDE:
        claudeAuth_.accessToken       = cfgStore_.claudeAt();
        claudeAuth_.refreshToken      = cfgStore_.claudeRt();
        {
          // User may paste ms or seconds; values past ~year 5138 must be ms.
          const long long e = atoll(cfgStore_.claudeExp().c_str());
          claudeAuth_.expiryEpoch = (e > 100000000000LL)
              ? static_cast<long>(e / 1000) : static_cast<long>(e);
        }
        claudeAuth_.usesAbsoluteExpiry = true;
        loadCache("claude", claudeAuth_, cfgStore_.claudeRt());
        oauth.configure(&http_, &claudeProvider_, &claudeAuth_);
        claudeClient_.configure(&oauth);
        authPtr = &claudeAuth_;
        client  = &claudeClient_;
        break;

      case UM_PROV_CODEX:
        codexAuth_.accessToken        = cfgStore_.codexAt();
        codexAuth_.refreshToken       = cfgStore_.codexRt();
        codexAuth_.accountId          = cfgStore_.codexAid();
        codexAuth_.expiryEpoch        = umParseIso8601(cfgStore_.codexLr().c_str());
        codexAuth_.usesAbsoluteExpiry = false;
        loadCache("codex", codexAuth_, cfgStore_.codexRt());
        oauth.configure(&http_, &codexProvider_, &codexAuth_);
        codexClient_.configure(&oauth);
        authPtr = &codexAuth_;
        client  = &codexClient_;
        break;

      case UM_PROV_COPILOT:
        copilotAuth_.accessToken = cfgStore_.copilotPat();
        loadCache("copilot", copilotAuth_, cfgStore_.copilotPat());
        oauth.configure(&http_, &copilotProvider_, &copilotAuth_);
        copilotClient_.configure(&oauth);
        authPtr = &copilotAuth_;
        client  = &copilotClient_;
        break;

      case UM_PROV_MINIMAX:
        minimaxAuth_.accessToken = cfgStore_.minimaxKey();
        loadCache("minimax", minimaxAuth_, cfgStore_.minimaxKey());
        oauth.configure(&http_, &minimaxProvider_, &minimaxAuth_);
        minimaxBaseUrl_ = (cfgStore_.minimaxRegion() == 1)
            ? "https://api.minimaxi.com" : "https://api.minimax.io";
        minimaxClient_.configure(&oauth, minimaxBaseUrl_.c_str());
        authPtr = &minimaxAuth_;
        client  = &minimaxClient_;
        break;

      case UM_PROV_KIMI:
        kimiAuth_.accessToken = cfgStore_.kimiToken();
        loadCache("kimi", kimiAuth_, cfgStore_.kimiToken());
        oauth.configure(&http_, &kimiProvider_, &kimiAuth_);
        kimiClient_.configure(&oauth);
        authPtr = &kimiAuth_;
        client  = &kimiClient_;
        break;

      case UM_PROV_ZAI:
        zaiAuth_.accessToken = cfgStore_.zaiKey();
        loadCache("zai", zaiAuth_, cfgStore_.zaiKey());
        oauth.configure(&http_, &zaiProvider_, &zaiAuth_);
        zaiEndpointStr_ = cfgStore_.zaiEndpoint();
        zaiClient_.configure(&oauth, zaiEndpointStr_.c_str());
        authPtr = &zaiAuth_;
        client  = &zaiClient_;
        break;

      case UM_PROV_CLAUDEPLAT: {
        const double prepaidCents = atof(cfgStore_.claudePlatPrepaid().c_str()) * 100.0;
        const bool spendMode = (cfgStore_.claudePlatMode() == "spend");
        int windowDays = spendMode ? atoi(cfgStore_.claudePlatSpendWin().c_str()) : 30;
        if (windowDays != 7 && windowDays != 14 && windowDays != 30) windowDays = 30;
        claudePlatClient_.configure(&http_, cfgStore_.claudePlatKey(),
                                    cfgStore_.claudePlatOrg(), prepaidCents,
                                    windowDays, spendMode);
        authPtr = nullptr;       // admin key never refreshed
        client  = &claudePlatClient_;
        break;
      }

      default:
        authPtr = nullptr;
        client  = nullptr;
        break;
    }
  }
}

void UsageApp::configureProviders() {
  wireProvider(cfgStore_.leftProvider(),  leftOAuth_,  leftClient_,  leftAuthPtr_);
  sysLog("[prov] left=%s at_len=%d rt_len=%d",
         providerName(cfgStore_.leftProvider()),
         leftAuthPtr_  ? (int)leftAuthPtr_->accessToken.length()  : -1,
         leftAuthPtr_  ? (int)leftAuthPtr_->refreshToken.length() : -1);
  wireProvider(cfgStore_.rightProvider(), rightOAuth_, rightClient_, rightAuthPtr_);
  sysLog("[prov] right=%s at_len=%d rt_len=%d",
         providerName(cfgStore_.rightProvider()),
         rightAuthPtr_ ? (int)rightAuthPtr_->accessToken.length()  : -1,
         rightAuthPtr_ ? (int)rightAuthPtr_->refreshToken.length() : -1);
}

// ---------------------------------------------------------------------------
// Token test + per-credential status
// ---------------------------------------------------------------------------

enum { kCredNone = 0, kCredOk = 1, kCredFail = 2, kCredTesting = 3 };

// Composes the on-screen failure reason; defined later in the file. Declared here
// so testProvider can reuse it to capture the same text for the Credentials page.
static void buildFailReason(char* buf, size_t n, const ProviderQuota& p,
                            int status, const String& err);

// Validate one provider's stored credentials by making its real usage call into
// a scratch wiring. Persists any rotated token on success. Returns kCredOk/Fail.
uint8_t UsageApp::testProvider(uint8_t prov) {
  // Respect the 429 backoff even on a manual Credentials-tab test: poking a
  // rate-limited endpoint just re-arms the 429. The backoff lives in the matching
  // displayed side's slot; report the remaining time instead of fetching.
  {
    const long tnow = now();
    const FetchSlot* slot = (prov == cfgStore_.leftProvider())  ? &g_left
                          : (prov == cfgStore_.rightProvider()) ? &g_right : nullptr;
    if (slot && slot->prov == prov && slot->retryEpoch > 0 &&
        tnow > kSanityFloorEpoch && tnow < slot->retryEpoch) {
      const long mins = (slot->retryEpoch - tnow + 59) / 60;
      char rb[64];
      snprintf(rb, sizeof(rb), "Rate limited (429) - retry in %ldm", mins);
      credError_[prov]  = rb;
      credStatus_[prov] = kCredFail;
      sysLog("[test] %s rate-limited, %lds left -> skip", providerName(prov),
             slot->retryEpoch - tnow);
      return kCredFail;
    }
  }
  OAuthClient scratch;
  UsageClientBase* client = nullptr;
  AuthState* authPtr = nullptr;
  wireProvider(prov, scratch, client, authPtr);
  if (!client) return kCredFail;
  ProviderQuota tmp;
  const bool ok = client->fetch(now(), tmp);
  if (ok) {
    credError_[prov] = "";
  } else {
    char rb[96];
    buildFailReason(rb, sizeof(rb), tmp, http_.lastStatus(), http_.lastError());
    credError_[prov] = rb;   // provider response text for the Credentials page
  }
  if (ok && authPtr) {
    store_.save(providerKey(prov), *authPtr, seedFor(prov, cfgStore_));
  }
  // Reuse this fresh fetch for a displayed side so the repaint's refreshAll()
  // doesn't immediately re-fetch the same provider (a duplicate HTTPS round trip
  // and a needless second hit on the rate-limited Claude usage endpoint).
  if (ok) {
    if (prov == UM_PROV_CLAUDE) {
      tmp.hasPlan = true;
      strncpy(tmp.planType, cfgStore_.claudeSub().c_str(), sizeof(tmp.planType) - 1);
    }
    // Persist the tested side's slot so the Credentials test counts as the
    // window's one call (gates the next scheduled fetch) and its result is the
    // saved last-good that survives the reboot the user often does after saving.
    const long tn = now();
    if (prov == cfgStore_.leftProvider()) {
      snapshot_.left = tmp; leftFresh_ = true;
      g_left = FetchSlot{ prov, tn, 0, 1, tmp }; saveFetchSlot(true);
    }
    if (prov == cfgStore_.rightProvider()) {
      snapshot_.right = tmp; rightFresh_ = true;
      g_right = FetchSlot{ prov, tn, 0, 1, tmp }; saveFetchSlot(false);
    }
  }
  sysLog("[test] %s -> %s (status=%d)", providerName(prov), ok ? "OK" : "FAIL",
         http_.lastStatus());
  return ok ? kCredOk : kCredFail;
}

void UsageApp::runPendingTokenTests() {
  const uint8_t mask = cfgStore_.pendingTestMask();
  cfgStore_.clearPendingTest();
  if (!mask) return;
  for (uint8_t prov = 1; prov <= 7; ++prov) {
    // Test only checked AND configured providers — a checked-but-empty provider
    // would just fail a pointless HTTPS round trip; re-testing a stored token
    // works because a saved token makes isConfigured() true.
    if ((mask & (uint8_t)(1u << prov)) && isConfigured(prov, cfgStore_)) {
      credStatus_[prov] = testProvider(prov);
    }
  }
  // testProvider repointed the member usage clients at a now-destroyed stack
  // OAuthClient; re-wire them here so the invariant is self-enforcing (no
  // dangling pointer if anything fetches before the caller reconfigures).
  configureProviders();
}

// Minimal JSON string escaper for provider error text (quotes/backslashes/control
// chars). credStatusJson hand-builds JSON, so the error field needs escaping.
static String jsonEsc(const String& in) {
  String o; o.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:
        if ((unsigned char)c < 0x20) { char b[7]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += c;
    }
  }
  return o;
}

String UsageApp::credStatusJson() {
  static const char* kKeys[8] = { "", "claude", "codex", "copilot", "minimax",
                                  "kimi", "zai", "claudeplat" };
  static const char* kVal[4]  = { "none", "ok", "fail", "testing" };
  String s = "{";
  for (uint8_t p = 1; p <= 7; ++p) {
    if (p > 1) s += ",";
    // No stored token -> "none" (white). A stored-but-untested token reports
    // "set" so the UI still shows Clear Token (testing is now opt-in, so an
    // untested token would otherwise stay "none" and hide the button).
    const bool cfg = isConfigured(p, cfgStore_);
    const uint8_t st = cfg ? credStatus_[p] : 0;
    const char* v = (cfg && st == kCredNone) ? "set" : kVal[st <= 3 ? st : 0];
    s += "\""; s += kKeys[p]; s += "\":\""; s += v; s += "\"";
    // Surface the provider's response text for a failed test.
    if (cfg && st == kCredFail && credError_[p].length()) {
      s += ",\""; s += kKeys[p]; s += "_err\":\""; s += jsonEsc(credError_[p]); s += "\"";
    }
  }
  s += "}";
  return s;
}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------

void UsageApp::begin() {
  Serial1.setTxBufferSize(4096);   // default ~256 drops burst log lines
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
  {
    espapplog::BeginConfig logCfg;
    logCfg.baud           = 115200;
    logCfg.txBufferSize   = 4096;
    logCfg.txTimeoutMs    = 0;
    logCfg.bootUsbWaitMs  = 0;
    logCfg.startupDelayMs = 0;
    espapplog::begin(logCfg);
    espapplog::setStream(&logTee);   // USB CDC + UART pins 43/44
  }
  sysLog("\n[boot] UsageMonitor v" UM_VERSION);

  // Release any pin-hold from before deep sleep, then drive the LED.
  gpio_hold_dis((gpio_num_t)UM_LED_PIN);
  gpio_deep_sleep_hold_dis();
  pinMode(UM_LED_PIN, OUTPUT);
  digitalWrite(UM_LED_PIN, LOW);

  // Buttons: GPIO3 (green) is the EXT1 wake source; GPIO4/5 (white) reserved.
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT);
  pinMode(5, INPUT);

  // Wake cause drives the awake-window length (radio-on time = the dominant
  // battery cost): cold power-on gets 5 min for first setup; the green button
  // (the "I want the settings page" signal) 2 min; a plain timer wake only ~3 s —
  // just long enough for an actively-polling browser to fire keepalive and
  // re-extend. Nobody is looking at an unattended timer wake.
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool firstBoot = (cause != ESP_SLEEP_WAKEUP_TIMER &&
                          cause != ESP_SLEEP_WAKEUP_EXT1);
  awakeWindowMs_ = firstBoot                          ? 5UL * 60UL * 1000UL
                 : (cause == ESP_SLEEP_WAKEUP_EXT1)   ? 2UL * 60UL * 1000UL
                                                      : 3UL * 1000UL;
  if (firstBoot) g_coldBootEpoch = 0;   // re-anchor uptime on a real cold boot
  ++g_wakeCount;
  sysLog("[boot] wake cause=%d firstBoot=%d count=%u window=%lums",
         (int)cause, firstBoot ? 1 : 0, (unsigned)g_wakeCount, awakeWindowMs_);

  cfgStore_.begin();
  loadFetchSlots();   // last-good values + gate stamps before the first draw/fetch
  // Apply the saved timezone immediately so the RTC time (kept across deep
  // sleep, in UTC) renders as local time right away — before NTP re-applies it.
  setenv("TZ", cfgStore_.tz().c_str(), 1);
  tzset();
  g_battFullMv = cfgStore_.battFullMv();
  sysLog("[sleep] boot_id=%u deep_sleep=%d", (unsigned)g_wakeCount,
         cfgStore_.deepSleepEnabled() ? 1 : 0);
  http_.configure(15000);   // providers answer in 1-3s; 45s only lengthened bad wakes
  configureProviders();
  localStats_.configure(&http_, cfgStore_.localStatsUrl().c_str());

  ui_.begin();
  ui_.setDarkMode(cfgStore_.darkMode());
  ui_.setFont(cfgStore_.uiFont());
  ui_.setSmoothing(cfgStore_.uiAa());
  ui_.setSharpness(cfgStore_.uiSharp());
  ui_.setTextWeight(cfgStore_.uiWeight());
  ui_.setSmallTextCrisp(cfgStore_.uiSmallCrisp());
  // No "Connecting…" splash on cold boot: e-paper is bistable, so the previous
  // dashboard stays on screen (blank only on the very first boot) while WiFi +
  // fetch run, then ONE clean full refresh shows the new data — cold boot now
  // costs a single GRAY4 flash. The portal screen below still draws on failure.
  if (!ensureWiFi(15000)) {
    sysLog("[wifi] STA failed — launching portal");
    WiFi.disconnect(false);
    delay(100);
    sysLog("[wifi] starting AP 'UsageMonitor'");
    ui_.drawBoot("Connect to 'UsageMonitor' AP", currentStatus(), now());
    AsyncWebServer portalServer(80);
    DNSServer dns;
    AsyncWiFiManager wm(&portalServer, &dns);
    wm.setDebugOutput(true);
    wm.setConfigPortalTimeout(0);
    sysLog("[wifi] calling startConfigPortal");
    bool saved = wm.startConfigPortal("UsageMonitor");
    sysLog("[wifi] portal returned saved=%d status=%d", (int)saved, (int)WiFi.status());
    delay(500);
    ESP.restart();
  }

  // Web server runs whenever we are awake (settings reachable during both the
  // cold-boot and the 30 s wake windows). onSaved runs in the async server task
  // — it only flips a flag; the e-paper repaint happens in loop() (not async-safe).
  MDNS.begin("usagemonitor");
  settings_.begin(&server_, &cfgStore_, &readBatteryPercent,
                  [this]() { redrawPending_ = true; },
                  [this]() { return credStatusJson(); },
                  [this]() { extendAwake("web action"); },   // keepalive / clear / save
                  [this]() { sleepNow_ = true; sysLog("[sleep] sleep-now requested (web)"); },
                  [this]() { return sleepInSec(); },         // seconds until sleep
                  [this]() { return (int)bootId(); },        // session token (wake count)
                  &readBatteryMv,                            // actual battery mV
                  [this]() -> int {  // hours: >=0 live, -2 calib, -3 charging, <=-10 placeholder
                    const bool deep = cfgStore_.deepSleepEnabled();
                    const float d = deep
                        ? battery_tracker_get_days_remaining(cfgStore_.refreshSec())
                        : battery_tracker_get_days_remaining_awake();
                    if (d >= 0.0f) return (int)(d * 24.0f + 0.5f);
                    if (battery_is_charging()) return -3;
                    const int soc = readBatteryPercent();
                    const float ph = (soc >= 0) ? cfgStore_.battEstLookup(deep, soc) : -1.0f;
                    if (ph >= 0.0f) return -((int)(ph * 24.0f + 0.5f) + 10);  // learned placeholder
                    return -2;
                  },
                  [this](int on, int font, int dark, int all, int crisp, int sz) {  // font-test
                    onFontTest(on, font, dark, all, crisp, sz);
                  },
                  [this]() -> long { return uptimeSec(); });   // uptime since cold boot
  server_.begin();
  sysLog("[settings] http://usagemonitor.local or http://%s",
         WiFi.localIP().toString().c_str());

  syncTime();   // non-blocking: starts background SNTP, no splash screen
  // TLS cert validation needs a real clock. Warm wakes keep RTC time across deep
  // sleep; a cold boot starts at 1970, so wait briefly for the first SNTP packet
  // before the first HTTPS fetch (else every cert reads as not-yet-valid).
  if (firstBoot) waitForClock(8000);
  refreshAll();
  lastRefreshMs_ = millis();
  awakeStartMs_  = millis();   // awake window starts after fetch + display
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------

void UsageApp::loop() {
  const unsigned long ms = millis();
  const unsigned long refreshMs = (unsigned long)cfgStore_.refreshSec() * 1000UL;

  // A settings save requested a repaint (dark-mode toggle, new credentials,
  // etc.). Re-arm any stopped providers, apply the palette, and refetch now —
  // all on the main task (SPI / the e-paper are not safe from the async task).
  // Font Testing playground owns the screen while on; service its repaint first.
  if (ftRedraw_) {
    ftRedraw_ = false;
    if (fontTestOn_) {
      if (ftAllView_) {
        ui_.drawAllFonts(kTestSizes[ftSizeIdx_], ftDark_, ftCrisp_);
      } else {
        ui_.drawFontTest(ftFont_, kTestSizes[ftSizeIdx_], ftDark_,
                         WiFi.localIP().toString());
      }
    } else if (!redrawPending_) {
      // Restore the normal renderer state + redraw from the last snapshot. Skip
      // when a settings Save is pending — its branch below does the single repaint,
      // avoiding a back-to-back double flash on playground-exit-with-save.
      ui_.setDarkMode(cfgStore_.darkMode());
      ui_.setFont(cfgStore_.uiFont());
      ui_.setSmoothing(cfgStore_.uiAa());
      ui_.setSharpness(cfgStore_.uiSharp());
      ui_.setTextWeight(cfgStore_.uiWeight());
      ui_.setSmallTextCrisp(cfgStore_.uiSmallCrisp());
      ui_.drawDashboard(snapshot_, currentStatus(), now());
      g_lastDrawHash = 0;       // direct draw -> invalidate the skip fingerprint
    }
    lastRefreshMs_ = millis();   // don't immediately refetch on the next tick
    extendAwake("font-test");
  }

  // A settings save requested a repaint (dark-mode toggle, new credentials,
  // etc.). Suppressed while the playground owns the screen (re-applies on exit).
  if (redrawPending_ && !fontTestOn_) {
    redrawPending_ = false;
    // Re-apply the timezone so a TZ change takes effect immediately (the clock is
    // kept in UTC; only this localtime offset changed) — without it the new TZ
    // would not show until the next reboot/deep-sleep wake re-runs begin().
    setenv("TZ", cfgStore_.tz().c_str(), 1);
    tzset();
    leftFailCount_ = rightFailCount_ = 0;
    leftDisabled_  = rightDisabled_  = false;
    leftFailReason_[0] = rightFailReason_[0] = '\0';
    ui_.setDarkMode(cfgStore_.darkMode());
    ui_.setFont(cfgStore_.uiFont());
    ui_.setSmoothing(cfgStore_.uiAa());
    ui_.setSharpness(cfgStore_.uiSharp());
    ui_.setTextWeight(cfgStore_.uiWeight());
    ui_.setSmallTextCrisp(cfgStore_.uiSmallCrisp());
    sysLog("[ui] settings applied (dark=%d), breakers reset",
           cfgStore_.darkMode() ? 1 : 0);
    if (ensureWiFi(10000)) {
      runPendingTokenTests();   // test changed creds (repoints member clients)
      configureProviders();     // always re-wire: left/right or tokens may have changed
      refreshAll(true);         // settings may change pixels without changing data
    } else {
      ui_.drawDashboard(snapshot_, currentStatus(), now());
      g_lastDrawHash = 0;       // direct draw -> invalidate the skip fingerprint
    }
    g_battFullMv = cfgStore_.battFullMv();   // pick up a changed battery setting
    sysLog("[sleep] settings applied: deep_sleep=%d window=%lus",
           cfgStore_.deepSleepEnabled() ? 1 : 0, awakeWindowMs_ / 1000UL);
    extendAwake("save");        // a settings save keeps the device awake 2 min
  }

  // Periodic refresh while awake (suppressed while the font-test playground is up).
  if (!fontTestOn_ && ms - lastRefreshMs_ >= refreshMs) {
    lastRefreshMs_ = ms;
    if (ensureWiFi(10000)) refreshAll();
  }

  // Countdown tick (~every 10 s) for diagnostics while deep sleep is enabled.
  const unsigned long nowMs = millis();
  uptimeSec();   // anchor cold-boot epoch as soon as the clock is valid
  if (cfgStore_.deepSleepEnabled() && nowMs - lastSleepTickMs_ >= 10000UL) {
    lastSleepTickMs_ = nowMs;
    sysLog("[sleep] awake, sleeps in %ds", sleepInSec());
  }

  // Awake battery sampling (deep-sleep mode samples in enterDeepSleep). Sample
  // SOC+mV every minute, >=10 s after boot per the Seeed ADC-settle rule. The mV
  // trend (not the unreliable HWCDC plug state) decides charging vs discharging.
  if (!cfgStore_.deepSleepEnabled() && nowMs >= 10000UL &&
      nowMs - lastBattSampleMs_ >= kAwakeSampleMs) {
    lastBattSampleMs_ = nowMs;
    const long ep  = now();
    const int  soc = readBatteryPercent();   // also refreshes g_battMv
    const int  mv  = readBatteryMv();
    battery_tracker_update_awake((float)soc, mv,
        ep >= kSanityFloorEpoch ? (uint32_t)ep : 0);
    sysLog("[batt] awake soc=%d mv=%d days=%.1f chg=%d", soc, mv,
           battery_tracker_get_days_remaining_awake(),
           battery_is_charging() ? 1 : 0);
  }

  // Deep sleep (when enabled): on an explicit web "Sleep", or after the awake
  // window elapses, sleep until the timer or green button wakes us. Use a fresh
  // millis() and a SIGNED compare — `ms` (captured at loop top) can be stale after
  // a multi-second refresh, and an unsigned underflow would sleep instantly.
  const bool windowElapsed = (long)(nowMs - awakeStartMs_) >= (long)awakeWindowMs_;
  if (cfgStore_.deepSleepEnabled() && (sleepNow_ || windowElapsed)) {
    sysLog("[sleep] entering deep sleep (reason=%s)", sleepNow_ ? "user" : "window-elapsed");
    enterDeepSleep();   // does not return
  }

  delay(50);
}

// A user web action keeps the device awake for at least 2 minutes.
void UsageApp::extendAwake(const char* reason) {
  awakeStartMs_  = millis();
  if (awakeWindowMs_ < 120000UL) awakeWindowMs_ = 120000UL;
  sysLog("[sleep] window extended to %lus by %s (sleeps in %ds)",
         awakeWindowMs_ / 1000UL, reason ? reason : "?", sleepInSec());
}

// Font Testing playground size ladder (px). Index moves via Next/Previous Page.
const int UsageApp::kTestSizes[11] = {6, 7, 8, 10, 12, 14, 16, 18, 24, 30, 34};

// Web control (async task): store runtime state, flag a repaint for loop().
// dark<0 / all<0 = "unchanged" so live size/font steps keep the saved dark + view.
void UsageApp::onFontTest(int on, int font, int dark, int all, int crisp, int sizeIdx) {
  fontTestOn_ = (on != 0);
  if (font >= 0 && font < TextRenderer::fontCount()) ftFont_ = font;
  if (dark  >= 0) ftDark_    = (dark != 0);
  if (all   >= 0) ftAllView_ = (all != 0);
  if (crisp >= 0) ftCrisp_   = (crisp != 0);
  const int n = (int)(sizeof(kTestSizes) / sizeof(kTestSizes[0]));
  ftSizeIdx_ = sizeIdx < 0 ? 0 : (sizeIdx >= n ? n - 1 : sizeIdx);
  ftRedraw_ = true;
  sysLog("[fonttest] on=%d font=%d size=%dpx dark=%d all=%d crisp=%d", (int)fontTestOn_,
         ftFont_, kTestSizes[ftSizeIdx_], (int)ftDark_, (int)ftAllView_, (int)ftCrisp_);
}

// Seconds until deep sleep, or -1 when deep sleep is disabled.
int UsageApp::sleepInSec() {
  if (!cfgStore_.deepSleepEnabled()) return -1;
  const long elapsed = (long)(millis() - awakeStartMs_);
  const long remain  = (long)awakeWindowMs_ - elapsed;
  return remain > 0 ? (int)(remain / 1000) : 0;
}

uint32_t UsageApp::bootId() { return g_wakeCount; }

// ---------------------------------------------------------------------------
// enterDeepSleep
// ---------------------------------------------------------------------------

void UsageApp::enterDeepSleep() {
  const uint32_t sec = cfgStore_.refreshSec();

  // 1. Let the ePaper finish its last refresh (update() blocks, but guard anyway).
  delay(50);

  // 2. Stop the web server (SettingsServer/AsyncTCP) before tearing down WiFi.
  server_.end();

  // 3. Clean WiFi teardown — without esp_wifi_stop()/deinit() the device draws
  //    high sleep current and is unstable on the next wake (the wake bug).
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_wifi_deinit();

  // 3b. Battery-runtime tracker: read SOC now — after WiFi is fully off (clean
  //     ADC) and >=10 s since wake (Seeed timing rule). Update the estimate.
  if (millis() < 10000UL) delay(10000UL - millis());
  const float soc = (float)readBatteryPercent();   // existing reader — unchanged
  battery_charging_update(readBatteryMv());         // mV trend -> charge/discharge
  const bool btnWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  battery_tracker_update(soc, btnWake);
  sysLog("[batt] cycle=%u btnwakes=%u soc=%.0f mv=%d days=%.1f chg=%d",
         battery_tracker_get_cycles(), battery_tracker_get_button_wakes(), soc,
         readBatteryMv(), battery_tracker_get_days_remaining(sec),
         battery_is_charging() ? 1 : 0);

  // 4. Park the LED off and hold the pin so it doesn't float during sleep.
  digitalWrite(UM_LED_PIN, HIGH);   // active-low: HIGH = off
  gpio_hold_en((gpio_num_t)UM_LED_PIN);
  gpio_deep_sleep_hold_en();

  // 5. Two wake sources: RTC timer (refresh interval) + GPIO3 green button
  //    (active-low -> ANY_LOW). Both trigger the same wake cycle.
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_sleep_enable_ext1_wakeup(1ULL << UM_BTN_WAKE, ESP_EXT1_WAKEUP_ANY_LOW);

  sysLog("[sleep] sleeping %us (timer+btn)", (unsigned)sec);
  delay(20);                  // flush the log line before powering down
  esp_deep_sleep_start();     // does not return
}

// ---------------------------------------------------------------------------
// WiFi helpers
// ---------------------------------------------------------------------------

static void cacheWifiBss() {
  uint8_t* b = WiFi.BSSID();
  if (!b) { g_wifiCacheValid = false; return; }
  memcpy(g_wifiBssid, b, 6);
  g_wifiChannel    = WiFi.channel();
  g_wifiCacheValid = true;
}

bool UsageApp::ensureWiFi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) {
    sysLog("[wifi] already connected");
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Fast path: associate directly to the cached AP (BSSID + channel) using the
  // creds stored in NVS, skipping the scan. Bounded to ~4s, then fall back.
  if (g_wifiCacheValid) {
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0]) {
      sysLog("[wifi] fast-connect ch=%d...", (int)g_wifiChannel);
      WiFi.begin(reinterpret_cast<const char*>(cfg.sta.ssid),
                 reinterpret_cast<const char*>(cfg.sta.password),
                 g_wifiChannel, g_wifiBssid);
      const uint32_t start = millis();
      const uint32_t fast = timeoutMs < 4000 ? timeoutMs : 4000;
      while (WiFi.status() != WL_CONNECTED && millis() - start < fast) delay(50);
      if (WiFi.status() == WL_CONNECTED) {
        cacheWifiBss();
        sysLog("[wifi] fast-connect ok: %s (%ums)", WiFi.localIP().toString().c_str(),
               (unsigned)(millis() - start));
        return true;
      }
    }
    g_wifiCacheValid = false;           // stale (AP moved/off) -> generic reconnect
    WiFi.disconnect(false);
  }

  sysLog("[wifi] connecting (timeout=%ums)...", (unsigned)timeoutMs);
  WiFi.begin();
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    cacheWifiBss();
    sysLog("[wifi] connected: %s", WiFi.localIP().toString().c_str());
    return true;
  }
  sysLog("[wifi] failed after %ums (status=%d)",
         (unsigned)(millis() - start), (int)WiFi.status());
  return false;
}

void UsageApp::syncTime() {
  // Kick off SNTP in the background and return immediately — never block, never
  // show it on screen. The ESP32 RTC keeps time between fetches and across deep
  // sleep, so warm wakes are already valid; SNTP corrects the clock when the
  // first packet lands (cold boot) and re-syncs periodically on its own.
  configTzTime(cfgStore_.tz().c_str(), "pool.ntp.org", "time.nist.gov");
  timeSynced_ = (static_cast<long>(time(nullptr)) >= kSanityFloorEpoch);
  sysLog("[ntp] background sync started (rtc_valid=%d)", timeSynced_ ? 1 : 0);
}

long UsageApp::now() { return static_cast<long>(time(nullptr)); }

// Block up to timeoutMs for SNTP to deliver a real clock (epoch past the sanity
// floor). Needed before the first HTTPS fetch on a cold boot so TLS cert dates
// validate; warm wakes already have valid RTC time and return immediately.
void UsageApp::waitForClock(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (static_cast<long>(time(nullptr)) < kSanityFloorEpoch &&
         millis() - start < timeoutMs) {
    delay(100);
  }
  timeSynced_ = (static_cast<long>(time(nullptr)) >= kSanityFloorEpoch);
  sysLog("[ntp] clock %s after %ums", timeSynced_ ? "valid" : "INVALID",
         (unsigned)(millis() - start));
}

// Seconds since the last cold (power-on) boot, spanning deep-sleep wakes. Anchors
// the cold-boot epoch on the first call with a valid clock; 0 until then.
long UsageApp::uptimeSec() {
  const long t = now();
  if (g_coldBootEpoch == 0 && t >= kSanityFloorEpoch) g_coldBootEpoch = t;
  return (g_coldBootEpoch > 0 && t >= g_coldBootEpoch) ? t - g_coldBootEpoch : 0;
}

UiStatus UsageApp::currentStatus() {
  UiStatus s;
  s.wifiConnected  = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = readBatteryPercent();
  if (s.wifiConnected) s.ipAddress = WiFi.localIP().toString();
  s.lastFetchEpoch = lastFetchEpoch_;
  s.nextFetchEpoch = lastFetchEpoch_ > 0
      ? lastFetchEpoch_ + (long)cfgStore_.refreshSec() : 0;
  // Battery runtime estimate: only show days when the device is actively
  // discharging (a real SOC drop measured over >=3 deep-sleep cycles). When on
  // USB / external power show "Charging" instead. No reliable VBUS pin on E1001,
  // so detect external power as: USB physically plugged (HWCDC) OR the tracker
  // saw no SOC drop after several cycles (battery being held).
  // Runtime estimate, same gate in both modes: a real SOC drop proves discharge
  // (estimate wins); else the mV trend decides "Charging" vs "Calibrating…". No
  // USB flag — HWCDC isPlugged() is unreliable on the E1001.
  const bool  deep = cfgStore_.deepSleepEnabled();
  const int   soc  = s.batteryPercent;
  const float days = deep ? battery_tracker_get_days_remaining(cfgStore_.refreshSec())
                          : battery_tracker_get_days_remaining_awake();
  if (days >= 0.0f) {
    s.batteryDays = days;   s.batteryCharging = false;          // live estimate
    s.batteryEstPlaceholder = false;
    if (soc >= 0) cfgStore_.battEstRecord(deep, soc, days);     // learn this SOC point
  } else if (battery_is_charging()) {
    s.batteryDays = -2.0f;  s.batteryCharging = true;           // mV rising
    s.batteryEstPlaceholder = false;
  } else {
    // Calibrating: show a learned placeholder (italic) if we have one for this
    // SOC + mode (or the carried last estimate); else fall back to "Calibrating".
    const float ph = (soc >= 0) ? cfgStore_.battEstLookup(deep, soc) : -1.0f;
    s.batteryCharging = false;
    if (ph >= 0.0f) { s.batteryDays = ph;    s.batteryEstPlaceholder = true; }
    else            { s.batteryDays = -1.0f; s.batteryEstPlaceholder = false; }
  }
  s.deepSleepOn = deep;   // header moon indicator
  return s;
}

// ---------------------------------------------------------------------------
// Provider name / snapshot
// ---------------------------------------------------------------------------

void UsageApp::setProviderNames() {
  strncpy(snapshot_.left.name,  providerName(cfgStore_.leftProvider()),
          sizeof(snapshot_.left.name)  - 1);
  strncpy(snapshot_.right.name, providerName(cfgStore_.rightProvider()),
          sizeof(snapshot_.right.name) - 1);
}

// ---------------------------------------------------------------------------
// NVS fetch cache (ns "umfetch"): the two per-side FetchSlot blobs survive deep
// sleep AND power loss, so a cold boot shows the last captured values instantly
// and the 5-min rate gate stays honest across reboots. ProviderQuota is POD, so
// the whole slot round-trips as raw bytes. One putBytes per real fetch (~700 B).
// ---------------------------------------------------------------------------

void UsageApp::loadFetchSlots() {
  Preferences p;
  if (!p.begin("umfetch", true)) return;   // read-only; absent on first ever boot
  if (p.getBytesLength("fc_left")  == sizeof(FetchSlot))
    p.getBytes("fc_left",  &g_left,  sizeof(FetchSlot));
  if (p.getBytesLength("fc_right") == sizeof(FetchSlot))
    p.getBytes("fc_right", &g_right, sizeof(FetchSlot));
  p.end();
  sysLog("[cache] loaded slots L:valid=%u prov=%u R:valid=%u prov=%u",
         (unsigned)g_left.valid,  (unsigned)g_left.prov,
         (unsigned)g_right.valid, (unsigned)g_right.prov);
}

void UsageApp::saveFetchSlot(bool left) {
  const FetchSlot& src = left ? g_left : g_right;
  const char* key = left ? "fc_left" : "fc_right";
  Preferences p;
  if (!p.begin("umfetch", false)) return;
  // Skip-if-unchanged: only erase/write flash when the stored blob actually
  // differs from what we're about to write (saves flash wear + the write time).
  FetchSlot cur{};
  if (p.getBytesLength(key) == sizeof(FetchSlot)) {
    p.getBytes(key, &cur, sizeof(FetchSlot));
    if (memcmp(&cur, &src, sizeof(FetchSlot)) == 0) { p.end(); return; }
  }
  p.putBytes(key, &src, sizeof(FetchSlot));
  p.end();
}

// ---------------------------------------------------------------------------
// Fetch
// ---------------------------------------------------------------------------

// Compose the on-screen stop reason from what the last fetch revealed.
static void buildFailReason(char* buf, size_t n, const ProviderQuota& p,
                            int status, const String& err) {
  if (p.needAdminKey)      snprintf(buf, n, "Needs Admin Key (sk-ant-admin)");
  else if (p.needsRelogin) snprintf(buf, n, "Token Revoked or Expired. Update Tokens");
  else if (p.refreshFailed) snprintf(buf, n, "Refresh Token Failing");
  else if (err.length())   snprintf(buf, n, "%.*s", (int)n - 1, err.c_str());
  else if (status > 0)     snprintf(buf, n, "HTTP %d", status);
  else if (status < 0)     snprintf(buf, n, "Network error");
  else                     snprintf(buf, n, "Check Provider Settings");
}

void UsageApp::fetchLeft(long n) {
  if (leftDisabled_) {   // stopped after 2 consecutive failures
    snapshot_.left = ProviderQuota();
    snapshot_.left.disabled = true;
    strncpy(snapshot_.left.failReason, leftFailReason_,
            sizeof(snapshot_.left.failReason) - 1);
    setProviderNames();
    sysLog("[api] %s stopped — skipping (%s)",
           providerName(cfgStore_.leftProvider()), leftFailReason_);
    return;
  }
  const uint8_t lprov = cfgStore_.leftProvider();
  const bool    lHave = g_left.valid && g_left.prov == lprov;
  sysLog("[fetch] left %s", providerName(lprov));
  // Rate-limit backoff: don't poke the endpoint (every retry re-arms the 429);
  // keep showing the preserved last-good, marked stale, with a retry time.
  if (g_left.retryEpoch > 0 && g_left.prov == lprov &&
      n > kSanityFloorEpoch && n < g_left.retryEpoch) {
    snapshot_.left = lHave ? g_left.q : ProviderQuota();
    snapshot_.left.rateLimited = true;
    snapshot_.left.retryEpoch  = g_left.retryEpoch;
    sysLog("[api] %s rate-limit backoff, %lds left", providerName(lprov),
           g_left.retryEpoch - n);
    setProviderNames();
    return;
  }
  if (!leftClient_ || !isConfigured(lprov, cfgStore_)) {
    sysLog("[left] not configured — skipping");
    snapshot_.left = ProviderQuota();
    setProviderNames();
    return;
  }
  // Rate gate: at most one real call per provider per 5 min. Inside the window
  // keep the saved values and defer to the next scheduled cycle (the credentials
  // token test is exempt — it doesn't come through here). Only gate when we have
  // cached values to show; a fresh boot with no cache must fetch to fill the screen.
  if (lHave && n > kSanityFloorEpoch && (n - g_left.lastFetch) < kMinFetchIntervalSec) {
    snapshot_.left = g_left.q;
    sysLog("[api] %s gated (%lds since last) -> deferred", providerName(lprov),
           n - g_left.lastFetch);
    setProviderNames();
    return;
  }
  ProviderQuota q;
  const bool ok = leftClient_->fetch(n, q);
  if (ok) {
    snapshot_.left = q;
    leftFailCount_ = 0;
    credStatus_[lprov] = kCredOk;
    sysLog("[api] %s OK session=%d%% weekly=%d%% (present s=%d w=%d)",
           providerName(lprov),
           (int)(snapshot_.left.session.usedPercent + 0.5),
           (int)(snapshot_.left.weekly.usedPercent + 0.5),
           snapshot_.left.session.present ? 1 : 0,
           snapshot_.left.weekly.present ? 1 : 0);
    if (lprov == UM_PROV_CLAUDE) {
      snapshot_.left.hasPlan = true;
      strncpy(snapshot_.left.planType, cfgStore_.claudeSub().c_str(),
              sizeof(snapshot_.left.planType) - 1);
    }
    g_left = FetchSlot{ lprov, n, 0, 1, snapshot_.left };   // cache values + gate stamp
    saveFetchSlot(true);
    if (leftAuthPtr_) {
      store_.save(providerKey(lprov), *leftAuthPtr_, seedFor(lprov, cfgStore_));
      sysLog("[fetch] left ok, token saved");
    }
  } else if (http_.lastStatus() == 429) {
    // 429: preserve last-good, mark rate-limited, back off — do NOT trip the breaker.
    snapshot_.left = lHave ? g_left.q : q;
    snapshot_.left.rateLimited = true;
    g_left.prov = lprov;
    if (n > kSanityFloorEpoch) {
      if (g_left.retryCount < 255) g_left.retryCount++;
      const long boff = backoff429Secs(g_left.retryCount, http_.lastRetryAfter());
      g_left.retryEpoch = n + boff;
      g_left.lastFetch  = n;
      sysLog("[api] %s 429 #%u -> backoff %lds (retryAfter=%ld)",
             providerName(lprov), g_left.retryCount, boff, http_.lastRetryAfter());
    }
    snapshot_.left.retryEpoch = g_left.retryEpoch;
    saveFetchSlot(true);
    credStatus_[lprov] = kCredFail;
  } else {
    snapshot_.left = q;
    if (n > kSanityFloorEpoch) {   // count the attempt for the gate (in-RAM only)
      g_left.prov = lprov; g_left.lastFetch = n;   // no NVS write on a plain fail
    }
    credStatus_[lprov] = kCredFail;
    buildFailReason(leftFailReason_, sizeof(leftFailReason_),
                    snapshot_.left, http_.lastStatus(), http_.lastError());
    if (++leftFailCount_ >= 2) {
      leftDisabled_ = true;
      snapshot_.left.disabled = true;
      strncpy(snapshot_.left.failReason, leftFailReason_,
              sizeof(snapshot_.left.failReason) - 1);
    }
    sysLog("[api] %s FAILED (%u/2) reason=%s",
           providerName(lprov), leftFailCount_, leftFailReason_);
    if (leftAuthPtr_ && snapshot_.left.refreshed) {
      // Refresh succeeded but the usage call failed: persist the rotated token now
      // or the next wake replays the stale one and forces a re-login.
      store_.save(providerKey(lprov), *leftAuthPtr_, seedFor(lprov, cfgStore_));
      sysLog("[fetch] left failed but token rotated -> saved");
    }
  }
  setProviderNames();
}

void UsageApp::fetchRight(long n) {
  if (rightDisabled_) {
    snapshot_.right = ProviderQuota();
    snapshot_.right.disabled = true;
    strncpy(snapshot_.right.failReason, rightFailReason_,
            sizeof(snapshot_.right.failReason) - 1);
    setProviderNames();
    sysLog("[api] %s stopped — skipping (%s)",
           providerName(cfgStore_.rightProvider()), rightFailReason_);
    return;
  }
  const uint8_t rprov = cfgStore_.rightProvider();
  const bool    rHave = g_right.valid && g_right.prov == rprov;
  sysLog("[fetch] right %s", providerName(rprov));
  if (g_right.retryEpoch > 0 && g_right.prov == rprov &&
      n > kSanityFloorEpoch && n < g_right.retryEpoch) {
    snapshot_.right = rHave ? g_right.q : ProviderQuota();
    snapshot_.right.rateLimited = true;
    snapshot_.right.retryEpoch  = g_right.retryEpoch;
    sysLog("[api] %s rate-limit backoff, %lds left", providerName(rprov),
           g_right.retryEpoch - n);
    setProviderNames();
    return;
  }
  if (!rightClient_ || !isConfigured(rprov, cfgStore_)) {
    sysLog("[right] not configured — skipping");
    snapshot_.right = ProviderQuota();
    setProviderNames();
    return;
  }
  if (rHave && n > kSanityFloorEpoch && (n - g_right.lastFetch) < kMinFetchIntervalSec) {
    snapshot_.right = g_right.q;
    sysLog("[api] %s gated (%lds since last) -> deferred", providerName(rprov),
           n - g_right.lastFetch);
    setProviderNames();
    return;
  }
  ProviderQuota q;
  const bool ok = rightClient_->fetch(n, q);
  if (ok) {
    snapshot_.right = q;
    rightFailCount_ = 0;
    credStatus_[rprov] = kCredOk;
    sysLog("[api] %s OK session=%d%% weekly=%d%% (present s=%d w=%d)",
           providerName(rprov),
           (int)(snapshot_.right.session.usedPercent + 0.5),
           (int)(snapshot_.right.weekly.usedPercent + 0.5),
           snapshot_.right.session.present ? 1 : 0,
           snapshot_.right.weekly.present ? 1 : 0);
    if (rprov == UM_PROV_CLAUDE) {
      snapshot_.right.hasPlan = true;
      strncpy(snapshot_.right.planType, cfgStore_.claudeSub().c_str(),
              sizeof(snapshot_.right.planType) - 1);
    }
    g_right = FetchSlot{ rprov, n, 0, 1, snapshot_.right };
    saveFetchSlot(false);
    if (rightAuthPtr_) {
      store_.save(providerKey(rprov), *rightAuthPtr_, seedFor(rprov, cfgStore_));
      sysLog("[fetch] right ok, token saved");
    }
  } else if (http_.lastStatus() == 429) {
    snapshot_.right = rHave ? g_right.q : q;
    snapshot_.right.rateLimited = true;
    g_right.prov = rprov;
    if (n > kSanityFloorEpoch) {
      if (g_right.retryCount < 255) g_right.retryCount++;
      const long boff = backoff429Secs(g_right.retryCount, http_.lastRetryAfter());
      g_right.retryEpoch = n + boff;
      g_right.lastFetch  = n;
      sysLog("[api] %s 429 #%u -> backoff %lds (retryAfter=%ld)",
             providerName(rprov), g_right.retryCount, boff, http_.lastRetryAfter());
    }
    snapshot_.right.retryEpoch = g_right.retryEpoch;
    saveFetchSlot(false);
    credStatus_[rprov] = kCredFail;
  } else {
    snapshot_.right = q;
    if (n > kSanityFloorEpoch) { g_right.prov = rprov; g_right.lastFetch = n; }   // no NVS write on a plain fail
    credStatus_[rprov] = kCredFail;
    buildFailReason(rightFailReason_, sizeof(rightFailReason_),
                    snapshot_.right, http_.lastStatus(), http_.lastError());
    if (++rightFailCount_ >= 2) {
      rightDisabled_ = true;
      snapshot_.right.disabled = true;
      strncpy(snapshot_.right.failReason, rightFailReason_,
              sizeof(snapshot_.right.failReason) - 1);
    }
    sysLog("[api] %s FAILED (%u/2) reason=%s",
           providerName(rprov), rightFailCount_, rightFailReason_);
    if (rightAuthPtr_ && snapshot_.right.refreshed) {
      store_.save(providerKey(rprov), *rightAuthPtr_, seedFor(rprov, cfgStore_));
      sysLog("[fetch] right failed but token rotated -> saved");
    }
  }
  setProviderNames();
}

void UsageApp::fetchLocalStats() {
  sysLog("[local/fetch] start url_len=%d", (int)cfgStore_.localStatsUrl().length());
  localStats_.fetch(
      providerKey(cfgStore_.leftProvider()),
      providerKey(cfgStore_.rightProvider()),
      snapshot_
  );
}

// ---------------------------------------------------------------------------
// Render fingerprint: hashes exactly what the dashboard paints, EXCLUDING the
// pure time-derived strings (Fetch Last/Next times, reset countdowns). When the
// fingerprint matches the last painted one, the wake repaint is skipped and the
// GRAY4 full-refresh flash never runs. FNV-1a.
// ---------------------------------------------------------------------------
static void fnv(uint32_t& h, const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 16777619u; }
}
static void fnvI(uint32_t& h, long v)        { fnv(h, &v, sizeof(v)); }
static void fnvS(uint32_t& h, const char* s) { fnv(h, s, strlen(s)); }

static void fpWindow(uint32_t& h, const WindowQuota& w) {
  fnvI(h, w.present ? 1 : 0);
  if (!w.present) return;
  fnvI(h, (long)(w.usedPercent + 0.5));   // the displayed integer percent
  fnvI(h, (long)w.status);                // bar color
  fnvI(h, w.resetEpoch);                  // changes only when the window rolls
}

static void fpProvider(uint32_t& h, const ProviderQuota& p, long n) {
  fnvS(h, p.name);
  fnvI(h, (p.ok ? 1 : 0) | (p.needsRelogin ? 2 : 0) | (p.disabled ? 4 : 0) |
          (p.isStale(n, 900) ? 8 : 0) | (p.rateLimited ? 16 : 0));
  fnvI(h, p.retryEpoch);   // fixed during a 429 backoff -> stays stable, no per-wake flash
  fnvS(h, p.failReason);
  fpWindow(h, p.session);
  fpWindow(h, p.weekly);
  fpWindow(h, p.weeklySonnet);
  fpWindow(h, p.weeklyOpus);
  fnvI(h, p.extraEnabled ? 1 : 0);
  fnvI(h, (long)p.extraUsedCents);
  fnvI(h, (long)p.extraLimitCents);
  fnvI(h, p.hasBalance ? 1 : 0);
  fnvI(h, (long)p.balance);               // platform token-usage line
  if (p.hasPlan) fnvS(h, p.planType);
  fnvI(h, (p.needAdminKey ? 1 : 0) | (p.hasCost ? 2 : 0) | (p.hasLeft ? 4 : 0) |
          (p.platSpendMode ? 8 : 0));
  fnvI(h, (long)p.costCents);
  fnvI(h, (long)p.prepaidCents);
  fnvI(h, (long)p.leftCents);
  fnvI(h, p.platWindowDays);
  fnvI(h, p.platCount);
  for (uint8_t i = 0; i < p.platCount; ++i) {
    fnvS(h, p.platModels[i].name);
    fnvI(h, (long)p.platModels[i].cents);
  }
  if (p.local.enabled) {
    fnvI(h, p.local.available ? 1 : 0);
    fnvS(h, p.local.status);
    fnvI(h, (long)p.local.todayTokens);
    fnvI(h, p.local.sessionCount);
    fnvI(h, p.local.modelCount);
  }
}

static uint32_t drawFingerprint(const UsageSnapshot& s, const UiStatus& st, long n) {
  uint32_t h = 2166136261u;
  fpProvider(h, s.left, n);
  fpProvider(h, s.right, n);
  fnvI(h, st.wifiConnected ? 1 : 0);
  fnvS(h, st.ipAddress.c_str());
  // Battery bucket with a 2% deadband: ADC jitter at a 5%-bucket edge would
  // otherwise flip the hash every wake and force a needless GRAY4 flash.
  static RTC_DATA_ATTR int s_battBucket = -1;
  if (st.batteryPercent < 0) {
    fnvI(h, -1);
  } else {
    if (s_battBucket < 0) s_battBucket = st.batteryPercent / 5;
    else if (st.batteryPercent >= (s_battBucket + 1) * 5 + 2) s_battBucket = st.batteryPercent / 5;
    else if (st.batteryPercent <= s_battBucket * 5 - 2)       s_battBucket = st.batteryPercent / 5;
    fnvI(h, s_battBucket);
  }
  fnvI(h, st.batteryDays >= 0.0f ? (long)(st.batteryDays * 24.0f + 0.5f) : -1);
  fnvI(h, (st.batteryCharging ? 1 : 0) | (st.batteryEstPlaceholder ? 2 : 0) |
          (st.deepSleepOn ? 4 : 0));
  // Excluded on purpose: lastFetchEpoch/nextFetchEpoch and the now-derived
  // countdowns — kMaxNoRepaintSec bounds how stale they can look.
  return h;
}

void UsageApp::refreshAll(bool forceDraw) {
  const long n = now();
  sysLog("[api] cycle start now=%ld time_synced=%d", n, timeSynced_ ? 1 : 0);
  // A just-run credential test may have already fetched a displayed side; reuse it.
  if (leftFresh_)  leftFresh_  = false; else fetchLeft(n);
  if (rightFresh_) rightFresh_ = false; else fetchRight(n);
  fetchLocalStats();
  setProviderNames();   // also names the sides reused from a cred test
  printSnapshot();
  lastFetchEpoch_ = now();   // for the Last/Next Fetch header
  const UiStatus st = currentStatus();
  const uint32_t fp = drawFingerprint(snapshot_, st, n);
  const bool capExpired = (g_lastDrawEpoch <= 0) ||
                          (n - g_lastDrawEpoch >= kMaxNoRepaintSec);
  if (!forceDraw && fp == g_lastDrawHash && g_lastDrawHash != 0 && !capExpired) {
    // Nothing the user can see has changed — keep the panel image (zero flicker).
    sysLog("[ui] unchanged, repaint skipped (age=%lds)", n - g_lastDrawEpoch);
    return;
  }
  ui_.drawDashboard(snapshot_, st, n);
  g_lastDrawHash  = fp;
  g_lastDrawEpoch = n;
  sysLog("[ui] dashboard drawn");
}

void UsageApp::printSnapshot() {
  const long n = now();
  auto pr = [&](const ProviderQuota& p) {
    sysLog("[%s] ok=%d relogin=%d stale=%d", p.name,
           p.ok ? 1 : 0, p.needsRelogin ? 1 : 0, p.isStale(n, 900) ? 1 : 0);
    if (p.session.present)
      sysLog("  session: used %.1f%%  left %.1f%%  reset_in %lds",
             p.session.usedPercent, p.session.remainingPercent(),
             p.session.resetEpoch > 0 ? (p.session.resetEpoch - n) : 0L);
    if (p.weekly.present)
      sysLog("  weekly : used %.1f%%  left %.1f%%  reset_in %lds",
             p.weekly.usedPercent, p.weekly.remainingPercent(),
             p.weekly.resetEpoch > 0 ? (p.weekly.resetEpoch - n) : 0L);
    if (p.hasBalance) sysLog("  balance : %.2f", p.balance);
    if (p.hasPlan)    sysLog("  plan    : %s", p.planType);
  };
  sysLog("---- usage snapshot ----");
  pr(snapshot_.left);
  pr(snapshot_.right);
  sysLog("------------------------");
}

}  // namespace usage_monitor
