#pragma once
#include <ESPAsyncWebServer.h>
#include "ConfigStore.h"

namespace usage_monitor {

class SettingsServer {
 public:
  // Register routes on server. Call before server->begin().
  void begin(AsyncWebServer* server, ConfigStore* cfg);

 private:
  ConfigStore* cfg_ = nullptr;
};

}  // namespace usage_monitor
