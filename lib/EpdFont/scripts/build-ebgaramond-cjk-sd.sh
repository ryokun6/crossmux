#!/bin/bash
#
# Build EB Garamond + Source Han Serif TC .cpfont files for SD card use.
#
# Includes full base CJK Unified Ideographs, CJK compatibility ideographs,
# Hiragana/Katakana, full Greek, and a small book-derived supplement.
#
# Layout:
#   Latin  — EB Garamond Regular / Bold / Italic / BoldItalic
#   CJK    — Source Han Serif TC Regular only (embedded in the regular style;
#            bold/italic faces fall back to regular for Han at runtime)
#
# Output: lib/EpdFont/scripts/output/EBGaramondSHS7000/EBGaramondSHS7000_{12,14,16,18}.cpfont
#
# Prerequisites:
#   pip install freetype-py fonttools OpenCC
#
# Override paths with env vars if your fonts live elsewhere:
#   EB_GARAMOND_DIR=/path/to/EB_Garamond/static
#   SHS_TC_DIR=/path/to/SourceHanSerifTC/OTF/TraditionalChinese
#   PYTHON=/path/to/venv/bin/python bash build-ebgaramond-cjk-sd.sh

set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"
FONTCONVERT="$PWD/fontconvert_sdcard.py"

EB_GARAMOND_DIR="${EB_GARAMOND_DIR:-$HOME/Downloads/EB_Garamond/static}"
SHS_TC_DIR="${SHS_TC_DIR:-$HOME/Downloads/10_SourceHanSerifTC/OTF/TraditionalChinese}"

EB_REGULAR="$EB_GARAMOND_DIR/EBGaramond-Regular.ttf"
EB_BOLD="$EB_GARAMOND_DIR/EBGaramond-Bold.ttf"
EB_ITALIC="$EB_GARAMOND_DIR/EBGaramond-Italic.ttf"
EB_BOLDITALIC="$EB_GARAMOND_DIR/EBGaramond-BoldItalic.ttf"

SHS_REGULAR="$SHS_TC_DIR/SourceHanSerifTC-Regular.otf"

FONT_NAME="${FONT_NAME:-EBGaramondSHS7000}"
TMP_DIR="instanced_fonts/EBGaramondSHS7000"
OUTPUT_DIR="output/$FONT_NAME"

LARGE_CHARSET_FILE_SC="chars_7000_common.txt"
EXTRA_CHARSET_FILE="${EXTRA_CHARSET_FILE:-chars_ebgaramond_extra.txt}"
LARGE_CHARSET_FILE="$TMP_DIR/chars_7000_common_tc.txt"
SHS_SUBSET_REGULAR="$TMP_DIR/SourceHanSerifTC-Regular.cn7000.otf"

# Full base CJK + kana are intentionally SD-only; unlike firmware-embedded fonts,
# .cpfont assets are not constrained by the OTA app partition.
SUBSET_UNICODES="U+0020-007E,U+00A0-00FF,U+0370-03FF,U+1F00-1FFF,U+2010-2026,U+2030-205F,U+2070-209F,U+20A0-20CF,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+2500-257F,U+2580-259F,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+3040-30FF,U+4E00-9FFF,U+F900-FAFF,U+FE10-FE19,U+FE30-FE48,U+FE50-FE6F,U+FF00-FFEF,U+FFFD"
LATIN_INTERVALS="latin-ext,greek,symbols,(0x0413-0x0413),(0x2030-0x205F),(0x2122-0x2122),(0x2460-0x24FF),(0x2580-0x259F)"
REGULAR_INTERVALS="latin-ext,greek,cjk,symbols,(0x0413-0x0413),(0x2030-0x205F),(0x2122-0x2122),(0x2460-0x24FF),(0x2580-0x259F),(0x3100-0x312F),(0xFE10-0xFE19),(0xFE30-0xFE48),(0xFE50-0xFE6F)"

for f in "$EB_REGULAR" "$EB_BOLD" "$EB_ITALIC" "$EB_BOLDITALIC" "$SHS_REGULAR" "$LARGE_CHARSET_FILE_SC" \
  "$EXTRA_CHARSET_FILE"; do
  if [ ! -f "$f" ]; then
    echo "Error: required file not found: $f" >&2
    exit 1
  fi
done

mkdir -p "$TMP_DIR" "$OUTPUT_DIR"

echo "Converting $LARGE_CHARSET_FILE_SC + $EXTRA_CHARSET_FILE → Traditional..."
"$PYTHON" - <<'PY' "$LARGE_CHARSET_FILE_SC" "$LARGE_CHARSET_FILE" "$EXTRA_CHARSET_FILE"
import sys
from pathlib import Path
from opencc import OpenCC

src, dst, extra = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
cc = OpenCC("s2t")
raw = [c for c in src.read_text(encoding="utf-8") + extra.read_text(encoding="utf-8") if not c.isspace()]
out = sorted({cc.convert(c) if len(cc.convert(c)) == 1 else c for c in raw})
dst.write_text("".join(out), encoding="utf-8")
print(f"  {len(raw)} base+extra → {len(out)} TC → {dst}", file=sys.stderr)
PY

echo "Subsetting $(basename "$SHS_REGULAR") → $(basename "$SHS_SUBSET_REGULAR")..."
"$PYTHON" -m fontTools.subset "$SHS_REGULAR" \
  --output-file="$SHS_SUBSET_REGULAR" \
  --text-file="$LARGE_CHARSET_FILE" \
  --unicodes="$SUBSET_UNICODES" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Generating .cpfont files into $OUTPUT_DIR ..."
# Regular: Latin + CJK (SHS Regular fallback). B/I/BI: Latin only.
"$PYTHON" "$FONTCONVERT" \
  --regular "$EB_REGULAR" \
  --bold "$EB_BOLD" \
  --italic "$EB_ITALIC" \
  --bolditalic "$EB_BOLDITALIC" \
  --fallback-regular "$SHS_SUBSET_REGULAR" \
  --intervals "$LATIN_INTERVALS" \
  --regular-intervals "$REGULAR_INTERVALS" \
  --sizes 12,14,16,18 \
  --name "$FONT_NAME" \
  --output-dir "$OUTPUT_DIR/"

echo ""
echo "Done. Install on SD card:"
echo "  mkdir -p <sd>/.fonts/$FONT_NAME"
echo "  cp $OUTPUT_DIR/${FONT_NAME}_*.cpfont <sd>/.fonts/$FONT_NAME/"
ls -lh "$OUTPUT_DIR"/*.cpfont
