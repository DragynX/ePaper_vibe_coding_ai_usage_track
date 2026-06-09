#include "UsageUI.h"

#include <string.h>
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
// Runtime palette (light by default). Dark mode inverts each gray level
// (new = GRAY_3 - old) via UsageUI::setDarkMode(). 0=black .. 3=white.
uint16_t kBg     = TFT_GRAY_3;
uint16_t kCard   = TFT_GRAY_2;
uint16_t kText   = TFT_GRAY_0;
uint16_t kInv    = TFT_GRAY_3;
uint16_t kMuted  = TFT_GRAY_1;
uint16_t kLine   = TFT_GRAY_1;
uint16_t kTrack  = TFT_GRAY_3;
uint16_t kHealthy = TFT_GRAY_0;
uint16_t kWarn    = TFT_GRAY_1;
uint16_t kCrit    = TFT_GRAY_0;
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
constexpr uint16_t kMuted  = TFT_GRAY_0;
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

// True when this panel is the large 800px panel (E1001 GRAY4 or E1003 GRAY16).
// E1002 COLOR6 uses the compact stacked layout.
constexpr bool kIsLarge = (UM_SCREEN_MODE == UM_SCREEN_GRAY16);

String fmtPercent(double pct) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(pct + 0.5));
  return String(buf);
}

String fmtTokens(uint32_t tokens) {
  char buf[18];
  if (tokens >= 1000000UL) {
    snprintf(buf, sizeof(buf), "%.1fM", tokens / 1000000.0);
  } else if (tokens >= 1000UL) {
    snprintf(buf, sizeof(buf), "%.1fK", tokens / 1000.0);
  } else {
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(tokens));
  }
  return String(buf);
}

String fmtClock(long epoch) {
  if (epoch <= 0) return "--:--";
  time_t t = static_cast<time_t>(epoch);
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", lt.tm_mon + 1, lt.tm_mday,
           lt.tm_hour, lt.tm_min);
  return String(buf);
}

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

