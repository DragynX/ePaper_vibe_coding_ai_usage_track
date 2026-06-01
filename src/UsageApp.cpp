#include "UsageApp.h"

#include <WiFi.h>
#include <stdlib.h>
#include <time.h>

#include "driver.h"
#include "IsoTime.h"

namespace usage_monitor {

// 2020-01-01; time() below this means the clock is not yet set.
static const long kSanityFloorEpoch = 1577836800L;

UsageApp::UsageApp(const UsageConfig& config) : config_(config) {}

void UsageApp::begin() {
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
  delay(300);
  Serial1.println("\n[boot] UsageMonitor");
  pinMode(UM_LED_PIN, OUTPUT);
  digitalWrite(UM_LED_PIN, LOW);

  // Seed auth from bootstrap config; this also sets the expiry model per provider.
  claudeAuth_.accessToken = config_.claudeAccessToken;
  claudeAuth_.refreshToken = config_.claudeRefreshToken;
  claudeAuth_.expiryEpoch = atoll(config_.claudeExpiresAtMs) / 1000L;  // ms -> sec
  claudeAuth_.usesAbsoluteExpiry = true;

  codexAuth_.accessToken = config_.codexAccessToken;
  codexAuth_.refreshToken = config_.codexRefreshToken;
  codexAuth_.accountId = config_.codexAccountId;
  codexAuth_.expiryEpoch = umParseIso8601(config_.codexLastRefresh);   // ISO -> sec
  codexAuth_.usesAbsoluteExpiry = false;

  // Prefer rotated tokens already in NVS over the bootstrap values.
  store_.begin();
  store_.load("c", claudeAuth_);
  store_.load("x", codexAuth_);

  http_.configure(config_.httpTimeoutMs);
  claudeOAuth_.configure(&http_, &claudeProvider_, &claudeAuth_);
  codexOAuth_.configure(&http_, &codexProvider_, &codexAuth_);
  claude_.configure(&claudeOAuth_);
  codex_.configure(&codexOAuth_);

  if (ensureWiFi(15000)) {
    syncTime();
    refreshAll();
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
  // configTzTime sets the system clock from NTP; time() is UTC regardless of TZ.
  configTzTime(config_.tz, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 20 && static_cast<long>(time(nullptr)) < kSanityFloorEpoch; ++i) {
    delay(500);
  }
  if (static_cast<long>(time(nullptr)) >= kSanityFloorEpoch) {
    timeSynced_ = true;
    Serial1.printf("[time] NTP ok: %ld\n", static_cast<long>(time(nullptr)));
  } else {
    Serial1.println("[time] NTP failed (countdowns may be wrong)");
  }
}

long UsageApp::now() { return static_cast<long>(time(nullptr)); }

void UsageApp::refreshAll() {
  const long n = now();
  if (claude_.fetch(n, snapshot_.claude)) {
    snapshot_.claude.hasPlan = true;
    strncpy(snapshot_.claude.planType, config_.claudeSubscription,
            sizeof(snapshot_.claude.planType) - 1);
    store_.save("c", claudeAuth_);   // TODO(nvs): persist only when tokens rotated
  } else if (snapshot_.claude.needsRelogin) {
    Serial1.println("[claude] needs relogin");
  }
  if (codex_.fetch(n, snapshot_.codex)) {
    store_.save("x", codexAuth_);
  } else if (snapshot_.codex.needsRelogin) {
    Serial1.println("[codex] needs relogin");
  }
  printSnapshot();
}

void UsageApp::printSnapshot() {
  const long n = now();
  auto pr = [&](const char* name, const ProviderQuota& p) {
    Serial1.printf("[%s] ok=%d relogin=%d stale=%d\n", name,
                   p.ok ? 1 : 0, p.needsRelogin ? 1 : 0, p.isStale(n, 900) ? 1 : 0);
    if (p.session.present)
      Serial1.printf("  session: used %.1f%%  left %.1f%%  reset_in %lds\n",
                     p.session.usedPercent, p.session.remainingPercent(),
                     p.session.resetEpoch > 0 ? (p.session.resetEpoch - n) : 0L);
    if (p.weekly.present)
      Serial1.printf("  weekly : used %.1f%%  left %.1f%%  reset_in %lds\n",
                     p.weekly.usedPercent, p.weekly.remainingPercent(),
                     p.weekly.resetEpoch > 0 ? (p.weekly.resetEpoch - n) : 0L);
    if (p.weeklySonnet.present)
      Serial1.printf("  sonnet7d: used %.1f%%\n", p.weeklySonnet.usedPercent);
    if (p.weeklyOpus.present)
      Serial1.printf("  opus7d  : used %.1f%%\n", p.weeklyOpus.usedPercent);
    if (p.hasBalance) Serial1.printf("  balance : %.2f\n", p.balance);
    if (p.extraEnabled)
      Serial1.printf("  extra   : %.0f/%.0f cents\n", p.extraUsedCents, p.extraLimitCents);
    if (p.hasPlan) Serial1.printf("  plan    : %s\n", p.planType);
  };
  Serial1.println("---- usage snapshot ----");
  pr("claude", snapshot_.claude);
  pr("codex", snapshot_.codex);
  Serial1.println("------------------------");
}

}  // namespace usage_monitor
