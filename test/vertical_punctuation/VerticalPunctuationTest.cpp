#include <gtest/gtest.h>

#include <string>

#include "VerticalPunctuation.h"

namespace {

const std::string kTwoDotLeader = "\xE2\x80\xA5";                // U+2025 ‥
const std::string kHorizontalEllipsis = "\xE2\x80\xA6";          // U+2026 …
const std::string kVerticalEllipsis = "\xE2\x8B\xAE";            // U+22EE ⋮
const std::string kMidlineEllipsis = "\xE2\x8B\xAF";             // U+22EF ⋯
const std::string kVerticalHorizontalEllipsis = "\xEF\xB8\x99";  // U+FE19 ︙
const std::string kVerticalTwoDotLeader = "\xEF\xB8\xB0";        // U+FE30 ︰
const std::string kEnDash = "\xE2\x80\x93";                      // U+2013 –
const std::string kEmDash = "\xE2\x80\x94";                      // U+2014 —
const std::string kHorizontalBar = "\xE2\x80\x95";               // U+2015 ―
const std::string kVerticalEmDash = "\xEF\xB8\xB1";              // U+FE31 ︱
const std::string kVerticalEnDash = "\xEF\xB8\xB2";              // U+FE32 ︲
const std::string kVerticalClosingDoubleQuote = "\xEF\xB9\x82";  // U+FE42 ﹂
const std::string kTextVariationSelector = "\xEF\xB8\x8E";       // U+FE0E

}  // namespace

TEST(VerticalPunctuation, NormalizesEveryEllipsisVariant) {
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x2025), 0xFE30u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0xFE30), 0xFE30u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x2026), 0xFE19u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x22EE), 0xFE19u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x22EF), 0xFE19u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0xFE19), 0xFE19u);
}

TEST(VerticalPunctuation, CountsRepeatedEllipsisVariantsAsSeparateCells) {
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kTwoDotLeader + kTwoDotLeader), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kHorizontalEllipsis + kHorizontalEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kVerticalEllipsis + kVerticalEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kMidlineEllipsis + kMidlineEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kVerticalHorizontalEllipsis + kVerticalHorizontalEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kVerticalTwoDotLeader + kVerticalTwoDotLeader), 2u);
}

TEST(VerticalPunctuation, CountsMixedEllipsisEncodingsAsOneRun) {
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kHorizontalEllipsis + kMidlineEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kTwoDotLeader + kVerticalHorizontalEllipsis), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kMidlineEllipsis + kTextVariationSelector + kVerticalEllipsis), 2u);
}

TEST(VerticalPunctuation, CountsUnderstandingMediaEllipsisAndClosingQuoteAsThreeCells) {
  const std::string remappedChapterToken =
      kVerticalHorizontalEllipsis + kVerticalHorizontalEllipsis + kVerticalClosingDoubleQuote;
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(remappedChapterToken), 3u);
}

TEST(VerticalPunctuation, PreservesRepeatedDashFamilyBehavior) {
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kEmDash + kEmDash), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kEnDash + kHorizontalBar), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kVerticalEmDash + kVerticalEnDash), 2u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kMidlineEllipsis + kEmDash), 2u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x2013), 0xFE32u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x2014), 0xFE31u);
  EXPECT_EQ(VerticalPunctuation::presentationCodepoint(0x2015), 0xFE31u);
}

TEST(VerticalPunctuation, RejectsSingletonsAndSidewaysText) {
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kMidlineEllipsis), 0u);
  EXPECT_EQ(VerticalPunctuation::stackedRunLength(kMidlineEllipsis + "x"), 0u);
}