void UsageUI::setDarkMode(bool dark) {
#if UM_SCREEN_MODE == UM_SCREEN_GRAY4
  // Invert the grayscale ramp for dark mode (new = GRAY_3 - old).
  if (dark) {
    kBg = TFT_GRAY_0; kCard = TFT_GRAY_1; kText = TFT_GRAY_3; kInv = TFT_GRAY_0;
    kMuted = TFT_GRAY_2; kLine = TFT_GRAY_2; kTrack = TFT_GRAY_0;
    kHealthy = TFT_GRAY_3; kWarn = TFT_GRAY_2; kCrit = TFT_GRAY_3;
  } else {
    kBg = TFT_GRAY_3; kCard = TFT_GRAY_2; kText = TFT_GRAY_0; kInv = TFT_GRAY_3;
    kMuted = TFT_GRAY_1; kLine = TFT_GRAY_1; kTrack = TFT_GRAY_3;
    kHealthy = TFT_GRAY_0; kWarn = TFT_GRAY_1; kCrit = TFT_GRAY_0;
  }
#else
  (void)dark;   // single fixed palette on color / gray16 panels
#endif
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

void UsageUI::drawBox(int x, int y, int w, int h, uint16_t fill) {
  display_.fillRect(x, y, w, h, fill);
  display_.drawRect(x, y, w, h, kLine);
}

void UsageUI::drawStatusBadge(int x, int y, const String& text, bool alert) {
  const int padX = 12;
  const int badgeW = renderer_.measureTextFace(text, TextFace::SansBold9) + padX * 2;
  const int badgeH = 28;
  const uint16_t fill = alert ? kTrack : kCard;
  display_.fillRect(x - badgeW, y, badgeW, badgeH, fill);
  display_.drawRect(x - badgeW, y, badgeW, badgeH, kLine);
  renderer_.drawTextFace(text, x - padX, y + 4, TextFace::SansBold9,
                         TextAlign::TopRight, kText, fill);
}

void UsageUI::drawInfoRow(int x, int y, int w, const char* label, const String& value,
                          int textSize, uint16_t bg) {
  if (kIsLarge) {
    renderer_.drawTextFace(label, x, y + 3, TextFace::SansBold9,
                           TextAlign::TopLeft, kText, bg);
    renderer_.drawTextFace(value, x + w, y, TextFace::SansBold12,
                           TextAlign::TopRight, kText, bg);
    return;
  }
  renderer_.drawTextFace(label, x, y, TextFace::Sans9, TextAlign::TopLeft, kText, bg);
  renderer_.drawTextFace(value, x + w, y, TextFace::SansBold9, TextAlign::TopRight, kText, bg);
}

void UsageUI::drawQuotaDetail(int x, int y, int w, int usedX, int leftX, int resetX,
                              const char* label, const WindowQuota& win, long nowEpoch) {
  if (kIsLarge) {
    renderer_.drawTextFace(label, x, y, TextFace::SansBold9,
                           TextAlign::TopLeft, kText, kBg);
  } else {
    renderer_.drawTextFace(label, x, y, TextFace::SansBold9, TextAlign::TopLeft, kText, kBg);
  }
  if (!win.present) {
    renderer_.drawTextFace("--", x + w, y, TextFace::Sans9, TextAlign::TopRight, kText, kBg);
    return;
  }

  char usedBuf[16];
  snprintf(usedBuf, sizeof(usedBuf), "%d%%", static_cast<int>(win.usedPercent + 0.5));
  if (kIsLarge) {
    renderer_.drawTextFace(usedBuf, usedX, y, TextFace::Sans9,
                           TextAlign::TopCenter, kText, kBg);
    renderer_.drawTextFace(fmtPercent(win.remainingPercent()), leftX, y,
                           TextFace::SansBold9, TextAlign::TopCenter, kText, kBg);
  } else {
    renderer_.drawTextFace(usedBuf, x + w / 3, y, TextFace::Sans9, TextAlign::TopRight, kText, kBg);
    renderer_.drawTextFace(fmtPercent(win.remainingPercent()), x + w * 2 / 3, y,
                           TextFace::Sans9, TextAlign::TopRight, kText, kBg);
  }
  const String resetText = win.resetEpoch > 0 ? fmtClock(win.resetEpoch) : String("--");
  if (kIsLarge) {
    renderer_.drawTextFace(resetText, resetX, y, TextFace::Sans9,
                           TextAlign::TopRight, kText, kBg);
  } else {
    renderer_.drawTextFace(resetText, x + w, y, TextFace::Sans9, TextAlign::TopRight, kText, kBg);
  }

  const int barY = y + 24;
  drawProgressBar(x, barY, w, kIsLarge ? 10 : 8, win.usedPercent,
                  statusColor(win.status), kTrack);
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

void UsageUI::drawWrappedFace(const String& text, int x, int y, int maxW, int lineH,
                              TextFace face, uint16_t color, int maxLines) {
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
    if (line.length() > 0 && renderer_.measureTextFace(candidate, face) > maxW) {
      renderer_.drawTextFace(line, x, y + lines * lineH, face,
                             TextAlign::TopLeft, color, kBg);
      line = token;
      lines++;
    } else {
      line = candidate;
    }
  }
  if (lines < maxLines && line.length() > 0) {
    renderer_.drawTextFace(line, x, y + lines * lineH, face,
                           TextAlign::TopLeft, color, kBg);
  }
}

void UsageUI::drawHeader(const UiStatus& st, long nowEpoch) {
  const int w = display_.width();
  const int margin = kIsLarge ? 44 : 24;
  const int topY = kIsLarge ? 14 : 12;
  const int headerLineY = kIsLarge ? topY + 94 : 0;
  const int headerMiddleY = kIsLarge ? topY + 44 : topY;
  const int titleSize = kIsLarge ? 4 : 3;

  // Left: app title + version (version drawn ~2x smaller than the app name).
  const String appName = uiStr(UiStringId::kAppName);
  if (kIsLarge) {
    const String title = appName + " " + UM_VERSION;
    renderer_.drawTextFace(title, margin, headerMiddleY,
                           TextFace::SansBold24, TextAlign::MiddleLeft, kText, kBg);
  } else {
    // App name inside a pill; version (smaller, raster) bottom-aligned past the pill.
    const int nameW = renderer_.measureTextFace(appName, TextFace::SansBold9);
    const int padX = 6, padY = 2, nameH = 13;
    const int pillX = margin - padX, pillY = topY - padY;
    const int pillW = nameW + 2 * padX, pillH = nameH + 2 * padY;
    display_.drawRoundRect(pillX, pillY, pillW, pillH, pillH / 2, kText);
    renderer_.drawTextFace(appName, margin, topY,
                           TextFace::SansBold9, TextAlign::TopLeft, kText, kBg);
    renderer_.drawTextFace(UM_VERSION, pillX + pillW + 8, topY + nameH,
                           TextFace::Sans7, TextAlign::BottomLeft, kText, kBg);
  }

  // Center: Last Fetch / Next Fetch times (local TZ) instead of a wall clock.
  auto hhmm = [](long epoch, char* out) {
    if (epoch <= 0) { strcpy(out, "--:--"); return; }
    time_t t = static_cast<time_t>(epoch);
    struct tm lt; localtime_r(&t, &lt);
    snprintf(out, 8, "%02d:%02d", lt.tm_hour, lt.tm_min);
  };
  char lastBuf[8], nextBuf[8];
  hhmm(st.lastFetchEpoch, lastBuf);
  hhmm(st.nextFetchEpoch, nextBuf);
  if (kIsLarge) {
    char fetchBuf[48];
    snprintf(fetchBuf, sizeof(fetchBuf), "Fetch:  Last %s Next %s", lastBuf, nextBuf);
    renderer_.drawTextFace(fetchBuf, w / 2, topY + 8, TextFace::SansBold18,
                           TextAlign::TopCenter, kText, kBg);
  } else {
    // Segmented so "Last"/"Next" render one size smaller (raster) than the times.
    const TextFace big = TextFace::SansBold9;
    const String s0 = "Fetch:  ", s1 = "Last ", s2 = String(lastBuf) + " ",
                 s3 = "Next ",    s4 = nextBuf;
    const TextFace small = TextFace::Sans7;   // one size smaller, smooth (OFR)
    const int w0 = renderer_.measureTextFace(s0, big);
    const int w1 = renderer_.measureTextFace(s1, small);
    const int w2 = renderer_.measureTextFace(s2, big);
    const int w3 = renderer_.measureTextFace(s3, small);
    const int w4 = renderer_.measureTextFace(s4, big);
    int sx = w / 2 - (w0 + w1 + w2 + w3 + w4) / 2;
    const int by = topY + 13;   // common bottom baseline for mixed sizes
    renderer_.drawTextFace(s0, sx, by, big,   TextAlign::BottomLeft, kText, kBg); sx += w0;
    renderer_.drawTextFace(s1, sx, by, small, TextAlign::BottomLeft, kText, kBg); sx += w1;
    renderer_.drawTextFace(s2, sx, by, big,   TextAlign::BottomLeft, kText, kBg); sx += w2;
    renderer_.drawTextFace(s3, sx, by, small, TextAlign::BottomLeft, kText, kBg); sx += w3;
    renderer_.drawTextFace(s4, sx, by, big,   TextAlign::BottomLeft, kText, kBg);
  }

  // Right: refresh note + WiFi/battery icons.
  const int wifiW = kIsLarge ? 42 : 30;
  const int wifiH = kIsLarge ? 30 : 22;
  if (kIsLarge) {
    const int noteW = renderer_.measureTextFace(uiStr(UiStringId::kRefreshNote),
                                                TextFace::SansBold9);
    const int groupGap = 18;
    const int groupW = noteW + groupGap + wifiW;
    const int groupX = w - margin - groupW;
    renderer_.drawTextFace(uiStr(UiStringId::kRefreshNote), groupX, headerMiddleY,
                           TextFace::SansBold9, TextAlign::MiddleLeft, kText, kBg);
    drawWifiIcon(groupX + noteW + groupGap, headerMiddleY - wifiH / 2,
                 wifiW, wifiH, st.wifiConnected, kText);
    if (st.batteryPercent >= 0) {
      drawBatteryIcon(w - margin - 56, headerLineY - 34, 56, 26,
                      st.batteryPercent, kText);
    }
    display_.fillRect(margin, headerLineY, w - margin * 2, 2, kLine);
  } else {
    const int wifiX = w - margin - wifiW;
    drawWifiIcon(wifiX, topY, wifiW, wifiH, st.wifiConnected, kText);
    // Moon (deep sleep) sits between the battery and the wifi bars; battery + IP
    // shift left by its slot to make room.
    int moonSlot = 0;
    if (st.deepSleepOn) {
      const int moonR = 6, moonGap = 8;
      const int mcx = wifiX - moonGap - moonR;
      const int mcy = topY + wifiH / 2;
      display_.fillCircle(mcx, mcy, moonR, kText);
      display_.fillCircle(mcx + 3, mcy - 2, moonR, kBg);
      moonSlot = moonGap + 2 * moonR;
    }
    int leftEdge = wifiX - moonSlot;
    // Battery beside the wifi bars; fill is proportional to charge.
    if (st.batteryPercent >= 0) {
      const int battW = 26, battH = 13;
      const int battX = leftEdge - battW - 10;
      const int battY = topY + (wifiH - battH) / 2 + 4;
      drawBatteryIcon(battX, battY, battW, battH, st.batteryPercent, kText);
      // Below the icon (right-aligned): runtime estimate only when discharging;
      // "Charging" when on USB/external power; nothing while still calibrating.
      const char* battNote = nullptr;
      char dbuf[24];
      if (st.batteryDays >= 0.0f) {
        const int totalHrs = (int)(st.batteryDays * 24.0f + 0.5f);
        snprintf(dbuf, sizeof(dbuf), "Est. %d days %d hrs", totalHrs / 24, totalHrs % 24);
        battNote = dbuf;
      } else if (st.batteryCharging) {
        battNote = "Charging";
      }
      if (battNote) {
        // Flush to the battery icon's right edge.
        renderer_.drawTextFace(battNote, battX + battW, battY + battH + 3,
                               TextFace::Sans9, TextAlign::TopRight, kMuted, kBg);
      }
      leftEdge = battX - 3;   // 3px nub drawn past battW
    }
    if (st.ipAddress.length() > 0 && st.ipAddress != "0.0.0.0") {
      const int ipX = leftEdge - 4;
      const int ipY = topY + (wifiH - 7) / 2 + 6;   // 2px higher than before
      // kText => black in light mode, white in dark mode (was muted gray).
      renderer_.drawText(st.ipAddress, ipX, ipY, 1, TextAlign::TopRight, kText, kBg);
    }
  }
}

void UsageUI::drawWindowCard(int x, int y, int w, int h, const char* label,
                             const WindowQuota& win, long nowEpoch, bool emphasize) {
  display_.fillRoundRect(x, y, w, h, kIsLarge ? 4 : 8, kCard);
  display_.drawRect(x, y, w, h, kLine);
  const int pad = kIsLarge ? 12 : 10;

  // Top row: window label (left).
  renderer_.drawTextFace(label, x + pad, y + pad, TextFace::SansBold9,
                         TextAlign::TopLeft, kText, kCard);

  if (!win.present) {
    renderer_.drawTextFace("--", x + pad, y + h / 2 - 10,
                           emphasize ? TextFace::SansBold48 : TextFace::SansBold36,
                           TextAlign::TopLeft, kMuted, kCard);
    return;
  }

  // Big used percentage (2x size; FreeFonts on all panel types).
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%",
           static_cast<int>(win.usedPercent + 0.5));
  const int bigY = y + pad + 18;
  renderer_.drawTextFace(pctBuf, x + pad, bigY,
                         emphasize ? TextFace::SansBold48 : TextFace::SansBold36,
                         TextAlign::TopLeft, kText, kCard);

  // Used% bar near the bottom; it thickens as usage grows (every full 10%
  // used adds 8% of the base height), anchored at a fixed bottom edge.
  const int baseH = kIsLarge ? 8 : 10;
  const int steps = static_cast<int>(win.usedPercent / 10.0);   // 0..10
  const int barH  = baseH + (baseH * 8 * steps) / 100;
  const int slotBottom = y + h - pad - (kIsLarge ? 20 : 18);
  const int barY = slotBottom - barH;
  drawProgressBar(x + pad, barY, w - pad * 2, barH, win.usedPercent,
                  statusColor(win.status), kTrack);

  // Bottom row: remaining% (left) + reset countdown (right).
  char remBuf[24];
  snprintf(remBuf, sizeof(remBuf), "%s %d%%", uiStr(UiStringId::kRemaining),
           static_cast<int>(win.remainingPercent() + 0.5));
  const int bottomRowY = y + h - pad - (kIsLarge ? 12 : 14);
  renderer_.drawTextFace(remBuf, x + pad, bottomRowY, TextFace::Sans9,
                         TextAlign::TopLeft, kText, kCard);

  if (win.resetEpoch > 0 && nowEpoch > 0) {
    char cd[16];
    umFormatCountdown(win.resetEpoch - nowEpoch, cd, sizeof(cd));
    char resetBuf[32];
    snprintf(resetBuf, sizeof(resetBuf), "%s %s", uiStr(UiStringId::kResets), cd);
    renderer_.drawTextFace(resetBuf, x + w - pad, bottomRowY, TextFace::SansBold9,
                           TextAlign::TopRight, kText, kCard);
  }
}

