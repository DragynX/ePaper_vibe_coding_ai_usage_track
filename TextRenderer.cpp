#include "TextRenderer.h"

#include "AppLog.h"
#include "ProjectConfig.h"
#include "UiLang.h"

// UM_USE_OFR routes the English build through OpenFontRender (TrueType + FreeType)
// so any pixel size is available. Define it in ProjectConfig.h; default on here so
// the file is self-contained. Set to 0 to fall back to the fixed-size GFXFF fonts.
#ifndef UM_USE_OFR
#define UM_USE_OFR 1
#endif

#if UM_LANG_ZH

#include "OpenFontRender.h"
#include "FontZH.h"   // const unsigned char um_font_zh[]; um_font_zh_len

// OpenFontRender's FileSupport.h declares these file hooks for its file-based
// loadFont path. We load the font from memory and never open a file, but the
// symbols must still resolve at link time, so provide inert stubs.
FT_FILE *OFR_fopen(const char *, const char *) { return nullptr; }
void OFR_fclose(FT_FILE *) {}
size_t OFR_fread(void *, size_t, size_t, FT_FILE *) { return 0; }
int OFR_fseek(FT_FILE *, long int, int) { return -1; }
long int OFR_ftell(FT_FILE *) { return -1; }

namespace {

// Single renderer bound to the panel (Chinese build only).
OpenFontRender g_ofr;

// The panel OFR draws into, and the current glyph color. The pixel hooks below
// are non-capturing (so they convert cleanly to std::function) and read these
// globals instead of capturing the display reference.
EPaper* g_disp = nullptr;
uint16_t g_ink = 0;

// Maps the bitmap "size unit" (the old setTextSize scale, ~8 px per unit) to
// OpenFontRender pixels. Tune on hardware so Chinese glyphs visually match the
// former bitmap sizes.
constexpr int UM_ZH_PX_PER_UNIT = 8;

}  // namespace

#else  // English build: map alignment onto TFT_eSPI text datums.

namespace {

uint8_t toTftDatum(TextAlign a) {
  switch (a) {
    case TextAlign::TopLeft:      return TL_DATUM;
    case TextAlign::TopCenter:    return TC_DATUM;
    case TextAlign::TopRight:     return TR_DATUM;
    case TextAlign::MiddleLeft:   return ML_DATUM;
    case TextAlign::MiddleCenter: return MC_DATUM;
    case TextAlign::MiddleRight:  return MR_DATUM;
    case TextAlign::BottomLeft:   return BL_DATUM;
    case TextAlign::BottomCenter: return BC_DATUM;
    case TextAlign::BottomRight:  return BR_DATUM;
  }
  return TL_DATUM;
}

const GFXfont* toFreeFont(TextFace face) {
  switch (face) {
    case TextFace::Sans7:      return &FreeSans9pt7b;   // GFXFF has no 7pt; nearest
    case TextFace::Sans9:      return &FreeSans9pt7b;
    case TextFace::SansBold9:  return &FreeSansBold9pt7b;
    case TextFace::SansBold12: return &FreeSansBold12pt7b;
    case TextFace::SansBold18: return &FreeSansBold18pt7b;
    case TextFace::SansBold24: return &FreeSansBold24pt7b;
    case TextFace::SansBold36: return &FreeSansBold24pt7b;   // GFXFF caps at 24pt
    case TextFace::SansBold48: return &FreeSansBold24pt7b;
    case TextFace::MonoBold12: return &FreeMonoBold12pt7b;
    default:                  return nullptr;
  }
}

}  // namespace

#if UM_USE_OFR
// ---- English build via OpenFontRender (Latin TTF, any size) ----------------
#include "OpenFontRender.h"
#include "FontLatinRegular.h"   // const unsigned char um_font_latin_reg[];  _len
#include "FontLatinBold.h"      // const unsigned char um_font_latin_bold[]; _len

// Inert link stubs for OFR's file-based path (we load from memory only).
FT_FILE *OFR_fopen(const char *, const char *) { return nullptr; }
void OFR_fclose(FT_FILE *) {}
size_t OFR_fread(void *, size_t, size_t, FT_FILE *) { return 0; }
int OFR_fseek(FT_FILE *, long int, int) { return -1; }
long int OFR_ftell(FT_FILE *) { return -1; }

