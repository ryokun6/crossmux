#pragma once

#include <cstddef>
#include <string>

#include "LanguageHyphenator.h"

// CJK SKUs keep English only (~323 KB of DE/RU/… tries stay out of flash).
// International firmware embeds the full Latin/Cyrillic set.
#ifdef ENABLE_CJK_VERSION
inline constexpr size_t kEmbeddedHyphenationLanguageCount = 1;
#else
inline constexpr size_t kEmbeddedHyphenationLanguageCount = 10;
#endif

struct LanguageEntry {
  const char* cliName;
  const char* primaryTag;
  const LanguageHyphenator* hyphenator;
};

struct LanguageEntryView {
  const LanguageEntry* data;
  size_t size;

  const LanguageEntry* begin() const { return data; }
  const LanguageEntry* end() const { return data + size; }
};

// Returns the Liang-backed hyphenator for a given primary language tag (e.g., "en", "fr").
const LanguageHyphenator* getLanguageHyphenatorForPrimaryTag(const std::string& primaryTag);

// Exposes the list of supported languages primarily for tooling/tests.
LanguageEntryView getLanguageEntries();
