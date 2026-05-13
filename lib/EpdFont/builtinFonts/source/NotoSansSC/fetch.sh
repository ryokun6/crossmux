#!/bin/bash
# Fetch the Noto Sans SC Regular source font used to generate CN built-in
# font headers. The OTF is not committed to git (8 MB, OFL-1.1).
#
# Usage: bash lib/EpdFont/builtinFonts/source/NotoSansSC/fetch.sh
#
# Re-run if the file is missing locally or the SHA-256 below changes.

set -e
cd "$(dirname "$0")"

URL="https://github.com/kartotherian/osm-bright.fonts/raw/master/fonts/NotoSansSC-Regular.otf"
EXPECTED_SHA256="8c37936063c7c8ab747a939e13833894f9edc80dd41b98874ca8f3938a33c32f"
OUT="NotoSansSC-Regular.otf"

if [ -f "$OUT" ]; then
  actual=$(shasum -a 256 "$OUT" | awk '{print $1}')
  if [ "$actual" = "$EXPECTED_SHA256" ]; then
    echo "[fetch.sh] $OUT already present and SHA-256 matches"
    exit 0
  fi
  echo "[fetch.sh] $OUT exists but SHA-256 mismatch (have $actual, want $EXPECTED_SHA256); refetching"
  rm -f "$OUT"
fi

echo "[fetch.sh] Downloading $URL"
curl -fsSL --max-time 180 -o "$OUT" "$URL"

actual=$(shasum -a 256 "$OUT" | awk '{print $1}')
if [ "$actual" != "$EXPECTED_SHA256" ]; then
  echo "[fetch.sh] ERROR: SHA-256 mismatch after download"
  echo "[fetch.sh]   have $actual"
  echo "[fetch.sh]   want $EXPECTED_SHA256"
  rm -f "$OUT"
  exit 1
fi

echo "[fetch.sh] Fetched $OUT ($(wc -c < "$OUT") bytes), SHA-256 verified"
