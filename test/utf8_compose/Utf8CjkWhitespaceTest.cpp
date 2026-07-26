#include <gtest/gtest.h>

#include "Utf8.h"

namespace {

constexpr uint32_t kHanZhong = 0x4E2D;    // 中
constexpr uint32_t kHanWen = 0x6587;      // 文
constexpr uint32_t kHiraA = 0x3042;       // あ
constexpr uint32_t kKataKa = 0x30AB;      // カ
constexpr uint32_t kHangulHan = 0xD55C;   // 한
constexpr uint32_t kHangulGeul = 0xAE00;  // 글
constexpr uint32_t kLatinA = 'A';

}  // namespace

TEST(Utf8IsHangul, ClassifiesHangulRanges) {
  EXPECT_TRUE(utf8IsHangul(kHangulHan));
  EXPECT_TRUE(utf8IsHangul(0x1100));  // Hangul Jamo
  EXPECT_FALSE(utf8IsHangul(kHanZhong));
  EXPECT_FALSE(utf8IsHangul(kHiraA));
  EXPECT_FALSE(utf8IsHangul(kKataKa));
  EXPECT_FALSE(utf8IsHangul(kLatinA));
}

// Pretty-printed Chinese/Japanese: newline (with or without indent spaces)
// between no-space CJK must NOT become a rendered gap.
TEST(Utf8CjkWhitespace, RemovesSegmentBreakBetweenNoSpaceCjk) {
  EXPECT_FALSE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/false, /*hadSegmentBreak=*/true, kHanZhong, kHanWen));
  EXPECT_FALSE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/true, /*hadSegmentBreak=*/true, kHanZhong,
                                             kHanWen));  // indent spaces around \n
  EXPECT_FALSE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/true, /*hadSegmentBreak=*/true, kHiraA, kKataKa));
}

// Korean word spacing: spaces-only runs keep a gap; Hangul is also exempt from
// segment-break removal so linebreak-separated Hangul still spaces.
TEST(Utf8CjkWhitespace, KeepsKoreanWordSpacing) {
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/true, /*hadSegmentBreak=*/false, kHangulHan, kHangulGeul));
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/false, /*hadSegmentBreak=*/true, kHangulHan, kHangulGeul));
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/true, /*hadSegmentBreak=*/true, kHangulHan, kHangulGeul));
}

// Intentional same-line space between Chinese ideographs still survives (no \n).
TEST(Utf8CjkWhitespace, KeepsIntentionalSameLineSpaceInChinese) {
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/true, /*hadSegmentBreak=*/false, kHanZhong, kHanWen));
}

// Mixed / non-CJK neighbors keep a space when a segment break is present.
TEST(Utf8CjkWhitespace, KeepsSegmentBreakBesideNonCjkOrHangul) {
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/false, /*hadSegmentBreak=*/true, kHanZhong, kLatinA));
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/false, /*hadSegmentBreak=*/true, kLatinA, kHanWen));
  EXPECT_TRUE(utf8CjkWhitespaceBecomesSpace(/*hadRealSpace=*/false, /*hadSegmentBreak=*/true, kHangulHan, kHanZhong));
}
