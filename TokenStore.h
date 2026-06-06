// TokenStore.h -- NVS persistence of rotated OAuth tokens (Preferences).
// 用 NVS 持久化刷新后的 OAuth token。键名前缀区分 provider("c"=Claude,"x"=Codex)。

#ifndef USAGE_MONITOR_TOKEN_STORE_H
#define USAGE_MONITOR_TOKEN_STORE_H

#include <Arduino.h>
#include "OAuthClient.h"   // AuthState

namespace usage_monitor {

class TokenStore {
 public:
  // Open the namespace and wipe it if the firmware language changed.
  bool begin();

  // Overwrite token fields of `out` from NVS when present (leaves
  // usesAbsoluteExpiry untouched). Returns true if an access token was stored.
  bool load(const char* providerKey, AuthState& out);

  // Persist the token fields under the provider key prefix. `seed` is the
  // user-entered credential (from ConfigStore) the token chain derives from;
  // the cached chain is only valid while the seed is unchanged.
  bool save(const char* providerKey, const AuthState& st, const String& seed);

  // True when the stored seed equals `seed` — i.e. the cached token chain
  // still derives from the credentials currently in ConfigStore.
  bool seedMatches(const char* providerKey, const String& seed);

  // Remove all cached fields (at/rt/aid/exp/seed) for the provider.
  void clearProvider(const char* providerKey);
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_TOKEN_STORE_H
