#include "UsageUI.h"

#include <time.h>

#include "QuotaMath.h"
#include "TimeFormat.h"
#include "UiLang.h"

namespace usage_monitor {

namespace {

// Palette per panel capability, so the same drawing code reuses each panel's
// headroom: E1001 gray4, E1002 six-color, E1003 gray16.
// 同一套绘制代码按面板能力选调色板。
#if UM_SCREEN_MODE == UM_SCREEN_GRAY4
constexpr uint16_t kBg     = TFT_GRAY_3;
constexpr uint16_t kCard   = TFT_GRAY_2;
constexpr uint16_t kText   = TFT_GRAY_0;
constexpr uint16_t kInv    = TFT_GRAY_3;
constexpr uint16_t kMuted  = TFT_GRAY_1;
constexpr uint16_t kLine   = TFT_GRAY_1;
constexpr uint16_t kTrack  = TFT_GRAY_3;
constexpr uint16_t kHealthy = TFT_GRAY_0;
constexpr uint16_t kWarn    = TFT_GRAY_1;
constexpr uint16_t kCrit    = TFT_GRAY_0;
#elif UM_SCREEN_MODE == UM_SCREEN_COLOR6
constexpr uint16_t kBg     = TFT_WHITE;
constexpr uint16_t kCard   = TFT_WHITE;
constexpr uint16_t kText   = TFT_BLACK;
constexpr uint16_t kInv    = TFT_WHITE;
constexpr uint16_t kMuted  = TFT_BLUE;
constexpr uint16_t kLine   = TFT_BLACK;
constexpr uint16_t kTrack  = TFT_WHITE;
constexpr uint16_t kHealthy = TFT_GREEN;
constexpr uint16_t kWarn    = TFT_YELLOW;
constexpr uint16_t kCrit    = TFT_RED;
#else  // UM_SCREEN_GRAY16
constexpr uint16_t kBg     = TFT_GRAY_15;
constexpr uint16_t kCard   = TFT_GRAY_13;
constexpr uint16_t kText   = TFT_GRAY_0;
constexpr uint16_t kInv    = TFT_GRAY_14;
constexpr uint16_t kMuted  = TFT_GRAY_7;
constexpr uint16_t kLine   = TFT_GRAY_9;
constexpr uint16_t kTrack  = TFT_GRAY_11;
constexpr uint16_t kHealthy = TFT_GRAY_2;
constexpr uint16_t kWarn    = TFT_GRAY_5;
constexpr uint16_t kCrit    = TFT_GRAY_0;
#endif

// Fill color for a progress bar by quota status (color panels go red/yellow/
// green; gray panels vary the ink weight).
uint16_t statusColor(QuotaStatus s) {
  switch (s) {
    case QuotaStatus::kCritical:
    case QuotaStatus::kDepleted: return kCrit;
    case QuotaStatus::kWarning:  return kWarn;
    default:                     return kHealthy;
  }
}

// True when this panel is the large E1003; small panels use a tighter layout.
constexpr bool kIsLarge = (UM_SCREEN_MODE == UM_SCREEN_GRAY16);

}  // namespace

void UsageUI::begin() {
  display_.begin();
#if UM_SCREEN_MODE == UM_SCREEN_GRAY4
  display_.initGrayMode(GRAY_LEVEL4);
#elif UM_SCREEN_MODE == UM_SCREEN_GRAY16
  display_.initGrayMode(GRAY_LEVEL16);
#endif
  renderer_.begin(display_);
}

uint16_t UsageUI::displayWidth()  { return static_cast<uint16_t>(display_.width()); }
uint16_t UsageUI::displayHeight() { return static_cast<uint16_t>(display_.height()); }

void UsageUI::drawProgressBar(int x, int y, int w, int h, double pct,
                              uint16_t fg, uint16_t track) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  display_.fillRoundRect(x, y, w, h, h / 2, track);
  const int fillW = static_cast<int>((w - 4) * pct / 100.0 + 0.5);
  if (fillW > 0) display_.fillRoundRect(x + 2, y + 2, fillW, h - 4, (h - 4) / 2, fg);
}

void UsageUI::drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color) {
  const int bars = 4, gap = 3;
  const int bw = (w - gap * (bars - 1)) / bars;
  for (int i = 0; i < bars; i++) {
    const int bh = h * (i + 1) / bars;
    const int bx = x + i * (bw + gap);
    const int by = y + h - bh;
    if (connected) display_.fillRect(bx, by, bw, bh, color);
    else           display_.drawRect(bx, by, bw, bh, color);
  }
  if (!connected) display_.drawLine(x, y, x + w, y + h, color);
}

