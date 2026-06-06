// TimeFormat.h -- format a countdown (seconds) into a compact reset label.
// 纯函数:把倒计时秒数格式化成紧凑的重置标签。

#ifndef USAGE_MONITOR_TIME_FORMAT_H
#define USAGE_MONITOR_TIME_FORMAT_H

#include <stddef.h>
#include <stdio.h>

namespace usage_monitor {

// Format a countdown into out[0..n):
//   <= 0       -> "now"
//   < 24 hours -> "H:MM"   (e.g. 7980s -> "2:13", 2700s -> "0:45")
//   >= 24 h    -> "Xd Yh"  (e.g. 86400s -> "1d 0h")
// out is always NUL-terminated when n > 0.
inline void umFormatCountdown(long secs, char* out, size_t n) {
  if (!out || n == 0) return;
  if (secs <= 0) {
    snprintf(out, n, "now");
    return;
  }
  if (secs >= 86400L) {
    const long days = secs / 86400L;
    const long hours = (secs % 86400L) / 3600L;
    snprintf(out, n, "%ldd %ldh", days, hours);
    return;
  }
  const long hours = secs / 3600L;
  const long mins = (secs % 3600L) / 60L;
  snprintf(out, n, "%ld:%02ld", hours, mins);
}

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_TIME_FORMAT_H
