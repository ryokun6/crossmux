#include "TextBlock.h"

#include <BidiUtils.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Memory.h>
#include <Serialization.h>
#include <Utf8.h>

#include <cstring>

#include "../VerticalPunctuation.h"

namespace {
bool isVerticalUprightWord(const char* word) {
  size_t upright = 0;
  size_t total = 0;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word);
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
bool isTateChuYokoWord(const char* word) {
  size_t n = 0;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word);
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

bool containsAsciiAlphaNumeric(const char* word) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(word);
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) return false;
    if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) {
      return true;
    }
  }
}

bool isSidewaysVerticalWordAt(const TextBlock& block, const uint16_t index) {
  const char* word = block.wordText(index);
  if (VerticalPunctuation::stackedRunLength(word) == 0 && !isVerticalUprightWord(word) && !isTateChuYokoWord(word)) {
    return true;
  }
  if (!isTateChuYokoWord(word)) {
    return false;
  }
  const bool latinBefore = index > 0 && containsAsciiAlphaNumeric(block.wordText(index - 1));
  const bool latinAfter = index + 1 < block.wordCount() && containsAsciiAlphaNumeric(block.wordText(index + 1));
  return latinBefore || latinAfter;
}
}  // namespace

uint8_t TextBlock::fakeBold = 0;

size_t TextBlock::arenaSize(const uint16_t wordCount, const bool hasFocus, const uint16_t textBytes) {
  // Layout documented in TextBlock.h: 16-bit arrays first, then 8-bit arrays, then text.
  size_t size = static_cast<size_t>(wordCount) * (sizeof(uint16_t) + sizeof(int16_t) + sizeof(uint8_t));
  if (hasFocus) {
    size += static_cast<size_t>(wordCount) * (sizeof(uint16_t) + sizeof(uint8_t));
  }
  return size + textBytes;
}

void TextBlock::bindArenaPointers() {
  uint8_t* base = arena.get();
  const size_t wc = numWords;
  textOffArr = reinterpret_cast<const uint16_t*>(base);
  xposArr = reinterpret_cast<const int16_t*>(base + wc * 2);
  size_t off = wc * 4;
  if (focusPresent) {
    focusSuffixXArr = reinterpret_cast<const uint16_t*>(base + off);
    off += wc * 2;
  }
  stylesArr = base + off;
  off += wc;
  if (focusPresent) {
    focusBoundaryArr = base + off;
    off += wc;
  }
  textArr = reinterpret_cast<const char*>(base + off);
}

