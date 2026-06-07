// UiLang.h -- compile-time UI language selection and the fixed-string table.
// 编译期界面语言选择与固定文案表。
//
// The whole firmware is built for exactly one language, chosen at compile time
// by the build flag UM_UI_LANG_ZH. Each fixed word has an English / Chinese
// pair (uiStrEn / uiStrZh); a macro picks the active column (uiStr).
// 整套固件只编译一种语言,由 UM_UI_LANG_ZH 决定。每个固定词条有英/中两列,宏选当前列。
//
// Brand / proper nouns (Claude, Codex, LIVE, Plus/Pro/Free) are intentionally
// NOT in this table -- they are not translated and would break the "every entry
// differs across languages" test; render them as literals or from the API value.
// 品牌/专有名词(Claude/Codex/LIVE/Plus/Pro/Free)不入表:它们不翻译,直接用字面量或
// API 原值,否则会破坏单测对"漏翻"的检查。

#ifndef USAGE_MONITOR_UI_LANG_H
#define USAGE_MONITOR_UI_LANG_H

// 1 for the Chinese build, 0 for the English build.
#if defined(UM_UI_LANG_ZH)
  #define UM_LANG_ZH 1
#else
  #define UM_LANG_ZH 0
#endif

// One id per fixed UI string. kCount is a sentinel for iteration in tests.
enum class UiStringId {
  kAppName,        // header title
  kWinSession,     // 5-hour rolling window label
  kWinWeekly,      // 7-day window label
  kWinSonnet,      // Claude Sonnet 7-day window label
  kWinOpus,        // Claude Opus 7-day window label
  kRemaining,      // "left" suffix on the big remaining percentage
  kUsed,           // "used" prefix on the consumed percentage
  kResets,         // countdown prefix
  kPlan,           // plan label
  kExtra,          // Claude extra-usage spend label
  kBalance,        // Codex credits balance label
  kUpdated,        // footer "updated HH:MM"
  kStale,          // stale-data marker
  kRefreshNote,    // footer refresh cadence note
  kBootStarting,   // boot splash: starting
  kBootWifi,       // boot splash: connecting WiFi
  kBootSync,       // boot splash: syncing time (NTP)
  kBootFetch,      // boot splash: fetching usage
  kReloginTitle,   // re-login notice title
  kReloginBody,    // re-login notice body
  kNoWifi,         // WiFi unavailable hint
  kNoData,         // no snapshot yet
  kCloudSummary,   // cloud quota summary title
  kLocalUsage,     // local stats title
  kStatus,         // status row label
  kSessionLeft,    // session remaining row label
  kWeeklyLeft,     // weekly remaining row label
  kQuotaReady,     // cloud quota status value
  kWaiting,        // cloud quota waiting value
  kFallback,       // fallback row label
  kCloudQuota,     // fallback row value
  kTodayTokens,    // local stats today tokens label
  kInOut,          // local stats input/output label
  kSessions,       // local stats session count label
  kLatest,         // local stats latest activity label
  kTopModels,      // local stats top models title
  kModelQuotas,    // model quota section title
  kCount
};