void UsageUI::drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color) {
  display_.drawRect(x, y, w, h, color);
  const int nubW = 3, nubH = h / 3;
  display_.fillRect(x + w, y + (h - nubH) / 2, nubW, nubH, color);
  if (percent < 0) return;
  const int fillW = (w - 4) * percent / 100;
  if (fillW > 0) display_.fillRect(x + 2, y + 2, fillW, h - 4, color);
}

void UsageUI::drawWrapped(const String& text, int x, int y, int maxW, int lineH,
                          int textSize, uint16_t color, int maxLines) {
  auto utf8Len = [&](size_t i) -> size_t {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
  };
  String line;
  int lines = 0;
  for (size_t i = 0; i < static_cast<size_t>(text.length()) && lines < maxLines; ) {
    const size_t n = utf8Len(i);
    const String token = text.substring(i, i + n);
    i += n;
    if (token == "\r" || token == "\n") continue;
    const String candidate = line + token;
    if (line.length() > 0 && renderer_.measureText(candidate, textSize) > maxW) {
      renderer_.drawText(line, x, y + lines * lineH, textSize,
                         TextAlign::TopLeft, color, kBg);
      line = token;
      lines++;
    } else {
      line = candidate;
    }
  }
  if (lines < maxLines && line.length() > 0) {
    renderer_.drawText(line, x, y + lines * lineH, textSize,
                       TextAlign::TopLeft, color, kBg);
  }
}

void UsageUI::drawHeader(const UiStatus& st, long nowEpoch) {
  const int w = display_.width();
  const int margin = kIsLarge ? 70 : 24;
  const int topY = kIsLarge ? 24 : 12;
  const int titleSize = kIsLarge ? 5 : 3;

  // Left: app title.
  renderer_.drawText(uiStr(UiStringId::kAppName), margin, topY, titleSize,
                     TextAlign::TopLeft, kText, kBg);

  // Center: wall clock + date from the NTP-synced system time (local TZ).
  char clockBuf[16] = "--:--";
  char dateBuf[24] = "";
  if (nowEpoch > 0) {
    time_t t = static_cast<time_t>(nowEpoch);
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    snprintf(dateBuf, sizeof(dateBuf), "%04d/%02d/%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  }
  const int clockSize = kIsLarge ? 5 : 3;
  renderer_.drawText(clockBuf, w / 2, topY, clockSize, TextAlign::TopCenter, kText, kBg);
  if (kIsLarge) {
    renderer_.drawText(dateBuf, w / 2, topY + clockSize * 8 + 12, 3,
                       TextAlign::TopCenter, kMuted, kBg);
  }

  // Right: refresh note + WiFi/battery icons.
  const int wifiW = kIsLarge ? 44 : 30;
  const int wifiH = kIsLarge ? 32 : 22;
  drawWifiIcon(w - margin - wifiW, topY, wifiW, wifiH, st.wifiConnected, kText);
  if (kIsLarge) {
    renderer_.drawText(uiStr(UiStringId::kRefreshNote), w - margin - wifiW - 16, topY + 4,
                       2, TextAlign::TopRight, kMuted, kBg);
    if (st.batteryPercent >= 0) {
      drawBatteryIcon(w - margin - wifiW, topY + wifiH + 14, 56, 26,
                      st.batteryPercent, kText);
    }
  }
}

void UsageUI::drawWindowCard(int x, int y, int w, int h, const char* label,
                             const WindowQuota& win, long nowEpoch, bool emphasize) {
  display_.fillRoundRect(x, y, w, h, kIsLarge ? 12 : 8, kCard);
  const int pad = kIsLarge ? 18 : 10;

  // Top row: window label (left) + "left" (right).
  const int labelSize = kIsLarge ? 3 : 2;
  renderer_.drawText(label, x + pad, y + pad, labelSize,
                     TextAlign::TopLeft, kMuted, kCard);
  renderer_.drawText(uiStr(UiStringId::kRemaining), x + w - pad, y + pad, labelSize,
                     TextAlign::TopRight, kMuted, kCard);

  if (!win.present) {
    renderer_.drawText("--", x + pad, y + h / 2 - 8, emphasize ? 6 : 5,
                       TextAlign::MiddleLeft, kMuted, kCard);
    return;
  }

  // Big remaining percentage.
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%",
           static_cast<int>(win.remainingPercent() + 0.5));
  const int bigSize = emphasize ? (kIsLarge ? 9 : 5) : (kIsLarge ? 7 : 4);
  const int bigY = y + (kIsLarge ? pad + 30 : pad + 18);
  renderer_.drawText(pctBuf, x + pad, bigY, bigSize, TextAlign::TopLeft, kText, kCard);

  // Used% bar near the bottom.
  const int barH = kIsLarge ? 18 : 10;
  const int barY = y + h - pad - barH - (kIsLarge ? 30 : 18);
  drawProgressBar(x + pad, barY, w - pad * 2, barH, win.usedPercent,
                  statusColor(win.status), kTrack);

  // Bottom row: used% (left) + reset countdown (right).
  const int smallSize = kIsLarge ? 2 : 2;
  char usedBuf[24];
  snprintf(usedBuf, sizeof(usedBuf), "%s %d%%", uiStr(UiStringId::kUsed),
           static_cast<int>(win.usedPercent + 0.5));
  renderer_.drawText(usedBuf, x + pad, y + h - pad - 14, smallSize,
                     TextAlign::TopLeft, kMuted, kCard);

  if (win.resetEpoch > 0 && nowEpoch > 0) {
    char cd[16];
    umFormatCountdown(win.resetEpoch - nowEpoch, cd, sizeof(cd));
    char resetBuf[32];
    snprintf(resetBuf, sizeof(resetBuf), "%s %s", uiStr(UiStringId::kResets), cd);
    renderer_.drawText(resetBuf, x + w - pad, y + h - pad - 14, smallSize,
                       TextAlign::TopRight, kMuted, kCard);
  }
}

void UsageUI::drawModelRow(int x, int y, int w, const char* name,
                           const WindowQuota& win) {
  if (!win.present) return;
  const int nameW = kIsLarge ? 200 : 120;
  renderer_.drawText(name, x, y, 2, TextAlign::TopLeft, kText, kBg);
  const int barX = x + nameW;
  const int barW = w - nameW - 70;
  const int barH = kIsLarge ? 14 : 10;
  if (barW > 20) {
    drawProgressBar(barX, y, barW, barH, win.usedPercent, statusColor(win.status), kTrack);
  }
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(win.usedPercent + 0.5));
  renderer_.drawText(pctBuf, x + w, y, 2, TextAlign::TopRight, kMuted, kBg);
}