TextBlock::TextBlock(const std::vector<std::string>& words, const std::vector<int16_t>& wordXpos,
                     const std::vector<EpdFontFamily::Style>& wordStyles, const std::vector<uint8_t>& focusBoundary,
                     const std::vector<uint16_t>& focusSuffixX, const BlockStyle& blockStyle,
                     std::vector<std::string> rubyTexts)
    : blockStyle(blockStyle), rubyTexts(std::move(rubyTexts)) {
  // Same invariant as deserialize(): a block never holds an all-empty rubyTexts, so a
  // ruby-less line costs nothing beyond its arena.
  if (!hasRuby()) {
    this->rubyTexts = std::vector<std::string>{};
  }

  const bool hasFocus = !focusBoundary.empty();
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size() || words.size() > 10000 ||
      (hasFocus && (words.size() != focusBoundary.size() || words.size() != focusSuffixX.size()))) {
    LOG_ERR("TXB", "Construction failed: size mismatch (words=%u, xpos=%u, styles=%u, boundary=%u, suffixX=%u)",
            static_cast<uint32_t>(words.size()), static_cast<uint32_t>(wordXpos.size()),
            static_cast<uint32_t>(wordStyles.size()), static_cast<uint32_t>(focusBoundary.size()),
            static_cast<uint32_t>(focusSuffixX.size()));
    isValid = false;
    return;
  }

  numWords = static_cast<uint16_t>(words.size());
  focusPresent = hasFocus;
  if (numWords == 0) {
    return;  // valid empty block, no arena
  }

  size_t totalText = 0;
  for (const auto& w : words) totalText += w.size() + 1;
  if (totalText > UINT16_MAX) {
    LOG_ERR("TXB", "Construction failed: text size %u exceeds arena limit", static_cast<uint32_t>(totalText));
    numWords = 0;
    focusPresent = false;
    isValid = false;
    return;
  }
  textBytes = static_cast<uint16_t>(totalText);

  const size_t size = arenaSize(numWords, focusPresent, textBytes);
  arena = makeUniqueNoThrow<uint8_t[]>(size);
  if (!arena) {
    LOG_ERR("TXB", "OOM: arena %u bytes", static_cast<uint32_t>(size));
    numWords = 0;
    textBytes = 0;
    focusPresent = false;
    isValid = false;
    return;
  }
  bindArenaPointers();

  auto* textOff = const_cast<uint16_t*>(textOffArr);
  auto* xpos = const_cast<int16_t*>(xposArr);
  auto* styles = const_cast<uint8_t*>(stylesArr);
  auto* text = const_cast<char*>(textArr);
  uint16_t off = 0;
  for (uint16_t i = 0; i < numWords; i++) {
    textOff[i] = off;
    xpos[i] = wordXpos[i];
    styles[i] = static_cast<uint8_t>(wordStyles[i]);
    memcpy(text + off, words[i].data(), words[i].size());
    off += static_cast<uint16_t>(words[i].size());
    text[off++] = '\0';
  }
  if (focusPresent) {
    auto* suffixX = const_cast<uint16_t*>(focusSuffixXArr);
    auto* boundary = const_cast<uint8_t*>(focusBoundaryArr);
    for (uint16_t i = 0; i < numWords; i++) {
      suffixX[i] = focusSuffixX[i];
      boundary[i] = focusBoundary[i];
    }
  }
}

