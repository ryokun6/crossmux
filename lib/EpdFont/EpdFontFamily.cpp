#include "EpdFontFamily.h"

const EpdFont* EpdFontFamily::getFont(const Style style) const {
  // Extract font style bits (ignore UNDERLINE bit for font selection)
  const bool hasBold = (style & BOLD) != 0;
  const bool hasItalic = (style & ITALIC) != 0;

  if (hasBold && hasItalic) {
    if (boldItalic) return boldItalic;
    if (bold) return bold;
    if (italic) return italic;
  } else if (hasBold && bold) {
    return bold;
  } else if (hasItalic && italic) {
    return italic;
  }

  return regular;
}

void EpdFontFamily::getTextDimensions(const char* string, int* w, int* h, const Style style) const {
  getFont(style)->getTextDimensions(string, w, h);
}

const EpdFontData* EpdFontFamily::getData(const Style style) const { return getFont(style)->data; }

EpdFontFamily::Style EpdFontFamily::resolveStyle(const Style style) const {
  const EpdFont* font = getFont(style);
  // Compare against the members rather than re-deriving getFont()'s fallback
  // chain, so this can never drift from it. Lowest matching index wins.
  if (font == regular) return REGULAR;
  if (font == bold) return BOLD;
  if (font == italic) return ITALIC;
  if (font == boldItalic) return BOLD_ITALIC;
  return REGULAR;
}

const EpdGlyph* EpdFontFamily::getGlyphNoReplacement(const uint32_t cp, const Style style,
                                                     const EpdFontData** outData) const {
  const EpdFont* font = getFont(style);
  const EpdGlyph* glyph = font->getGlyphNoReplacement(cp);
  if (glyph) {
    if (outData) *outData = font->data;
    return glyph;
  }
  // Styled face lacks this codepoint (e.g. italic CJK only embedded in
  // regular for SD fonts) — use regular's glyph so styled Latin still works.
  if (font != regular && regular) {
    glyph = regular->getGlyphNoReplacement(cp);
    if (glyph) {
      if (outData) *outData = regular->data;
      return glyph;
    }
  }
  return nullptr;
}

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style, const EpdFontData** outData) const {
  return getGlyph(cp, style, nullptr, outData);
}

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style, bool* const usedReplacement,
                                        const EpdFontData** outData) const {
  if (usedReplacement) *usedReplacement = false;
  const EpdGlyph* glyph = getGlyphNoReplacement(cp, style, outData);
  if (glyph) return glyph;
  if (usedReplacement) *usedReplacement = true;

  // Selected face (often a Latin-only SD font) lacks this codepoint — try the
  // builtin system font so CJK books still render instead of tofu / blank.
  if (glyphFallback_) {
    glyph = glyphFallback_->getGlyphNoReplacement(cp, style, outData);
    if (glyph) return glyph;
  }

  const EpdFont* font = getFont(style);
  glyph = font->getGlyph(cp);  // replacement glyph from the styled face
  if (outData) *outData = font->data;
  return glyph;
}

int8_t EpdFontFamily::getKerning(const uint32_t leftCp, const uint32_t rightCp, const Style style) const {
  return getFont(style)->getKerning(leftCp, rightCp);
}

uint32_t EpdFontFamily::applyLigatures(const uint32_t cp, const char*& text, const Style style) const {
  return getFont(style)->applyLigatures(cp, text);
}
