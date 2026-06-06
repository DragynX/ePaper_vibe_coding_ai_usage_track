#include "ProjectHttpClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "AppLog.h"

namespace usage_monitor {

void HttpClient::configure(uint32_t timeoutMs) {
  timeoutMs_ = timeoutMs;
}

// Open either an https (TLS) or http connection. Mirrors VoiceMemo's beginHttp.
// TODO(security): replace setInsecure() with pinned CA roots for the four hosts.
static bool beginHttp(HTTPClient& http, WiFiClientSecure& secure, const String& url) {
  if (url.startsWith("https://")) {
    secure.setInsecure();
    return http.begin(secure, url);
  }
  return http.begin(url);
}

HttpResult HttpClient::send(bool isPost, const String& url, const HttpHeader* req,
                            size_t reqN, const String& body, const char* contentType,
                            const char* const* respKeys, size_t respKeyN,
                            const char* userAgent) {
  HttpResult r;
  HTTPClient http;
  WiFiClientSecure secure;
  http.setTimeout(timeoutMs_);
  if (userAgent) http.setUserAgent(userAgent);

  if (!beginHttp(http, secure, url)) {
    r.status = -1;
    return r;
  }

  for (size_t i = 0; i < reqN; ++i) http.addHeader(req[i].name, req[i].value);
  if (isPost && contentType) http.addHeader("Content-Type", contentType);

  // Register the response headers we want to read back (caller keys + Retry-After).
  // The key strings must outlive the request; callers pass static literals.
  const char* keys[5];
  size_t nk = 0;
  for (size_t i = 0; i < respKeyN && nk < 4; ++i) keys[nk++] = respKeys[i];
  keys[nk++] = "Retry-After";
  http.collectHeaders(keys, nk);

  const int code = isPost ? http.POST(body) : http.GET();
  r.status = code;
  if (code > 0) {
    r.body = http.getString();
    if (respKeyN > 0) r.h0 = http.header(respKeys[0]);
    if (respKeyN > 1) r.h1 = http.header(respKeys[1]);
    if (respKeyN > 2) r.h2 = http.header(respKeys[2]);
    const String ra = http.header("Retry-After");
    if (ra.length()) r.retryAfterSeconds = ra.toInt();
  }
  http.end();

  // Log method, URL (truncated — no tokens in these URLs), status, and size.
  sysLog("[http] %s %.60s -> %d (%u bytes)",
         isPost ? "POST" : "GET", url.c_str(), code,
         static_cast<unsigned>(r.body.length()));
  if (code != 200 && r.body.length() > 0)
    sysLog("[http] err: %.100s", r.body.c_str());
  return r;
}

HttpResult HttpClient::get(const String& url, const HttpHeader* req, size_t reqN,
                           const char* const* respKeys, size_t respKeyN,
                           const char* userAgent) {
  return send(false, url, req, reqN, String(), nullptr, respKeys, respKeyN, userAgent);
}

HttpResult HttpClient::post(const String& url, const HttpHeader* req, size_t reqN,
                            const String& body, const char* contentType,
                            const char* userAgent) {
  return send(true, url, req, reqN, body, contentType, nullptr, 0, userAgent);
}

}  // namespace usage_monitor
