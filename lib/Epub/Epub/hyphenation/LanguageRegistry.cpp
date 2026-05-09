#include "LanguageRegistry.h"

#include <algorithm>
#include <iterator>

#include "HyphenationCommon.h"
#include "generated/hyph-en.trie.h"
#ifndef OMIT_HYPHEN_FR
#include "generated/hyph-fr.trie.h"
#endif
#ifndef OMIT_HYPHEN_DE
#include "generated/hyph-de.trie.h"
#endif
#ifndef OMIT_HYPHEN_RU
#include "generated/hyph-ru.trie.h"
#endif
#ifndef OMIT_HYPHEN_ES
#include "generated/hyph-es.trie.h"
#endif
#ifndef OMIT_HYPHEN_IT
#include "generated/hyph-it.trie.h"
#endif
#ifndef OMIT_HYPHEN_UK
#include "generated/hyph-uk.trie.h"
#endif

namespace {

// English hyphenation patterns (3/3 minimum prefix/suffix length)
LanguageHyphenator englishHyphenator(en_patterns, isLatinLetter, toLowerLatin, 3, 3);
#ifndef OMIT_HYPHEN_FR
LanguageHyphenator frenchHyphenator(fr_patterns, isLatinLetter, toLowerLatin);
#endif
#ifndef OMIT_HYPHEN_DE
LanguageHyphenator germanHyphenator(de_patterns, isLatinLetter, toLowerLatin);
#endif
#ifndef OMIT_HYPHEN_RU
LanguageHyphenator russianHyphenator(ru_patterns, isCyrillicLetter, toLowerCyrillic);
#endif
#ifndef OMIT_HYPHEN_ES
LanguageHyphenator spanishHyphenator(es_patterns, isLatinLetter, toLowerLatin);
#endif
#ifndef OMIT_HYPHEN_IT
LanguageHyphenator italianHyphenator(it_patterns, isLatinLetter, toLowerLatin);
#endif
#ifndef OMIT_HYPHEN_UK
LanguageHyphenator ukrainianHyphenator(uk_patterns, isCyrillicLetter, toLowerCyrillic);
#endif

// Plain C array so its size is deduced — entries vanish via OMIT_HYPHEN_* macros.
const LanguageEntry kEntries[] = {
    {"english", "en", &englishHyphenator},
#ifndef OMIT_HYPHEN_FR
    {"french", "fr", &frenchHyphenator},
#endif
#ifndef OMIT_HYPHEN_DE
    {"german", "de", &germanHyphenator},
#endif
#ifndef OMIT_HYPHEN_RU
    {"russian", "ru", &russianHyphenator},
#endif
#ifndef OMIT_HYPHEN_ES
    {"spanish", "es", &spanishHyphenator},
#endif
#ifndef OMIT_HYPHEN_IT
    {"italian", "it", &italianHyphenator},
#endif
#ifndef OMIT_HYPHEN_UK
    {"ukrainian", "uk", &ukrainianHyphenator},
#endif
};

}  // namespace

const LanguageHyphenator* getLanguageHyphenatorForPrimaryTag(const std::string& primaryTag) {
  const auto* end = std::end(kEntries);
  const auto it = std::find_if(std::begin(kEntries), end,
                               [&primaryTag](const LanguageEntry& entry) { return primaryTag == entry.primaryTag; });
  return (it != end) ? it->hyphenator : nullptr;
}

LanguageEntryView getLanguageEntries() { return LanguageEntryView{kEntries, std::size(kEntries)}; }
