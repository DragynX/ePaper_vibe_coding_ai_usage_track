// HttpClient.h -- thin HTTPS transport over HTTPClient + WiFiClientSecure.
// 对 HTTPClient + WiFiClientSecure 的薄封装:GET/POST、超时、响应头捕获、Retry-After。
//
// Centralizes what VoiceMemo copy-pasted into three clients, and adds the two
// things it never did: reading response headers and parsing Retry-After.

#ifndef USAGE_MONITOR_HTTP_CLIENT_H
#define USAGE_MONITOR_HTTP_CLIENT_H

#include <Arduino.h>

namespace usage_monitor {

// A request header (value owned as a String so callers can build it inline).
struct HttpHeader {
  const char* name;
  String value;
};

struct HttpResult {
  int    status = 0;              // HTTP status, or <0 on transport failure
  String body;
  long   retryAfterSeconds = -1;  // parsed Retry-After, or -1 if absent
  String h0, h1, h2;             // captured response headers, in respKeys order
};

class HttpClient {
 public:
  void configure(uint32_t timeoutMs);

  // GET with optional request headers, optional response-header capture (up to
  // 3 keys -> h0/h1/h2), and an optional per-call User-Agent.
  HttpResult get(const String& url, const HttpHeader* req, size_t reqN,
                 const char* const* respKeys, size_t respKeyN,
                 const char* userAgent = nullptr);

  // POST a body with a content type and optional per-call User-Agent.
  HttpResult post(const String& url, const HttpHeader* req, size_t reqN,
                  const String& body, const char* contentType,
                  const char* userAgent = nullptr);

  // Result of the most recent send() — used by the caller to surface a failure
  // reason. lastStatus < 0 = transport/network failure; lastError = parsed
  // error "message" from the body (empty when none).
  int           lastStatus() const { return lastStatus_; }
  const String& lastError()  const { return lastError_; }
  // Parsed Retry-After of the most recent send() in seconds, or -1 if absent.
  // Drives the 429 backoff (server value overrides the exponential schedule).
  long          lastRetryAfter() const { return lastRetryAfter_; }

 private:
  HttpResult send(bool isPost, const String& url, const HttpHeader* req, size_t reqN,
                  const String& body, const char* contentType,
                  const char* const* respKeys, size_t respKeyN,
                  const char* userAgent);

  uint32_t timeoutMs_ = 45000;
  int      lastStatus_ = 0;
  String   lastError_;
  long     lastRetryAfter_ = -1;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_HTTP_CLIENT_H