void UsageUI::drawModelRow(int x, int y, int w, const char* name,
                           const WindowQuota& win) {
  if (!win.present) return;
  const int nameW = kIsLarge ? 120 : 100;
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
  if (kIsLarge) {
    renderer_.drawText(name, x, y, 4, TextAlign::TopLeft, kText, kBg);
    renderer_.drawText(uiStr(UiStringId::kReloginTitle), x, y + 70, 4,
                       TextAlign::TopLeft, kCrit, kBg);
    drawWrapped(uiStr(UiStringId::kReloginBody), x, y + 130, w, 40, 3, kText, 4);
  } else {
    renderer_.drawTextFace(name, x, y, TextFace::SansBold12, TextAlign::TopLeft, kText, kBg);
    renderer_.drawTextFace(uiStr(UiStringId::kReloginTitle), x, y + 40,
                           TextFace::SansBold12, TextAlign::TopLeft, kCrit, kBg);
    drawWrappedFace(uiStr(UiStringId::kReloginBody), x, y + 80, w, 20,
                    TextFace::Sans9, kText, 4);
  }
}

void UsageUI::drawNoticeColumn(int x, int y, int w, int h, const char* name,
                              const char* reason) {
  const char* msg = (reason && reason[0]) ? reason : "Check Provider Settings";
  if (kIsLarge) {
    renderer_.drawText(name, x, y, 4, TextAlign::TopLeft, kText, kBg);
    renderer_.drawText("FETCH STOPPED", x, y + 70, 4, TextAlign::TopLeft, kCrit, kBg);
    drawWrapped(msg, x, y + 130, w, 40, 3, kText, 4);
  } else {
    renderer_.drawTextFace(name, x, y, TextFace::SansBold12, TextAlign::TopLeft, kText, kBg);
    renderer_.drawTextFace("FETCH STOPPED", x, y + 40, TextFace::SansBold12,
                           TextAlign::TopLeft, kCrit, kBg);
    drawWrappedFace(msg, x, y + 80, w, 20, TextFace::Sans9, kText, 4);
  }
}

