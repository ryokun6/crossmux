#include "TextBlock.h"

#include <BidiUtils.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Serialization.h>
#include <Utf8.h>

#include <cstring>

#include "../VerticalPunctuation.h"

namespace {
bool isVerticalUprightWord(const std::string& word) {
  size_t upright = 0;
  size_t total = 0;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) break;
    if (cp == 0x00AD) continue;
    total++;
    if (utf8IsCjkBreakable(cp)) upright++;
  }
  return total > 0 && upright * 2 >= total;
}

// 縦中横: short ASCII digit/letter runs stay upright in one em cell (years like
// "19" fit; longer runs still rotate sideways down the column).
bool isTateChuYokoWord(const std::string& word) {
  size_t n = 0;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) break;
    if (cp == 0x00AD) continue;
    const bool asciiWordChar = (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
    if (!asciiWordChar) return false;
    n++;
    if (n > 2) return false;
  }
  return n > 0;
}

bool containsAsciiAlphaNumeric(const std::string& word) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) return false;
    if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) {
      return true;
    }
  }
}

bool isSidewaysVerticalWordAt(const std::vector<std::string>& words, const size_t index) {
  const std::string& word = words[index];
  if (VerticalPunctuation::stackedRunLength(word) == 0 && !isVerticalUprightWord(word) && !isTateChuYokoWord(word)) {
    return true;
  }
  if (!isTateChuYokoWord(word)) {
    return false;
  }
  const bool latinBefore = index > 0 && containsAsciiAlphaNumeric(words[index - 1]);
  const bool latinAfter = index + 1 < words.size() && containsAsciiAlphaNumeric(words[index + 1]);
  return latinBefore || latinAfter;
}
}  // namespace