namespace {

// Two faces: regular + bold (one OFR instance each).
OpenFontRender g_ofrReg;
OpenFontRender g_ofrBold;
EPaper* g_disp = nullptr;
uint16_t g_ink = 0;             // real panel gray index to paint glyph ink with

// drawText() "size unit" (old setTextSize scale) -> OFR pixels.
constexpr int UM_PX_PER_UNIT = 8;

// We feed OFR pure white fg / black bg, so the color it hands the pixel hooks
// encodes glyph coverage. Threshold that to crisp 1-bit (the GRAY4 panel mangles
// real anti-aliasing): paint ink only where coverage clears the cutoff. The
// green channel (6 bits, 0..63) is the luminance proxy. Lower = bolder strokes.
constexpr uint8_t UM_OFR_THRESH = 30;   // ~45% coverage; tunable 20..40
static inline bool ofrCovered(uint16_t c) { return ((c >> 5) & 0x3F) >= UM_OFR_THRESH; }

// Pixel height per face. GRAY4 is crisp at these sizes; tuned on device.
int facePx(TextFace f) {
  switch (f) {
    case TextFace::Sans7:      return 10;
    case TextFace::Sans9:      return 13;
    case TextFace::SansBold9:  return 13;
    case TextFace::SansBold12: return 17;
    case TextFace::SansBold18: return 25;
    case TextFace::SansBold24: return 33;
    case TextFace::SansBold36: return 50;
    case TextFace::SansBold48: return 66;
    case TextFace::MonoBold12: return 17;
    default:                  return 13;
  }
}

// Bold faces render through the bold instance; everything else regular.
OpenFontRender& faceOfr(TextFace f) {
  switch (f) {
    case TextFace::SansBold9:
    case TextFace::SansBold12:
    case TextFace::SansBold18:
    case TextFace::SansBold24:
    case TextFace::SansBold36:
    case TextFace::SansBold48:
    case TextFace::MonoBold12: return g_ofrBold;
    default:                  return g_ofrReg;   // Bitmap, Sans7, Sans9
  }
}

// Resolve a TopLeft origin from the requested anchor for a w x h box.
void anchorTopLeft(TextAlign a, int& x, int& y, int w, int h) {
  switch (a) {
    case TextAlign::TopCenter:
    case TextAlign::MiddleCenter:
    case TextAlign::BottomCenter: x -= w / 2; break;
    case TextAlign::TopRight:
    case TextAlign::MiddleRight:
    case TextAlign::BottomRight:  x -= w; break;
    default: break;
  }
  switch (a) {
    case TextAlign::MiddleLeft:
    case TextAlign::MiddleCenter:
    case TextAlign::MiddleRight:  y -= h / 2; break;
    case TextAlign::BottomLeft:
    case TextAlign::BottomCenter:
    case TextAlign::BottomRight:  y -= h; break;
    default: break;
  }
}

void ofrDraw(OpenFontRender& ofr, const String& text, int x, int y, int px,
             TextAlign align, uint16_t color, uint16_t bg) {
  (void)bg;
  g_ink = color;                       // the gray index the threshold hooks paint
  ofr.setFontSize(static_cast<unsigned>(px));
  // "%s" wrapper: getTextWidth is printf-style; a literal '%' would be a format.
  const int w = static_cast<int>(ofr.getTextWidth("%s", text.c_str()));
  const int h = px;
  int ox = x, oy = y;
  anchorTopLeft(align, ox, oy, w, h);
  FT_BBox bbox;
  FT_Error error;
  // Pass white fg / black bg so the hook's color arg = coverage; the hook
  // thresholds it and paints g_ink. (bg fill is unused — sprite is pre-cleared.)
  ofr.drawHString(text.c_str(), ox, oy, 0xFFFF, 0x0000, Align::TopLeft,
                  Drawing::Execute, bbox, error);
}

int ofrWidth(OpenFontRender& ofr, const String& text, int px) {
  ofr.setFontSize(static_cast<unsigned>(px));
  return static_cast<int>(ofr.getTextWidth("%s", text.c_str()));
}

}  // namespace
#endif  // UM_USE_OFR

