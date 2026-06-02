#include "UsageApp.h"

#include <WiFi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "driver.h"
#include "IsoTime.h"
#include "ProviderSelect.h"
#include "UiLang.h"

namespace usage_monitor {

static const long kSanityFloorEpoch = 1577836800L;

// Seed an AuthState from config fields. Helpers avoid repeating #if chains.
static void seedClaude(AuthState& st, const UsageConfig& cfg) {
  st.accessToken = cfg.claudeAccessToken;
  st.refreshToken = cfg.claudeRefreshToken;
  st.expiryEpoch = atoll(cfg.claudeExpiresAtMs) / 1000L;
  st.usesAbsoluteExpiry = true;
}
static void seedCodex(AuthState& st, const UsageConfig& cfg) {
  st.accessToken = cfg.codexAccessToken;
  st.refreshToken = cfg.codexRefreshToken;
  st.accountId = cfg.codexAccountId;
  st.expiryEpoch = umParseIso8601(cfg.codexLastRefresh);
  st.usesAbsoluteExpiry = false;
}
static void seedStaticKey(AuthState& st, const char* key) {
  st.accessToken = key;
}

UsageApp::UsageApp(const UsageConfig& config) : config_(config) {}

void UsageApp::setProviderNames() {
  strncpy(snapshot_.left.name, UM_LEFT_NAME, sizeof(snapshot_.left.name) - 1);
  strncpy(snapshot_.right.name, UM_RIGHT_NAME, sizeof(snapshot_.right.name) - 1);
}

void UsageApp::begin() {
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
  delay(300);
  Serial1.println("\n[boot] UsageMonitor");
  pinMode(UM_LED_PIN, OUTPUT);
  digitalWrite(UM_LED_PIN, LOW);

  // Seed LEFT provider auth.
#if UM_LEFT_PROVIDER == UM_PROV_CLAUDE
  seedClaude(leftAuth_, config_);
#elif UM_LEFT_PROVIDER == UM_PROV_CODEX
  seedCodex(leftAuth_, config_);
#elif UM_LEFT_PROVIDER == UM_PROV_COPILOT
  seedStaticKey(leftAuth_, config_.copilotPat);
#elif UM_LEFT_PROVIDER == UM_PROV_MINIMAX
  seedStaticKey(leftAuth_, config_.minimaxApiKey);
#elif UM_LEFT_PROVIDER == UM_PROV_KIMI
  seedStaticKey(leftAuth_, config_.kimiAuthToken);
#elif UM_LEFT_PROVIDER == UM_PROV_ZAI
  seedStaticKey(leftAuth_, config_.zaiApiKey);
#endif

  // Seed RIGHT provider auth.
#if UM_RIGHT_PROVIDER == UM_PROV_CLAUDE
  seedClaude(rightAuth_, config_);
#elif UM_RIGHT_PROVIDER == UM_PROV_CODEX
  seedCodex(rightAuth_, config_);
#elif UM_RIGHT_PROVIDER == UM_PROV_COPILOT
  seedStaticKey(rightAuth_, config_.copilotPat);
#elif UM_RIGHT_PROVIDER == UM_PROV_MINIMAX
  seedStaticKey(rightAuth_, config_.minimaxApiKey);
#elif UM_RIGHT_PROVIDER == UM_PROV_KIMI
  seedStaticKey(rightAuth_, config_.kimiAuthToken);
#elif UM_RIGHT_PROVIDER == UM_PROV_ZAI
  seedStaticKey(rightAuth_, config_.zaiApiKey);
#endif

  store_.begin();
  store_.load("L", leftAuth_);
  store_.load("R", rightAuth_);

  http_.configure(config_.httpTimeoutMs);
  leftOAuth_.configure(&http_, &leftProvider_, &leftAuth_);
  rightOAuth_.configure(&http_, &rightProvider_, &rightAuth_);

  // Configure the usage clients.
#if UM_LEFT_PROVIDER == UM_PROV_MINIMAX
  leftClient_.configure(&leftOAuth_,
      config_.minimaxRegion == 1 ? "https://api.minimaxi.com" : "https://api.minimax.io");
#elif UM_LEFT_PROVIDER == UM_PROV_ZAI
  leftClient_.configure(&leftOAuth_, config_.zaiEndpoint);
#else
  leftClient_.configure(&leftOAuth_);
#endif

#if UM_RIGHT_PROVIDER == UM_PROV_MINIMAX
  rightClient_.configure(&rightOAuth_,
      config_.minimaxRegion == 1 ? "https://api.minimaxi.com" : "https://api.minimax.io");
#elif UM_RIGHT_PROVIDER == UM_PROV_ZAI
  rightClient_.configure(&rightOAuth_, config_.zaiEndpoint);
#else
  rightClient_.configure(&rightOAuth_);
#endif

  setProviderNames();
#if defined(UM_ENABLE_LOCAL_STATS)
  localStats_.configure(&http_, config_.localStatsUrl);
#endif

  ui_.begin();
  ui_.drawBoot(uiStr(UiStringId::kBootWifi), currentStatus(), now());

  if (ensureWiFi(15000)) {
    ui_.drawBoot(uiStr(UiStringId::kBootSync), currentStatus(), now());
    syncTime();
    ui_.drawBoot(uiStr(UiStringId::kBootFetch), currentStatus(), now());
    refreshAll();
  } else {
    ui_.drawBoot(uiStr(UiStringId::kNoWifi), currentStatus(), now());
  }
  lastRefreshMs_ = millis();
}

void UsageApp::loop() {
  const unsigned long ms = millis();
  if (ms - lastRefreshMs_ >= config_.refreshIntervalMs) {
    lastRefreshMs_ = ms;
    if (ensureWiFi(10000)) refreshAll();
  }
  delay(50);
}

bool UsageApp::ensureWiFi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config_.wifiSsid, config_.wifiPassword);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial1.print(".");
  }
  Serial1.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial1.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial1.println("[wifi] failed");
  return false;
}

