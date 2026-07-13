#include "I18n.h"

#include <cstddef>
#include <cstring>

#include "I18nStrings.h"

using namespace i18n_strings;

I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}

const char* I18n::get(StrId id) const {
  const auto index = static_cast<size_t>(id);
  if (index >= static_cast<size_t>(StrId::_COUNT)) {
    return "???";
  }

  // Use generated helper function - no hardcoded switch needed!
  const LangStrings lang = getLanguageStrings(_language);

  // If bit 15 of the offset is set, apply the offset to the English lookup table
  const uint16_t off = lang.offsets[index];
  if (off & 0x8000) return STRINGS_EN_DATA + (off & 0x7FFF);
  return lang.data + off;
}

void I18n::setLanguage(Language lang) {
  if (lang >= Language::_COUNT) {
    return;
  }
  _language = lang;
}

const char* I18n::getLanguageName(Language lang) const {
  const auto index = static_cast<size_t>(lang);
  if (index >= static_cast<size_t>(Language::_COUNT)) {
    return "???";
  }
  return LANGUAGE_NAMES[index];
}

Language I18n::languageFromCode(const char* code) {
  if (code == nullptr) return Language::EN;
  for (uint8_t i = 0; i < getLanguageCount(); i++) {
    if (strcmp(code, LANGUAGE_CODES[i]) == 0) return static_cast<Language>(i);
  }
  // Legacy settings.json / --lang values before the ZH_CN → ZH rename.
  if (strcmp(code, "ZH_CN") == 0) {
    for (uint8_t i = 0; i < getLanguageCount(); i++) {
      if (strcmp(LANGUAGE_CODES[i], "ZH") == 0) return static_cast<Language>(i);
    }
  }
  return Language::EN;
}

bool I18n::isLanguageAvailable(Language lang) {
  if (static_cast<uint8_t>(lang) >= getLanguageCount()) return false;
#ifndef ENABLE_CHINESE_VERSION
  // CJK glyphs ship only with the Chinese SKU; the global build embeds Latin
  // fonts only, so Chinese strings render as garbled boxes even though ZH's
  // strings are compiled in. Treat it as unavailable here.
  if (lang == Language::ZH) return false;
#endif
  return true;
}

// Generate character set for a specific language
const char* I18n::getCharacterSet(Language lang) {
  const auto langIndex = static_cast<size_t>(lang);
  if (langIndex >= static_cast<size_t>(Language::_COUNT)) {
    lang = Language::EN;  // Fallback to first language
  }

  return CHARACTER_SETS[static_cast<size_t>(lang)];
}