void UsageUI::drawPlatformColumn(int x, int y, int w, int h, const char* name,
                                 const ProviderQuota& p, long nowEpoch) {
  (void)name; (void)nowEpoch;
  // Header: always the full provider name (caller passes the short "ClaudePlat").
  renderer_.drawTextFace("Claude Platform", x, y,
                         TextFace::SansBold12, TextAlign::TopLeft, kText, kBg);

  const bool spend = p.platSpendMode;
  const int  win   = p.platWindowDays;
  int cy = y + 26;
  char buf[56];

  // Primary big number + inline label (same line, just to the right).
  //  Prepaid mode: remaining = prepaid - 30-day cost; Spend mode: window cost.
  const char* label = nullptr;
  char lblbuf[16];
  bool dim = false;
  if (!spend) {
    if (p.hasLeft)      { snprintf(buf, sizeof(buf), "$%.2f", p.leftCents / 100.0); label = "estimated remaining"; }
    else if (p.hasCost) { snprintf(buf, sizeof(buf), "$%.2f", p.costCents / 100.0); label = "30-day cost"; }
    else                { snprintf(buf, sizeof(buf), "$--"); dim = true; }
  } else {
    if (p.hasCost) { snprintf(buf, sizeof(buf), "$%.2f", p.costCents / 100.0);
                     snprintf(lblbuf, sizeof(lblbuf), "%d-day cost", win); label = lblbuf; }
    else           { snprintf(buf, sizeof(buf), "$--"); dim = true; }
  }
  renderer_.drawTextFace(buf, x, cy, TextFace::SansBold24, TextAlign::TopLeft,
                         dim ? kMuted : kText, kBg);
  if (label) {
    const int numW = renderer_.measureTextFace(buf, TextFace::SansBold24);
    renderer_.drawTextFace(label, x + numW + 10, cy + 24, TextFace::Sans9,
                           TextAlign::BottomLeft, kMuted, kBg);
  }
  cy += 36;
  display_.fillRect(x, cy, w, 1, kLine);
  cy += 12;
  // Prepaid amount sits under the separator, above the models.
  if (!spend && p.prepaidCents > 0) {
    snprintf(buf, sizeof(buf), "Prepaid amount: $%.2f", p.prepaidCents / 100.0);
    renderer_.drawTextFace(buf, x, cy, TextFace::Sans9, TextAlign::TopLeft, kMuted, kBg);
    cy += 18;
  }

  // Per-model spend bars (top models, already sorted by cost desc). Reserve the
  // bottom for the token-usage line + the four report headers.
  double maxCents = 0;
  for (uint8_t i = 0; i < p.platCount; ++i)
    if (p.platModels[i].cents > maxCents) maxCents = p.platModels[i].cents;

  const int rowH = 26;
  const int labelW = 120;
  const int valW = 64;
  const int reserveBottom = 100;
  for (uint8_t i = 0; i < p.platCount && cy + rowH <= y + h - reserveBottom; ++i) {
    const ProviderQuota::PlatModel& m = p.platModels[i];
    const char* nm = m.name;
    if (strncmp(nm, "claude-", 7) == 0) nm += 7;   // shorten for width
    renderer_.drawTextFace(nm, x, cy, TextFace::Sans9, TextAlign::TopLeft, kText, kBg);
    const int barX = x + labelW;
    const int barW = w - labelW - valW;
    const int barH = 12;
    if (barW > 10) {
      display_.fillRect(barX, cy + 2, barW, barH, kTrack);
      const int fill = (maxCents > 0)
          ? static_cast<int>(barW * (m.cents / maxCents)) : 0;
      if (fill > 0) display_.fillRect(barX, cy + 2, fill, barH, kText);
    }
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "$%.2f", m.cents / 100.0);
    renderer_.drawTextFace(vbuf, x + w, cy, TextFace::SansBold9,
                           TextAlign::TopRight, kText, kBg);
    cy += rowH;
  }
  if (p.platCount == 0 && p.ok) {
    snprintf(buf, sizeof(buf), "No usage in last %d days", win);
    renderer_.drawTextFace(buf, x, cy, TextFace::Sans9, TextAlign::TopLeft, kMuted, kBg);
    cy += rowH;
  }

  // Token usage for the active window.
  const double tk = p.balance;
  char tkBuf[20];
  if (tk >= 1e9) snprintf(tkBuf, sizeof(tkBuf), "%.2fB", tk / 1e9);
  else if (tk >= 1e6) snprintf(tkBuf, sizeof(tkBuf), "%.1fM", tk / 1e6);
  else if (tk >= 1e3) snprintf(tkBuf, sizeof(tkBuf), "%.1fK", tk / 1e3);
  else snprintf(tkBuf, sizeof(tkBuf), "%.0f", tk);
  snprintf(buf, sizeof(buf), "%d day token usage: %s", win, tkBuf);
  renderer_.drawTextFace(buf, x, cy, TextFace::Sans9, TextAlign::TopLeft, kText, kBg);
  cy += 18;

  // Extra cost reports (placeholders this pass — real fetches to follow).
  static const char* const kReports[4] = {
    "Cost by workspace: n/a",
    "Cost by description: n/a",
    "Rate limits: n/a",
    "Claude Code analytics: n/a",
  };
  for (int i = 0; i < 4 && cy + 14 <= y + h; ++i) {
    renderer_.drawTextFace(kReports[i], x, cy, TextFace::Sans9,
                           TextAlign::TopLeft, kMuted, kBg);
    cy += 16;
  }
}

