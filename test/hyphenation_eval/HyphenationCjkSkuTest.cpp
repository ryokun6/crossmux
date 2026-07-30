#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "lib/Epub/Epub/hyphenation/Hyphenator.h"
#include "lib/Epub/Epub/hyphenation/LanguageRegistry.h"

TEST(HyphenationCjkSku, EnglishOnlyRegistry) {
  ASSERT_EQ(kEmbeddedHyphenationLanguageCount, 1u);
  const auto entries = getLanguageEntries();
  ASSERT_EQ(entries.size, 1u);
  ASSERT_NE(entries.data, nullptr);
  EXPECT_STREQ(entries.data[0].primaryTag, "en");
  EXPECT_NE(getLanguageHyphenatorForPrimaryTag("en"), nullptr);
  EXPECT_EQ(getLanguageHyphenatorForPrimaryTag("de"), nullptr);
  EXPECT_EQ(getLanguageHyphenatorForPrimaryTag("fr"), nullptr);
  EXPECT_EQ(getLanguageHyphenatorForPrimaryTag("ru"), nullptr);
}

TEST(HyphenationCjkSku, NonEnglishHasNoInsertedPatternBreaks) {
  Hyphenator::setPreferredLanguage("de");
  const auto legalBreaks = Hyphenator::breakOffsets("Quadratkilometer", false);
  EXPECT_TRUE(std::none_of(legalBreaks.begin(), legalBreaks.end(),
                           [](const auto& info) { return info.requiresInsertedHyphen; }));
}

TEST(HyphenationCjkSku, EnglishPatternsStillWork) {
  Hyphenator::setPreferredLanguage("en");
  const auto legalBreaks = Hyphenator::breakOffsets("hyphenation", false);
  EXPECT_TRUE(std::any_of(legalBreaks.begin(), legalBreaks.end(),
                          [](const auto& info) { return info.requiresInsertedHyphen; }));
}
