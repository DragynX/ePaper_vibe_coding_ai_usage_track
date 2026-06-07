// UsageApp.h -- application orchestrator (boot, WiFi, NTP, poll, persist).
// All seven providers compiled in; active selection persisted in NVS via ConfigStore.

#ifndef USAGE_MONITOR_USAGE_APP_H
#define USAGE_MONITOR_USAGE_APP_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "ClaudePlatformUsageClient.h"
#include "ClaudeUsageClient.h"
#include "CodexUsageClient.h"
#include "ConfigStore.h"
#include "CopilotUsageClient.h"
#include "KimiUsageClient.h"
#include "LocalStatsClient.h"
#include "MiniMaxUsageClient.h"
#include "OAuthClient.h"
#include "ProjectHttpClient.h"
#include "ProviderSelect.h"
#include "SettingsServer.h"
#include "TokenStore.h"
#include "UsageClientBase.h"
#include "UsageSnapshot.h"
#include "UsageUI.h"
#include "ZaiUsageClient.h"

namespace usage_monitor {

class UsageApp {
 public:
  UsageApp();
  void begin();
  void loop();

 private:
  bool ensureWiFi(uint32_t timeoutMs);
  void syncTime();
  long now();
  void refreshAll();
  void printSnapshot();
  UiStatus currentStatus();
  void setProviderNames();
  void fetchLeft(long nowEpoch);
  void fetchRight(long nowEpoch);
  void fetchLocalStats();
  void configureProviders();
  void wireProvider(uint8_t prov, OAuthClient& oauth,
                    UsageClientBase*& client, AuthState*& authPtr);
  uint8_t testProvider(uint8_t prov);   // returns kCred* status
  void runPendingTokenTests();
  String credStatusJson();
  void enterDeepSleep();

  HttpClient    http_;
  TokenStore    store_;
  ConfigStore   cfgStore_;
  SettingsServer settings_;
  AsyncWebServer server_;    // initialized in constructor initializer list
  UsageUI       ui_;
  UsageSnapshot snapshot_;
  LocalStatsClient localStats_;

  unsigned long lastRefreshMs_     = 0;
  unsigned long bootWindowStartMs_ = 0;
  unsigned long awakeStartMs_      = 0;   // when the current awake window started
  unsigned long awakeWindowMs_     = 0;   // how long to stay awake before sleeping
  long          lastFetchEpoch_    = 0;   // wall-clock of the last successful refresh
  volatile bool sleepNow_          = false;  // web "Sleep" button requested
  bool          timeSynced_        = false;
  bool          settingsAvailable_ = false;

  void extendAwake();          // a user web action -> keep awake 2 min
  int  sleepInSec();           // seconds until deep sleep, or -1 if disabled
  volatile bool redrawPending_     = false;  // set by settings save, handled in loop()

  // Per-side circuit breaker: after 2 consecutive fetch failures a side is
  // stopped (no more network calls) until the next settings save. Reason text
  // is shown on screen.
  uint8_t leftFailCount_  = 0, rightFailCount_ = 0;
  bool    leftDisabled_   = false, rightDisabled_ = false;
  char    leftFailReason_[48] = {0}, rightFailReason_[48] = {0};

  // Per-provider credential status for the Credentials page (index = provider
  // id 0..7): 0=none/white 1=ok/green 2=fail/red 3=testing.
  uint8_t credStatus_[8] = {0};

  // Per-provider auth states
  AuthState claudeAuth_, codexAuth_, copilotAuth_,
            minimaxAuth_, kimiAuth_, zaiAuth_;

  // Per-provider auth providers
  ClaudeAuthProvider    claudeProvider_;
  CodexAuthProvider     codexProvider_;
  StaticKeyAuthProvider copilotProvider_;
  StaticKeyAuthProvider minimaxProvider_;
  KimiAuthProvider      kimiProvider_;
  StaticKeyAuthProvider zaiProvider_;

  // Per-side OAuth clients
  OAuthClient leftOAuth_, rightOAuth_;

  // Per-provider usage clients
  ClaudeUsageClient         claudeClient_;
  CodexUsageClient          codexClient_;
  CopilotUsageClient        copilotClient_;
  MiniMaxUsageClient        minimaxClient_;
  KimiUsageClient           kimiClient_;
  ZaiUsageClient            zaiClient_;
  ClaudePlatformUsageClient claudePlatClient_;

  // Active side pointers (set by configureProviders)
  UsageClientBase* leftClient_   = nullptr;
  UsageClientBase* rightClient_  = nullptr;
  AuthState*       leftAuthPtr_  = nullptr;  // auth state to save after left refresh
  AuthState*       rightAuthPtr_ = nullptr;

  // Owned strings for clients that store raw const char* pointers
  String minimaxBaseUrl_;
  String zaiEndpointStr_;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_USAGE_APP_H