void UsageUI::drawLocalStatsBlock(int x, int y, int w, int h, const ProviderQuota& p,
                                  long nowEpoch) {
  drawBox(x, y, w, h, kBg);
  const int pad = kIsLarge ? 18 : 10;
  const int rowH = kIsLarge ? (UM_LANG_ZH ? 48 : 38) : 34;
  const int headingGap = kIsLarge ? (UM_LANG_ZH ? 76 : 64) : 58;
  int cy = y + pad;

  if (p.local.enabled) {
    renderer_.drawTextFace(uiStr(UiStringId::kLocalUsage), x + pad, cy, TextFace::SansBold18,
                           TextAlign::TopLeft, kText, kBg);
    drawStatusBadge(x + w - pad, cy, p.local.available ? "READY" : p.local.status,
                    !p.local.available);
    cy += headingGap;

    if (!p.local.available) {
      drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kStatus), p.local.status, 2, kBg);
      cy += rowH;
      drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kFallback),
                  uiStr(UiStringId::kCloudQuota), 2, kBg);
      return;
    }

    drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kTodayTokens),
                fmtTokens(p.local.todayTokens), 3, kBg);
    cy += rowH;
    if (cy + rowH < y + h) {
      drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kInOut),
                  fmtTokens(p.local.inputTokens) + " / " + fmtTokens(p.local.outputTokens), 2, kBg);
      cy += rowH;
    }
    if (cy + rowH < y + h) {
      drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kSessions),
                  String(p.local.sessionCount), 2, kBg);
      cy += rowH;
    }
    if (cy + rowH < y + h) {
      drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kLatest),
                  fmtClock(p.local.latestEpoch), 2, kBg);
      cy += rowH + 8;
    }

    if (cy + 36 >= y + h) return;
    renderer_.drawTextFace(uiStr(UiStringId::kTopModels), x + pad, cy, TextFace::SansBold12,
                           TextAlign::TopLeft, kText, kBg);
    cy += 40;
    uint32_t maxTokens = 1;
    for (uint8_t i = 0; i < p.local.modelCount; ++i) {
      if (p.local.models[i].tokens > maxTokens) maxTokens = p.local.models[i].tokens;
    }
    for (uint8_t i = 0; i < p.local.modelCount && cy + 34 < y + h; ++i) {
      const LocalModelStat& m = p.local.models[i];
      renderer_.drawTextFace(m.name[0] ? m.name : "unknown", x + pad, cy,
                             TextFace::Sans9, TextAlign::TopLeft, kText, kBg);
      renderer_.drawTextFace(fmtTokens(m.tokens), x + w - pad, cy, TextFace::SansBold9,
                             TextAlign::TopRight, kText, kBg);
      const int barW = w - pad * 2;
      const int fill = static_cast<int>(barW * (static_cast<double>(m.tokens) / maxTokens));
      display_.fillRect(x + pad, cy + 24, barW, 8, kTrack);
      if (fill > 0) display_.fillRect(x + pad, cy + 24, fill, 8, kText);
      cy += 44;
    }
    return;
  }

  renderer_.drawTextFace(uiStr(UiStringId::kCloudSummary), x + pad, cy, TextFace::SansBold18,
                         TextAlign::TopLeft, kText, kBg);
  drawStatusBadge(x + w - pad, cy, p.isStale(nowEpoch, 900) ? uiStr(UiStringId::kStale) : "LIVE",
                  p.isStale(nowEpoch, 900));
  cy += headingGap;

  drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kStatus),
              p.ok ? uiStr(UiStringId::kQuotaReady) : uiStr(UiStringId::kWaiting), 2, kBg);
  cy += rowH;
  drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kSessionLeft),
              p.session.present ? fmtPercent(p.session.remainingPercent()) : String("--"), 2, kBg);
  cy += rowH;
  drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kWeeklyLeft),
              p.weekly.present ? fmtPercent(p.weekly.remainingPercent()) : String("--"), 2, kBg);
  cy += rowH;
  if (p.hasPlan && p.planType[0] && cy + rowH < y + h) {
    drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kPlan), p.planType, 2, kBg);
    cy += rowH;
  }
  if (p.hasBalance && cy + rowH < y + h) {
    char balBuf[20];
    snprintf(balBuf, sizeof(balBuf), "%.2f", p.balance);
    drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kBalance), balBuf, 2, kBg);
    cy += rowH;
  }
  if (p.extraEnabled && cy + rowH < y + h) {
    char exBuf[28];
    snprintf(exBuf, sizeof(exBuf), "%.2f / %.2f", p.extraUsedCents / 100.0,
             p.extraLimitCents / 100.0);
    drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kExtra), exBuf, 2, kBg);
    cy += rowH;
  }
  if ((p.weeklyOpus.present || p.weeklySonnet.present) && cy + 104 < y + h) {
    renderer_.drawTextFace(uiStr(UiStringId::kModelQuotas), x + pad, cy, TextFace::SansBold12,
                           TextAlign::TopLeft, kText, kBg);
    cy += 40;
    drawModelRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kWinOpus), p.weeklyOpus);
    cy += 36;
    drawModelRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kWinSonnet), p.weeklySonnet);
    cy += 36;
  }
  if (cy + rowH < y + h) {
    drawInfoRow(x + pad, cy, w - pad * 2, uiStr(UiStringId::kUpdated),
                fmtClock(p.lastSuccessEpoch), 2, kBg);
  }
}