uint8_t TextBlock::fakeBold = 0;
void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y) const {
  // Focus annotations are optional: empty vectors mean no word in this block has a split.
  // When present, they must be sized in lockstep with words[].
  const bool hasFocus = !wordFocusBoundary.empty();
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size() ||
      (hasFocus && (words.size() != wordFocusBoundary.size() || words.size() != wordFocusSuffixX.size()))) {
    LOG_ERR("TXB", "Render skipped: size mismatch (words=%u, xpos=%u, styles=%u, boundary=%u, suffixX=%u)\n",
            (uint32_t)words.size(), (uint32_t)wordXpos.size(), (uint32_t)wordStyles.size(),
            (uint32_t)wordFocusBoundary.size(), (uint32_t)wordFocusSuffixX.size());
    return;
  }

  const bool scanning = renderer.isFontCacheScanning();
  const int ascender = renderer.getFontAscenderSize(fontId);
  const bool verticalRtl = blockStyle.isVerticalRtl;
  for (size_t i = 0; i < words.size(); i++) {
    const int wordX = verticalRtl ? x : wordXpos[i] + x;
    const int wordY = verticalRtl ? y + wordXpos[i] : y;
    const EpdFontFamily::Style currentStyle = wordStyles[i];
    const auto baseDir = static_cast<BidiUtils::BidiBaseDir>(
        BidiUtils::detectParagraphLevel(words[i].c_str(), blockStyle.isRtl ? 1 : 0));
    const uint8_t boundary = hasFocus ? wordFocusBoundary[i] : 0;

    // SUP/SUB shift the baseline passed to the renderer; glyphs are scaled 50% there.
    // Horizontal text shifts along Y; vertical text rotates the same shift onto X.
    int drawX = wordX;
    int drawY = wordY;
    if (verticalRtl) {
      // A baseline shift rotates with the text: superscript moves toward the
      // right edge of the column, not upward into the preceding character.
      if ((currentStyle & EpdFontFamily::SUP) != 0) {
        drawX += ascender * 2 / 5;
      } else if ((currentStyle & EpdFontFamily::SUB) != 0) {
        drawX -= ascender / 4;
      }
    } else if ((currentStyle & EpdFontFamily::SUP) != 0) {
      drawY -= ascender * 2 / 5;
    } else if ((currentStyle & EpdFontFamily::SUB) != 0) {
      drawY += ascender / 4;
    }

    const size_t stackedCount = verticalRtl ? VerticalPunctuation::stackedRunLength(words[i]) : 0;
    if (stackedCount > 0) {
      int cellStep = renderer.getTextAdvanceX(fontId, "\xe6\x9c\xac", EpdFontFamily::REGULAR);  // 本
      if (cellStep <= 0) cellStep = ascender;

      const auto* punctuationPtr = reinterpret_cast<const unsigned char*>(words[i].c_str());
      size_t punctuationIndex = 0;
      while (*punctuationPtr != '\0' && punctuationIndex < stackedCount) {
        const uint32_t cp = utf8NextCodepoint(&punctuationPtr);
        if (VerticalPunctuation::isVariationSelector(cp)) continue;
        char glyph[5];
        if (!VerticalPunctuation::writeGlyphUtf8(cp, glyph)) continue;
        renderer.drawText(fontId, drawX, drawY + static_cast<int>(punctuationIndex) * cellStep, glyph, true,
                          currentStyle, baseDir);
        ++punctuationIndex;
      }
      continue;
    }

    if (verticalRtl && isSidewaysVerticalWordAt(words, i)) {
      // Sideways Latin in vertical-rl: CCW (un-flipped vs CW side-button helper) and
      // advance down the reserved cell so the run cannot climb into the previous glyph.
      renderer.drawTextRotated90CCW(fontId, drawX, drawY, words[i].c_str(), true, currentStyle);
      continue;
    }

    if (boundary > 0) {
      // Focus split: draw bold prefix, then the regular suffix at a pre-computed x offset.
      // The bold prefix is bounded to 9 codepoints by the clamp on targetBoldChars in
      // ParsedText::addWord; 9 UTF-8 codepoints occupy at most 9 * 4 = 36 bytes, +1 for null = 37.
      // suffixX is computed at cache-creation time to avoid font metric lookups at render time.
      static constexpr size_t MAX_FOCUS_PREFIX_BYTES = 9 * 4 + 1;
      char boldBuf[40];
      static_assert(sizeof(boldBuf) >= MAX_FOCUS_PREFIX_BYTES,
                    "boldBuf too small for max focus prefix (9 codepoints * 4 UTF-8 bytes + null)");
      const auto boldStyle = static_cast<EpdFontFamily::Style>(currentStyle | EpdFontFamily::BOLD);
      const size_t boldLen = std::min<size_t>({static_cast<size_t>(boundary), words[i].size(), sizeof(boldBuf) - 1});
      memcpy(boldBuf, words[i].c_str(), boldLen);
      boldBuf[boldLen] = '\0';
      renderer.drawText(fontId, drawX, drawY, boldBuf, true, boldStyle, baseDir);
      const int suffixX = drawX + wordFocusSuffixX[i];
      renderer.drawText(fontId, suffixX, drawY, words[i].c_str() + boldLen, true, currentStyle, baseDir);
    } else {
      if (fakeBold && (currentStyle & EpdFontFamily::BOLD) != 0) {
        auto fbStyle = static_cast<EpdFontFamily::Style>(currentStyle & ~EpdFontFamily::BOLD);
        if (fakeBold >= 2) {
          // Extra Bold: 3-pass at x-1, x, x+1
          renderer.drawText(fontId, drawX - 1, drawY, words[i].c_str(), true, fbStyle, baseDir);
          renderer.drawText(fontId, drawX, drawY, words[i].c_str(), true, fbStyle, baseDir);
          renderer.drawText(fontId, drawX + 1, drawY, words[i].c_str(), true, fbStyle, baseDir);
        } else {
          // Bold: 2-pass at x, x+1
          renderer.drawText(fontId, drawX, drawY, words[i].c_str(), true, fbStyle, baseDir);
          renderer.drawText(fontId, drawX + 1, drawY, words[i].c_str(), true, fbStyle, baseDir);
        }
      } else {
        renderer.drawText(fontId, drawX, drawY, words[i].c_str(), true, currentStyle, baseDir);
      }
    }

    if (!scanning && !verticalRtl && (currentStyle & EpdFontFamily::UNDERLINE) != 0) {
      const std::string& w = words[i];
      const int fullWordWidth = renderer.getTextWidth(fontId, w.c_str(), currentStyle, baseDir);
      // y is the top of the text line; add ascender to reach baseline, then offset 2px below
      const int underlineY = wordY + ascender + 2;

      int startX = wordX;
      int underlineWidth = fullWordWidth;

      // if word starts with em-space ("\xe2\x80\x83"), account for the additional indent before drawing the line
      if (w.size() >= 3 && static_cast<uint8_t>(w[0]) == 0xE2 && static_cast<uint8_t>(w[1]) == 0x80 &&
          static_cast<uint8_t>(w[2]) == 0x83) {
        const char* visiblePtr = w.c_str() + 3;
        const int prefixWidth = renderer.getTextAdvanceX(fontId, "\xe2\x80\x83", currentStyle);
        const int visibleWidth = renderer.getTextWidth(fontId, visiblePtr, currentStyle, baseDir);
        startX = wordX + prefixWidth;
        underlineWidth = visibleWidth;
      }

      // SUP/SUB glyphs are rendered at 50% scale; halve the underline to match (#2255).
      if ((currentStyle & (EpdFontFamily::SUP | EpdFontFamily::SUB)) != 0) {
        underlineWidth = (underlineWidth + 1) / 2;
      }

      renderer.drawLine(startX, underlineY, startX + underlineWidth, underlineY, true);
    }
  }
}

