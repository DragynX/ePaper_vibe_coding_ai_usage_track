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

  // Edge rendering: true = 4-level grayscale anti-aliasing (smooth), false = crisp 1-bit.
  void setSmoothing(bool on);
  // AA contrast 0..100 (0 = soft/linear edges, 100 = near-crisp). Smooth mode only.
  void setSharpness(int v);
  // Text weight 0..100 (0 = off, higher = darker thin strokes). Smooth mode only.
  void setWeight(int v);
  // Small text: true = crisp baked bitmap (Open Sans), false = vector (OFR/AA).
  void setSmallCrisp(bool on);
  // Faux-italic: shear subsequent text (forces the vector path so the shear shows).
  // Call setItalic(true) around a draw, then setItalic(false) to restore.
  void setItalic(bool on);

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

  // --- Font Testing playground helpers (English/OFR build) ---
  // Draw text at an exact pixel height via OpenFontRender (honors smoothing/weight).
  void drawTextPx(const String& text, int x, int y, int px,
                  TextAlign align, uint16_t color, uint16_t bg, bool bold);
  // Draw via a baked Open Sans bitmap at exact px; returns false if none baked at px.
  bool drawTextPxBaked(const String& text, int x, int y, int px,
                       TextAlign align, uint16_t color, uint16_t bg, bool bold);
  // OpenFontRender pixel width of text at px.
  int textWidthPx(const String& text, int px, bool bold);
  // Display name of font index i (for the playground header). Empty if out of range.
  static const char* fontName(int idx);

 private:
  // Render/measure small text via a baked GFXfont (crisp 1-bit). English/OFR builds.
  void drawGfx(const GFXfont* font, const String& text, int x, int y,
               TextAlign align, uint16_t color, uint16_t bg);
  int  gfxWidth(const GFXfont* font, const String& text);

  EPaper* display_ = nullptr;
  bool fontReady_ = false;
};

#endif  // USAGE_MONITOR_TEXT_RENDERER_H
