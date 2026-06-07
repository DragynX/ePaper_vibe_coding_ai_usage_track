// OAuthClient.h -- provider-agnostic "authed request with refresh+retry" engine.
// 与 provider 无关的鉴权引擎:带 Bearer 发请求,401/403 时刷新 token 重试一次。
//
// Provider differences (Claude JSON refresh + absolute expiry; Codex form
// refresh + 8-day rolling) live behind the OAuthProvider interface.

#ifndef USAGE_MONITOR_OAUTH_CLIENT_H
#define USAGE_MONITOR_OAUTH_CLIENT_H

#include <Arduino.h>
#include "ProjectHttpClient.h"

namespace usage_monitor {

// Mutable token state for one provider, persisted by TokenStore.
struct AuthState {
  String accessToken;
  String refreshToken;
  String accountId;                 // Codex only
  long   expiryEpoch = 0;           // Claude: absolute expiry (sec); Codex: last_refresh (sec)
  bool   usesAbsoluteExpiry = true; // true=Claude (expiry), false=Codex (8-day age)
};

// Provider-specific token refresh. Implementations mutate `st` on success and
// set needsRelogin=true + return false when the refresh token is dead.
class OAuthProvider {
 public:
  virtual ~OAuthProvider() {}
  virtual bool refresh(HttpClient& http, AuthState& st, long nowEpoch,
                       bool& needsRelogin) = 0;
};

// For providers with static keys (PAT, API key) that never need refresh.
// tokenExpired() returns true on expiryEpoch==0, triggers this no-op, and
// proceeds — no special neverExpires flag needed.
class StaticKeyAuthProvider : public OAuthProvider {
 public:
  bool refresh(HttpClient&, AuthState&, long, bool& needsRelogin) override {
    needsRelogin = false;
    return true;
  }
};

struct AuthedResult {
  HttpResult http;
  bool refreshed = false;       // a token refresh happened during this call
  bool needsRelogin = false;    // refresh token is dead; user must re-login
  bool refreshFailed = false;   // a refresh was attempted and failed (not revoked)
};

class OAuthClient {
 public:
  void configure(HttpClient* http, OAuthProvider* provider, AuthState* state);
  AuthState* state() { return state_; }

  bool tokenExpired(long nowEpoch) const;

  // Authed GET/POST: proactive refresh if expired, then on 401/403 refresh
  // once and retry exactly once.
  AuthedResult get(const String& url, const HttpHeader* extra, size_t extraN,
                   const char* const* respKeys, size_t respKeyN,
                   long nowEpoch, const char* userAgent);
  AuthedResult post(const String& url, const HttpHeader* extra, size_t extraN,
                    const String& body, const char* contentType,
                    long nowEpoch, const char* userAgent);
  HttpClient* http() { return http_; }

 private:
  HttpClient*    http_ = nullptr;
  OAuthProvider* provider_ = nullptr;
  AuthState*     state_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_OAUTH_CLIENT_H
