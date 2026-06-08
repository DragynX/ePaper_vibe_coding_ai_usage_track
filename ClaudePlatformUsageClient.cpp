#include "ClaudePlatformUsageClient.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Find or create the per-model accumulator slot in out.platModels.
static ProviderQuota::PlatModel* platSlot(ProviderQuota& out, const char* model) {
  if (!model || !model[0]) model = "unknown";
  for (uint8_t i = 0; i < out.platCount; ++i)
    if (strncmp(out.platModels[i].name, model, sizeof(out.platModels[i].name) - 1) == 0)
      return &out.platModels[i];
  if (out.platCount >= 5) return nullptr;   // cap; small models drop off
  ProviderQuota::PlatModel* m = &out.platModels[out.platCount++];
  strncpy(m->name, model, sizeof(m->name) - 1);
  return m;
}

bool ClaudePlatformUsageClient::fetch(long nowEpoch, ProviderQuota& out) {
  out = ProviderQuota();
  out.id = ProviderId::kClaudePlatform;
  out.hasPlan = true;
  strncpy(out.planType, "Platform", sizeof(out.planType) - 1);

  if (!http_ || adminKey_.length() == 0) return false;

  // The usage/cost endpoints accept ONLY an Admin key (sk-ant-admin...). A
  // standard sk-ant-api key returns a misleading 401 "invalid x-api-key"; catch
  // it here so the UI can say exactly what's wrong.
  if (!adminKey_.startsWith("sk-ant-admin")) {
    out.needAdminKey = true;
    sysLog("[claudeplat] key is not an admin key (sk-ant-admin) — cannot use usage API");
    return false;
  }

  // Prepaid mode always reports the 30-day cost (remaining = prepaid - 30d cost);
  // spend mode reports the user-chosen 7/14/30-day window.
  const int win = spendMode_ ? windowDays_ : 30;
  const String limitStr = String("&limit=") + String(win + 1);   // <=win daily buckets

  char startBuf[21], endBuf[21];
  fmtIso(nowEpoch - (long)win * 24 * 3600, startBuf);
  fmtIso(nowEpoch, endBuf);
  sysLog("[claudeplat] fetch %dd %s..%s", win, startBuf, endBuf);

  const HttpHeader extra[] = {
    { "x-api-key",         adminKey_.c_str() },
    { "anthropic-version", "2023-06-01"       },
    { "Accept",            "application/json"  },
  };

  // --- Usage report: tokens per model -------------------------------------
  String usageUrl = String("https://api.anthropic.com/v1/organizations/usage_report/messages"
                           "?starting_at=") + startBuf + "&ending_at=" + endBuf +
                    "&group_by[]=model&bucket_width=1d" + limitStr;   // 1d defaults to
  // only 7 buckets; a wider range without limit returns the oldest 7 (empty).
  HttpResult ur = http_->get(usageUrl, extra, 3, nullptr, 0, "UsageMonitor/1.4");
  if (ur.status != 200) {
    sysLog("[claudeplat/usage] status %d", ur.status);
    return false;
  }
  {
    JsonDocument doc;
    if (deserializeJson(doc, ur.body)) {
      sysLog("[claudeplat/usage] json parse error");
      return false;
    }
    double totalTokens = 0;
    JsonArray data = doc["data"];
    if (!data.isNull()) {
      for (JsonVariant bucket : data) {
        JsonVariant results = bucket["results"];
        JsonArray rows = results.isNull() ? JsonArray() : results.as<JsonArray>();
        if (results.isNull()) {
          // flat layout: model fields directly on the bucket
          const double t = (double)(bucket["input_tokens"] | 0L)
                         + (double)(bucket["output_tokens"] | 0L)
                         + (double)(bucket["cache_read_input_tokens"] | 0L);
          totalTokens += t;
          ProviderQuota::PlatModel* m = platSlot(out, bucket["model"] | "unknown");
          if (m) m->tokens += t;
          continue;
        }
        for (JsonVariant row : rows) {
          const double t = (double)(row["input_tokens"] | 0L)
                         + (double)(row["output_tokens"] | 0L)
                         + (double)(row["cache_read_input_tokens"] | 0L);
          totalTokens += t;
          ProviderQuota::PlatModel* m = platSlot(out, row["model"] | "unknown");
          if (m) m->tokens += t;
        }
      }
    }
    out.hasBalance = true;
    out.balance    = totalTokens;
  }

  // --- Cost report: USD cents per model -----------------------------------
  String costUrl = String("https://api.anthropic.com/v1/organizations/cost_report"
                          "?starting_at=") + startBuf + "&ending_at=" + endBuf +
                   "&group_by[]=description&bucket_width=1d" + limitStr;   // all win days
  HttpResult cr = http_->get(costUrl, extra, 3, nullptr, 0, "UsageMonitor/1.4");
  if (cr.status == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, cr.body)) {
      double totalCents = 0;
      JsonArray data = doc["data"];
      if (!data.isNull()) {
        for (JsonVariant bucket : data) {
          JsonVariant results = bucket["results"];
          JsonArray rows = results.isNull() ? JsonArray() : results.as<JsonArray>();
          for (JsonVariant row : rows) {
            // amount is a decimal string of USD cents
            const double cents = atof(row["amount"] | "0");
            totalCents += cents;
            ProviderQuota::PlatModel* m = platSlot(out, row["model"] | "unknown");
            if (m) m->cents += cents;
          }
        }
      }
      out.hasCost = true;
      out.costCents = totalCents;
    } else {
      sysLog("[claudeplat/cost] json parse error");
    }
  } else {
    sysLog("[claudeplat/cost] status %d (tokens still shown)", cr.status);
  }

  // --- Prepaid mode: $ remaining = prepaid - 30-day cost ------------------
  // (win is forced to 30 in prepaid mode, so out.costCents is the 30-day cost.)
  if (!spendMode_ && prepaidCents_ > 0 && out.hasCost) {
    out.prepaidCents = prepaidCents_;
    out.leftCents    = prepaidCents_ - out.costCents;   // can go negative
    out.hasLeft      = true;
  }

  // Sort top models by cost (simple insertion sort on the small array).
  for (uint8_t i = 1; i < out.platCount; ++i) {
    ProviderQuota::PlatModel key = out.platModels[i];
    int j = i - 1;
    while (j >= 0 && out.platModels[j].cents < key.cents) {
      out.platModels[j + 1] = out.platModels[j]; --j;
    }
    out.platModels[j + 1] = key;
  }

  out.platWindowDays = win;
  out.platSpendMode  = spendMode_;
  out.ok = true;
  out.lastSuccessEpoch = nowEpoch;
  sysLog("[claudeplat] ok %dd tokens=%.0f cost=%.0f cents left=%.0f models=%d",
         win, out.balance, out.costCents, out.hasLeft ? out.leftCents : 0.0,
         (int)out.platCount);
  return true;
}

}  // namespace usage_monitor
