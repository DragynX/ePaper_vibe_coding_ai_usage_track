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
             std::function<String()> credJson = nullptr);

 private:
  ConfigStore* cfg_ = nullptr;
  int (*battPct_)() = nullptr;
  std::function<void()> onSaved_;
  std::function<String()> credJson_;
};

}  // namespace usage_monitor
