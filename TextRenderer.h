// TextRenderer.h -- text rendering facade for the UI.
//
// MemoUI draws every string through drawText() / measureText() instead of
// touching the panel font API directly. The English build routes these to the
// built-in Seeed_GFX bitmap font; the Chinese build (UM_LANG_ZH) routes them to
// OpenFontRender with a TrueType font stored in SPIFFS. The English build never
// includes or links OpenFontRender, so it carries no new risk.

#ifndef USAGE_MONITOR_TEXT_RENDERER_H
#define USAGE_MONITOR_TEXT_RENDERER_H

#include <Arduino.h>

#include "driver.h"
#include <TFT_eSPI.h>

// The anchor of the text box that the (x, y) given to drawText refers to.
enum class TextAlign {
  TopLeft, TopCenter, TopRight,
  MiddleLeft, MiddleCenter, MiddleRight,
  BottomLeft, BottomCenter, BottomRight
};

enum class TextFace {
  Bitmap,
  Sans7,       // small regular — for tiny header labels (OFR builds render true size)
  Sans9,
  SansBold9,
  SansBold12,
  SansBold18,
  SansBold24,
  SansBold36,   // ~2x SansBold18 (OFR builds only; GFXFF clamps to 24pt)
  SansBold48,   // ~2x SansBold24
  MonoBold12
};

class TextRenderer {
 public:
  // English build: always succeeds (the bitmap font is built in).
  // Chinese build: mounts SPIFFS, binds the OpenFontRender drawer to the
  // display, and loads the .ttf. Returns false and leaves fontReady() false
  // if the font cannot be loaded.
  bool begin(EPaper& display);

  // Switch the active typeface (OFR builds hot-reload without a reboot; clamps to
  // range; no-op if already active). Index order matches the ui_font picker.
  bool setFontIndex(int i);
  static int fontCount();

  bool fontReady() const { return fontReady_; }

  // Draws `text` so the chosen anchor lands at (x, y). `sizeUnit` keeps the
  // bitmap setTextSize() scale; the Chinese path maps it to pixels via a
  // single tunable constant. `bg` is used for anti-aliased edge blending.
  void drawText(const String& text, int x, int y, int sizeUnit,
                TextAlign align, uint16_t color, uint16_t bg);

  // Draws with one of the built-in GFX FreeFonts in English builds. Chinese
  // builds map the face to the existing embedded font sizes.
  // 英文构建使用内置 GFX FreeFonts 中文构建映射到现有内嵌字体字号。
  void drawTextFace(const String& text, int x, int y, TextFace face,
                    TextAlign align, uint16_t color, uint16_t bg);

  // Pixel width of `text` at `sizeUnit`, for layout that needs to measure.
  int measureText(const String& text, int sizeUnit);
  int measureTextFace(const String& text, TextFace face);

 private:
  EPaper* display_ = nullptr;
  bool fontReady_ = false;
};

#endif  // USAGE_MONITOR_TEXT_RENDERER_H