void UsageApp::syncTime() {
  configTzTime(config_.tz, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 20 && static_cast<long>(time(nullptr)) < kSanityFloorEpoch; ++i) {
    delay(500);
  }
  if (static_cast<long>(time(nullptr)) >= kSanityFloorEpoch) {
    timeSynced_ = true;
    Serial1.printf("[time] NTP ok: %ld\n", static_cast<long>(time(nullptr)));
  } else {
    Serial1.println("[time] NTP failed");
  }
}

long UsageApp::now() { return static_cast<long>(time(nullptr)); }

UiStatus UsageApp::currentStatus() {
  UiStatus s;
  s.wifiConnected = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = -1;
  return s;
}

void UsageApp::fetchLeft(long n) {
  if (leftClient_.fetch(n, snapshot_.left)) {
#if UM_LEFT_PROVIDER == UM_PROV_CLAUDE
    snapshot_.left.hasPlan = true;
    strncpy(snapshot_.left.planType, config_.claudeSubscription,
            sizeof(snapshot_.left.planType) - 1);
#endif
    store_.save("L", leftAuth_);
  } else if (snapshot_.left.needsRelogin) {
    Serial1.printf("[%s] needs relogin\n", UM_LEFT_NAME);
  }
  setProviderNames();
}

void UsageApp::fetchRight(long n) {
  if (rightClient_.fetch(n, snapshot_.right)) {
#if UM_RIGHT_PROVIDER == UM_PROV_CLAUDE
    snapshot_.right.hasPlan = true;
    strncpy(snapshot_.right.planType, config_.claudeSubscription,
            sizeof(snapshot_.right.planType) - 1);
#endif
    store_.save("R", rightAuth_);
  } else if (snapshot_.right.needsRelogin) {
    Serial1.printf("[%s] needs relogin\n", UM_RIGHT_NAME);
  }
  setProviderNames();
}

void UsageApp::fetchLocalStats() {
#if defined(UM_ENABLE_LOCAL_STATS)
  localStats_.fetch(UM_LEFT_KEY, UM_RIGHT_KEY, snapshot_);
#else
  snapshot_.left.local = LocalProviderStats();
  snapshot_.right.local = LocalProviderStats();
#endif
}

void UsageApp::refreshAll() {
  const long n = now();
  fetchLeft(n);
  fetchRight(n);
  fetchLocalStats();
  setProviderNames();
  printSnapshot();
  ui_.drawDashboard(snapshot_, currentStatus(), now());
}

void UsageApp::printSnapshot() {
  const long n = now();
  auto pr = [&](const ProviderQuota& p) {
    Serial1.printf("[%s] ok=%d relogin=%d stale=%d\n", p.name,
                   p.ok ? 1 : 0, p.needsRelogin ? 1 : 0, p.isStale(n, 900) ? 1 : 0);
    if (p.session.present)
      Serial1.printf("  session: used %.1f%%  left %.1f%%  reset_in %lds\n",
                     p.session.usedPercent, p.session.remainingPercent(),
                     p.session.resetEpoch > 0 ? (p.session.resetEpoch - n) : 0L);
    if (p.weekly.present)
      Serial1.printf("  weekly : used %.1f%%  left %.1f%%  reset_in %lds\n",
                     p.weekly.usedPercent, p.weekly.remainingPercent(),
                     p.weekly.resetEpoch > 0 ? (p.weekly.resetEpoch - n) : 0L);
    if (p.hasBalance) Serial1.printf("  balance : %.2f\n", p.balance);
    if (p.hasPlan) Serial1.printf("  plan    : %s\n", p.planType);
  };
  Serial1.println("---- usage snapshot ----");
  pr(snapshot_.left);
  pr(snapshot_.right);
  Serial1.println("------------------------");
}

}  // namespace usage_monitor
