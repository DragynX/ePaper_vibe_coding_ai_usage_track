#pragma once
#include "UsageSnapshot.h"

namespace usage_monitor {

class UsageClientBase {
 public:
  virtual bool fetch(long nowEpoch, ProviderQuota& out) = 0;
  virtual ~UsageClientBase() = default;
};

}  // namespace usage_monitor