#endif  // UM_LANG_ZH

bool TextRenderer::begin(EPaper& display)
{
  display_ = &display;
#if UM_LANG_ZH
  g_disp = &display;
  g_ofr.setDrawer(static_cast<TFT_eSPI&>(display));
  // The gray16 panel is a 4-bit sprite: drawPixel keeps only the low 4 bits of
  // the color as a gray index (Sprite.cpp). OpenFontRender anti-aliases by
  // blending fg/bg as RGB565, so the value it feeds drawPixel has a meaningless
  // low nibble here -- the glyph comes out hollow and blurry. Override OFR's
  // pixel hooks to paint SOLID ink (g_ink, a real gray index) for every covered
  // pixel via the panel's own virtual drawPixel (the path MemoUI uses). This
  // trades anti-aliasing for crisp, solid Chinese text.
  g_ofr.set_drawPixel([](int32_t px, int32_t py, uint16_t) {
    if (g_disp) g_disp->drawPixel(px, py, g_ink);
  });
  g_ofr.set_drawFastHLine([](int32_t px, int32_t py, int32_t pw, uint16_t) {
    if (g_disp) {
      for (int32_t i = 0; i < pw; ++i) g_disp->drawPixel(px + i, py, g_ink);
    }
  });
  // loadFont returns non-zero on failure. The font is embedded in flash
  // (FontZH.h), so there is no filesystem to mount.
  if (g_ofr.loadFont(um_font_zh, um_font_zh_len)) {
    fontReady_ = false;
    sysLog("[ofr] loadFont (embedded) failed");
    return false;
  }
  g_ofr.showCredit();   // FreeType FTL license attribution
  fontReady_ = true;
  return true;
#elif UM_USE_OFR
  // English build: bind both faces and load the embedded Latin TTFs. The same
  // 4-bit solid-ink override as the ZH path keeps glyphs crisp on GRAY4.
  g_disp = &display;
  auto bind = [&](OpenFontRender& ofr, const unsigned char* data, unsigned len) -> bool {
    ofr.setDrawer(static_cast<TFT_eSPI&>(display));
    // Coverage-thresholded ink: drop faint anti-aliased fringe pixels so glyphs
    // stay crisp on the 4-bit GRAY4 panel instead of smudging.
    ofr.set_drawPixel([](int32_t px, int32_t py, uint16_t c) {
      if (g_disp && ofrCovered(c)) g_disp->drawPixel(px, py, g_ink);
    });
    ofr.set_drawFastHLine([](int32_t px, int32_t py, int32_t pw, uint16_t c) {
      if (g_disp && ofrCovered(c)) {
        for (int32_t i = 0; i < pw; ++i) g_disp->drawPixel(px + i, py, g_ink);
      }
    });
    return ofr.loadFont(data, len) == 0;   // 0 = success
  };
  const bool okReg  = bind(g_ofrReg,  um_font_latin_reg,  um_font_latin_reg_len);
  const bool okBold = bind(g_ofrBold, um_font_latin_bold, um_font_latin_bold_len);
  fontReady_ = okReg && okBold;
  if (!fontReady_) sysLog("[ofr] latin loadFont failed -> GFXFF fallback");
  return true;   // GFXFF/bitmap fallback still renders even if !fontReady_
#else
  fontReady_ = true;    // bitmap font is always available
  return true;
#endif
}

