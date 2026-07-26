#include "InflateReader.h"

#include <Logging.h>

#include <cstring>
#include <type_traits>

#if defined(ESP32) || defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {
constexpr size_t INFLATE_DICT_SIZE = 32768;

// Kept after the first successful streaming inflate in a reader session so
// mid-book section rebuilds do not need a fresh 32KB MaxAlloc (X3 often sits
// at ~32756 once the SD font is warm — 12 bytes short of a new malloc).
uint8_t* sharedRing = nullptr;
int sharedRingUsers = 0;
}  // namespace

// Guarantee the cast pattern in the header comment is valid.
static_assert(std::is_standard_layout<InflateReader>::value,
              "InflateReader must be standard-layout for the uzlib callback cast to work");

InflateReader::~InflateReader() { deinit(); }

bool InflateReader::acquireSharedDictionary() {
  if (sharedRing != nullptr) {
    return true;
  }
  sharedRing = static_cast<uint8_t*>(malloc(INFLATE_DICT_SIZE));
  if (sharedRing == nullptr) {
#if defined(ESP32) || defined(ARDUINO)
    LOG_ERR("INF", "Shared inflate ring OOM (%u bytes) Free=%u MaxAlloc=%u", static_cast<unsigned>(INFLATE_DICT_SIZE),
            static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
#else
    LOG_ERR("INF", "Shared inflate ring OOM (%u bytes)", static_cast<unsigned>(INFLATE_DICT_SIZE));
#endif
    return false;
  }
  LOG_DBG("INF", "Pinned shared %u-byte inflate dictionary", static_cast<unsigned>(INFLATE_DICT_SIZE));
  return true;
}

void InflateReader::releaseSharedDictionary() {
  if (sharedRingUsers > 0) {
    LOG_ERR("INF", "releaseSharedDictionary while %d users still active", sharedRingUsers);
    return;
  }
  free(sharedRing);
  sharedRing = nullptr;
}

bool InflateReader::init(const bool streaming) {
  deinit();

  if (streaming) {
    if (sharedRing != nullptr && sharedRingUsers == 0) {
      ringBuffer = sharedRing;
      ownsRing = false;
      sharedRingUsers = 1;
    } else {
      ringBuffer = static_cast<uint8_t*>(malloc(INFLATE_DICT_SIZE));
      ownsRing = true;
      if (ringBuffer == nullptr) {
#if defined(ESP32) || defined(ARDUINO)
        LOG_ERR("INF", "Inflate ring OOM (%u bytes) Free=%u MaxAlloc=%u shared=%d",
                static_cast<unsigned>(INFLATE_DICT_SIZE), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()), sharedRing != nullptr ? 1 : 0);
#endif
        return false;
      }
    }
    memset(ringBuffer, 0, INFLATE_DICT_SIZE);
  }

  uzlib_uncompress_init(&decomp, ringBuffer, ringBuffer ? INFLATE_DICT_SIZE : 0);
  return true;
}

void InflateReader::deinit() {
  if (ringBuffer != nullptr) {
    if (ownsRing) {
      // Do not promote into a session-long pin: a resident 32KB ring caps MaxAlloc
      // below CJK glyph mini-bitmap size (~28–34KB) and forces per-glyph SD overflow
      // on every page flip. createSectionFile acquires/releases explicitly instead.
      free(ringBuffer);
    } else if (sharedRingUsers > 0) {
      sharedRingUsers--;
    }
    ringBuffer = nullptr;
    ownsRing = false;
  }
  memset(&decomp, 0, sizeof(decomp));
}

void InflateReader::setSource(const uint8_t* src, size_t len) {
  decomp.source = src;
  decomp.source_limit = src + len;
}

void InflateReader::setReadCallback(int (*cb)(struct uzlib_uncomp*)) { decomp.source_read_cb = cb; }

void InflateReader::skipZlibHeader() {
  uzlib_get_byte(&decomp);
  uzlib_get_byte(&decomp);
}

bool InflateReader::read(uint8_t* dest, size_t len) {
  if (!ringBuffer) {
    decomp.dest_start = dest;
  }
  decomp.dest = dest;
  decomp.dest_limit = dest + len;

  const int res = uzlib_uncompress(&decomp);
  if (res < 0) return false;
  return decomp.dest == decomp.dest_limit;
}

InflateStatus InflateReader::readAtMost(uint8_t* dest, size_t maxLen, size_t* produced) {
  if (!ringBuffer) {
    decomp.dest_start = dest;
  }
  decomp.dest = dest;
  decomp.dest_limit = dest + maxLen;

  const int res = uzlib_uncompress(&decomp);
  *produced = static_cast<size_t>(decomp.dest - dest);

  if (res == TINF_DONE) return InflateStatus::Done;
  if (res < 0) return InflateStatus::Error;
  return InflateStatus::Ok;
}
