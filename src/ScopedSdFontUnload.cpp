#include "ScopedSdFontUnload.h"

#include <Arduino.h>
#include <Logging.h>

#include "SdCardFontSystem.h"

// Guards nest: startActivityForResult keeps the parent alive, so a child screen
// (WeRead book cache under the shelf, say) can hold one while its parent still
// needs the font gone. Only the outermost reloads — otherwise the inner
// destructor would hand the font back mid-session and re-starve the parent.
// Single-threaded: every Activity lifecycle call runs on the main loop task.
namespace {
int g_unloadDepth = 0;
}

ScopedSdFontUnload::ScopedSdFontUnload(GfxRenderer& renderer) : renderer_(renderer) {
  if (g_unloadDepth++ > 0) return;
  sdFontSystem.unloadAll(renderer_);
  LOG_DBG("SDFU", "SD reader font dropped for network session (Free=%u MaxAlloc=%u)",
          static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

ScopedSdFontUnload::~ScopedSdFontUnload() {
  if (--g_unloadDepth > 0) return;
  sdFontSystem.ensureLoaded(renderer_);
}
