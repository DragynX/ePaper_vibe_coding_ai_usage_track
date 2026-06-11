#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include "ConfigStore.h"

namespace usage_monitor {

class SettingsServer {
 public:
  // Register routes on server. Call before server->begin().
  // battPct: optional battery percentage provider (return -1 = unknown).
  // onSaved: optional callback fired after a successful POST /api/settings save
  //          (runs in the async server task — set a flag, don't touch the e-paper).
  // credJson: optional provider of the per-credential status JSON for /api/credstatus.
  void begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)() = nullptr,
             std::function<void()> onSaved = nullptr,
             std::function<String()> credJson = nullptr,
             std::function<void()> onKeepAlive = nullptr,
             std::function<void()> onSleepNow = nullptr,
             std::function<int()> sleepInSec = nullptr,
             std::function<int()> bootId = nullptr,
             int (*battMv)() = nullptr,
             std::function<int()> battDays = nullptr,
             std::function<void(int,int,int,int,int,int)> onFontTest = nullptr);

 private:
  ConfigStore* cfg_ = nullptr;
  int (*battPct_)() = nullptr;
  int (*battMv_)() = nullptr;
  std::function<int()> battDays_;   // est hours on battery, -1 = n/a
  std::function<void()> onSaved_;
  std::function<String()> credJson_;
  std::function<void()> onKeepAlive_;
  std::function<void()> onSleepNow_;
  std::function<int()> sleepInSec_;
  std::function<int()> bootId_;
  std::function<void(int,int,int,int,int,int)> onFontTest_;  // (on,font,dark,all,crisp,sizeIdx); <0 = unchanged
};

}  // namespace usage_monitor
