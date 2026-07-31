#pragma once

#include <EpdFontFamily.h>
#include <WordList.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "blocks/BlockStyle.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  WordList words;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;      // true = word attaches to previous with no break
  std::vector<bool> wordNoSpaceBefore;  // true = may break before token, but no synthetic space when joined
  std::vector<bool> wordIsFocusSuffix;  // true = token is the regular tail of a focus bold-prefix split
  std::vector<bool> wordSpaceBefore;    // true = source whitespace preceded this word; keeps author spaces in CJK text
  BlockStyle blockStyle;
  bool extraParagraphSpacing;
  bool hyphenationEnabled;
  bool focusReadingEnabled;
<<<<<<< HEAD
  bool punctCompressionEnabled;
=======
>>>>>>> upstream/master
  bool isNaturalAlign;
  bool hasRtlWord;
  std::vector<std::string> reorderedWordsScratch;
  std::vector<EpdFontFamily::Style> reorderedStylesScratch;
  std::vector<uint16_t> reorderedWidthsScratch;
  std::vector<bool> reorderedContinuesScratch;
  std::vector<bool> reorderedNoSpaceBeforeScratch;
<<<<<<< HEAD
  std::vector<bool> reorderedSpaceBeforeScratch;
  std::vector<bool> reorderedFocusSuffixScratch;
  std::vector<uint16_t> visualOrderScratch;

  void reserveForTokens(size_t extraTokens, size_t extraBytes);
  bool tryReserveForTokens(size_t extraTokens, size_t extraBytes);
=======
  std::vector<bool> reorderedFocusSuffixScratch;
  std::vector<uint16_t> visualOrderScratch;

>>>>>>> upstream/master
  int resolveFirstLineIndent(bool isFirstLine, const GfxRenderer& renderer, int fontId) const;
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                        std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec,
                                        std::vector<bool>& noSpaceBeforeVec);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec,
                                                  std::vector<bool>& noSpaceBeforeVec);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks);
  void extractLine(size_t breakIndex, int pageWidth, const std::vector<uint16_t>& wordWidths,
                   const std::vector<bool>& continuesVec, const std::vector<bool>& noSpaceBeforeVec,
                   const std::vector<size_t>& lineBreakIndices,
                   const std::function<void(std::shared_ptr<TextBlock>)>& processLine, const GfxRenderer& renderer,
                   int fontId);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);

 public:
  explicit ParsedText(const bool extraParagraphSpacing, const bool hyphenationEnabled = false,
                      const bool focusReadingEnabled = false, const bool punctCompressionEnabled = true,
                      const BlockStyle& blockStyle = BlockStyle())
      : blockStyle(blockStyle),
        extraParagraphSpacing(extraParagraphSpacing),
        hyphenationEnabled(hyphenationEnabled),
        focusReadingEnabled(focusReadingEnabled),
<<<<<<< HEAD
        punctCompressionEnabled(punctCompressionEnabled),
=======
>>>>>>> upstream/master
        isNaturalAlign(false),
        hasRtlWord(false) {}
  ~ParsedText() = default;

  void addWord(std::string word, EpdFontFamily::Style fontStyle, bool underline = false, bool attachToPrevious = false,
               bool spaceBefore = false);
  // Same as addWord but returns false on OOM (WordList / style vector growth) so the
  // chapter indexer can latch allocationFailed instead of abort()ing.
  bool tryAddWord(std::string word, EpdFontFamily::Style fontStyle, bool underline = false,
                  bool attachToPrevious = false, bool spaceBefore = false);
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  BlockStyle& getBlockStyle() { return blockStyle; }
  size_t size() const { return words.size(); }
  bool isEmpty() const { return words.empty(); }
  void layoutAndExtractLines(const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
<<<<<<< HEAD
                             bool includeLastLine = true, uint16_t viewportHeight = 0);

 private:
  void layoutAndExtractVerticalColumns(const GfxRenderer& renderer, int fontId, uint16_t columnHeight,
                                       const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                                       bool includeLastLine);
  void extractVerticalColumn(size_t startIdx, size_t endIdx, const std::vector<uint16_t>& verticalExtents,
                             const GfxRenderer& renderer, int fontId,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine);
  std::vector<size_t> computeVerticalColumnBreaks(const GfxRenderer& renderer, int fontId, int columnHeight,
                                                  const std::vector<uint16_t>& verticalExtents);
=======
                             bool includeLastLine = true);
>>>>>>> upstream/master
};
