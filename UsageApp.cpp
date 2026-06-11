#include "UsageApp.h"

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
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

// Green button (GPIO3) is the EXT1 deep-sleep wake source (active-low).
#define UM_BTN_WAKE GPIO_NUM_3

// Survives deep sleep; diagnostics only.
RTC_DATA_ATTR static uint32_t g_wakeCount = 0;

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
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(10);
  uint32_t mv = 0;
  for (int i = 0; i < 16; ++i) mv += analogReadMilliVolts(1);
  digitalWrite(21, LOW);
  const int adcMv = static_cast<int>(mv / 16);
  g_battMv = adcMv * 2;   // hardware halves the battery voltage
  return umBatteryPercent(adcMv, g_battFullMv);
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

// Validate one provider's stored credentials by making its real usage call into
// a scratch wiring. Persists any rotated token on success. Returns kCredOk/Fail.
uint8_t UsageApp::testProvider(uint8_t prov) {
  OAuthClient scratch;
  UsageClientBase* client = nullptr;
  AuthState* authPtr = nullptr;
  wireProvider(prov, scratch, client, authPtr);
  if (!client) return kCredFail;
  ProviderQuota tmp;
  const bool ok = client->fetch(now(), tmp);
  if (ok && authPtr) {
    store_.save(providerKey(prov), *authPtr, seedFor(prov, cfgStore_));
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
    if (mask & (uint8_t)(1u << prov)) {
      credStatus_[prov] = kCredTesting;
      credStatus_[prov] = testProvider(prov);
    }
  }
  // Note: testProvider repoints member usage clients at a scratch OAuthClient;
  // the caller must re-run configureProviders() before normal fetching resumes.
}

