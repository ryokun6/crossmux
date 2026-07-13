#include "EpdFont.h"

#include <Utf8.h>

#include <algorithm>

#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
#include "TcToScRemap.h"
#else
#include "ScToTcRemap.h"
#endif
#endif

namespace {

uint32_t resolveCnCodepoint(uint32_t cp) {
#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
  // SC fonts store Simplified glyphs; Traditional EPUB codepoints remap here.
  return mapTraditionalToSimplified(cp);
#else
  // Builtin CJK fonts store Traditional glyphs only; UI/EPUB may still pass
  // Simplified codepoints. Remap without duplicating bitmaps.
  return mapSimplifiedToTraditional(cp);
#endif
#else
  return cp;
#endif
}

}  // namespace

void EpdFont::getTextBounds(const char* string, const int startX, const int startY, int* minX, int* minY, int* maxX,
                            int* maxY) const {
  *minX = startX;
  *minY = startY;
  *maxX = startX;
  *maxY = startY;

  if (*string == '\0') {
    return;
  }

  int lastBaseX = startX;
  int lastBaseLeft = 0;
  int lastBaseWidth = 0;
  int lastBaseTop = 0;
  int32_t prevAdvanceFP = 0;  // 12.4 fixed-point: prev glyph's advance + next kern for snap
  uint32_t cp;
  uint32_t prevCp = 0;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&string)))) {
    const bool isCombining = utf8IsCombiningMark(cp);

    if (!isCombining) {
      cp = applyLigatures(cp, string);
    }

    const EpdGlyph* glyph = getGlyph(cp);
    if (!glyph) {
      // Keep cursor movement stable when a base glyph is missing, but don't attach subsequent
      // combining marks to stale base metrics.
      if (!isCombining) {
        lastBaseX += fp4::toPixel(prevAdvanceFP);  // flush pending advance before resetting
        prevCp = 0;
        prevAdvanceFP = 0;
        lastBaseLeft = 0;
        lastBaseWidth = 0;
        lastBaseTop = 0;
      }
      continue;
    }

    const int raiseBy = isCombining ? combiningMark::raiseAboveBase(glyph->top, glyph->height, lastBaseTop) : 0;

    if (!isCombining && prevCp != 0) {
      const auto kernFP = getKerning(prevCp, cp);  // 4.4 fixed-point kern
      lastBaseX += fp4::toPixel(prevAdvanceFP + kernFP);
    }

    const int glyphBaseX =
        isCombining ? combiningMark::centerOver(lastBaseX, lastBaseLeft, lastBaseWidth, glyph->left, glyph->width)
                    : lastBaseX;
    const int glyphBaseY = startY - raiseBy;

    *minX = std::min(*minX, glyphBaseX + glyph->left);
    *maxX = std::max(*maxX, glyphBaseX + glyph->left + glyph->width);
    *minY = std::min(*minY, glyphBaseY + glyph->top - glyph->height);
    *maxY = std::max(*maxY, glyphBaseY + glyph->top);

    if (!isCombining) {
      lastBaseLeft = glyph->left;
      lastBaseWidth = glyph->width;
      lastBaseTop = glyph->top;
      prevAdvanceFP = glyph->advanceX;  // 12.4 fixed-point
      prevCp = cp;
    }
  }
}

void EpdFont::getTextDimensions(const char* string, int* w, int* h) const {
  int minX = 0, minY = 0, maxX = 0, maxY = 0;

  getTextBounds(string, 0, 0, &minX, &minY, &maxX, &maxY);

  *w = maxX - minX;
  *h = maxY - minY;
}

static uint8_t lookupKernClass(const EpdKernClassEntry* entries, const uint16_t count, const uint32_t cp) {
  if (!entries || count == 0 || cp > 0xFFFF) {
    return 0;
  }

  const auto target = static_cast<uint16_t>(cp);
  const auto* end = entries + count;

  // lower_bound: exact-key lookup. Finds the first entry with codepoint >= target,
  // then the equality check confirms an exact match exists.
  const auto it = std::lower_bound(
      entries, end, target, [](const EpdKernClassEntry& entry, uint16_t value) { return entry.codepoint < value; });

  if (it != end && it->codepoint == target) {
    return it->classId;
  }

  return 0;
}

