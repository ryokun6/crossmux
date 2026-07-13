#!/bin/bash
#
# Generates per-size Japanese font headers for ENABLE_JAPANESE_VERSION.
# Coverage tiers (no OpenCC / no Han conversion — Japanese orthography as-is):
#
#   8/10/12pt : 常用漢字 2136 ∪ kana ∪ i18n              (ja_common_chars.txt)
#   14pt      : JIS X 0208 Lv1+Lv2 6355 ∪ joyo ∪ kana ∪ i18n + symbols
#   16/18pt   : i18n + kana only                         (ja_i18n_chars.txt)
#
# Joyo is unioned into the 14pt pool so MEDIUM is a strict superset of the
# minimal set (JIS X 0208 omits 塡/頰 from the 2010 常用漢字表).
#
# Source face: GenSen Rounded 2 JP Regular
#   https://github.com/ButTaiwan/gensen-font  (GenSenRounded2JP-R.otf)
#
# Set PYTHON=/path/to/venv/bin/python to override.

set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"

SOURCE_OTF="../builtinFonts/source/GenSenRounded2JP/GenSenRounded2JP-R.otf"
CHARSET_FILE="ja_common_chars.txt"
REQUIRE_FROM=(../../I18n/translations/japanese.yaml chars_ja_kana.txt)
TMP_DIR="instanced_fonts/GenSenRounded2JP"
SUBSET_OTF="$TMP_DIR/GenSenRounded2JP-R.jacommon.otf"
LARGE_OTF="$TMP_DIR/GenSenRounded2JP-R.jajis.otf"
LARGE_CHARSET_FILE="$TMP_DIR/ja_jis_plus_kana.txt"
I18N_OTF="$TMP_DIR/GenSenRounded2JP-R.i18nonly.otf"
I18N_CHARSET_FILE="ja_i18n_chars.txt"
JOYO_FILE="chars_joyo_2136.txt"
JIS_FILE="chars_jis_l1l2_6355.txt"
KANA_FILE="chars_ja_kana.txt"

JA_FONT_SIZES_SMALL=(8 10 12)
JA_FONT_SIZES_LARGE=(14)
JA_FONT_SIZES_I18N=(16 18)

# Shared GenSen metrics with the Chinese TW build (same family, JP orthography).
baseline_adjust_for() {
  local size="$1"
  local env_key="JA_BASELINE_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then echo "${!env_key}"; return; fi
  if [ -n "${JA_BASELINE_ADJUST:-}" ]; then echo "$JA_BASELINE_ADJUST"; return; fi
  case "$size" in
    8|10) echo -2 ;;
    12) echo 4 ;;
    14|16|18) echo 1 ;;
    *) echo 1 ;;
  esac
}

line_height_adjust_for() {
  local size="$1"
  local env_key="JA_LINE_HEIGHT_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then echo "${!env_key}"; return; fi
  if [ -n "${JA_LINE_HEIGHT_ADJUST:-}" ]; then echo "$JA_LINE_HEIGHT_ADJUST"; return; fi
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
  echo "Drop GenSenRounded2JP-R.otf into lib/EpdFont/builtinFonts/source/GenSenRounded2JP/." >&2
  exit 1
fi

mkdir -p "$TMP_DIR"

if [ -z "${SKIP_CHARSET:-}" ]; then
  require_args=()
  for f in "${REQUIRE_FROM[@]}"; do
    require_args+=(--require-from "$f")
  done
  echo "Refreshing $CHARSET_FILE / $I18N_CHARSET_FILE..."
  "$PYTHON" build_ja_charset.py "${require_args[@]}"
fi

if [ ! -f "$CHARSET_FILE" ] || [ ! -f "$I18N_CHARSET_FILE" ]; then
  echo "Error: charset files missing; run without SKIP_CHARSET=1" >&2
  exit 1
fi

if [ ! -f "$JOYO_FILE" ] || [ ! -f "$JIS_FILE" ] || [ ! -f "$KANA_FILE" ]; then
  echo "Error: $JOYO_FILE, $JIS_FILE, or $KANA_FILE missing." >&2
  exit 1
fi

# 14pt pool: JIS Lv1+Lv2 ∪ 常用漢字 ∪ kana ∪ i18n (no Han conversion).
echo "Building 14pt JIS+Joyo+kana charset..."
"$PYTHON" - <<'PY' "$JIS_FILE" "$JOYO_FILE" "$KANA_FILE" "$I18N_CHARSET_FILE" "$LARGE_CHARSET_FILE"
import sys
from pathlib import Path
parts = []
for p in sys.argv[1:5]:
    parts.extend(c for c in Path(p).read_text(encoding="utf-8") if not c.isspace())
out = sorted(set(parts))
Path(sys.argv[5]).write_text("".join(out), encoding="utf-8")
print(f"  {len(out)} chars → {sys.argv[5]}", file=sys.stderr)
PY

JA_UNICODEM="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+3040-309F,U+30A0-30FF,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD"
JA_UNICODEM_LARGE="U+0020-007E,U+00A0-00FF,U+2010-2026,U+2030-205F,U+2070-209F,U+20A0-20CF,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+2500-257F,U+2580-259F,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+3040-309F,U+30A0-30FF,U+FE10-FE19,U+FE30-FE48,U+FF00-FFEF,U+FFFD"

echo "Subsetting $(basename "$SOURCE_OTF") → small..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$SUBSET_OTF" \
  --text-file="$CHARSET_FILE" \
  --unicodes="$JA_UNICODEM" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") → large (JIS Lv1+Lv2)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$LARGE_OTF" \
  --text-file="$LARGE_CHARSET_FILE" \
  --unicodes="$JA_UNICODEM_LARGE" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

echo "Subsetting $(basename "$SOURCE_OTF") → i18n..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$I18N_OTF" \
  --text-file="$I18N_CHARSET_FILE" \
  --unicodes="$JA_UNICODEM" \
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
  local font_name="notosans_ja_${size}"
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
    --additional-intervals 0x4E00,0x9FFF \
    --additional-intervals 0x3040,0x309F \
    --additional-intervals 0x30A0,0x30FF \
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

for size in "${JA_FONT_SIZES_SMALL[@]}"; do
  emit_size "$size" "$SUBSET_OTF"
done
for size in "${JA_FONT_SIZES_LARGE[@]}"; do
  emit_size "$size" "$LARGE_OTF" "${LARGE_EXTRA_INTERVALS[@]}"
done
for size in "${JA_FONT_SIZES_I18N[@]}"; do
  emit_size "$size" "$I18N_OTF"
done

echo ""
echo "Done. Generated Japanese font headers in ../builtinFonts/"
