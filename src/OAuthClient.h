// OAuthClient.h -- provider-agnostic "authed request with refresh+retry" engine.
// 与 provider 无关的鉴权引擎:带 Bearer 发请求,401/403 时刷新 token 重试一次。
//
// Provider differences (Claude JSON refresh + absolute expiry; Codex form
// refresh + 8-day rolling) live behind the OAuthProvider interface.

#ifndef USAGE_MONITOR_OAUTH_CLIENT_H
#define USAGE_MONITOR_OAUTH_CLIENT_H

#include <Arduino.h>
#include "HttpClient.h"

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

struct AuthedResult {
  HttpResult http;
  bool refreshed = false;       // a token refresh happened during this call
  bool needsRelogin = false;    // refresh token is dead; user must re-login
};

class OAuthClient {
 public:
  void configure(HttpClient* http, OAuthProvider* provider, AuthState* state);
  AuthState* state() { return state_; }

  bool tokenExpired(long nowEpoch) const;

  // Authed GET: proactive refresh if expired, then on 401/403 refresh once and
  // retry exactly once.
  AuthedResult get(const String& url, const HttpHeader* extra, size_t extraN,
                   const char* const* respKeys, size_t respKeyN,
                   long nowEpoch, const char* userAgent);

 private:
  HttpClient*    http_ = nullptr;
  OAuthProvider* provider_ = nullptr;
  AuthState*     state_ = nullptr;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_OAUTH_CLIENT_H
