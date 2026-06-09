# Device fonts (OpenFontRender / TrueType)

The English build renders all on-device text through **OpenFontRender** (TrueType +
FreeType) so any pixel size is available. The active typeface is **Arimo**
(`Arimo-Regular.ttf` / `Arimo-Bold.ttf`) — Google's metric-compatible clone of
**Liberation Sans / Arial**. Roboto and DejaVu Sans are kept here as drop-in backups.

Toggle the whole system off with `#define UM_USE_OFR 0` in `ProjectConfig.h` (reverts to
the fixed-size GFXFF fonts).

## Files
- `Arimo-Regular.ttf`, `Arimo-Bold.ttf` — active (full TTFs, kept for re-subsetting).
- `Arimo-*.subset.ttf` — ASCII-subset actually embedded.
- `Roboto-*.ttf`, `DejaVuSans*.ttf` — backup typefaces.
- The embedded C arrays live at the repo root: `FontLatinRegular.h`
  (`um_font_latin_reg[]` / `_len`) and `FontLatinBold.h` (`um_font_latin_bold[]` / `_len`).

## Regenerate / swap typeface
Requires Python + `fonttools` (`python -m pip install fonttools brotli`) and `xxd`.

```bash
cd fonts
U="U+0020-007E,U+00B0,U+2013,U+2014,U+2022,U+00B7,U+2192"   # ASCII + a few punctuation
# pick a face, e.g. Roboto:
for w in Regular Bold; do
  python -m fontTools.subset "Roboto-$w.ttf" --unicodes="$U" \
    --layout-features='*' --notdef-outline --output-file="Roboto-$w.subset.ttf"
done
# regenerate the embedded headers (rename symbols, mark const):
gen(){ { echo '#pragma once';
         xxd -i "$1" | sed -e "s/unsigned char .*\[\]/const unsigned char $2[]/" \
                            -e "s/unsigned int .*_len/const unsigned int ${2}_len/"; } > "../$3"; }
gen Roboto-Regular.subset.ttf um_font_latin_reg  FontLatinRegular.h
gen Roboto-Bold.subset.ttf    um_font_latin_bold FontLatinBold.h
```
Then rebuild. Per-face pixel sizes are tuned in `TextRenderer.cpp` (`facePx()`); the
`TextFace` enum is in `TextRenderer.h`.

## Sources
- Arimo: github.com/googlefonts/Arimo (Apache-2.0)
- Roboto: github.com/googlefonts/roboto (Apache-2.0)
- DejaVu Sans: dejavu-fonts.github.io (Bitstream Vera / public-domain-ish)
