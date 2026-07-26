#include <WordList.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

std::vector<std::string> collect(const WordList& words) {
  std::vector<std::string> out;
  out.reserve(words.size());
  for (size_t i = 0; i < words.size(); ++i) {
    out.emplace_back(words[i]);
  }
  return out;
}

WordList makeList(const std::vector<std::string>& tokens) {
  WordList words;
  for (const auto& token : tokens) {
    words.push_back(token);
  }
  return words;
}

}  // namespace

TEST(WordList, EmptyByDefault) {
  const WordList words;
  EXPECT_EQ(words.size(), 0u);
  EXPECT_TRUE(words.empty());
  EXPECT_EQ(words.view().size(), 0u);
}

TEST(WordList, StoresTokensAndKeepsThemNulTerminated) {
  const WordList words = makeList({"\xE4\xB8\xAD", "\xE6\x96\x87", "abc"});  // 中 文 abc
  ASSERT_EQ(words.size(), 3u);
  EXPECT_EQ(words[0], "\xE4\xB8\xAD");
  EXPECT_EQ(words[2], "abc");
  EXPECT_EQ(words.back(), "abc");
  EXPECT_STREQ(words.cStr(1), "\xE6\x96\x87");
  EXPECT_STREQ(words.cStr(2), "abc");
}

TEST(WordList, ViewMatchesOwner) {
  const WordList words = makeList({"one", "two", "three"});
  const WordListView view = words.view();
  ASSERT_EQ(view.size(), 3u);
  EXPECT_EQ(view[1], "two");
  EXPECT_STREQ(view.cStr(2), "three");

  std::vector<std::string> iterated;
  for (const char* token : view) {
    iterated.emplace_back(token);
  }
  EXPECT_EQ(iterated, (std::vector<std::string>{"one", "two", "three"}));
}

TEST(WordList, ReplaceHandlesEveryLengthChange) {
  WordList words = makeList({"aa", "bb", "cc"});

  words.replace(1, "XY");  // same length
  EXPECT_EQ(collect(words), (std::vector<std::string>{"aa", "XY", "cc"}));

  words.replace(1, "\xEF\xBC\xB9");  // longer: Ｙ
  EXPECT_EQ(collect(words), (std::vector<std::string>{"aa", "\xEF\xBC\xB9", "cc"}));
  EXPECT_STREQ(words.cStr(2), "cc");

  words.replace(1, "z");  // shorter
  EXPECT_EQ(collect(words), (std::vector<std::string>{"aa", "z", "cc"}));
  EXPECT_STREQ(words.cStr(2), "cc");
}

TEST(WordList, SplitAtInsertsHyphenatedPrefixAndRemainder) {
  WordList words = makeList({"200", "Quadratkilometer", "tail"});
  words.splitAt(1, 7, /*appendHyphenToPrefix=*/true);
  EXPECT_EQ(collect(words), (std::vector<std::string>{"200", "Quadrat-", "kilometer", "tail"}));
  EXPECT_STREQ(words.cStr(1), "Quadrat-");
  EXPECT_STREQ(words.cStr(2), "kilometer");
  EXPECT_STREQ(words.cStr(3), "tail");
}

TEST(WordList, SplitAtExistingSeparatorAddsNoHyphen) {
  WordList words = makeList({"US-Satellit", "tail"});
  words.splitAt(0, 3, /*appendHyphenToPrefix=*/false);
  EXPECT_EQ(collect(words), (std::vector<std::string>{"US-", "Satellit", "tail"}));
  EXPECT_STREQ(words.cStr(1), "Satellit");
}

TEST(WordList, EraseFrontDropsConsumedTokens) {
  WordList words = makeList({"a", "bb", "ccc", "dddd"});
  words.eraseFront(2);
  EXPECT_EQ(collect(words), (std::vector<std::string>{"ccc", "dddd"}));
  EXPECT_STREQ(words.cStr(0), "ccc");
  EXPECT_STREQ(words.cStr(1), "dddd");

  words.push_back("e");
  EXPECT_EQ(collect(words), (std::vector<std::string>{"ccc", "dddd", "e"}));

  words.eraseFront(words.size());
  EXPECT_TRUE(words.empty());
  words.push_back("fresh");
  EXPECT_EQ(collect(words), (std::vector<std::string>{"fresh"}));
}

TEST(WordList, SurvivesMutationsAfterReserve) {
  WordList words;
  words.reserveMore(4, 16);
  for (const char* token : {"\xE4\xB8\xAD", "\xE6\x96\x87", "\xE3\x80\x82"}) {  // 中 文 。
    words.push_back(token);
  }
  words.replace(0, "\xEF\xBC\xA1\xEF\xBC\xA2");  // ＡＢ, longer than 中
  words.splitAt(0, 3, /*appendHyphenToPrefix=*/false);
  words.eraseFront(1);
  EXPECT_EQ(collect(words), (std::vector<std::string>{"\xEF\xBC\xA2", "\xE6\x96\x87", "\xE3\x80\x82"}));
  EXPECT_STREQ(words.cStr(0), "\xEF\xBC\xA2");
}
