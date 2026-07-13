#pragma once

#include <Utf8.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace VerticalPunctuation {

// Normalize source and presentation-form variants to the glyph used in a
// vertical column. Keeping this table shared prevents layout and rendering from
// disagreeing about how many character cells a punctuation run occupies.
constexpr uint32_t presentationCodepoint(const uint32_t cp) {
  switch (cp) {
    case 0x2025:  // ‥ TWO DOT LEADER
    case 0xFE30:  // ︰ PRESENTATION FORM FOR VERTICAL TWO DOT LEADER
      return 0xFE30;
    case 0x2026:  // … HORIZONTAL ELLIPSIS
    case 0x22EE:  // ⋮ VERTICAL ELLIPSIS
    case 0x22EF:  // ⋯ MIDLINE HORIZONTAL ELLIPSIS
    case 0xFE19:  // ︙ PRESENTATION FORM FOR VERTICAL HORIZONTAL ELLIPSIS
      return 0xFE19;
    case 0x2013:  // – EN DASH
    case 0xFE32:  // ︲ PRESENTATION FORM FOR VERTICAL EN DASH
      return 0xFE32;
    case 0x2014:  // — EM DASH
    case 0x2015:  // ― HORIZONTAL BAR
    case 0xFE31:  // ︱ PRESENTATION FORM FOR VERTICAL EM DASH
      return 0xFE31;
    default:
      return 0;
  }
}

constexpr bool isVariationSelector(const uint32_t cp) { return cp == 0xFE0E || cp == 0xFE0F; }

inline bool occupiesVerticalCell(const uint32_t cp) { return presentationCodepoint(cp) != 0 || utf8IsCjkBreakable(cp); }

// Some source punctuation is grouped into one parser token before it is
// remapped to vertical forms. Count every upright glyph in such a token so
// sequences like U+FE19 U+FE19 U+FE42 (two ellipses plus a closing quote) use
// three cells instead of being painted together in one cell.
inline size_t stackedRunLength(const std::string& word) {
  size_t count = 0;
  bool followsStackedGlyph = false;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) break;
    if (isVariationSelector(cp) && followsStackedGlyph) {
      continue;
    }

    if (!occupiesVerticalCell(cp)) return 0;
    followsStackedGlyph = true;
    ++count;
  }
  return count >= 2 ? count : 0;
}

// Encode one normalized vertical glyph into a fixed stack buffer. This avoids
// allocating a temporary std::string for every character during page renders.
inline bool writeGlyphUtf8(uint32_t cp, char (&out)[5]) {
  if (const uint32_t presentation = presentationCodepoint(cp); presentation != 0) {
    cp = presentation;
  }

  size_t length = 0;
  if (cp < 0x80) {
    out[length++] = static_cast<char>(cp);
  } else if (cp < 0x800) {
    out[length++] = static_cast<char>(0xC0 | (cp >> 6));
    out[length++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out[length++] = static_cast<char>(0xE0 | (cp >> 12));
    out[length++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[length++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0x10FFFF) {
    out[length++] = static_cast<char>(0xF0 | (cp >> 18));
    out[length++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out[length++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[length++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    return false;
  }
  out[length] = '\0';
  return true;
}

}  // namespace VerticalPunctuation
