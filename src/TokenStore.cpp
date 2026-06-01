#include "TokenStore.h"

#include <Preferences.h>

#include "UiLang.h"

namespace usage_monitor {

namespace {
constexpr const char* kNs = "umtok";   // NVS namespace (<= 15 chars)

// Build a per-provider key like "L_at" / "R_rt" into a stack buffer (<= 15 chars).
void key(char* buf, size_t n, const char* providerKey, const char* field) {
  snprintf(buf, n, "%s_%s", providerKey, field);
}
}  // namespace

bool TokenStore::begin() {
  Preferences prefs;
  if (!prefs.begin(kNs, /*readOnly=*/false)) return false;
  const int stored = prefs.getInt("lang", -1);
  if (umShouldWipeForLanguage(stored, UM_LANG_ZH)) {
    prefs.clear();
    prefs.putInt("lang", UM_LANG_ZH);
  }
  prefs.end();
  return true;
}

bool TokenStore::load(const char* pk, AuthState& out) {
  Preferences prefs;
  if (!prefs.begin(kNs, /*readOnly=*/true)) return false;

  char k[16];
  key(k, sizeof(k), pk, "at");
  const String at = prefs.getString(k, "");
  if (at.length()) {
    out.accessToken = at;
    key(k, sizeof(k), pk, "rt");  out.refreshToken = prefs.getString(k, out.refreshToken);
    key(k, sizeof(k), pk, "aid"); out.accountId    = prefs.getString(k, out.accountId);
    key(k, sizeof(k), pk, "exp"); out.expiryEpoch  = static_cast<long>(
        prefs.getUInt(k, static_cast<uint32_t>(out.expiryEpoch)));
  }
  prefs.end();
  return at.length() > 0;
}

bool TokenStore::save(const char* pk, const AuthState& st) {
  Preferences prefs;
  if (!prefs.begin(kNs, /*readOnly=*/false)) return false;
  char k[16];
  key(k, sizeof(k), pk, "at");  prefs.putString(k, st.accessToken);
  key(k, sizeof(k), pk, "rt");  prefs.putString(k, st.refreshToken);
  key(k, sizeof(k), pk, "aid"); prefs.putString(k, st.accountId);
  key(k, sizeof(k), pk, "exp"); prefs.putUInt(k, static_cast<uint32_t>(st.expiryEpoch));
  prefs.end();
  return true;
}

}  // namespace usage_monitor
