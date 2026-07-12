#!/bin/bash
#
# Generates the per-size CJK font headers used by the ENABLE_CHINESE_VERSION
# build. Coverage tiers per point size:
#
#   8/10/12pt (SMALL, UI text)   : 3500 常用字 ∪ i18n   (cn_common_chars.txt)
#   14pt      (LARGE, reader MEDIUM default) : 7000 通用汉字 + 扩展符号
#                                              (chars_7000_common.txt → TC)
#   16/18pt   (I18N, reader LARGE/EXTRA_LARGE) : ~i18n-only chars
#                                                (cn_i18n_chars.txt)
#
# Source face: GenSen Rounded 2 TW Regular (Traditional). Charset lists are
# converted Simplified→Traditional (OpenCC s2t) and deduped so bitmaps store
# one glyph per character. Runtime remaps SC codepoints via ScToTcRemap.h.
#
# Every tier also ships ASCII + Latin-1 + CJK punctuation + full-width forms.
# The 14pt tier additionally ships number forms / enclosed alphanumerics /
# box drawing / block elements / geometric shapes / misc symbols / dingbats
# for richer EPUB rendering at the reader-default size.
#
# Pipeline:
#   0. build_cn_charset.py --traditional + build_sc_to_tc_remap.py
#   1. pyftsubset trims GenSenRounded2TW-R.otf to the requested character set
#   2. fontconvert.py emits a 2-bit raw bitmap header for each point size
#
# Raw bitmaps (no --compress): with 6 simultaneously-loaded CJK fonts each
# group would need ~50 KB scratch, fragmenting the heap on boot and crashing
# FontDecompressor with std::bad_alloc. Latin fonts ship compressed because
# their groups are tiny (~5 KB).
#
# Set PYTHON env var to override the interpreter (e.g. a virtualenv with
# freetype-py and fonttools installed):
#   PYTHON=/path/to/venv/bin/python bash build-cn-builtin-fonts.sh

set -euo pipefail

cd "$(dirname "$0")"

# Project requires fontTools + freetype-py — both Python 3 only. Default to
# python3 (override with PYTHON=/path/to/venv/bin/python if a venv is needed).
PYTHON="${PYTHON:-python3}"

SOURCE_OTF="../builtinFonts/source/GenSenRounded2TW/GenSenRounded2TW-R.otf"
# Frequency-ranked subset produced by build_cn_charset.py. Defaults to the
# top 3500 most common Chinese characters by wordfreq Zipf score, plus every
# CJK ideograph used in the Chinese i18n strings, converted to Traditional.
CHARSET_FILE="cn_common_chars.txt"
# Force-include sources fed to build_cn_charset.py --require-from. Each
# feature that needs CJK glyphs absent from both the 3500 SC pool and the
# natural chinese.yaml STR_ values adds its own cn_<feature>_chars.txt here.
# cn_almanac_chars.txt: ganzhi + lunar-row chars for ChineseCalendarFace.
REQUIRE_FROM=(../../I18n/translations/chinese.yaml cn_almanac_chars.txt)
TMP_DIR="instanced_fonts/GenSenRounded2TW"
SUBSET_OTF="$TMP_DIR/GenSenRounded2TW-R.cncommon.otf"
# Reader-default (14pt) subset: 7000 通用汉字 (Traditionalized) + symbols.
LARGE_OTF="$TMP_DIR/GenSenRounded2TW-R.cn7000.otf"
LARGE_CHARSET_FILE_SC="chars_7000_common.txt"
LARGE_CHARSET_FILE="$TMP_DIR/chars_7000_common_tc.txt"
# Tiny OTF holding only the CJK chars that appear in i18n YAML files.
I18N_OTF="$TMP_DIR/GenSenRounded2TW-R.i18nonly.otf"
I18N_CHARSET_FILE="cn_i18n_chars.txt"

# Font sizes split by character coverage:
CN_FONT_SIZES_SMALL=(8 10 12)
CN_FONT_SIZES_LARGE=(14)
CN_FONT_SIZES_I18N=(16 18)

