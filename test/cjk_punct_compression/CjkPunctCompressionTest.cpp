#include <gtest/gtest.h>

#include "CjkPunctCompression.h"

using CjkPunctCompression::Class;
using CjkPunctCompression::Profile;

TEST(CjkPunctCompression, ClassifyOpenClosePauseMiddle) {
  EXPECT_EQ(CjkPunctCompression::classify(0x300C), Class::OpenBracket);   // 「
  EXPECT_EQ(CjkPunctCompression::classify(0xFF08), Class::OpenBracket);   // （
  EXPECT_EQ(CjkPunctCompression::classify(0xFE41), Class::OpenBracket);   // ﹁
  EXPECT_EQ(CjkPunctCompression::classify(0x300D), Class::CloseBracket);  // 」
  EXPECT_EQ(CjkPunctCompression::classify(0xFF09), Class::CloseBracket);  // ）
  EXPECT_EQ(CjkPunctCompression::classify(0xFE42), Class::CloseBracket);  // ﹂
  EXPECT_EQ(CjkPunctCompression::classify(0x3002), Class::PauseStop);     // 。
  EXPECT_EQ(CjkPunctCompression::classify(0xFF0C), Class::PauseStop);     // ，
  EXPECT_EQ(CjkPunctCompression::classify(0xFE12), Class::PauseStop);     // ︒
  EXPECT_EQ(CjkPunctCompression::classify(0x30FB), Class::MiddleDot);     // ・
  EXPECT_EQ(CjkPunctCompression::classify(0x4E2D), Class::None);          // 中
}

TEST(CjkPunctCompression, AdjacentTcSkipsPauseStopPairs) {
  constexpr int em = 20;
  // TC: 。」 not compressed
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::PauseStop, Class::CloseBracket, Profile::TraditionalChinese, em),
            0);
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::PauseStop, Class::OpenBracket, Profile::TraditionalChinese, em),
            0);
  // TC: 「『 still compressed
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::OpenBracket, Class::OpenBracket, Profile::TraditionalChinese, em),
            em / 2);
  EXPECT_EQ(
      CjkPunctCompression::adjacentTrimPx(Class::CloseBracket, Class::OpenBracket, Profile::TraditionalChinese, em),
      em / 2);
}

TEST(CjkPunctCompression, AdjacentScAndJaCompressPauseStop) {
  constexpr int em = 20;
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::PauseStop, Class::CloseBracket, Profile::SimplifiedChinese, em),
            em / 2);
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::PauseStop, Class::CloseBracket, Profile::Japanese, em), em / 2);
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::OpenBracket, Class::OpenBracket, Profile::SimplifiedChinese, em),
            em / 2);
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::MiddleDot, Class::MiddleDot, Profile::Japanese, em), em / 4);
}

TEST(CjkPunctCompression, LineEdgeTcOmitsPauseStopEnd) {
  constexpr int em = 20;
  EXPECT_EQ(CjkPunctCompression::lineStartTrimPx(Class::OpenBracket, Profile::TraditionalChinese, em), em / 2);
  EXPECT_EQ(CjkPunctCompression::lineEndTrimPx(Class::CloseBracket, Profile::TraditionalChinese, em), em / 2);
  EXPECT_EQ(CjkPunctCompression::lineEndTrimPx(Class::PauseStop, Profile::TraditionalChinese, em), 0);
  EXPECT_EQ(CjkPunctCompression::lineEndTrimPx(Class::PauseStop, Profile::SimplifiedChinese, em), em / 2);
  EXPECT_EQ(CjkPunctCompression::lineEndTrimPx(Class::PauseStop, Profile::Japanese, em), em / 2);
}

TEST(CjkPunctCompression, OffProfileAlwaysZero) {
  constexpr int em = 20;
  EXPECT_EQ(CjkPunctCompression::adjacentTrimPx(Class::OpenBracket, Class::OpenBracket, Profile::Off, em), 0);
  EXPECT_EQ(CjkPunctCompression::lineStartTrimPx(Class::OpenBracket, Profile::Off, em), 0);
  EXPECT_EQ(CjkPunctCompression::lineEndTrimPx(Class::CloseBracket, Profile::Off, em), 0);
}

TEST(CjkPunctCompression, ApplyAdjacentRespectsKinsokuInseparable) {
  constexpr int em = 20;
  std::vector<std::string> words = {"\xe2\x80\xa6", "\xe2\x80\xa6"};  // ……
  std::vector<bool> continues = {false, false};
  std::vector<uint16_t> advances = {20, 20};
  CjkPunctCompression::applyAdjacentPunctCompression(words, continues, advances, em, Profile::SimplifiedChinese);
  EXPECT_EQ(advances[0], 20);
  EXPECT_EQ(advances[1], 20);
}

TEST(CjkPunctCompression, ApplyAdjacentScCompressesStopCloser) {
  constexpr int em = 20;
  std::vector<std::string> words = {"\xe3\x80\x82", "\xe3\x80\x8d"};  // 。」
  std::vector<bool> continues = {false, false};
  std::vector<uint16_t> advances = {20, 20};
  CjkPunctCompression::applyAdjacentPunctCompression(words, continues, advances, em, Profile::SimplifiedChinese);
  EXPECT_EQ(advances[0], 10);
  EXPECT_EQ(advances[1], 20);

  advances = {20, 20};
  CjkPunctCompression::applyAdjacentPunctCompression(words, continues, advances, em, Profile::TraditionalChinese);
  EXPECT_EQ(advances[0], 20);
  EXPECT_EQ(advances[1], 20);
}

TEST(CjkPunctCompression, EdgeTrimAfterKinsokuBounds) {
  constexpr int em = 20;
  // Line that starts with 「 and ends with 。 — SC trims both edges; TC trims start only.
  std::vector<std::string> words = {"\xe3\x80\x8c", "\xe4\xb8\xad", "\xe3\x80\x82"};  // 「中。
  auto sc = CjkPunctCompression::lineEdgeTrimPx(words, 0, 3, Profile::SimplifiedChinese, em);
  EXPECT_EQ(sc.startTrim, 10);
  EXPECT_EQ(sc.endTrim, 10);
  auto tc = CjkPunctCompression::lineEdgeTrimPx(words, 0, 3, Profile::TraditionalChinese, em);
  EXPECT_EQ(tc.startTrim, 10);
  EXPECT_EQ(tc.endTrim, 0);
}

// Kinsoku still refuses a break before 。 even when compression would tighten the pair.
TEST(CjkPunctCompression, KinsokuStillBlocksBreakBeforeCompressedStop) {
  EXPECT_FALSE(CjkKinsoku::isLegalBreakBetween(0x4E2D, 0x3002));                         // 中。
  EXPECT_FALSE(CjkKinsoku::isLegalBreakBetween(0x3002, 0x300D));                         // 。」
  EXPECT_TRUE(CjkKinsoku::isLineStartProhibited(0x3002));
  std::vector<std::string> words = {"\xe4\xb8\xad", "\xe3\x80\x82", "\xe3\x80\x8d"};  // 中。」
  std::vector<bool> continues = {false, false, false};
  // Candidate break after 中 would put 。 at line head — repair retreats.
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 1), 1u);
  // Break after 。」 is fine at end; break between 。 and 」 retreats.
  EXPECT_EQ(CjkKinsoku::repairBreakIndex(words, continues, 0, 2), 1u);
}
