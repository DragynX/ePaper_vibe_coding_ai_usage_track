#include "OAuthClient.h"

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
    ar.http.status = -1;
    return ar;
  }

  // Proactive refresh when the token is known-expired.
  if (tokenExpired(nowEpoch)) {
    if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) {
      ar.refreshed = true;
    } else if (ar.needsRelogin) {
      return ar;
    }
  }

  // Up to two attempts: the first, then one more after a reactive refresh.
  for (int attempt = 0; attempt < 2; ++attempt) {
    HttpHeader headers[8];
    size_t n = 0;
    String token = state_->accessToken;
    token.trim();   // Claude requires a trimmed bearer token
    headers[n++] = { "Authorization", String("Bearer ") + token };
    for (size_t i = 0; i < extraN && n < 8; ++i) headers[n++] = extra[i];

    ar.http = http_->get(url, headers, n, respKeys, respKeyN, userAgent);

    if (ar.http.status != 401 && ar.http.status != 403) break;
    if (attempt == 0) {
      if (provider_->refresh(*http_, *state_, nowEpoch, ar.needsRelogin)) {
        ar.refreshed = true;
      } else {
        break;   // refresh failed (needsRelogin already set if hard failure)
      }
    }
  }
  return ar;
}

}  // namespace usage_monitor