bool TextBlock::serialize(HalFile& file) const {
  // Focus annotations are optional; vectors are either empty (no splits in this block)
  // or sized in lockstep with words[].
  const bool hasFocus = !wordFocusBoundary.empty();
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size() ||
      (hasFocus && (words.size() != wordFocusBoundary.size() || words.size() != wordFocusSuffixX.size()))) {
    LOG_ERR("TXB", "Serialization failed: size mismatch (words=%u, xpos=%u, styles=%u, boundary=%u, suffixX=%u)\n",
            static_cast<uint32_t>(words.size()), static_cast<uint32_t>(wordXpos.size()),
            static_cast<uint32_t>(wordStyles.size()), static_cast<uint32_t>(wordFocusBoundary.size()),
            static_cast<uint32_t>(wordFocusSuffixX.size()));
    return false;
  }

  // Word data
  serialization::writePod(file, static_cast<uint16_t>(words.size()));
  for (const auto& w : words) serialization::writeString(file, w);
  for (auto x : wordXpos) serialization::writePod(file, x);
  for (auto s : wordStyles) serialization::writePod(file, s);
  // Focus block: 1-byte presence flag, followed by per-word vectors only when present.
  // Saves 3 bytes/word when focus reading is disabled or no word on this line was split.
  serialization::writePod(file, static_cast<uint8_t>(hasFocus ? 1 : 0));
  if (hasFocus) {
    for (auto b : wordFocusBoundary) serialization::writePod(file, b);
    for (auto sx : wordFocusSuffixX) serialization::writePod(file, sx);
  }

  // Style (alignment + margins/padding/indent)
  serialization::writePod(file, blockStyle.alignment);
  serialization::writePod(file, blockStyle.textAlignDefined);
  serialization::writePod(file, blockStyle.marginTop);
  serialization::writePod(file, blockStyle.marginBottom);
  serialization::writePod(file, blockStyle.marginLeft);
  serialization::writePod(file, blockStyle.marginRight);
  serialization::writePod(file, blockStyle.paddingTop);
  serialization::writePod(file, blockStyle.paddingBottom);
  serialization::writePod(file, blockStyle.paddingLeft);
  serialization::writePod(file, blockStyle.paddingRight);
  serialization::writePod(file, blockStyle.textIndent);
  serialization::writePod(file, blockStyle.textIndentDefined);
  serialization::writePod(file, blockStyle.isRtl);
  serialization::writePod(file, blockStyle.directionDefined);
  serialization::writePod(file, blockStyle.isVerticalRtl);

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(HalFile& file) {
  uint16_t wc;
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<uint8_t> wordFocusBoundary;
  std::vector<uint16_t> wordFocusSuffixX;
  BlockStyle blockStyle;

  // Word count
  serialization::readPod(file, wc);

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }

  // Word data
  words.resize(wc);
  wordXpos.resize(wc);
  wordStyles.resize(wc);
  for (auto& w : words) serialization::readString(file, w);
  for (auto& x : wordXpos) serialization::readPod(file, x);
  for (auto& s : wordStyles) serialization::readPod(file, s);
  // Focus block: presence flag, then vectors only if present. Empty vectors when absent
  // signal "no splits in this block" to render() (zero per-word RAM cost).
  uint8_t hasFocus;
  serialization::readPod(file, hasFocus);
  if (hasFocus) {
    wordFocusBoundary.resize(wc);
    wordFocusSuffixX.resize(wc);
    for (auto& b : wordFocusBoundary) serialization::readPod(file, b);
    for (auto& sx : wordFocusSuffixX) serialization::readPod(file, sx);
  }

  // Style (alignment + margins/padding/indent)
  serialization::readPod(file, blockStyle.alignment);
  serialization::readPod(file, blockStyle.textAlignDefined);
  serialization::readPod(file, blockStyle.marginTop);
  serialization::readPod(file, blockStyle.marginBottom);
  serialization::readPod(file, blockStyle.marginLeft);
  serialization::readPod(file, blockStyle.marginRight);
  serialization::readPod(file, blockStyle.paddingTop);
  serialization::readPod(file, blockStyle.paddingBottom);
  serialization::readPod(file, blockStyle.paddingLeft);
  serialization::readPod(file, blockStyle.paddingRight);
  serialization::readPod(file, blockStyle.textIndent);
  serialization::readPod(file, blockStyle.textIndentDefined);
  serialization::readPod(file, blockStyle.isRtl);
  serialization::readPod(file, blockStyle.directionDefined);
  serialization::readPod(file, blockStyle.isVerticalRtl);

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(words), std::move(wordXpos), std::move(wordStyles),
                                                  std::move(wordFocusBoundary), std::move(wordFocusSuffixX),
                                                  blockStyle));
}
