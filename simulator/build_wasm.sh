#!/usr/bin/env bash
# Build the CrossPoint Reader simulator as WebAssembly (Emscripten).
#
# Output: build_wasm/crosspoint_simulator_wasm.{js,wasm,data}
# These are the artifacts the crosspoint-web site serves under public/simulator/.
#
# Prerequisites: emsdk installed + activated. Point EMSDK at it (defaults to ~/emsdk):
#   EMSDK=/path/to/emsdk simulator/build_wasm.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${EMSDK:=$HOME/emsdk}"

if [[ ! -f "$EMSDK/emsdk_env.sh" ]]; then
  echo "error: emsdk not found at \$EMSDK=$EMSDK" >&2
  echo "       install it: git clone https://github.com/emscripten-core/emsdk && cd emsdk && ./emsdk install latest && ./emsdk activate latest" >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1

BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build_wasm}"
emcmake cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo
echo "Artifacts:"
ls -la "$BUILD_DIR"/crosspoint_simulator_wasm.* 2>/dev/null || echo "  (build produced no crosspoint_simulator_wasm.* — check errors above)"