String UsageApp::credStatusJson() {
  static const char* kKeys[8] = { "", "claude", "codex", "copilot", "minimax",
                                  "kimi", "zai", "claudeplat" };
  static const char* kVal[4]  = { "none", "ok", "fail", "testing" };
  String s = "{";
  for (uint8_t p = 1; p <= 7; ++p) {
    if (p > 1) s += ",";
    // A provider with no stored token is always "none" (white), even if it
    // failed earlier — covers the Clear Token action and empty providers.
    const uint8_t st = isConfigured(p, cfgStore_) ? credStatus_[p] : 0;
    s += "\""; s += kKeys[p]; s += "\":\"";
    s += kVal[st <= 3 ? st : 0];
    s += "\"";
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

  // Wake cause drives the awake-window length: cold power-on gets a long window
  // for first setup; timer/button wakes get a short 30 s window.
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool firstBoot = (cause != ESP_SLEEP_WAKEUP_TIMER &&
                          cause != ESP_SLEEP_WAKEUP_EXT1);
  awakeWindowMs_ = firstBoot ? 5UL * 60UL * 1000UL : 30UL * 1000UL;
  ++g_wakeCount;
  sysLog("[boot] wake cause=%d firstBoot=%d count=%u window=%lums",
         (int)cause, firstBoot ? 1 : 0, (unsigned)g_wakeCount, awakeWindowMs_);

  cfgStore_.begin();
  // Apply the saved timezone immediately so the RTC time (kept across deep
  // sleep, in UTC) renders as local time right away — before NTP re-applies it.
  setenv("TZ", cfgStore_.tz().c_str(), 1);
  tzset();
  g_battFullMv = cfgStore_.battFullMv();
  sysLog("[sleep] boot_id=%u deep_sleep=%d", (unsigned)g_wakeCount,
         cfgStore_.deepSleepEnabled() ? 1 : 0);
  http_.configure(45000);
  configureProviders();
  localStats_.configure(&http_, cfgStore_.localStatsUrl().c_str());

  ui_.begin();
  ui_.setDarkMode(cfgStore_.darkMode());
  ui_.setFont(cfgStore_.uiFont());
  ui_.setSmoothing(cfgStore_.uiAa());
  ui_.setSharpness(cfgStore_.uiSharp());
  ui_.setTextWeight(cfgStore_.uiWeight());
  ui_.setSmallTextCrisp(cfgStore_.uiSmallCrisp());
  // Cold boot: full splash. Wake: draw nothing — GRAY4 only refreshes cleanly
  // with a full update, so leave the persisted dashboard on screen during
  // WiFi+fetch and do one clean full refresh when the new data is ready.
  if (firstBoot) ui_.drawBoot(uiStr(UiStringId::kBootWifi), currentStatus(), now());

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
                  [this]() -> int {                          // est hours on battery, -1 = n/a
                    if (!cfgStore_.deepSleepEnabled()) return -1;
                    const float d = battery_tracker_get_days_remaining(cfgStore_.refreshSec());
                    return d >= 0.0f ? (int)(d * 24.0f + 0.5f) : -1;
                  },
                  [this](int on, int font, int dark, int all, int crisp, int sz) {  // font-test
                    onFontTest(on, font, dark, all, crisp, sz);
                  });
  server_.begin();
  sysLog("[settings] http://usagemonitor.local or http://%s",
         WiFi.localIP().toString().c_str());

  syncTime();   // non-blocking: starts background SNTP, no splash screen
  if (firstBoot) ui_.drawBoot(uiStr(UiStringId::kBootFetch), currentStatus(), now());
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
    } else {
      // Restore the normal renderer state + redraw the dashboard from the last snapshot.
      ui_.setDarkMode(cfgStore_.darkMode());
      ui_.setFont(cfgStore_.uiFont());
      ui_.setSmoothing(cfgStore_.uiAa());
      ui_.setSharpness(cfgStore_.uiSharp());
      ui_.setTextWeight(cfgStore_.uiWeight());
      ui_.setSmallTextCrisp(cfgStore_.uiSmallCrisp());
      ui_.drawDashboard(snapshot_, currentStatus(), now());
    }
    lastRefreshMs_ = millis();   // don't immediately refetch on the next tick
    extendAwake("font-test");
  }

  // A settings save requested a repaint (dark-mode toggle, new credentials,
  // etc.). Suppressed while the playground owns the screen (re-applies on exit).
  if (redrawPending_ && !fontTestOn_) {
    redrawPending_ = false;
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
      refreshAll();             // repaint with the new palette + displayed data
    } else {
      ui_.drawDashboard(snapshot_, currentStatus(), now());
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
  if (cfgStore_.deepSleepEnabled() && nowMs - lastSleepTickMs_ >= 10000UL) {
    lastSleepTickMs_ = nowMs;
    sysLog("[sleep] awake, sleeps in %ds", sleepInSec());
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
  const bool btnWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  battery_tracker_update(soc, btnWake);
  sysLog("[batt] cycle=%u btnwakes=%u soc=%.0f days=%.1f",
         battery_tracker_get_cycles(), battery_tracker_get_button_wakes(), soc,
         battery_tracker_get_days_remaining(sec));

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

bool UsageApp::ensureWiFi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) {
    sysLog("[wifi] already connected");
    return true;
  }
  sysLog("[wifi] connecting (timeout=%ums)...", (unsigned)timeoutMs);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
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
  const bool deep = cfgStore_.deepSleepEnabled();
  const float days = deep ? battery_tracker_get_days_remaining(cfgStore_.refreshSec())
                          : -2.0f;
  const uint32_t cyc = battery_tracker_get_cycles();
  const bool draining = (days >= 0.0f);
  s.batteryDays = days;
  s.batteryCharging = !draining && (Serial.isPlugged() || (deep && cyc >= 3));
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
  sysLog("[fetch] left %s", providerName(cfgStore_.leftProvider()));
  if (!leftClient_ || !isConfigured(cfgStore_.leftProvider(), cfgStore_)) {
    sysLog("[left] not configured — skipping");
    snapshot_.left = ProviderQuota();
    setProviderNames();
    return;
  }
  if (leftClient_->fetch(n, snapshot_.left)) {
    leftFailCount_ = 0;
    credStatus_[cfgStore_.leftProvider()] = kCredOk;
    sysLog("[api] %s OK session=%d%% weekly=%d%% (present s=%d w=%d)",
           providerName(cfgStore_.leftProvider()),
           (int)(snapshot_.left.session.usedPercent + 0.5),
           (int)(snapshot_.left.weekly.usedPercent + 0.5),
           snapshot_.left.session.present ? 1 : 0,
           snapshot_.left.weekly.present ? 1 : 0);
    if (cfgStore_.leftProvider() == UM_PROV_CLAUDE) {
      snapshot_.left.hasPlan = true;
      strncpy(snapshot_.left.planType, cfgStore_.claudeSub().c_str(),
              sizeof(snapshot_.left.planType) - 1);
    }
    if (leftAuthPtr_) {
      store_.save(providerKey(cfgStore_.leftProvider()), *leftAuthPtr_,
                  seedFor(cfgStore_.leftProvider(), cfgStore_));
      sysLog("[fetch] left ok, token saved");
    }
  } else {
    credStatus_[cfgStore_.leftProvider()] = kCredFail;
    buildFailReason(leftFailReason_, sizeof(leftFailReason_),
                    snapshot_.left, http_.lastStatus(), http_.lastError());
    if (++leftFailCount_ >= 2) {
      leftDisabled_ = true;
      snapshot_.left.disabled = true;
      strncpy(snapshot_.left.failReason, leftFailReason_,
              sizeof(snapshot_.left.failReason) - 1);
    }
    sysLog("[api] %s FAILED (%u/2) reason=%s",
           providerName(cfgStore_.leftProvider()), leftFailCount_, leftFailReason_);
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
  sysLog("[fetch] right %s", providerName(cfgStore_.rightProvider()));
  if (!rightClient_ || !isConfigured(cfgStore_.rightProvider(), cfgStore_)) {
    sysLog("[right] not configured — skipping");
    snapshot_.right = ProviderQuota();
    setProviderNames();
    return;
  }
  if (rightClient_->fetch(n, snapshot_.right)) {
    rightFailCount_ = 0;
    credStatus_[cfgStore_.rightProvider()] = kCredOk;
    sysLog("[api] %s OK session=%d%% weekly=%d%% (present s=%d w=%d)",
           providerName(cfgStore_.rightProvider()),
           (int)(snapshot_.right.session.usedPercent + 0.5),
           (int)(snapshot_.right.weekly.usedPercent + 0.5),
           snapshot_.right.session.present ? 1 : 0,
           snapshot_.right.weekly.present ? 1 : 0);
    if (cfgStore_.rightProvider() == UM_PROV_CLAUDE) {
      snapshot_.right.hasPlan = true;
      strncpy(snapshot_.right.planType, cfgStore_.claudeSub().c_str(),
              sizeof(snapshot_.right.planType) - 1);
    }
    if (rightAuthPtr_) {
      store_.save(providerKey(cfgStore_.rightProvider()), *rightAuthPtr_,
                  seedFor(cfgStore_.rightProvider(), cfgStore_));
      sysLog("[fetch] right ok, token saved");
    }
  } else {
    credStatus_[cfgStore_.rightProvider()] = kCredFail;
    buildFailReason(rightFailReason_, sizeof(rightFailReason_),
                    snapshot_.right, http_.lastStatus(), http_.lastError());
    if (++rightFailCount_ >= 2) {
      rightDisabled_ = true;
      snapshot_.right.disabled = true;
      strncpy(snapshot_.right.failReason, rightFailReason_,
              sizeof(snapshot_.right.failReason) - 1);
    }
    sysLog("[api] %s FAILED (%u/2) reason=%s",
           providerName(cfgStore_.rightProvider()), rightFailCount_, rightFailReason_);
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

void UsageApp::refreshAll() {
  const long n = now();
  sysLog("[api] cycle start now=%ld time_synced=%d", n, timeSynced_ ? 1 : 0);
  fetchLeft(n);
  fetchRight(n);
  fetchLocalStats();
  setProviderNames();
  printSnapshot();
  lastFetchEpoch_ = now();   // for the Last/Next Fetch header
  ui_.drawDashboard(snapshot_, currentStatus(), now());
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
