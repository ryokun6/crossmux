#pragma once
#include "EpdFontData.h"

class EpdFont {
  void getTextBounds(const char* string, int startX, int startY, int* minX, int* minY, int* maxX, int* maxY) const;

#ifdef ENABLE_CHINESE_VERSION
  // Tiny per-font codepoint→glyph LRU, CN-only to keep the int'l build
  // byte-for-byte unchanged. getGlyph() normally does upper_bound over the
  // interval table (O(log intervalCount)) plus an indirect glyph[] index.
  // CJK rendering calls getGlyph from both getTextBounds and drawText for
  // every character on every line, so recently-seen codepoints repeat
  // constantly — an 8-slot ring covers the working set of a CN UI list row
  // or EPUB page line. ASCII bypasses the cache (it lives in the first
  // interval, so upper_bound terminates in a single compare).
  static constexpr uint8_t kGlyphCacheSize = 8;
  struct GlyphCacheSlot {
    uint32_t cp;
    const EpdGlyph* glyph;
  };
  mutable GlyphCacheSlot glyphCache_[kGlyphCacheSize] = {};
  mutable uint8_t glyphCacheWrite_ = 0;

  const EpdGlyph* lookupGlyphCache(uint32_t cp) const {
    if (cp < 0x80) return nullptr;
    for (uint8_t i = 0; i < kGlyphCacheSize; ++i) {
      if (glyphCache_[i].cp == cp) return glyphCache_[i].glyph;
    }
    return nullptr;
  }
  const EpdGlyph* storeGlyphCache(uint32_t cp, const EpdGlyph* glyph) const {
    if (cp >= 0x80) {
      glyphCache_[glyphCacheWrite_] = {cp, glyph};
      glyphCacheWrite_ = (glyphCacheWrite_ + 1) % kGlyphCacheSize;
    }
    return glyph;
  }
#else
  // No-op stubs in the int'l build — the compiler inlines them away,
  // leaving getGlyph() byte-identical to the pre-LRU revision.
  const EpdGlyph* lookupGlyphCache(uint32_t) const { return nullptr; }
  const EpdGlyph* storeGlyphCache(uint32_t, const EpdGlyph* glyph) const { return glyph; }
#endif

 public:
  const EpdFontData* data;
  explicit EpdFont(const EpdFontData* data) : data(data) {}
  ~EpdFont() = default;
  void getTextDimensions(const char* string, int* w, int* h) const;

  const EpdGlyph* getGlyph(uint32_t cp) const;

  /// Returns the kerning adjustment (4.4 fixed-point in pixels) between two codepoints.
  /// Returns 0 if no kerning data exists for the pair.
  int8_t getKerning(uint32_t leftCp, uint32_t rightCp) const;

  /// Returns the ligature codepoint for a pair, or 0 if no ligature exists.
  uint32_t getLigature(uint32_t leftCp, uint32_t rightCp) const;

  /// Greedily applies ligature substitutions starting from cp, consuming
  /// as many following codepoints from text as possible. Returns the
  /// (possibly substituted) codepoint; advances text past consumed chars.
  uint32_t applyLigatures(uint32_t cp, const char*& text) const;
};
