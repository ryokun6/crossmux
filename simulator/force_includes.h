// Forced into every translation unit for the native desktop simulator.
// Firmware sources often rely on Arduino/ESP headers to pull these in
// transitively; host clang does not, so keep them here instead of patching
// every call site.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
