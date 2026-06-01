// IsoTime.h -- UTC time parsing and token-expiry predicates (pure, testable).
// 纯函数:UTC 时间解析与 token 过期判定。
//
// Unlike VoiceMemo's parseLocalDateTime (sscanf + mktime, which treats input as
// LOCAL time and ignores 'Z'), this parses with timegm so an API timestamp like
// "2026-05-30T12:00:00Z" yields the correct UTC epoch.
// 区别于 VoiceMemo 的本地时间解析:这里用 timegm 按 UTC 解释,正确处理 'Z' 与偏移。

#ifndef USAGE_MONITOR_ISO_TIME_H
#define USAGE_MONITOR_ISO_TIME_H

#include <time.h>
#include <stdio.h>
#include <string.h>

namespace usage_monitor {

// Days since epoch for a UTC calendar date (civil-from-days algorithm by
// Howard Hinnant). Avoids depending on timegm, which is absent on ESP32 newlib.
// 把 UTC 年月日换算成自 1970-01-01 起的天数(避免依赖 ESP32 上缺失的 timegm)。
inline long umDaysFromCivil(long y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + static_cast<long>(doe) - 719468L;
}

// Convert a broken-down UTC time to a Unix epoch (seconds). Portable
// replacement for timegm; identical result on host and device.
inline long umTimegm(int year, int mon, int day, int hour, int min, int sec) {
  const long days = umDaysFromCivil(year, static_cast<unsigned>(mon),
                                    static_cast<unsigned>(day));
  return days * 86400L + hour * 3600L + min * 60L + sec;
}

// Parse a timezone offset (seconds) from an ISO8601 string. Scans only after
// the date/time separator so the date's hyphens are not mistaken for a sign.
// 'Z' (or no offset) -> 0.
inline long umParseTzOffset(const char* s) {
  const char* p = strchr(s, 'T');
  if (!p) p = strchr(s, ' ');
  if (!p) return 0;
  for (; *p; ++p) {
    if (*p == 'Z' || *p == 'z') return 0;
    if (*p == '+' || *p == '-') {
      int oh = 0, om = 0;
      if (sscanf(p + 1, "%d:%d", &oh, &om) >= 1) {
        const long off = static_cast<long>(oh) * 3600 + static_cast<long>(om) * 60;
        return (*p == '-') ? -off : off;
      }
    }
  }
  return 0;
}

// Parse an ISO8601 timestamp to a UTC epoch (seconds) via timegm. Accepts 'T'
// or ' ' as separator, optional fractional seconds, and an optional
// +hh:mm / -hh:mm offset or 'Z'. Returns 0 on parse failure.
inline long umParseIso8601(const char* s) {
  if (!s) return 0;
  int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
  if (sscanf(s, "%d-%d-%dT%d:%d:%d", &year, &mon, &day, &hour, &min, &sec) < 5 &&
      sscanf(s, "%d-%d-%d %d:%d:%d", &year, &mon, &day, &hour, &min, &sec) < 5) {
    return 0;
  }
  if (year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
  const long epoch = umTimegm(year, mon, day, hour, min, sec);
  return epoch - umParseTzOffset(s);
}

// Normalize a reset time: prefer a plausible absolute unix timestamp, else
// now + relative seconds, else 0.
// Codex returns reset_at (absolute) or reset_after_seconds (relative).
inline long umNormalizeReset(long resetAtUnix, long resetAfterSec, long nowEpoch) {
  if (resetAtUnix > 1000000000L) return resetAtUnix;   // ~2001+, plausible absolute
  if (resetAfterSec > 0) return nowEpoch + resetAfterSec;
  return 0;
}

// Claude access token: expired if missing or within a 5-minute buffer of expiry.
inline bool umClaudeTokenExpired(long expiryEpochSec, long nowEpoch) {
  if (expiryEpochSec <= 0) return true;
  return nowEpoch + 300 >= expiryEpochSec;
}

// Codex access token: refreshed on an 8-day rolling basis (no explicit expiry,
// mirroring the Codex CLI strategy).
inline bool umCodexTokenExpired(long lastRefreshSec, long nowEpoch) {
  if (lastRefreshSec <= 0) return true;
  return (nowEpoch - lastRefreshSec) > 8L * 24 * 3600;
}

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_ISO_TIME_H