// English column.
inline const char* uiStrEn(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "AI Usage Monitor";
    case UiStringId::kWinSession:   return "Session";
    case UiStringId::kWinWeekly:    return "Weekly";
    case UiStringId::kWinSonnet:    return "Sonnet 7d";
    case UiStringId::kWinOpus:      return "Opus 7d";
    case UiStringId::kRemaining:    return "Remaining";
    case UiStringId::kUsed:         return "used";
    case UiStringId::kResets:       return "resets";
    case UiStringId::kPlan:         return "Plan";
    case UiStringId::kExtra:        return "Extra $";
    case UiStringId::kBalance:      return "Balance $";
    case UiStringId::kUpdated:      return "updated";
    case UiStringId::kStale:        return "stale";
    case UiStringId::kRefreshNote:  return "5 min refresh";
    case UiStringId::kBootStarting: return "Starting...";
    case UiStringId::kBootWifi:     return "Connecting WiFi...";
    case UiStringId::kBootSync:     return "Syncing time...";
    case UiStringId::kBootFetch:    return "Fetching usage...";
    case UiStringId::kReloginTitle: return "Sign-in expired";
    case UiStringId::kReloginBody:  return "Re-login on your computer then re-flash credentials.";
    case UiStringId::kNoWifi:       return "WiFi unavailable";
    case UiStringId::kNoData:       return "No data yet";
    case UiStringId::kCloudSummary: return "CLOUD SUMMARY";
    case UiStringId::kLocalUsage:   return "LOCAL USAGE";
    case UiStringId::kStatus:       return "STATUS";
    case UiStringId::kSessionLeft:  return "SESSION LEFT";
    case UiStringId::kWeeklyLeft:   return "WEEKLY LEFT";
    case UiStringId::kQuotaReady:   return "quota ready";
    case UiStringId::kWaiting:      return "waiting";
    case UiStringId::kFallback:     return "FALLBACK";
    case UiStringId::kCloudQuota:   return "cloud quota";
    case UiStringId::kTodayTokens:  return "TODAY TOKENS";
    case UiStringId::kInOut:        return "IN / OUT";
    case UiStringId::kSessions:     return "SESSIONS";
    case UiStringId::kLatest:       return "LATEST";
    case UiStringId::kTopModels:    return "TOP MODELS";
    case UiStringId::kModelQuotas:  return "MODEL QUOTAS";
    default:                        return "";
  }
}

// Chinese column. Punctuation is avoided (spaces only) for the subset font.
inline const char* uiStrZh(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "用量监控屏";
    case UiStringId::kWinSession:   return "5 小时";
    case UiStringId::kWinWeekly:    return "1 周";
    case UiStringId::kWinSonnet:    return "Sonnet 7天";
    case UiStringId::kWinOpus:      return "Opus 7天";
    case UiStringId::kRemaining:    return "剩余";
    case UiStringId::kUsed:         return "已用";
    case UiStringId::kResets:       return "重置";
    case UiStringId::kPlan:         return "套餐";
    case UiStringId::kExtra:        return "额外消费";
    case UiStringId::kBalance:      return "余额";
    case UiStringId::kUpdated:      return "更新于";
    case UiStringId::kStale:        return "数据陈旧";
    case UiStringId::kRefreshNote:  return "5 分钟刷新";
    case UiStringId::kBootStarting: return "启动中";
    case UiStringId::kBootWifi:     return "连接 WiFi";
    case UiStringId::kBootSync:     return "同步时间";
    case UiStringId::kBootFetch:    return "获取用量";
    case UiStringId::kReloginTitle: return "登录已过期";
    case UiStringId::kReloginBody:  return "请在电脑上重新登录后重新写入凭证";
    case UiStringId::kNoWifi:       return "WiFi 不可用";
    case UiStringId::kNoData:       return "暂无数据";
    case UiStringId::kCloudSummary: return "云端汇总";
    case UiStringId::kLocalUsage:   return "本地用量";
    case UiStringId::kStatus:       return "状态";
    case UiStringId::kSessionLeft:  return "5小时剩余";
    case UiStringId::kWeeklyLeft:   return "7天剩余";
    case UiStringId::kQuotaReady:   return "额度正常";
    case UiStringId::kWaiting:      return "等待数据";
    case UiStringId::kFallback:     return "回退";
    case UiStringId::kCloudQuota:   return "云端额度";
    case UiStringId::kTodayTokens:  return "今日 Token";
    case UiStringId::kInOut:        return "输入 输出";
    case UiStringId::kSessions:     return "会话数";
    case UiStringId::kLatest:       return "最近活动";
    case UiStringId::kTopModels:    return "模型排行";
    case UiStringId::kModelQuotas:  return "模型额度";
    default:                        return "";
  }
}

// Active column for the current build.
#if UM_LANG_ZH
inline const char* uiStr(UiStringId id) { return uiStrZh(id); }
#else
inline const char* uiStr(UiStringId id) { return uiStrEn(id); }
#endif

// True when a stored language tag requires wiping persisted state: the tag is
// missing (-1) or differs from the firmware's language. Pure for testing.
// 当持久化的语言标记缺失(-1)或与固件语言不符时返回真(需清空)。纯函数,便于测试。
inline bool umShouldWipeForLanguage(int storedTag, int firmwareTag) {
  return storedTag != firmwareTag;
}

#endif  // USAGE_MONITOR_UI_LANG_H