void TextRenderer::drawText(const String& text, int x, int y, int sizeUnit,
                            TextAlign align, uint16_t color, uint16_t bg)
{
  if (!display_) return;
#if UM_LANG_ZH
  // Never call into OpenFontRender without a loaded font: it dereferences a
  // null face and crashes. Degrade by skipping the glyphs instead.
  if (!fontReady_) return;

  // Solid ink the pixel hooks paint for this glyph run.
  g_ink = color;

  const unsigned px = static_cast<unsigned>(sizeUnit * UM_ZH_PX_PER_UNIT);
  g_ofr.setFontSize(px);
  // "%s" wrapper: getTextWidth is printf-style, so a literal '%' in the text
  // would otherwise be read as a format specifier.
  const int w = static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
  const int h = static_cast<int>(px);

  // Resolve the anchor to a top-left origin ourselves, then draw with
  // Align::TopLeft. OFR's other alignment modes are reported as fiddly, and
  // TopLeft is the mode whose behavior is least ambiguous.
  int ox = x;
  int oy = y;
  switch (align) {
    case TextAlign::TopCenter:
    case TextAlign::MiddleCenter:
    case TextAlign::BottomCenter: ox = x - w / 2; break;
    case TextAlign::TopRight:
    case TextAlign::MiddleRight:
    case TextAlign::BottomRight:  ox = x - w; break;
    default: break;
  }
  switch (align) {
    case TextAlign::MiddleLeft:
    case TextAlign::MiddleCenter:
    case TextAlign::MiddleRight:  oy = y - h / 2; break;
    case TextAlign::BottomLeft:
    case TextAlign::BottomCenter:
    case TextAlign::BottomRight:  oy = y - h; break;
    default: break;
  }

  FT_BBox bbox;
  FT_Error error;
  g_ofr.drawHString(text.c_str(), ox, oy, color, bg,
                    Align::TopLeft, Drawing::Execute, bbox, error);
#else
#if UM_USE_OFR
  if (fontReady_) {
    ofrDraw(g_ofrReg, text, x, y, sizeUnit * UM_PX_PER_UNIT, align, color, bg);
    return;
  }
#endif
  display_->setTextSize(sizeUnit);
  display_->setTextColor(color, bg, true);
  display_->setTextDatum(toTftDatum(align));
  display_->drawString(text, x, y);
#endif
}

void TextRenderer::drawTextFace(const String& text, int x, int y, TextFace face,
                                TextAlign align, uint16_t color, uint16_t bg)
{
  if (!display_) return;
#if UM_LANG_ZH
  int sizeUnit = 2;
  switch (face) {
    case TextFace::SansBold12:
    case TextFace::SansBold18:
    case TextFace::SansBold24:
    case TextFace::MonoBold12: sizeUnit = 4; break;
    case TextFace::SansBold9:  sizeUnit = 3; break;
    default:                  sizeUnit = 2; break;
  }
  drawText(text, x, y, sizeUnit, align, color, bg);
#else
#if UM_USE_OFR
  if (fontReady_) {
    ofrDraw(faceOfr(face), text, x, y, facePx(face), align, color, bg);
    return;
  }
#endif
  const GFXfont* font = toFreeFont(face);
  if (!font) {
    drawText(text, x, y, 2, align, color, bg);
    return;
  }
  display_->setFreeFont(font);
  display_->setTextSize(1);
  display_->setTextColor(color, bg, true);
  display_->setTextDatum(toTftDatum(align));
  display_->drawString(text, x, y);
  display_->setFreeFont(nullptr);
#endif
}

int TextRenderer::measureText(const String& text, int sizeUnit)
{
  if (!display_) return 0;
#if UM_LANG_ZH
  if (!fontReady_) return 0;
  g_ofr.setFontSize(static_cast<unsigned>(sizeUnit * UM_ZH_PX_PER_UNIT));
  return static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
#else
#if UM_USE_OFR
  if (fontReady_) return ofrWidth(g_ofrReg, text, sizeUnit * UM_PX_PER_UNIT);
#endif
  display_->setTextSize(sizeUnit);
  return static_cast<int>(display_->textWidth(text));
#endif
}

int TextRenderer::measureTextFace(const String& text, TextFace face)
{
  if (!display_) return 0;
#if UM_LANG_ZH
  int sizeUnit = 2;
  switch (face) {
    case TextFace::SansBold12:
    case TextFace::SansBold18:
    case TextFace::SansBold24:
    case TextFace::MonoBold12: sizeUnit = 4; break;
    case TextFace::SansBold9:  sizeUnit = 3; break;
    default:                  sizeUnit = 2; break;
  }
  return measureText(text, sizeUnit);
#else
#if UM_USE_OFR
  if (fontReady_) return ofrWidth(faceOfr(face), text, facePx(face));
#endif
  const GFXfont* font = toFreeFont(face);
  if (!font) return measureText(text, 2);
  display_->setFreeFont(font);
  display_->setTextSize(1);
  const int w = static_cast<int>(display_->textWidth(text));
  display_->setFreeFont(nullptr);
  return w;
#endif
}
