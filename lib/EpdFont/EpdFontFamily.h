#pragma once
#include "EpdFont.h"

class EpdFontFamily {
 public:
  // Bitmask of text style flags carried per-word through layout and serialized in page cache.
  // Bits 0-1 select the font variant (BOLD/ITALIC); bits 2-5 are decoration/positioning overlays
  // applied at render time without changing the underlying font. getFont() ignores all bits
  // above bit 1 so decorations compose freely with bold/italic (e.g. BOLD | UNDERLINE | SUP).
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,       // drawn as a line below baseline by TextBlock::render()
    STRIKETHROUGH = 8,   // drawn as a line through midline by TextBlock::render()
    SUP = 16,            // superscript: glyph scaled 50%, raised ~40% of ascender
    SUB = 32,            // subscript: glyph scaled 50%, lowered ~25% of ascender
    RUBY_CONTINUE = 64,  // Group ruby follower marker (used internally by Epub layout)
  };
  static constexpr uint8_t TEXT_DECORATION_MASK = static_cast<uint8_t>(UNDERLINE | STRIKETHROUGH);
  static constexpr bool hasTextDecoration(const Style style) {
    return (static_cast<uint8_t>(style) & TEXT_DECORATION_MASK) != 0;
  }

  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr)
      : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic) {}
  ~EpdFontFamily() = default;
  void getTextDimensions(const char* string, int* w, int* h, Style style = REGULAR) const;
  const EpdFontData* getData(Style style = REGULAR) const;
  /// Style index of the face that getFont() actually selects for \p style.
  /// Styles that share a face — bold-italic on a family with no bold-italic,
  /// italic on a family with no italic — collapse onto the same index, and the
  /// lowest such index wins. Callers that do per-face work (font cache prewarm)
  /// must group by this, not by the requested style: doing the same face twice
  /// with different text makes the second pass discard the first one's glyphs.
  Style resolveStyle(Style style) const;
  /// Resolve a glyph for \p style, falling back to regular when the styled face
  /// lacks the codepoint (hybrid SD fonts: Latin-only italic face), then to
  /// \p glyphFallback_ (builtin system font) before U+FFFD. When \p outData
  /// is non-null, it receives the EpdFontData that owns the returned glyph —
  /// callers must use that for bitmap lookup, not getData(style).
  const EpdGlyph* getGlyph(uint32_t cp, Style style = REGULAR, const EpdFontData** outData = nullptr) const;
  /// Like getGlyph, but returns nullptr instead of substituting U+FFFD.
  /// Still applies same-family styled→regular fallback. Does not consult
  /// glyphFallback_ (callers that want cross-family fallback use getGlyph).
  const EpdGlyph* getGlyphNoReplacement(uint32_t cp, Style style = REGULAR,
                                        const EpdFontData** outData = nullptr) const;
  int8_t getKerning(uint32_t leftCp, uint32_t rightCp, Style style = REGULAR) const;
  uint32_t applyLigatures(uint32_t cp, const char*& text, Style style = REGULAR) const;

  /// Cross-family glyph source used when this family lacks a codepoint
  /// (Latin-only SD fonts → builtin CJK on gh_release_cn). Pointer must
  /// outlive this family (builtin fonts in GfxRenderer::fontMap are stable).
  void setGlyphFallback(const EpdFontFamily* fallback) { glyphFallback_ = fallback; }
  const EpdFontFamily* getGlyphFallback() const { return glyphFallback_; }

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;
  const EpdFontFamily* glyphFallback_ = nullptr;

  const EpdFont* getFont(Style style) const;
};
