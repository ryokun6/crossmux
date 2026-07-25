#include "ScopedSdFontUnload.h"

#include <Arduino.h>
#include <Logging.h>

#include "SdCardFontSystem.h"

ScopedSdFontUnload::ScopedSdFontUnload(GfxRenderer& renderer) : renderer_(renderer) {
  sdFontSystem.unloadAll(renderer_);
  LOG_DBG("SDFU", "SD reader font dropped for network session (Free=%u MaxAlloc=%u)",
          static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

ScopedSdFontUnload::~ScopedSdFontUnload() { sdFontSystem.ensureLoaded(renderer_); }
