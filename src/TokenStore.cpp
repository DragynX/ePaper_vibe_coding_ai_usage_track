#include "TokenStore.h"

#include <Preferences.h>

#include "UiLang.h"

namespace usage_monitor {

namespace {
constexpr const char* kNs = "umtok";   // NVS namespace (<= 15 chars)

// Build a per-provider key like "c_at" / "x_rt" (<= 15 chars).
String key(const char* providerKey, const char* field) {
  return String(providerKey) + "_" + field;
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

  const String at = prefs.getString(key(pk, "at").c_str(), "");
  if (at.length()) {
    out.accessToken  = at;
    out.refreshToken = prefs.getString(key(pk, "rt").c_str(), out.refreshToken);
    out.accountId    = prefs.getString(key(pk, "aid").c_str(), out.accountId);
    out.expiryEpoch  = static_cast<long>(
        prefs.getUInt(key(pk, "exp").c_str(), static_cast<uint32_t>(out.expiryEpoch)));
  }
  prefs.end();
  return at.length() > 0;
}

bool TokenStore::save(const char* pk, const AuthState& st) {
  Preferences prefs;
  if (!prefs.begin(kNs, /*readOnly=*/false)) return false;
  prefs.putString(key(pk, "at").c_str(), st.accessToken);
  prefs.putString(key(pk, "rt").c_str(), st.refreshToken);
  prefs.putString(key(pk, "aid").c_str(), st.accountId);
  prefs.putUInt(key(pk, "exp").c_str(), static_cast<uint32_t>(st.expiryEpoch));
  prefs.end();
  return true;
}

}  // namespace usage_monitor
