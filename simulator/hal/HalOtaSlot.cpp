// Host stub for the inactive-OTA scratch partition used by SdCardFontCache.
// Desktop has no dual OTA slots; report Confirmed/unusable so boot continues and
// flash preload safely returns NotSafe / isValidFor=false.

#include <HalOtaSlot.h>

#include <cstring>

HalOtaSlot HalOtaSlot::inactive() { return {}; }

HalOtaSlot::RunningImageState HalOtaSlot::runningImageState() { return RunningImageState::Confirmed; }

bool HalOtaSlot::confirmRunningImage() { return true; }

bool HalOtaSlot::safeForScratchWrite() const { return false; }

bool HalOtaSlot::read(size_t /*offset*/, void* /*data*/, size_t /*length*/) const { return false; }

bool HalOtaSlot::erase(size_t /*offset*/, size_t /*length*/) const { return false; }

bool HalOtaSlot::write(size_t /*offset*/, const void* /*data*/, size_t /*length*/) const { return false; }

bool HalOtaSlot::contains(size_t offset, size_t length) const {
  return valid() && offset <= size_ && length <= size_ - offset;
}
