#!/bin/bash
#
# Generates Simplified-Chinese (SC) per-size CJK font headers for
# CHINESE_UI_SIMPLIFIED builds. Coverage tiers match the Traditional pipeline
# but codepoints stay Simplified (no --traditional) and the source face is
# Source Han Sans CN Regular.
#
# Output headers: notosans_sc_{8,10,12,14,16,18}.h
# Also regenerates TcToScRemap.h.
#
#   PYTHON=/path/to/venv/bin/python bash build-sc-builtin-fonts.sh

set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"

SOURCE_OTF="../builtinFonts/source/SourceHanSansCN/SourceHanSansCN-Regular.otf"
CHARSET_FILE="sc_common_chars.txt"
I18N_CHARSET_FILE="sc_i18n_chars.txt"
# Force-include sources (Traditional YAML is t2s'd inside build_cn_charset for SC).
REQUIRE_FROM=(../../I18n/translations/chinese.yaml cn_almanac_chars.txt cn_weread_chars.txt)
TMP_DIR="instanced_fonts/SourceHanSansCN"
SUBSET_OTF="$TMP_DIR/SourceHanSansCN-Regular.sccommon.otf"
LARGE_OTF="$TMP_DIR/SourceHanSansCN-Regular.sc7000.otf"
LARGE_CHARSET_FILE_SC="chars_7000_common.txt"
LARGE_CHARSET_FILE="$TMP_DIR/chars_7000_common_sc.txt"
I18N_OTF="$TMP_DIR/SourceHanSansCN-Regular.i18nonly.otf"

CN_FONT_SIZES_SMALL=(8 10 12)
CN_FONT_SIZES_LARGE=(14)
CN_FONT_SIZES_I18N=(16 18)

baseline_adjust_for() {
  local size="$1"
  case "$size" in
    8|10) echo -2 ;;
    12) echo 4 ;;
    14|16|18) echo 1 ;;
    *) echo 1 ;;
  esac
}

line_height_adjust_for() {
  local size="$1"
  case "$size" in
    8) echo 6 ;;
    10) echo 7 ;;
    12) echo 9 ;;
    14) echo 11 ;;
    16) echo 12 ;;
    18) echo 13 ;;
    *) echo 6 ;;
  esac
}

if [ ! -f "$SOURCE_OTF" ]; then
  echo "Error: $SOURCE_OTF not found." >&2
  echo "Drop SourceHanSansCN-Regular.otf into lib/EpdFont/builtinFonts/source/SourceHanSansCN/." >&2
  exit 1
fi

mkdir -p "$TMP_DIR"

echo "Generating TcToScRemap.h..."
"$PYTHON" build_tc_to_sc_remap.py

if [ -z "${SKIP_CHARSET:-}" ]; then
  require_args=()
  for f in "${REQUIRE_FROM[@]}"; do
    require_args+=(--require-from "$f")
  done
  # Build SC charsets: same pools as CN, but WITHOUT --traditional, and
  # t2s any Traditional-only chars pulled from chinese.yaml require-from.
  echo "Refreshing $CHARSET_FILE / $I18N_CHARSET_FILE (Simplified)..."
  "$PYTHON" build_cn_charset.py \
    --output "$CHARSET_FILE" \
    --i18n-output "$I18N_CHARSET_FILE" \
    "${require_args[@]}"
  # chinese.yaml is Traditional; force-include chars must be Simplified for SC fonts.
  "$PYTHON" - <<'PY' "$CHARSET_FILE" "$I18N_CHARSET_FILE"
import sys
from pathlib import Path
from opencc import OpenCC
cc = OpenCC("t2s")
for path in map(Path, sys.argv[1:]):
    raw = [c for c in path.read_text(encoding="utf-8") if not c.isspace()]
    out = sorted({cc.convert(c) if len(cc.convert(c)) == 1 else c for c in raw})
    path.write_text("".join(out), encoding="utf-8")
    print(f"  t2s {path.name}: {len(raw)} → {len(out)}", file=sys.stderr)
PY
fi

if [ ! -f "$CHARSET_FILE" ] || [ ! -f "$I18N_CHARSET_FILE" ]; then
  echo "Error: missing $CHARSET_FILE or $I18N_CHARSET_FILE" >&2
  exit 1
fi

# 14pt: keep Simplified 7000 pool as-is (already SC).
cp "$LARGE_CHARSET_FILE_SC" "$LARGE_CHARSET_FILE"

echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$SUBSET_OTF") (small)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$SUBSET_OTF" \
  --text-file="$CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$LARGE_OTF") (large)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$LARGE_OTF" \
  --text-file="$LARGE_CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+2030-205F,U+2070-209F,U+20A0-20CF,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+2500-257F,U+2580-259F,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$I18N_OTF") (i18n)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$I18N_OTF" \
  --text-file="$I18N_CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

emit_size() {
  local size="$1"
  local otf="$2"
  shift 2
  local extra_intervals=("$@")
  # Keep the C symbol name notosans_cjk_* so main.cpp stays shared across
  # TW/SC SKUs; only the header filename differs (notosans_sc_*.h).
  local font_name="notosans_cjk_${size}"
  local output_path="../builtinFonts/notosans_sc_${size}.h"
  local tmp_path="${output_path}.tmp"
  local baseline_adjust line_height_adjust
  baseline_adjust="$(baseline_adjust_for "$size")"
  line_height_adjust="$(line_height_adjust_for "$size")"
  echo "Generating ${output_path} (symbol ${font_name}) from $(basename "$otf")..."
  "$PYTHON" fontconvert.py "$font_name" "$size" "$otf" \
    --2bit \
    --baseline-adjust "$baseline_adjust" \
    --line-height-adjust "$line_height_adjust" \
    --additional-intervals 0x4E00,0x9FFF \
    --additional-intervals 0x3000,0x303F \
    --additional-intervals 0xFE10,0xFE19 \
    --additional-intervals 0xFE30,0xFE48 \
    --additional-intervals 0xFF00,0xFFEF \
    ${extra_intervals[@]+"${extra_intervals[@]}"} \
    > "$tmp_path"
  if [ ! -s "$tmp_path" ]; then
    echo "Error: fontconvert.py produced empty $tmp_path for ${font_name}" >&2
    rm -f "$tmp_path"
    exit 1
  fi
  mv "$tmp_path" "$output_path"
  echo "  $(wc -c < "$output_path") bytes"
}

LARGE_EXTRA_INTERVALS=(
  --additional-intervals 0x2150,0x218F
  --additional-intervals 0x2460,0x24FF
  --additional-intervals 0x2500,0x257F
  --additional-intervals 0x2580,0x259F
  --additional-intervals 0x25A0,0x25FF
  --additional-intervals 0x2600,0x26FF
  --additional-intervals 0x2700,0x27BF
)

for size in "${CN_FONT_SIZES_SMALL[@]}"; do
  emit_size "$size" "$SUBSET_OTF"
done
for size in "${CN_FONT_SIZES_LARGE[@]}"; do
  emit_size "$size" "$LARGE_OTF" "${LARGE_EXTRA_INTERVALS[@]}"
done
for size in "${CN_FONT_SIZES_I18N[@]}"; do
  emit_size "$size" "$I18N_OTF"
done

echo ""
echo "Done. Generated SC CJK font headers in ../builtinFonts/"
echo "Runtime TC→SC remap: ../TcToScRemap.h"
