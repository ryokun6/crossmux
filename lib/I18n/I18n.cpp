#include "I18n.h"

#include <cstddef>
#include <cstring>

#include "I18nStrings.h"

using namespace i18n_strings;

namespace {

Language findLanguageByPersistedCode(const char* code) {
  for (uint8_t i = 0; i < getLanguageCount(); i++) {
    if (strcmp(code, LANGUAGE_CODES[i]) == 0) {
      return static_cast<Language>(i);
    }
  }
  return Language::_COUNT;
}

Language findChineseDefaultForThisBuild() {
#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
  Language lang = findLanguageByPersistedCode("zh-CN");
  if (lang != Language::_COUNT) return lang;
#else
  Language lang = findLanguageByPersistedCode("zh-TW");
  if (lang != Language::_COUNT) return lang;
#endif
#endif
  // Fallbacks for full/global tables or mixed legacy values.
  Language tw = findLanguageByPersistedCode("zh-TW");
  if (tw != Language::_COUNT) return tw;
  Language cn = findLanguageByPersistedCode("zh-CN");
  if (cn != Language::_COUNT) return cn;
  return Language::EN;
}

}  // namespace

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

  Language direct = findLanguageByPersistedCode(code);
  if (direct != Language::_COUNT) return direct;

  // Enum-style and legacy aliases from earlier SKUs (ZH / ZH_CN / ZH_TW).
  if (strcmp(code, "ZH_TW") == 0 || strcmp(code, "ZH") == 0) {
    Language tw = findLanguageByPersistedCode("zh-TW");
    if (tw != Language::_COUNT) return tw;
    return findChineseDefaultForThisBuild();
  }
  if (strcmp(code, "ZH_CN") == 0) {
    Language cn = findLanguageByPersistedCode("zh-CN");
    if (cn != Language::_COUNT) return cn;
    // Pre-dual-SKU devices stored ZH_CN for "Chinese"; on TW firmware that
    // meant Traditional UI.
    return findChineseDefaultForThisBuild();
  }
  return Language::EN;
}

bool I18n::isLanguageAvailable(Language lang) {
  if (static_cast<uint8_t>(lang) >= getLanguageCount()) return false;
#ifndef ENABLE_CHINESE_VERSION
  // CJK glyphs ship only with Chinese SKUs; hide Chinese locales on global.
  // Full international tables include ZH_TW (from chinese.yaml) but no CJK font.
  if (lang == Language::ZH_TW) return false;
#endif
#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
  // SC firmware tables are EN+ZH_CN only — nothing else to hide.
#else
  // TW firmware tables are EN+ZH_TW only — nothing else to hide.
#endif
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
