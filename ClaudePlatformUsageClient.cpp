#include "ClaudePlatformUsageClient.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <time.h>

#include "AppLog.h"

namespace usage_monitor {

// Format a UTC epoch as "YYYY-MM-DDTHH:MM:SSZ" into buf (must be >= 21 bytes).
static void fmtIso(long epoch, char* buf) {
  time_t t = (time_t)epoch;
  struct tm* tm = gmtime(&t);
  snprintf(buf, 21, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
}

bool ClaudePlatformUsageClient::fetch(long nowEpoch, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kClaudePlatform;

  if (!http_ || adminKey_.length() == 0) return false;

  char startBuf[21], endBuf[21];
  fmtIso(nowEpoch - 7L * 24 * 3600, startBuf);
  fmtIso(nowEpoch, endBuf);
  sysLog("[claudeplat/usage] fetch start %s..%s", startBuf, endBuf);

  String url = String("https://api.anthropic.com/v1/organizations/usage_report/messages"
                      "?starting_at=") + startBuf +
               "&ending_at=" + endBuf +
               "&group_by[]=model&bucket_width=1d";

  const HttpHeader extra[] = {
    { "x-api-key",           adminKey_.c_str() },
    { "anthropic-version",   "2023-06-01"       },
    { "Accept",              "application/json"  },
  };
  HttpResult r = http_->get(url, extra, 3, nullptr, 0, "UsageMonitor");
  if (r.status != 200) {
    sysLog("[claudeplat/usage] status %d", r.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, r.body)) {
    sysLog("[claudeplat/usage] json parse error");
    return false;
  }

  // Sum tokens across all buckets and models.
  long totalInput = 0, totalOutput = 0, totalCache = 0;
  JsonArray data = doc["data"];
  if (!data.isNull()) {
    for (JsonVariant bucket : data) {
      JsonVariant results = bucket["results"];
      if (results.isNull()) {
        // Flat structure: some API revisions put models directly in the bucket.
        totalInput  += bucket["input_tokens"]  | 0L;
        totalOutput += bucket["output_tokens"] | 0L;
        continue;
      }
      for (JsonVariant row : results.as<JsonArray>()) {
        totalInput  += row["input_tokens"]  | 0L;
        totalOutput += row["output_tokens"] | 0L;
        totalCache  += row["cache_read_input_tokens"] | 0L;
      }
    }
  }

  long totalTokens = totalInput + totalOutput + totalCache;
  out.hasBalance = true;
  out.balance    = static_cast<double>(totalTokens);
  out.hasPlan    = true;
  strncpy(out.planType, "Platform", sizeof(out.planType) - 1);
  out.ok              = true;
  out.lastSuccessEpoch = nowEpoch;
  sysLog("[claudeplat/usage] total=%ld (in=%ld out=%ld cache=%ld)",
         totalTokens, totalInput, totalOutput, totalCache);
  return true;
}

}  // namespace usage_monitor
