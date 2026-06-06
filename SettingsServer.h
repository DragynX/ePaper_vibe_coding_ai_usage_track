#pragma once
#include <ESPAsyncWebServer.h>
#include "ConfigStore.h"

namespace usage_monitor {

class SettingsServer {
 public:
  // Register routes on server. Call before server->begin().
  // battPct: optional battery percentage provider (return -1 = unknown).
  void begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)() = nullptr);

 private:
  ConfigStore* cfg_ = nullptr;
  int (*battPct_)() = nullptr;
};

}  // namespace usage_monitor
