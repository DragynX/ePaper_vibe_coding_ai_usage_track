// UsageSnapshot.h -- the in-memory data model the UI consumes.
// UI 消费的内存数据模型。两个 provider 各一份 ProviderQuota,各含 session/weekly
// 等窗口、credits、套餐与陈旧判定。

#ifndef USAGE_MONITOR_USAGE_SNAPSHOT_H
#define USAGE_MONITOR_USAGE_SNAPSHOT_H

#include "QuotaMath.h"

namespace usage_monitor {

enum class ProviderId { kClaude, kCodex, kCopilot, kMiniMax, kKimi, kZai, kClaudePlatform };

struct LocalModelStat {
  char name[24] = {0};
  uint32_t tokens = 0;
  uint16_t count = 0;
};

struct LocalProviderStats {
  bool enabled = false;
  bool available = false;
  char status[24] = {0};
  uint32_t todayTokens = 0;
  uint32_t inputTokens = 0;
  uint32_t outputTokens = 0;
  uint32_t cacheTokens = 0;
  uint16_t sessionCount = 0;
  long latestEpoch = 0;
  uint8_t modelCount = 0;
  LocalModelStat models[3];
};

// One rate-limit window (5-hour session or 7-day weekly).
struct WindowQuota {
  bool        present = false;        // the field existed in the response
  double      usedPercent = 0.0;      // 0..100
  long        resetEpoch = 0;         // normalized UTC epoch (0 = unknown)
  QuotaStatus status = QuotaStatus::kUnknown;
  double remainingPercent() const { return umRemainingPercent(usedPercent); }
};

// A single provider's quota snapshot.
struct ProviderQuota {
  ProviderId id = ProviderId::kClaude;
  char name[16] = {0};               // display name, e.g. "Claude", "Codex", "Copilot"
  bool ok = false;                    // last fetch succeeded
  bool needsRelogin = false;          // refresh token revoked -> user must re-login
  bool refreshFailed = false;         // a token refresh was attempted and failed
  bool disabled = false;              // circuit breaker tripped (2 fails) -> stopped
  char failReason[48] = {0};          // human-readable stop reason for the UI notice
  long lastSuccessEpoch = 0;          // for staleness

  WindowQuota session;                // Claude five_hour / Codex primary_window
  WindowQuota weekly;                 // Claude seven_day  / Codex secondary_window
  WindowQuota weeklySonnet;           // Claude only (present=false on Codex)
  WindowQuota weeklyOpus;             // Claude only

  bool   extraEnabled = false;        // Claude extra_usage.is_enabled
  double extraUsedCents = 0.0;        // Claude extra_usage.used_credits (cents)
  double extraLimitCents = 0.0;       // Claude extra_usage.monthly_limit (cents)

  bool   hasBalance = false;          // Codex credits present
  double balance = 0.0;               // Codex credits.balance (provider-native units)

  bool   hasPlan = false;
  char   planType[16] = {0};          // "plus" / "pro" / "free" / ... from the API
  LocalProviderStats local;

  // Stale when the last fetch failed, never succeeded, or is older than ttlSec.
  bool isStale(long nowEpoch, long ttlSec) const {
    if (!ok || lastSuccessEpoch <= 0) return true;
    return (nowEpoch - lastSuccessEpoch) > ttlSec;
  }
};

// The two displayed providers (left column and right column).
struct UsageSnapshot {
  ProviderQuota left;
  ProviderQuota right;
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_USAGE_SNAPSHOT_H
