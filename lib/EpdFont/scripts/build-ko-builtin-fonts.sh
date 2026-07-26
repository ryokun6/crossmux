#!/bin/bash
#
# Generates per-size Korean font headers for ENABLE_KOREAN_VERSION.
# Coverage tiers (no OpenCC / no Han conversion):
#
#   8/10/12pt : KS X 1001 완성형 2350 common syllables ∪ i18n ∪ modern jamo
#               (ko_ui_chars.txt) — UI chrome + SMALL reader. Covers arbitrary
#               Korean book/file titles in lists at a fraction of the cost of
#               full Hangul (full Hangul at 8/10/12 as well as 14 overflows
#               the 0x640000 OTA slot by ~1.5 MiB).
#   14pt      : ALL modern Hangul (11 172) ∪ 기초 한자 1800 ∪ jamo ∪ i18n
#               + extended EPUB symbols (reader MEDIUM default)
#   16/18pt   : i18n + modern jamo only (ko_i18n_chars.txt)
#
# Source face: Resource Han Rounded KR Regular
#   https://github.com/CyanoHao/Resource-Han-Rounded
#   (ResourceHanRoundedKR-Regular.ttf)
#
# Hangul syllables are listed explicitly in chars_ko_hangul_all.txt (fed via
# --text-file). Do not add a bare U+AC00–D7A3 range to --unicodes for the
# i18n-only OTF — that would inflate 8/10/12/16/18pt. Do not list full
# U+1100–11FF in --unicodes (pulls obsolete jamo).
#
# Set PYTHON=/path/to/venv/bin/python to override.

set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"

SOURCE_OTF="../builtinFonts/source/ResourceHanRoundedKR/ResourceHanRoundedKR-Regular.ttf"
CHARSET_FILE="ko_common_chars.txt"
REQUIRE_FROM=(../../I18n/translations/korean.yaml chars_ko_jamo.txt)
TMP_DIR="instanced_fonts/ResourceHanRoundedKR"
LARGE_OTF="$TMP_DIR/ResourceHanRoundedKR-R.kolarge.otf"
I18N_OTF="$TMP_DIR/ResourceHanRoundedKR-R.i18nonly.otf"
I18N_CHARSET_FILE="ko_i18n_chars.txt"
UI_OTF="$TMP_DIR/ResourceHanRoundedKR-R.koui.otf"
UI_CHARSET_FILE="ko_ui_chars.txt"

KO_FONT_SIZES_LARGE=(14)
KO_FONT_SIZES_UI=(8 10 12)
KO_FONT_SIZES_I18N=(16 18)

# Resource Han Rounded is Source-Han-derived like GenSen; reuse GenSen-tuned metrics.
baseline_adjust_for() {
  local size="$1"
  local env_key="KO_BASELINE_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then echo "${!env_key}"; return; fi
  if [ -n "${KO_BASELINE_ADJUST:-}" ]; then echo "$KO_BASELINE_ADJUST"; return; fi
  case "$size" in
    8|10) echo -2 ;;
    12) echo 4 ;;
    14|16|18) echo 1 ;;
    *) echo 1 ;;
  esac
}

line_height_adjust_for() {
  local size="$1"
  local env_key="KO_LINE_HEIGHT_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then echo "${!env_key}"; return; fi
  if [ -n "${KO_LINE_HEIGHT_ADJUST:-}" ]; then echo "$KO_LINE_HEIGHT_ADJUST"; return; fi
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
  echo "Drop ResourceHanRoundedKR-Regular.ttf into lib/EpdFont/builtinFonts/source/ResourceHanRoundedKR/." >&2
  exit 1
fi

mkdir -p "$TMP_DIR"

if [ -z "${SKIP_CHARSET:-}" ]; then
  require_args=()
  for f in "${REQUIRE_FROM[@]}"; do
    require_args+=(--require-from "$f")
  done
  echo "Refreshing $CHARSET_FILE / $I18N_CHARSET_FILE..."
  "$PYTHON" build_ko_charset.py "${require_args[@]}"
fi

if [ ! -f "$CHARSET_FILE" ] || [ ! -f "$UI_CHARSET_FILE" ] || [ ! -f "$I18N_CHARSET_FILE" ]; then
  echo "Error: charset files missing; run without SKIP_CHARSET=1" >&2
  exit 1
fi

# ASCII / Latin-1 / punctuation / fullwidth / compatibility jamo.
# Modern Hangul syllables + Hanja + modern combining jamo come from --text-file
# (chars_ko_hangul_all / hanja_1800 / jamo). Do NOT list U+1100–11FF here — that
# range includes obsolete jamo the SKU intentionally omits.
KO_UNICODEM="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+3131-318E,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD"
KO_UNICODEM_LARGE="U+0020-007E,U+00A0-00FF,U+2010-2026,U+2030-205F,U+2070-209F,U+20A0-20CF,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+2500-257F,U+2580-259F,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+3131-318E,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD"

echo "Subsetting $(basename "$SOURCE_OTF") -> large (all Hangul + Hanja 1800 + symbols)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$LARGE_OTF" \
  --text-file="$CHARSET_FILE" \
  --unicodes="$KO_UNICODEM_LARGE" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") -> i18n..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$I18N_OTF" \
  --text-file="$I18N_CHARSET_FILE" \
  --unicodes="$KO_UNICODEM" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") -> ui (KS X 1001 + i18n)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$UI_OTF" \
  --text-file="$UI_CHARSET_FILE" \
  --unicodes="$KO_UNICODEM" \
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
  local font_name="notosans_ko_${size}"
  local output_path="../builtinFonts/${font_name}.h"
  local tmp_path="${output_path}.tmp"
  local baseline_adjust line_height_adjust
  baseline_adjust="$(baseline_adjust_for "$size")"
  line_height_adjust="$(line_height_adjust_for "$size")"
  echo "Generating ${output_path} from $(basename "$otf")..."
  "$PYTHON" fontconvert.py "$font_name" "$size" "$otf" \
    --2bit \
    --baseline-adjust "$baseline_adjust" \
    --line-height-adjust "$line_height_adjust" \
    --additional-intervals 0xAC00,0xD7A3 \
    --additional-intervals 0x1100,0x1112 \
    --additional-intervals 0x1161,0x1175 \
    --additional-intervals 0x11A8,0x11C2 \
    --additional-intervals 0x3131,0x318E \
    --additional-intervals 0x4E00,0x9FFF \
    --additional-intervals 0x3000,0x303F \
    --additional-intervals 0xFE10,0xFE19 \
    --additional-intervals 0xFE30,0xFE48 \
    --additional-intervals 0xFF00,0xFFEF \
    ${extra_intervals[@]+"${extra_intervals[@]}"} \
    > "$tmp_path"
  if [ ! -s "$tmp_path" ]; then
    echo "Error: empty $tmp_path" >&2
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

for size in "${KO_FONT_SIZES_LARGE[@]}"; do
  emit_size "$size" "$LARGE_OTF" "${LARGE_EXTRA_INTERVALS[@]}"
done
for size in "${KO_FONT_SIZES_UI[@]}"; do
  emit_size "$size" "$UI_OTF"
done
for size in "${KO_FONT_SIZES_I18N[@]}"; do
  emit_size "$size" "$I18N_OTF"
done

echo ""
echo "Done. Generated Korean font headers in ../builtinFonts/"
