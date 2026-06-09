# Device fonts (OpenFontRender / TrueType) — runtime-selectable

The English build renders all on-device text through **OpenFontRender** (TrueType +
FreeType), so any pixel size is available. **8 typefaces** are embedded and the active one
is chosen at runtime from the web settings **Display → Device font** dropdown (hot-swaps,
no reboot; persisted in NVS as `ui_font`).

Toggle the whole engine off with `#define UM_USE_OFR 0` in `ProjectConfig.h` (reverts to the
fixed-size GFXFF fonts).

## Font table (index = `ui_font` value = `<select>` order = `kFonts[]` in TextRenderer.cpp)
0 Arimo · 1 DejaVu Sans · 2 Atkinson Hyperlegible · 3 B612 · 4 Lexend · 5 Hack (mono) ·
6 JetBrains Mono · 7 Carlito · 8 Roboto · 9 Open Sans · 10 Noto Sans · 11 Source Sans 3 ·
12 IBM Plex Sans · 13 Fira Sans

Edge rendering is also selectable: **Smooth** (4-level grayscale AA, default) vs **Crisp**
(1-bit) via the `ui_aa` checkbox — handled in `TextRenderer.cpp` `ofrPaint()` /
`setSmoothing()`, not the font files.

Each is embedded as a regular + bold ASCII subset in the generated `FontData.h` (repo root)
as `um_f<i>_reg[]` / `um_f<i>_bold[]` (+ `_len`). Full TTFs are kept in this folder for
re-subsetting.

## Regenerate `FontData.h` (after adding/swapping a font or changing glyph coverage)
Requires Python + `fonttools` (`python -m pip install fonttools brotli`) and `xxd`.
Edit the `fonts=( ... )` list (index → `Name|Regular.ttf|Bold.ttf`) and run:

```bash
cd fonts
U="U+0020-007E,U+00B0,U+2013,U+2014,U+2022,U+00B7,U+2192"   # ASCII + a little punctuation
fonts=(
"Arimo|Arimo-Regular.ttf|Arimo-Bold.ttf"
"Roboto|Roboto-Regular.ttf|Roboto-Bold.ttf"
"Open Sans|OpenSans-Regular.ttf|OpenSans-Bold.ttf"
"Noto Sans|NotoSans-Regular.ttf|NotoSans-Bold.ttf"
"Source Sans 3|SourceSans3-Regular.ttf|SourceSans3-Bold.ttf"
"IBM Plex Sans|IBMPlexSans-Regular.ttf|IBMPlexSans-Bold.ttf"
"Fira Sans|FiraSans-Regular.ttf|FiraSans-Bold.ttf"
"DejaVu Sans|DejaVuSans.ttf|DejaVuSans-Bold.ttf"
)
OUT=../FontData.h
{ echo "#pragma once"; echo "// Generated. Do not edit by hand."; } > "$OUT"
i=0
for e in "${fonts[@]}"; do rest="${e#*|}"; reg="${rest%%|*}"; bold="${rest##*|}"
  for w in reg bold; do
    src=$([ "$w" = reg ] && echo "$reg" || echo "$bold")
    python -m fontTools.subset "$src" --unicodes="$U" --layout-features='*' \
      --notdef-outline --output-file="_s.ttf" >/dev/null 2>&1
    sym="um_f${i}_${w}"
    xxd -i _s.ttf | sed -e "s/unsigned char .*\[\]/const unsigned char ${sym}[]/" \
                        -e "s/unsigned int .*_len/const unsigned int ${sym}_len/" >> "$OUT"
    rm -f _s.ttf
  done; i=$((i+1))
done
```
If you change the count/order, also update `kFonts[]` (TextRenderer.cpp) and the `ui_font`
`<option>` list (SettingsServer.cpp) to match. Per-face pixel sizes: `facePx()` in
TextRenderer.cpp. ~650 KB flash for all 8.

## Sources (all open-license, fetched via jsdelivr `gh`)
Arimo googlefonts/Arimo · Roboto googlefonts/roboto · Open Sans googlefonts/opensans ·
Noto Sans notofonts/noto-fonts · Source Sans adobe-fonts/source-sans · IBM Plex IBM/plex ·
Fira Sans mozilla/Fira · DejaVu dejavu-fonts.