void UsageUI::drawReloginColumn(int x, int y, int w, int h, const char* name) {
  renderer_.drawText(name, x, y, kIsLarge ? 4 : 3, TextAlign::TopLeft, kText, kBg);
  renderer_.drawText(uiStr(UiStringId::kReloginTitle), x, y + (kIsLarge ? 70 : 40),
                     kIsLarge ? 4 : 3, TextAlign::TopLeft, kCrit, kBg);
  drawWrapped(uiStr(UiStringId::kReloginBody), x, y + (kIsLarge ? 130 : 80),
              w, kIsLarge ? 40 : 30, kIsLarge ? 3 : 2, kText, 4);
}

void UsageUI::drawProviderColumn(int x, int y, int w, int h, const char* name,
                                 const ProviderQuota& p, long nowEpoch) {
  if (p.needsRelogin) {
    drawReloginColumn(x, y, w, h, name);
    return;
  }

  // Column header: provider name (left) + plan + LIVE/stale (right).
  const int nameSize = kIsLarge ? 4 : 3;
  renderer_.drawText(name, x, y, nameSize, TextAlign::TopLeft, kText, kBg);

  int rightY = y + 4;
  if (p.hasPlan && p.planType[0]) {
    renderer_.drawText(p.planType, x + w, rightY, kIsLarge ? 3 : 2,
                       TextAlign::TopRight, kMuted, kBg);
  }
  const bool stale = p.isStale(nowEpoch, 900);
  if (kIsLarge) {
    renderer_.drawText(stale ? uiStr(UiStringId::kStale) : "LIVE",
                       x + w, rightY + 34, 2, TextAlign::TopRight,
                       stale ? kCrit : kMuted, kBg);
  }

  const int headerH = kIsLarge ? 60 : 40;
  int cy = y + headerH;

  // Two window cards: session emphasized, weekly secondary. Large panels place
  // them side by side; small panels stack them.
  if (kIsLarge) {
    const int gap = 16;
    const int cardW = (w - gap) / 2;
    const int cardH = 220;
    drawWindowCard(x, cy, cardW, cardH, uiStr(UiStringId::kWinSession),
                   p.session, nowEpoch, /*emphasize=*/true);
    drawWindowCard(x + cardW + gap, cy, cardW, cardH, uiStr(UiStringId::kWinWeekly),
                   p.weekly, nowEpoch, /*emphasize=*/false);
    cy += cardH + 24;
  } else {
    const int cardH = (h - headerH - 20) / 2;
    drawWindowCard(x, cy, w, cardH, uiStr(UiStringId::kWinSession),
                   p.session, nowEpoch, /*emphasize=*/true);
    cy += cardH + 10;
    drawWindowCard(x, cy, w, cardH, uiStr(UiStringId::kWinWeekly),
                   p.weekly, nowEpoch, /*emphasize=*/false);
    return;   // small panels have no room for per-model rows / extras
  }

  // Lower block: Claude per-model 7-day rows, else Codex balance.
  if (p.weeklySonnet.present || p.weeklyOpus.present) {
    drawModelRow(x, cy, w, uiStr(UiStringId::kWinOpus), p.weeklyOpus);
    cy += 34;
    drawModelRow(x, cy, w, uiStr(UiStringId::kWinSonnet), p.weeklySonnet);
    cy += 34;
  }
  if (p.hasBalance) {
    char balBuf[32];
    snprintf(balBuf, sizeof(balBuf), "%s %.2f", uiStr(UiStringId::kBalance), p.balance);
    renderer_.drawText(balBuf, x, cy, 3, TextAlign::TopLeft, kText, kBg);
    cy += 40;
  }
  if (p.extraEnabled) {
    char exBuf[40];
    snprintf(exBuf, sizeof(exBuf), "%s %.2f / %.2f", uiStr(UiStringId::kExtra),
             p.extraUsedCents / 100.0, p.extraLimitCents / 100.0);
    renderer_.drawText(exBuf, x, cy, 3, TextAlign::TopLeft, kText, kBg);
  }
}

