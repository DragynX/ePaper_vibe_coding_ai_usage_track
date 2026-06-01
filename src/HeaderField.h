// HeaderField.h -- "header value preferred, body value as fallback" selection.
// 纯函数:数值字段"响应头优先、body 回退",带解析容错。
//
// Codex returns usage both as HTTP response headers (x-codex-*) and in the body.
// The header is authoritative when present and numeric; otherwise fall back to
// the parsed body value.
// Codex 同时在响应头(x-codex-*)和 body 里给用量:头存在且是数字时以头为准,否则回退 body。

#ifndef USAGE_MONITOR_HEADER_FIELD_H
#define USAGE_MONITOR_HEADER_FIELD_H

#include <stdlib.h>

namespace usage_monitor {

// Parse a double from a C-string. Returns false on null/empty/non-numeric input
// so the caller can fall back.
inline bool umParseDouble(const char* s, double& out) {
  if (!s || s[0] == '\0') return false;
  char* end = nullptr;
  const double v = strtod(s, &end);
  if (end == s) return false;   // no digits consumed
  out = v;
  return true;
}

// Pick a numeric field, preferring the header string, falling back to a parsed
// body value. outPresent reports whether any source supplied a value.
inline double umPickNumber(const char* headerVal, double bodyVal, bool bodyPresent,
                           bool& outPresent) {
  double h = 0.0;
  if (umParseDouble(headerVal, h)) {
    outPresent = true;
    return h;
  }
  outPresent = bodyPresent;
  return bodyPresent ? bodyVal : 0.0;
}

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_HEADER_FIELD_H