void UsageUI::drawProviderColumn(int x, int y, int w, int h, const char* name,
                                 const ProviderQuota& p, long nowEpoch) {
  if (p.disabled) {
    drawNoticeColumn(x, y, w, h, name, p.failReason);
    return;
  }
  if (p.needsRelogin) {
    drawNoticeColumn(x, y, w, h, name, "Token Revoked or Expired. Update Tokens");
    return;
  }
  if (p.needAdminKey) {
    drawNoticeColumn(x, y, w, h, name, "Needs Admin Key (sk-ant-admin)");
    return;
  }
  if (p.id == ProviderId::kClaudePlatform) {
    drawPlatformColumn(x, y, w, h, name, p, nowEpoch);
    return;
  }

  // Column header: provider name + plan level inline (e.g. "Claude Max").
  String hdr = name;
  if (p.hasPlan && p.planType[0]) {
    String lvl = p.planType;
    lvl.setCharAt(0, (char)toupper((unsigned char)lvl[0]));   // free->Free, pro->Pro...
    hdr += " " + lvl;
  }
  if (kIsLarge) {
    renderer_.drawText(hdr, x, y, 4, TextAlign::TopLeft, kText, kBg);
  } else {
    renderer_.drawTextFace(hdr, x, y, TextFace::SansBold12, TextAlign::TopLeft, kText, kBg);
  }

  int rightY = y + 4;
  const bool stale = p.isStale(nowEpoch, 900);
  if (kIsLarge) {
    renderer_.drawText(stale ? uiStr(UiStringId::kStale) : "LIVE",
                       x + w, rightY + 34, 2, TextAlign::TopRight,
                       stale ? kCrit : kMuted, kBg);
  }

  const int headerH = kIsLarge ? 60 : 40;
  int cy = y + headerH;

  // Two window cards stacked: session on top, weekly below.
  {
    const int cardH = (h - headerH - 20) / 2;
    drawWindowCard(x, cy, w, cardH, uiStr(UiStringId::kWinSession),
                   p.session, nowEpoch, /*emphasize=*/true);
    cy += cardH + 10;
    drawWindowCard(x, cy, w, cardH, uiStr(UiStringId::kWinWeekly),
                   p.weekly, nowEpoch, /*emphasize=*/false);
    if (!kIsLarge) return;
    cy += cardH + 10;
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
  if (kIsLarge) {
    renderer_.drawText(statusText, display_.width() / 2, display_.height() / 2,
                       4, TextAlign::MiddleCenter, kText, kBg);
  } else {
    renderer_.drawTextFace(statusText, display_.width() / 2, display_.height() / 2,
                           TextFace::SansBold12, TextAlign::MiddleCenter, kText, kBg);
  }
  display_.update();
}

void UsageUI::drawDashboard(const UsageSnapshot& snap, const UiStatus& st, long nowEpoch) {
  const int w = display_.width();
  const int h = display_.height();
  const int margin = kIsLarge ? 44 : 20;

  display_.fillSprite(kBg);
  drawHeader(st, nowEpoch);

  const int headerH = kIsLarge ? 124 : 70;
  const int footerH = 0;
  const int colTop = headerH;
  const int colH = h - headerH - footerH;
  const int colGap = kIsLarge ? 24 : 16;
  const int colW = (w - margin * 2 - colGap) / 2;

  // Vertical divider between the two columns (large panels only).
  if (kIsLarge) {
    const int dividerX = margin + colW + colGap / 2;
    display_.fillRect(dividerX, colTop, 1, colH, kLine);
  }

  // Left and right providers, names from the snapshot (set by ProviderSelect.h).
  drawProviderColumn(margin, colTop, colW, colH, snap.left.name, snap.left, nowEpoch);
  drawProviderColumn(margin + colW + colGap, colTop, colW, colH, snap.right.name,
                     snap.right, nowEpoch);

  // GRAY4 only refreshes cleanly with a full update; partial/overlay refresh
  // corrupts the 4-bpp buffer, so always full-refresh.
  display_.update();
}

}  // namespace usage_monitor
