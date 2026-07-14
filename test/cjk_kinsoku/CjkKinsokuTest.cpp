#include <gtest/gtest.h>

#include "CjkKinsoku.h"

namespace {

// Pure break-policy helper matching ParsedText's candidate filter: a break after
// `left` (next line starts with `right`) is legal only when 禁則 allows it.
bool mayBreakAfter(const uint32_t left, const uint32_t right) { return CjkKinsoku::isLegalBreakBetween(left, right); }

}  // namespace

TEST(CjkKinsoku, LineStartProhibitedCoversStopsAndClosers) {
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3002));   // 。
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3001));   // 、
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFF0C));   // ，
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x300D));   // 」
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFF09));   // ）
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x30FB));   // ・
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x2026));   // …
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x4E2D));  // 中
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x300C));  // 「
}

TEST(CjkKinsoku, LineEndProhibitedCoversOpenersAndCurrency) {
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0x300C));   // 「
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFF08));   // （
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0x201C));   // “
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFFE5));   // ￥
  EXPECT_FALSE(CjkKinsoku::isLineEndProhibited(0x3002));  // 。
  EXPECT_FALSE(CjkKinsoku::isLineEndProhibited(0x4E2D));  // 中
}

TEST(CjkKinsoku, InseparablePairs) {
  EXPECT_TRUE(CjkKinsoku::isInseparablePair(0x2026, 0x2026));  // ……
  EXPECT_TRUE(CjkKinsoku::isInseparablePair(0x2014, 0x2014));  // ——
  EXPECT_TRUE(CjkKinsoku::isInseparablePair('1', '%'));
  EXPECT_TRUE(CjkKinsoku::isInseparablePair(0xFFE5, '9'));  // ￥9
  EXPECT_FALSE(CjkKinsoku::isInseparablePair(0x4E2D, 0x6587));
  EXPECT_FALSE(CjkKinsoku::isInseparablePair(0x4E2D, 0x3002));
}

TEST(CjkKinsoku, BreakOpportunityMatrix) {
  // Ideograph + ideograph: break allowed
  EXPECT_TRUE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x4E2D, 0x6587));
  // Ideograph + 。 : no break before stop
  EXPECT_FALSE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x4E2D, 0x3002));
  // 「 + ideograph: no break after opener
  EXPECT_FALSE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x300C, 0x4E2D));
  // Latin-only pair: not a CJK opportunity
  EXPECT_FALSE(CjkKinsoku::hasCjkBreakOpportunityBetween('a', 'b'));
}

TEST(CjkKinsoku, LegalBreakPolicyKeepsPunctWithNeighbors) {
  // Near a width boundary, never break before 。 or after 「
  EXPECT_FALSE(mayBreakAfter(0x4E2D, 0x3002));  // 中。
  EXPECT_FALSE(mayBreakAfter(0x300C, 0x4E2D));  // 「中
  EXPECT_TRUE(mayBreakAfter(0x4E2D, 0x6587));   // 中文
  EXPECT_TRUE(mayBreakAfter(0x3002, 0x4E2D));   // 。中 (break after stop is fine)
}

TEST(CjkKinsoku, VerticalPresentationFormsFollowSameRules) {
  // After vertical-rl remap, stops/closers/openers use FE** forms.
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFE12));  // ︒
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFE11));  // ︑
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFE42));  // ﹂
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFE41));    // ﹁
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFE35));    // ︵
  EXPECT_FALSE(CjkKinsoku::isLegalBreakBetween(0x4E2D, 0xFE12));
  EXPECT_FALSE(CjkKinsoku::isLegalBreakBetween(0xFE41, 0x4E2D));
}

TEST(CjkKinsoku, RepairBreakRetreatsBeforeColumnHeadStop) {
  // Column would break before 。 — pull previous ideograph onto the next column.
  const std::vector<std::string> words = {"\xE4\xB8\xAD", "\xE6\x96\x87", "\xE3\x80\x82"};  // 中 文 。
  const std::vector<bool> continues = {false, false, false};
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 2), 1u);
}

TEST(CjkKinsoku, RepairBreakNeverOverflowsSingleTokenColumn) {
  // Only one ideograph fits; next is 。. The impossible one-cell case keeps the
  // bounded break rather than drawing punctuation outside the column measure.
  const std::vector<std::string> words = {"\xE4\xB8\xAD", "\xE3\x80\x82"};  // 中 。
  const std::vector<bool> continues = {false, false};
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 1), 1u);
}

TEST(CjkKinsoku, RepairBreakRetreatsOpenerFromColumnEnd) {
  const std::vector<std::string> words = {"\xE4\xB8\xAD", "\xE3\x80\x8C", "\xE6\x96\x87"};  // 中 「 文
  const std::vector<bool> continues = {false, false, false};
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 2), 1u);
}

TEST(CjkKinsoku, RepairBreakKeepsVerticalRemappedStopBounded) {
  // U+FE12 PRESENTATION FORM FOR VERTICAL IDEOGRAPHIC FULL STOP
  const std::vector<std::string> words = {"\xE4\xB8\xAD", "\xEF\xB8\x92"};  // 中 ︒
  const std::vector<bool> continues = {false, false};
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 1), 1u);
}

TEST(CjkKinsoku, RepairBreakByteOffsetRetreatsBeforeStop) {
  // "中文。" — candidate break before 。 pulls 文 onto the next line.
  const std::string text = "\xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82";
  EXPECT_EQ(CjkKinsoku::repairBreakByteOffset(text, 0, 6), 3u);
}

TEST(CjkKinsoku, RepairBreakByteOffsetRetreatsOpenerFromLineEnd) {
  // "中「文" — candidate break after 「 pulls the opener onto the next line.
  const std::string text = "\xE4\xB8\xAD\xE3\x80\x8C\xE6\x96\x87";
  EXPECT_EQ(CjkKinsoku::repairBreakByteOffset(text, 0, 6), 3u);
}

TEST(CjkKinsoku, RepairBreakByteOffsetNeverEmptiesLine) {
  // Only "中" fits before "。" — keep the bounded break.
  const std::string text = "\xE4\xB8\xAD\xE3\x80\x82";
  EXPECT_EQ(CjkKinsoku::repairBreakByteOffset(text, 0, 3), 3u);
}

TEST(CjkKinsoku, RepairBreakByteOffsetKeepsEllipsisRun) {
  // "あ……い" — candidate break between …… retreats so the run stays together.
  const std::string text = "\xE3\x81\x82\xE2\x80\xA6\xE2\x80\xA6\xE3\x81\x84";
  EXPECT_EQ(CjkKinsoku::repairBreakByteOffset(text, 0, 6), 3u);
}

#if defined(ENABLE_JAPANESE_VERSION)
TEST(CjkKinsoku, JapaneseSmallKanaAreLineStartProhibited) {
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3083));                   // ゃ
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3063));                   // っ
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x30FC));                   // ー
  EXPECT_FALSE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x304D, 0x3083));  // きゃ
}
#else
TEST(CjkKinsoku, NonJapaneseBuildAllowsBreakBeforeSmallKana) {
  // Without ENABLE_JAPANESE_VERSION, small kana are ordinary hiragana for break purposes.
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x3083));
  EXPECT_TRUE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x304D, 0x3083));
}
#endif