bool TextBlock::hasRuby() const {
  for (const auto& rt : rubyTexts) {
    if (!rt.empty()) return true;
  }
  return false;
}

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y) const {
  if (!isValid) {
    LOG_ERR("TXB", "Render skipped: invalid block");
    return;
  }

  const bool scanning = renderer.isFontCacheScanning();
  const int ascender = renderer.getFontAscenderSize(fontId);
  const bool verticalRtl = blockStyle.isVerticalRtl;
  const int rubyShift = getRubyShift(ascender);

  struct DecorationLineTracker {
    EpdFontFamily::Style style;
    int yOffset;
    int startX = -1;
    int endX = -1;
    int yPos = 0;

    bool active() const { return startX != -1; }
    void reset() {
      startX = -1;
      endX = -1;
      yPos = 0;
    }
  };

  DecorationLineTracker decorationLines[] = {
      {EpdFontFamily::UNDERLINE, ascender + 2},
      {EpdFontFamily::STRIKETHROUGH, ascender * 4 / 5},
  };

  const auto flushDecoration = [&](DecorationLineTracker& line) {
    if (line.active()) {
      renderer.drawLine(line.startX, line.yPos, line.endX, line.yPos, 2, true);
      line.reset();
    }
  };
  const auto flushDecorations = [&]() {
    for (auto& line : decorationLines) {
      flushDecoration(line);
    }
  };

  for (uint16_t i = 0; i < numWords; i++) {
    const char* word = wordText(i);
    const int wordX = verticalRtl ? x : xposArr[i] + x;
    const int wordY = verticalRtl ? y + xposArr[i] : y + rubyShift;
    const EpdFontFamily::Style currentStyle = wordStyle(i);
    const auto baseDir =
        static_cast<BidiUtils::BidiBaseDir>(BidiUtils::detectParagraphLevel(word, blockStyle.isRtl ? 1 : 0));
    const uint8_t boundary = focusBoundary(i);

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

    const size_t stackedCount = verticalRtl ? VerticalPunctuation::stackedRunLength(word) : 0;
    if (stackedCount > 0) {
      int cellStep = renderer.getTextAdvanceX(fontId, "\xe6\x9c\xac", EpdFontFamily::REGULAR);  // 本
      if (cellStep <= 0) cellStep = ascender;

      const auto* punctuationPtr = reinterpret_cast<const unsigned char*>(word);
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

    if (verticalRtl && isSidewaysVerticalWordAt(*this, i)) {
      // Sideways Latin in vertical-rl: CCW (un-flipped vs CW side-button helper) and
      // advance down the reserved cell so the run cannot climb into the previous glyph.
      renderer.drawTextRotated90CCW(fontId, drawX, drawY, word, true, currentStyle);
      continue;
    }

    if (boundary > 0) {
      // Focus split: draw bold prefix, then the regular suffix at a pre-computed x offset.
      static constexpr size_t MAX_FOCUS_PREFIX_BYTES = 9 * 4 + 1;
      char boldBuf[40];
      static_assert(sizeof(boldBuf) >= MAX_FOCUS_PREFIX_BYTES,
                    "boldBuf too small for max focus prefix (9 codepoints * 4 UTF-8 bytes + null)");
      const auto boldStyle = static_cast<EpdFontFamily::Style>(currentStyle | EpdFontFamily::BOLD);
      const size_t boldLen =
          std::min<size_t>({static_cast<size_t>(boundary), static_cast<size_t>(wordTextLen(i)), sizeof(boldBuf) - 1});
      memcpy(boldBuf, word, boldLen);
      boldBuf[boldLen] = '\0';
      renderer.drawText(fontId, drawX, drawY, boldBuf, true, boldStyle, baseDir);
      const int suffixX = drawX + focusSuffixX(i);
      renderer.drawText(fontId, suffixX, drawY, word + boldLen, true, currentStyle, baseDir);
    } else if (!verticalRtl && fakeBold && (currentStyle & EpdFontFamily::BOLD) != 0) {
      auto fbStyle = static_cast<EpdFontFamily::Style>(currentStyle & ~EpdFontFamily::BOLD);
      if (fakeBold >= 2) {
        // Extra Bold: 3-pass at x-1, x, x+1
        renderer.drawText(fontId, drawX - 1, drawY, word, true, fbStyle, baseDir);
        renderer.drawText(fontId, drawX, drawY, word, true, fbStyle, baseDir);
        renderer.drawText(fontId, drawX + 1, drawY, word, true, fbStyle, baseDir);
      } else {
        // Bold: 2-pass at x, x+1
        renderer.drawText(fontId, drawX, drawY, word, true, fbStyle, baseDir);
        renderer.drawText(fontId, drawX + 1, drawY, word, true, fbStyle, baseDir);
      }
    } else {
      renderer.drawText(fontId, drawX, drawY, word, true, currentStyle, baseDir);
    }

    if (scanning || verticalRtl) {
      continue;
    }

    if (EpdFontFamily::hasTextDecoration(currentStyle)) {
      int lineStartX = drawX;
      int lineWidth = renderer.getTextWidth(fontId, word, currentStyle, baseDir);

      // SUP/SUB glyphs are rendered at 50% scale; halve the underline to match (#2255).
      if ((currentStyle & (EpdFontFamily::SUP | EpdFontFamily::SUB)) != 0) {
        lineWidth = (lineWidth + 1) / 2;
      }

      // Do not decorate the synthetic em-space used for paragraph indentation.
      if (wordTextLen(i) >= 3 && static_cast<uint8_t>(word[0]) == 0xE2 && static_cast<uint8_t>(word[1]) == 0x80 &&
          static_cast<uint8_t>(word[2]) == 0x83) {
        const char* visibleText = word + 3;
        lineStartX += renderer.getTextAdvanceX(fontId, "\xe2\x80\x83", currentStyle);
        lineWidth = renderer.getTextWidth(fontId, visibleText, currentStyle, baseDir);
        if ((currentStyle & (EpdFontFamily::SUP | EpdFontFamily::SUB)) != 0) {
          lineWidth = (lineWidth + 1) / 2;
        }
      }

      for (auto& line : decorationLines) {
        if ((currentStyle & line.style) == 0) {
          flushDecoration(line);
          continue;
        }

        const int lineY = drawY + line.yOffset;
        if (line.active() && line.yPos != lineY) {
          flushDecoration(line);
        }
        if (!line.active()) {
          line.startX = lineStartX;
          line.yPos = lineY;
        }
        line.endX = lineStartX + lineWidth;
      }
    } else {
      flushDecorations();
    }
  }
  if (!verticalRtl) {
    flushDecorations();
  }
}

bool TextBlock::serialize(HalFile& file) const {
  if (!isValid) {
    LOG_ERR("TXB", "Serialization failed: invalid block");
    return false;
  }

  // Word data: scalars, then the arena verbatim -- its in-memory layout is
  // exactly the on-disk layout (see TextBlock.h), so one write covers all
  // per-word arrays and the text blob.
  serialization::writePod(file, numWords);
  serialization::writePod(file, static_cast<uint8_t>(focusPresent ? 1 : 0));
  serialization::writePod(file, textBytes);
  if (numWords > 0) {
    const size_t size = arenaSize(numWords, focusPresent, textBytes);
    if (file.write(arena.get(), size) != size) {
      LOG_ERR("TXB", "Serialization failed: arena write (%u bytes)", static_cast<uint32_t>(size));
      return false;
    }
  }

  // Ruby text data (empty strings when the line has no annotations).
  for (size_t i = 0; i < numWords; i++) {
    serialization::writeString(file, (i < rubyTexts.size()) ? rubyTexts[i] : std::string());
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
  uint8_t hasFocus;
  uint16_t textBytes;
  serialization::readPod(file, wc);
  serialization::readPod(file, hasFocus);
  serialization::readPod(file, textBytes);

  // Sanity checks: cap the arena allocation and reject impossible geometry
  // (every word carries at least its NUL terminator).
  if (wc > 10000) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }
  if ((wc == 0 && textBytes != 0) || (wc > 0 && textBytes < wc)) {
    LOG_ERR("TXB", "Deserialization failed: bad text size %u for %u words", textBytes, wc);
    return nullptr;
  }

  // C API ownership: unique_ptr takes over the nothrow allocation immediately.
  std::unique_ptr<TextBlock> block(new (std::nothrow) TextBlock());
  if (!block) {
    LOG_ERR("TXB", "OOM: TextBlock");
    return nullptr;
  }
  block->numWords = wc;
  block->textBytes = textBytes;
  block->focusPresent = hasFocus != 0;

  if (wc > 0) {
    const size_t size = arenaSize(wc, block->focusPresent, textBytes);
    block->arena = makeUniqueNoThrow<uint8_t[]>(size);
    if (!block->arena) {
      LOG_ERR("TXB", "OOM: arena %u bytes", static_cast<uint32_t>(size));
      return nullptr;
    }
    if (file.read(block->arena.get(), size) != size) {
      LOG_ERR("TXB", "Deserialization failed: arena read (%u bytes)", static_cast<uint32_t>(size));
      return nullptr;
    }
    block->bindArenaPointers();

    // Validate offsets before anything dereferences wordText(): offset 0 first,
    // strictly increasing, in bounds, and every word NUL-terminated.
    const uint16_t* textOff = block->textOffArr;
    const char* text = block->textArr;
    if (textOff[0] != 0 || text[textBytes - 1] != '\0') {
      LOG_ERR("TXB", "Deserialization failed: corrupt text layout");
      return nullptr;
    }
    for (uint16_t i = 1; i < wc; i++) {
      if (textOff[i] <= textOff[i - 1] || textOff[i] >= textBytes || text[textOff[i] - 1] != '\0') {
        LOG_ERR("TXB", "Deserialization failed: corrupt word offset %u", i);
        return nullptr;
      }
    }
  }

  // Ruby text data. Allocate lazily and only once a non-empty annotation shows up.
  std::string scratch;
  for (uint16_t i = 0; i < wc; i++) {
    serialization::readString(file, scratch);
    if (scratch.empty()) continue;
    if (block->rubyTexts.empty()) {
      block->rubyTexts.resize(wc);
    }
    block->rubyTexts[i] = std::move(scratch);
  }

  // Style (alignment + margins/padding/indent)
  BlockStyle& blockStyle = block->blockStyle;
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

  return block;
}