# Per-size metrics for GenSen Rounded TW (150 DPI, same as fontconvert).
# baseline_adjust: subtract from FreeType ascender (higher → ink rises).
# line_height_adjust: add to FreeType height (pads getLineHeight toward Latin Noto).
#   8/10  — settings rows use itemY+7; GenSen sat high → negative baseline
#   12    — home/apps menu; GenSen sat low → larger positive baseline
#   line height — GenSen's FreeType box is far tighter than Latin Noto Sans at
#     the same pt; pad to match notosans_*_regular advanceY (10pt has no Latin
#     twin → interpolate between 8 and 12).
# Override with CN_BASELINE_ADJUST[_SIZE] / CN_LINE_HEIGHT_ADJUST[_SIZE].
baseline_adjust_for() {
  local size="$1"
  local env_key="CN_BASELINE_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then
    echo "${!env_key}"
    return
  fi
  if [ -n "${CN_BASELINE_ADJUST:-}" ]; then
    echo "$CN_BASELINE_ADJUST"
    return
  fi
  case "$size" in
    8|10) echo -2 ;;
    12) echo 4 ;;
    14|16|18) echo 1 ;;
    *) echo 1 ;;
  esac
}

line_height_adjust_for() {
  local size="$1"
  local env_key="CN_LINE_HEIGHT_ADJUST_${size}"
  if [ -n "${!env_key:-}" ]; then
    echo "${!env_key}"
    return
  fi
  if [ -n "${CN_LINE_HEIGHT_ADJUST:-}" ]; then
    echo "$CN_LINE_HEIGHT_ADJUST"
    return
  fi
  case "$size" in
    8) echo 6 ;;    # 17 → 23 (notosans_8)
    10) echo 7 ;;   # 21 → 28 (between Latin 8/12)
    12) echo 9 ;;   # 25 → 34 (notosans_12)
    14) echo 11 ;;  # 29 → 40 (notosans_14)
    16) echo 12 ;;  # 33 → 45 (notosans_16)
    18) echo 13 ;;  # 38 → 51 (notosans_18)
    *) echo 6 ;;
  esac
}

if [ ! -f "$SOURCE_OTF" ]; then
  echo "Error: $SOURCE_OTF not found." >&2
  echo "Drop GenSenRounded2TW-R.otf into lib/EpdFont/builtinFonts/source/GenSenRounded2TW/." >&2
  exit 1
fi

mkdir -p "$TMP_DIR"

# Step 0a: SC→TC remap table for runtime (committed header).
echo "Generating ScToTcRemap.h..."
"$PYTHON" build_sc_to_tc_remap.py

# Step 0b: refresh cn_common_chars.txt / cn_i18n_chars.txt as Traditional.
# Set SKIP_CHARSET=1 to keep existing charset files as-is.
if [ -z "${SKIP_CHARSET:-}" ]; then
  require_args=()
  for f in "${REQUIRE_FROM[@]}"; do
    require_args+=(--require-from "$f")
  done
  echo "Refreshing $CHARSET_FILE (Traditional, require-from: ${REQUIRE_FROM[*]})..."
  "$PYTHON" build_cn_charset.py --traditional "${require_args[@]}"
fi

if [ ! -f "$CHARSET_FILE" ]; then
  echo "Error: $CHARSET_FILE not found in $(pwd)." >&2
  exit 1
fi

if [ ! -f "$I18N_CHARSET_FILE" ]; then
  echo "Error: $I18N_CHARSET_FILE not found in $(pwd)." >&2
  echo "       Run build_cn_charset.py with at least one --require-from arg," >&2
  echo "       or remove SKIP_CHARSET=1 so Step 0b above runs it for you." >&2
  exit 1
fi

if [ ! -f "$LARGE_CHARSET_FILE_SC" ]; then
  echo "Error: $LARGE_CHARSET_FILE_SC not found in $(pwd)." >&2
  exit 1
fi

# Convert the 7000 通用汉字 pool to Traditional (deduped) for the 14pt subset.
echo "Converting $LARGE_CHARSET_FILE_SC → Traditional..."
"$PYTHON" - <<'PY' "$LARGE_CHARSET_FILE_SC" "$LARGE_CHARSET_FILE"
import sys
from pathlib import Path
from opencc import OpenCC

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
cc = OpenCC("s2t")
raw = [c for c in src.read_text(encoding="utf-8") if not c.isspace()]
out = sorted({cc.convert(c) if len(cc.convert(c)) == 1 else c for c in raw})
dst.write_text("".join(out), encoding="utf-8")
print(f"  {len(raw)} SC → {len(out)} TC → {dst}", file=sys.stderr)
PY

