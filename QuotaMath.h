// QuotaMath.h -- pure helpers mapping a used-percentage to a quota status.
// 纯函数:把"已用百分比"映射成额度状态。Header-only,可在 native 单测里直接验证。

#ifndef USAGE_MONITOR_QUOTA_MATH_H
#define USAGE_MONITOR_QUOTA_MATH_H

namespace usage_monitor {

// Health buckets for a quota window, mirroring ClaudeBar's QuotaStatus.
enum class QuotaStatus { kUnknown, kHealthy, kWarning, kCritical, kDepleted };

// Clamp a raw used-percent into [0, 100].
inline double umClampPercent(double usedPercent) {
  if (usedPercent < 0.0) return 0.0;
  if (usedPercent > 100.0) return 100.0;
  return usedPercent;
}

// Remaining percent = 100 - clamped(used).
inline double umRemainingPercent(double usedPercent) {
  return 100.0 - umClampPercent(usedPercent);
}

// Status from remaining percent:
//   <= 0  depleted, < 20 critical, < 50 warning, otherwise healthy.
inline QuotaStatus umStatusFromRemaining(double remainingPercent) {
  if (remainingPercent <= 0.0) return QuotaStatus::kDepleted;
  if (remainingPercent < 20.0) return QuotaStatus::kCritical;
  if (remainingPercent < 50.0) return QuotaStatus::kWarning;
  return QuotaStatus::kHealthy;
}

// Convenience: status directly from a used percentage.
inline QuotaStatus umStatusFromUsed(double usedPercent) {
  return umStatusFromRemaining(umRemainingPercent(usedPercent));
}

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_QUOTA_MATH_H
