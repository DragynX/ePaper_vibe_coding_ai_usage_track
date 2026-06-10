// UsageUI.h -- all e-paper rendering for the usage monitor.
// 墨水屏渲染:双栏仪表盘(左 Codex / 右 Claude)、开机页、重登录提示。
//
// Drawing goes to the in-RAM EPaper sprite (fillSprite -> primitives ->
// update()). Every string is drawn through TextRenderer so the bilingual
// dispatch (bitmap font vs OpenFontRender) stays invisible here.
// 所有绘制先写入内存 sprite,最后 update() 整刷;文字统一走 TextRenderer。

#ifndef USAGE_MONITOR_USAGE_UI_H
#define USAGE_MONITOR_USAGE_UI_H

#include <Arduino.h>

#include "driver.h"
#include "TextRenderer.h"
#include "UsageSnapshot.h"

namespace usage_monitor {

// Status drawn in the header (battery is optional; -1 hides it).
struct UiStatus {
  bool   wifiConnected  = false;
  int    batteryPercent = -1;
  bool   refreshing     = false;
  String ipAddress;
  long   lastFetchEpoch = 0;   // when the last refresh completed (local epoch)
  long   nextFetchEpoch = 0;   // lastFetch + refresh interval
  float  batteryDays = -2.0f;  // >=0 estimated days left (only shown when discharging)
  bool   batteryCharging = false;  // on external/USB power -> show "Charging" not days
  bool   deepSleepOn = false;  // deep sleep enabled -> draw moon in header
};

class UsageUI {
 public:
  void begin();
  uint16_t displayWidth();
  uint16_t displayHeight();

  // Select the screen palette. Dark mode inverts the grayscale ramp
  // (black background, white ink) on the GRAY4 panel. No-op on other panels.
  void setDarkMode(bool dark);

  // Select the device typeface (index into the font table). Hot-swaps on OFR
  // builds; caller repaints afterward.
  void setFont(int i) { renderer_.setFontIndex(i); }

  // Edge smoothing: true = grayscale anti-aliased, false = crisp 1-bit. Repaint after.
  void setSmoothing(bool on) { renderer_.setSmoothing(on); }
  void setSharpness(int v)   { renderer_.setSharpness(v); }

  // One-line boot/splash reusing the header band (full clear + full refresh).
  void drawBoot(const String& statusText, const UiStatus& status, long nowEpoch);

  // Main two-provider dashboard. nowEpoch (UTC) drives the reset countdowns.
  void drawDashboard(const UsageSnapshot& snap, const UiStatus& status, long nowEpoch);

 private:
  EPaper display_;
  TextRenderer renderer_;

  // Header: title (left) + wall clock/date (center) + status icons (right).
  void drawHeader(const UiStatus& status, long nowEpoch);

  // One provider column inside [x, x+w). Renders the two big windows, the
  // Claude-only per-model rows or the Codex balance, and the plan.
  void drawProviderColumn(int x, int y, int w, int h, const char* name,
                          const ProviderQuota& p, long nowEpoch);

  // One rate-limit window card: label + big remaining% + used% bar + reset.
  void drawWindowCard(int x, int y, int w, int h, const char* label,
                      const WindowQuota& win, long nowEpoch, bool emphasize);

  // A horizontal progress bar filled to `pct` percent (the battery-fill idiom).
  void drawProgressBar(int x, int y, int w, int h, double pct, uint16_t fg,
                       uint16_t track);

  // A slim "model 70%" row: name + thin bar + percent (Claude Sonnet/Opus).
  void drawModelRow(int x, int y, int w, const char* name, const WindowQuota& win);

  void drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color);
  void drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color);
  void drawWrapped(const String& text, int x, int y, int maxW, int lineH,
                   int textSize, uint16_t color, int maxLines);
  void drawWrappedFace(const String& text, int x, int y, int maxW, int lineH,
                       TextFace face, uint16_t color, int maxLines);

  // Re-login notice filling a provider column (refresh token revoked).
  void drawReloginColumn(int x, int y, int w, int h, const char* name);

  // Circuit-breaker notice: provider stopped after repeated fetch failures.
  void drawNoticeColumn(int x, int y, int w, int h, const char* name,
                        const char* reason);

  // Claude Platform: 7-day cost (USD) + token total + per-model cost bars.
  void drawPlatformColumn(int x, int y, int w, int h, const char* name,
                          const ProviderQuota& p, long nowEpoch);

  void drawBox(int x, int y, int w, int h, uint16_t fill);
  void drawStatusBadge(int x, int y, const String& text, bool alert);
  void drawInfoRow(int x, int y, int w, const char* label, const String& value,
                   int textSize, uint16_t bg);
  void drawQuotaDetail(int x, int y, int w, int usedX, int leftX, int resetX,
                       const char* label, const WindowQuota& win, long nowEpoch);
  void drawLocalStatsBlock(int x, int y, int w, int h, const ProviderQuota& p,
                           long nowEpoch);
};

}  // namespace usage_monitor

#endif  // USAGE_MONITOR_USAGE_UI_H