# Step 1a: subset the OTF down to cn_common_chars (TC) + ASCII + Latin-1 +
# CJK punctuation. Used by the 8/10/12pt bitmap headers (UI sizes).
echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$SUBSET_OTF") (small)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$SUBSET_OTF" \
  --text-file="$CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

# Step 1b: subset for 14pt — 7000 TC 通用汉字 + extended symbol coverage.
echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$LARGE_OTF") (large)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$LARGE_OTF" \
  --text-file="$LARGE_CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+2030-205F,U+2070-209F,U+20A0-20CF,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+2500-257F,U+2580-259F,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

# Step 1c: subset down to i18n-only CJK + ASCII + Latin-1 + CJK punctuation.
echo "Subsetting $(basename "$SOURCE_OTF") → $(basename "$I18N_OTF") (i18n)..."
"$PYTHON" -m fontTools.subset "$SOURCE_OTF" \
  --output-file="$I18N_OTF" \
  --text-file="$I18N_CHARSET_FILE" \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2010-2026,U+3000-303F,U+FF00-FFEF,U+FFFD" \
  --layout-features='*' \
  --notdef-outline \
  --recommended-glyphs \
  --no-hinting \
  --drop-tables+=DSIG,GSUB,GPOS

# Step 2: emit one 2-bit raw bitmap header per requested point size.
# Keep output names notosans_cjk_* so main.cpp / all.h stay stable; comments
# in the generated headers record the real GenSen source face.
emit_size() {
  local size="$1"
  local otf="$2"
  shift 2
  local extra_intervals=("$@")
  local font_name="notosans_cjk_${size}"
  local output_path="../builtinFonts/${font_name}.h"
  local tmp_path="${output_path}.tmp"
  local baseline_adjust line_height_adjust
  baseline_adjust="$(baseline_adjust_for "$size")"
  line_height_adjust="$(line_height_adjust_for "$size")"
  echo "Generating ${output_path} from $(basename "$otf") (baseline-adjust=${baseline_adjust}, line-height-adjust=${line_height_adjust})..."
  "$PYTHON" fontconvert.py "$font_name" "$size" "$otf" \
    --2bit \
    --baseline-adjust "$baseline_adjust" \
    --line-height-adjust "$line_height_adjust" \
    --additional-intervals 0x4E00,0x9FFF \
    --additional-intervals 0x3000,0x303F \
    --additional-intervals 0xFF00,0xFFEF \
    ${extra_intervals[@]+"${extra_intervals[@]}"} \
    > "$tmp_path"
  if [ ! -s "$tmp_path" ]; then
    echo "Error: fontconvert.py produced empty $tmp_path for ${font_name}" >&2
    rm -f "$tmp_path"
    exit 1
  fi
  mv "$tmp_path" "$output_path"
  echo "  $(wc -c < "$output_path") bytes ($(grep -E "Bitmaps\[" "$output_path" | head -1))"
}

LARGE_EXTRA_INTERVALS=(
  --additional-intervals 0x2150,0x218F  # Number Forms (½ ⅓ Ⅻ)
  --additional-intervals 0x2460,0x24FF  # Enclosed Alphanumerics (① Ⓐ)
  --additional-intervals 0x2500,0x257F  # Box Drawing
  --additional-intervals 0x2580,0x259F  # Block Elements
  --additional-intervals 0x25A0,0x25FF  # Geometric Shapes (■ ● ★)
  --additional-intervals 0x2600,0x26FF  # Miscellaneous Symbols (☀ ♠ ♥)
  --additional-intervals 0x2700,0x27BF  # Dingbats (✓ ✗ ❀)
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
echo "Done. Generated $((${#CN_FONT_SIZES_SMALL[@]} + ${#CN_FONT_SIZES_LARGE[@]} + ${#CN_FONT_SIZES_I18N[@]})) CJK font headers in ../builtinFonts/"
echo "Runtime SC→TC remap: ../ScToTcRemap.h"
