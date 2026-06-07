#include "ProjectHttpClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "AppLog.h"

namespace usage_monitor {

void HttpClient::configure(uint32_t timeoutMs) {
  timeoutMs_ = timeoutMs;
}

// Pull the value of a JSON "message" field out of an error body without a full
// parse: find "message", skip to the value's opening quote, copy until the next
// unescaped quote. Returns "" if absent. Good enough for provider error blobs
// like {"error":{"type":"...","message":"invalid x-api-key"}}.
static String extractJsonMessage(const String& body) {
  int k = body.indexOf("\"message\"");
  if (k < 0) return String();
  int colon = body.indexOf(':', k);
  if (colon < 0) return String();
  int q = body.indexOf('"', colon);
  if (q < 0) return String();
  String out;
  for (int i = q + 1; i < (int)body.length(); ++i) {
    const char c = body[i];
    if (c == '\\') { if (i + 1 < (int)body.length()) out += body[++i]; continue; }
    if (c == '"') break;
    out += c;
    if (out.length() >= 60) break;
  }
  return out;
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

  // Remember the outcome so the caller can surface a failure reason.
  lastStatus_ = code;
  lastError_  = (code > 0 && code != 200) ? extractJsonMessage(r.body) : String();

  // Log method, URL (truncated — no tokens in these URLs), status, and size.
  sysLog("[http] %s %.60s -> %d (%u bytes)",
         isPost ? "POST" : "GET", url.c_str(), code,
         static_cast<unsigned>(r.body.length()));
  if (code <= 0)
    sysLog("[http] NO RESPONSE %.60s err=%d (%s)", url.c_str(), code,
           HTTPClient::errorToString(code).c_str());
  else if (code != 200 && r.body.length() > 0)
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
