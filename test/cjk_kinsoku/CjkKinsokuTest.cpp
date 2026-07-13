#include <gtest/gtest.h>

#include "CjkKinsoku.h"

namespace {

// Pure break-policy helper matching ParsedText's candidate filter: a break after
// `left` (next line starts with `right`) is legal only when 禁則 allows it.
bool mayBreakAfter(const uint32_t left, const uint32_t right) { return CjkKinsoku::isLegalBreakBetween(left, right); }

}  // namespace

TEST(CjkKinsoku, LineStartProhibitedCoversStopsAndClosers) {
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3002));  // 。
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3001));  // 、
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFF0C));  // ，
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x300D));  // 」
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0xFF09));  // ）
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x30FB));  // ・
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x2026));  // …
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x4E2D));  // 中
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x300C));  // 「
}

TEST(CjkKinsoku, LineEndProhibitedCoversOpenersAndCurrency) {
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0x300C));  // 「
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFF08));  // （
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0x201C));  // “
  EXPECT_TRUE(CjkKinsoku::isLineEndProhibited(0xFFE5));  // ￥
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

#if defined(ENABLE_JAPANESE_VERSION)
TEST(CjkKinsoku, JapaneseSmallKanaAreLineStartProhibited) {
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3083));  // ゃ
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3063));  // っ
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x30FC));  // ー
  EXPECT_FALSE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x304D, 0x3083));  // きゃ
}
#else
TEST(CjkKinsoku, NonJapaneseBuildAllowsBreakBeforeSmallKana) {
  // Without ENABLE_JAPANESE_VERSION, small kana are ordinary hiragana for break purposes.
  EXPECT_FALSE(CjkKinsoku::isLineStartProhibited(0x3083));
  EXPECT_TRUE(CjkKinsoku::hasCjkBreakOpportunityBetween(0x304D, 0x3083));
}
#endif
