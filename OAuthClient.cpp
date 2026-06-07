#include "OAuthClient.h"

#include "AppLog.h"
#include "IsoTime.h"

namespace usage_monitor {

void OAuthClient::configure(HttpClient* http, OAuthProvider* provider, AuthState* state) {
  http_ = http;
  provider_ = provider;
  state_ = state;
}

bool OAuthClient::tokenExpired(long nowEpoch) const {
  if (!state_) return true;
  return state_->usesAbsoluteExpiry
             ? umClaudeTokenExpired(state_->expiryEpoch, nowEpoch)
             : umCodexTokenExpired(state_->expiryEpoch, nowEpoch);
}

AuthedResult OAuthClient::get(const String& url, const HttpHeader* extra, size_t extraN,
                              const char* const* respKeys, size_t respKeyN,
                              long nowEpoch, const char* userAgent) {
  AuthedResult ar;
  if (!http_ || !provider_ || !state_) {
    sysLog("[oauth] get: not configured");
    ar.http.status = -1;
    return ar;
  }

  // Proactive refresh when the token is known-expired.
  if (tokenExpired(nowEpoch)) {
    sysLog("[oauth] token expired, proactive refresh");
    if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) {
      ar.refreshed = true;
    } else {
      if (!ar.needsRelogin) ar.refreshFailed = true;  // failed but not revoked
      sysLog("[oauth] proactive refresh failed, needsRelogin=%d", (int)ar.needsRelogin);
      return ar;
    }
  }

  sysLog("[oauth] at_len=%d rt_len=%d exp=%ld now=%ld",
         (int)state_->accessToken.length(),
         (int)state_->refreshToken.length(),
         state_->expiryEpoch, nowEpoch);

  // Up to two attempts: the first, then one more after a reactive refresh.
  for (int attempt = 0; attempt < 2; ++attempt) {
    HttpHeader headers[8];
    size_t n = 0;
    String token = state_->accessToken;
    token.trim();   // Claude requires a trimmed bearer token
    if (token.length() == 0) sysLog("[oauth] WARN empty access token");
    headers[n++] = { "Authorization", String("Bearer ") + token };
    for (size_t i = 0; i < extraN && n < 8; ++i) headers[n++] = extra[i];

    ar.http = http_->get(url, headers, n, respKeys, respKeyN, userAgent);

    if (ar.http.status != 401 && ar.http.status != 403) break;
    if (attempt == 0) {
      sysLog("[oauth] %d -> reactive refresh", ar.http.status);
      if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) {
        ar.refreshed = true;
      } else {
        if (!ar.needsRelogin) ar.refreshFailed = true;
        sysLog("[oauth] reactive refresh failed, needsRelogin=%d", (int)ar.needsRelogin);
        break;
      }
    }
  }
  return ar;
}

AuthedResult OAuthClient::post(const String& url, const HttpHeader* extra, size_t extraN,
                               const String& body, const char* contentType,
                               long nowEpoch, const char* userAgent) {
  AuthedResult ar;
  if (!http_ || !provider_ || !state_) {
    sysLog("[oauth] post: not configured");
    ar.http.status = -1;
    return ar;
  }

  if (tokenExpired(nowEpoch)) {
    sysLog("[oauth] token expired, proactive refresh");
    if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) {
      ar.refreshed = true;
    } else {
      if (!ar.needsRelogin) ar.refreshFailed = true;
      sysLog("[oauth] proactive refresh failed, needsRelogin=%d", (int)ar.needsRelogin);
      return ar;
    }
  }

  sysLog("[oauth] at_len=%d rt_len=%d exp=%ld now=%ld",
         (int)state_->accessToken.length(),
         (int)state_->refreshToken.length(),
         state_->expiryEpoch, nowEpoch);

  for (int attempt = 0; attempt < 2; ++attempt) {
    HttpHeader headers[8];
    size_t n = 0;
    String token = state_->accessToken;
    token.trim();
    if (token.length() == 0) sysLog("[oauth] WARN empty access token");
    headers[n++] = { "Authorization", String("Bearer ") + token };
    for (size_t i = 0; i < extraN && n < 8; ++i) headers[n++] = extra[i];

    ar.http = http_->post(url, headers, n, body, contentType, userAgent);
    if (ar.http.status != 401 && ar.http.status != 403) break;
    if (attempt == 0) {
      sysLog("[oauth] %d -> reactive refresh", ar.http.status);
      if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) ar.refreshed = true;
      else { if (!ar.needsRelogin) ar.refreshFailed = true;
             sysLog("[oauth] reactive refresh failed, needsRelogin=%d", (int)ar.needsRelogin); break; }
    }
  }
  return ar;
}

}  // namespace usage_monitor
