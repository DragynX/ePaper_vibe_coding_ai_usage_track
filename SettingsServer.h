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
  void begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)() = nullptr,
             std::function<void()> onSaved = nullptr);

 private:
  ConfigStore* cfg_ = nullptr;
  int (*battPct_)() = nullptr;
  std::function<void()> onSaved_;
};

}  // namespace usage_monitor