int8_t EpdFont::getKerning(const uint32_t leftCp, const uint32_t rightCp) const {
  const uint32_t left = resolveCnCodepoint(leftCp);
  const uint32_t right = resolveCnCodepoint(rightCp);
  if (utf8IsCjkBreakable(left) || utf8IsCjkBreakable(right)) {
    return 0;
  }
  if (!data->kernMatrix) {
    return 0;
  }
  const uint8_t lc = lookupKernClass(data->kernLeftClasses, data->kernLeftEntryCount, left);
  if (lc == 0) return 0;
  const uint8_t rc = lookupKernClass(data->kernRightClasses, data->kernRightEntryCount, right);
  if (rc == 0) return 0;
  return data->kernMatrix[(lc - 1) * data->kernRightClassCount + (rc - 1)];
}

uint32_t EpdFont::getLigature(const uint32_t leftCp, const uint32_t rightCp) const {
  const auto* pairs = data->ligaturePairs;
  const auto count = data->ligaturePairCount;
  if (!pairs || count == 0 || leftCp > 0xFFFF || rightCp > 0xFFFF) {
    return 0;
  }
  const uint32_t left = resolveCnCodepoint(leftCp);
  const uint32_t right = resolveCnCodepoint(rightCp);
  if (left > 0xFFFF || right > 0xFFFF) {
    return 0;
  }

  const uint32_t key = (left << 16) | right;
  const auto* end = pairs + count;

  // lower_bound: exact-key lookup. Finds the first entry with pair >= key,
  // then the equality check confirms an exact match exists.
  const auto it =
      std::lower_bound(pairs, end, key, [](const EpdLigaturePair& pair, uint32_t value) { return pair.pair < value; });

  if (it != end && it->pair == key) {
    return it->ligatureCp;
  }

  return 0;
}

uint32_t EpdFont::applyLigatures(uint32_t cp, const char*& text) const {
  if (!data->ligaturePairs || data->ligaturePairCount == 0) {
    return cp;
  }
  while (true) {
    const auto saved = reinterpret_cast<const uint8_t*>(text);
    const uint32_t nextCp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&text));
    if (nextCp == 0) break;
    const uint32_t lig = getLigature(cp, nextCp);
    if (lig == 0) {
      text = reinterpret_cast<const char*>(saved);
      break;
    }
    cp = lig;
  }
  return cp;
}

const EpdGlyph* EpdFont::getGlyphNoReplacement(const uint32_t cpIn) const {
  const uint32_t cp = resolveCnCodepoint(cpIn);
  const int count = data->intervalCount;
  if (count == 0 && !data->glyphMissHandler) return nullptr;

  if (count > 0) {
    const EpdUnicodeInterval* intervals = data->intervals;
    const auto* end = intervals + count;
    const auto it = std::upper_bound(
        intervals, end, cp, [](uint32_t value, const EpdUnicodeInterval& interval) { return value < interval.first; });
    if (it != intervals) {
      const auto& interval = *(it - 1);
      if (cp <= interval.last) {
        return &data->glyph[interval.offset + (cp - interval.first)];
      }
    }
  }

  if (data->glyphMissHandler) {
    return data->glyphMissHandler(data->glyphMissCtx, cp);
  }
  return nullptr;
}

const EpdGlyph* EpdFont::getGlyph(const uint32_t cpIn) const {
  const EpdGlyph* glyph = getGlyphNoReplacement(cpIn);
  if (glyph) return glyph;

  const uint32_t cp = resolveCnCodepoint(cpIn);
  if (cp != REPLACEMENT_GLYPH) {
    return getGlyphNoReplacement(REPLACEMENT_GLYPH);
  }
  return nullptr;
}