void UsageUI::drawBoot(const String& statusText, const UiStatus& st, long nowEpoch) {
  display_.fillSprite(kBg);
  drawHeader(st, nowEpoch);
  renderer_.drawText(statusText, display_.width() / 2, display_.height() / 2,
                     kIsLarge ? 4 : 3, TextAlign::MiddleCenter, kText, kBg);
  display_.update();
}

void UsageUI::drawDashboard(const UsageSnapshot& snap, const UiStatus& st, long nowEpoch) {
  const int w = display_.width();
  const int h = display_.height();
  const int margin = kIsLarge ? 80 : 20;

  display_.fillSprite(kBg);
  drawHeader(st, nowEpoch);

  const int headerH = kIsLarge ? 170 : 70;
  const int footerH = kIsLarge ? 60 : 0;
  const int colTop = headerH;
  const int colH = h - headerH - footerH;
  const int colGap = kIsLarge ? 40 : 16;
  const int colW = (w - margin * 2 - colGap) / 2;

  // Vertical divider between the two columns (large panels only).
  if (kIsLarge) {
    const int dividerX = margin + colW + colGap / 2;
    display_.fillRect(dividerX, colTop, 1, colH, kLine);
  }

  // Left = Codex, right = Claude (matches the reference layout).
  drawProviderColumn(margin, colTop, colW, colH, "Codex", snap.codex, nowEpoch);
  drawProviderColumn(margin + colW + colGap, colTop, colW, colH, "Claude",
                     snap.claude, nowEpoch);

  // Footer: "updated HH:MM - 5 min refresh".
  if (kIsLarge && nowEpoch > 0) {
    time_t t = static_cast<time_t>(nowEpoch);
    struct tm lt;
    localtime_r(&t, &lt);
    char foot[48];
    snprintf(foot, sizeof(foot), "%s %02d:%02d  -  %s", uiStr(UiStringId::kUpdated),
             lt.tm_hour, lt.tm_min, uiStr(UiStringId::kRefreshNote));
    renderer_.drawText(foot, w / 2, h - 36, 2, TextAlign::TopCenter, kMuted, kBg);
  }

  display_.update();
}

}  // namespace usage_monitor
