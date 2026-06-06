#include "UsageApp.h"

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AppLog.h"
#include "DragynESPAsyncWiFiManager.h"
#include "IsoTime.h"
#include "UiLang.h"
#include "driver.h"

// Required symbol for DragynESPAsyncWiFiManager — no WPA3 hardening needed here.
void dragynWifiApplyWpa3Hardening(void) {}

namespace usage_monitor {

static const long kSanityFloorEpoch = 1577836800L;

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

void UsageApp::configureProviders() {
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

  auto doSide = [this, &loadCache](uint8_t prov, OAuthClient& oauth,
                                   UsageClientBase*& client, AuthState*& authPtr) {
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

      case UM_PROV_CLAUDEPLAT:
        claudePlatClient_.configure(&http_, cfgStore_.claudePlatKey(),
                                    cfgStore_.claudePlatOrg());
        authPtr = nullptr;       // admin key never refreshed
        client  = &claudePlatClient_;
        break;

      default:
        authPtr = nullptr;
        client  = nullptr;
        break;
    }
  };

  doSide(cfgStore_.leftProvider(),  leftOAuth_,  leftClient_,  leftAuthPtr_);
  sysLog("[prov] left=%s at_len=%d rt_len=%d",
         providerName(cfgStore_.leftProvider()),
         leftAuthPtr_  ? (int)leftAuthPtr_->accessToken.length()  : -1,
         leftAuthPtr_  ? (int)leftAuthPtr_->refreshToken.length() : -1);
  doSide(cfgStore_.rightProvider(), rightOAuth_, rightClient_, rightAuthPtr_);
  sysLog("[prov] right=%s at_len=%d rt_len=%d",
         providerName(cfgStore_.rightProvider()),
         rightAuthPtr_ ? (int)rightAuthPtr_->accessToken.length()  : -1,
         rightAuthPtr_ ? (int)rightAuthPtr_->refreshToken.length() : -1);
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
  pinMode(UM_LED_PIN, OUTPUT);
  digitalWrite(UM_LED_PIN, LOW);

  // Detect timer wake from deep sleep (vs power-on or manual reset).
  bool isTimerWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
  if (!isTimerWake) {
    bootWindowStartMs_ = millis();
    settingsAvailable_ = true;
    sysLog("[boot] normal boot (settings window open)");
  } else {
    sysLog("[boot] timer wake (settings skipped)");
  }

  cfgStore_.begin();
  http_.configure(45000);
  configureProviders();
  localStats_.configure(&http_, cfgStore_.localStatsUrl().c_str());

  ui_.begin();
  ui_.drawBoot(uiStr(UiStringId::kBootWifi), currentStatus(), now());

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

  if (settingsAvailable_) {
    MDNS.begin("usagemonitor");
    settings_.begin(&server_, &cfgStore_);
    server_.begin();
    sysLog("[settings] http://usagemonitor.local or http://%s",
           WiFi.localIP().toString().c_str());
  }

  ui_.drawBoot(uiStr(UiStringId::kBootSync), currentStatus(), now());
  syncTime();
  ui_.drawBoot(uiStr(UiStringId::kBootFetch), currentStatus(), now());
  refreshAll();
  lastRefreshMs_ = millis();
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------

void UsageApp::loop() {
  const unsigned long ms = millis();
  const unsigned long refreshMs = (unsigned long)cfgStore_.refreshSec() * 1000UL;

  if (!settingsAvailable_) {
    // Timer-wake path: already fetched in begin(). Go back to sleep.
    if (cfgStore_.deepSleepEnabled()) {
      enterDeepSleep();
      // enterDeepSleep() never returns; fall through only if deep sleep failed.
    }
    // Deep sleep disabled: run normal refresh loop without settings server.
    if (ms - lastRefreshMs_ >= refreshMs) {
      lastRefreshMs_ = ms;
      if (ensureWiFi(10000)) refreshAll();
    }
    delay(50);
    return;
  }

  // Normal boot path.
  if (cfgStore_.deepSleepEnabled() &&
      ms - bootWindowStartMs_ >= 5UL * 60UL * 1000UL) {
    sysLog("[sleep] boot window closed — entering sleep-refresh cycle");
    settingsAvailable_ = false;
    if (ensureWiFi(10000)) refreshAll();
    enterDeepSleep();
    // Only reached if enterDeepSleep fails.
  }

  if (ms - lastRefreshMs_ >= refreshMs) {
    lastRefreshMs_ = ms;
    if (ensureWiFi(10000)) refreshAll();
  }
  delay(50);
}

// ---------------------------------------------------------------------------
// enterDeepSleep
// ---------------------------------------------------------------------------

void UsageApp::enterDeepSleep() {
  const uint32_t sec = cfgStore_.refreshSec();
  sysLog("[sleep] sleeping %us", (unsigned)sec);
  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
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
  sysLog("[ntp] tz=%s servers=pool.ntp.org,time.nist.gov", cfgStore_.tz().c_str());
  configTzTime(cfgStore_.tz().c_str(), "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 60 && static_cast<long>(time(nullptr)) < kSanityFloorEpoch; ++i) {
    delay(500);
  }
  if (static_cast<long>(time(nullptr)) >= kSanityFloorEpoch) {
    timeSynced_ = true;
    sysLog("[time] NTP ok: %ld", static_cast<long>(time(nullptr)));
  } else {
    sysLog("[time] NTP failed");
    sysLog("[time] WARN proceeding without synced clock — expiry math degraded");
  }
}

long UsageApp::now() { return static_cast<long>(time(nullptr)); }

UiStatus UsageApp::currentStatus() {
  UiStatus s;
  s.wifiConnected  = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = -1;
  if (s.wifiConnected) s.ipAddress = WiFi.localIP().toString();
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

void UsageApp::fetchLeft(long n) {
  sysLog("[fetch] left %s", providerName(cfgStore_.leftProvider()));
  if (!leftClient_ || !isConfigured(cfgStore_.leftProvider(), cfgStore_)) {
    sysLog("[left] not configured — skipping");
    snapshot_.left = ProviderQuota();
    setProviderNames();
    return;
  }
  if (leftClient_->fetch(n, snapshot_.left)) {
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
    sysLog("[api] %s FAILED (relogin=%d)",
           providerName(cfgStore_.leftProvider()),
           snapshot_.left.needsRelogin ? 1 : 0);
  }
  setProviderNames();
}

void UsageApp::fetchRight(long n) {
  sysLog("[fetch] right %s", providerName(cfgStore_.rightProvider()));
  if (!rightClient_ || !isConfigured(cfgStore_.rightProvider(), cfgStore_)) {
    sysLog("[right] not configured — skipping");
    snapshot_.right = ProviderQuota();
    setProviderNames();
    return;
  }
  if (rightClient_->fetch(n, snapshot_.right)) {
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
    sysLog("[api] %s FAILED (relogin=%d)",
           providerName(cfgStore_.rightProvider()),
           snapshot_.right.needsRelogin ? 1 : 0);
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
