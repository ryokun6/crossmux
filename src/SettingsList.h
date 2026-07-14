#pragma once

#include <HalClock.h>
#include <HalTiltSensor.h>
#include <I18n.h>
#include <SdCardFontRegistry.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

#include "CrossPointSettings.h"
#include "KOReaderCredentialStore.h"
#include "activities/settings/SettingsActivity.h"

// Build the font family setting dynamically. When registry is non-null, SD card fonts
// are appended after the built-in fonts. Otherwise only built-in fonts are listed.
inline SettingInfo buildFontFamilySetting(const SdCardFontRegistry* registry) {
  // Built-in font labels (StrId)
  std::vector<StrId> enumValues = {StrId::STR_NOTO_SERIF, StrId::STR_NOTO_SANS};
  // Runtime string labels for SD card fonts
  std::vector<std::string> enumStringValues;

  // Reserve: first CrossPointSettings::BUILTIN_FONT_COUNT entries use StrId, rest use strings
  if (registry) {
    const auto& families = registry->getFamilies();
    enumStringValues.reserve(families.size());
    std::transform(families.begin(), families.end(), std::back_inserter(enumStringValues),
                   [](const SdCardFontFamilyInfo& f) { return f.name; });
  }

  // Capture the SD font count for the lambdas
  const int sdFontCount = static_cast<int>(enumStringValues.size());

  // Total option count = built-in + SD card families
  // For the combined enumStringValues: we need all entries as strings (built-in names + SD names)
  // The render code checks enumStringValues first, then enumValues. So we build enumStringValues
  // with all options when SD fonts are present.
  std::vector<std::string> allStringValues;
  if (sdFontCount > 0) {
    allStringValues.push_back(I18N.get(StrId::STR_NOTO_SERIF));
    allStringValues.push_back(I18N.get(StrId::STR_NOTO_SANS));
    allStringValues.insert(allStringValues.end(), enumStringValues.begin(), enumStringValues.end());
  }

  SettingInfo s;
  s.nameId = StrId::STR_FONT_FAMILY;
  s.type = SettingType::ENUM;
  s.enumValues = std::move(enumValues);
  s.enumStringValues = std::move(allStringValues);
  s.key = "fontFamily";
  s.category = StrId::STR_CAT_READER;

  // Capture registry families by copy for the lambdas
  std::vector<std::string> sdFamilyNames;
  if (registry) {
    const auto& families = registry->getFamilies();
    sdFamilyNames.reserve(families.size());
    std::transform(families.begin(), families.end(), std::back_inserter(sdFamilyNames),
                   [](const SdCardFontFamilyInfo& f) { return f.name; });
  }

  s.valueGetter = [sdFamilyNames]() -> uint8_t {
    // If an SD card font is selected, find its index
    if (SETTINGS.sdFontFamilyName[0] != '\0') {
      for (int i = 0; i < static_cast<int>(sdFamilyNames.size()); i++) {
        if (sdFamilyNames[i] == SETTINGS.sdFontFamilyName) {
          return static_cast<uint8_t>(CrossPointSettings::BUILTIN_FONT_COUNT + i);
        }
      }
      // SD font name not found in registry — fall through to built-in
    }
    return SETTINGS.fontFamily < CrossPointSettings::BUILTIN_FONT_COUNT ? SETTINGS.fontFamily : 0;
  };

  s.valueSetter = [sdFamilyNames](uint8_t v) {
    if (v < CrossPointSettings::BUILTIN_FONT_COUNT) {
      SETTINGS.fontFamily = v;
      SETTINGS.sdFontFamilyName[0] = '\0';
    } else {
      int sdIdx = v - CrossPointSettings::BUILTIN_FONT_COUNT;
      if (sdIdx < static_cast<int>(sdFamilyNames.size())) {
        strncpy(SETTINGS.sdFontFamilyName, sdFamilyNames[sdIdx].c_str(), sizeof(SETTINGS.sdFontFamilyName) - 1);
        SETTINGS.sdFontFamilyName[sizeof(SETTINGS.sdFontFamilyName) - 1] = '\0';
      }
    }
  };

  return s;
}

// Shared settings list used by both the device settings UI and the web settings API.
// Each entry has a key (for JSON API) and category (for grouping).
// ACTION-type entries and entries without a key are device-only.
//
// The static list is constructed exactly once (master's optimization, #1086 +
// #1636) so the per-entry SettingInfo cost is paid once. When an
// SdCardFontRegistry is supplied AND has SD card fonts installed, the
// font-family entry is replaced in a per-call copy with a registry-aware
// version. Callers without SD fonts pay only a vector copy.
inline std::vector<SettingInfo> getSettingsList(const SdCardFontRegistry* registry = nullptr) {
  static const std::vector<SettingInfo> baseList = [] {
    // Build via push_back (not a giant braced-init) so only one SettingInfo
    // temporary (~208 B) lives on the stack at a time. Braced-init of ~50
    // entries overflows the 8 KB Arduino loopTask stack at first boot load.
    std::vector<SettingInfo> v;
    v.reserve(64);
    // --- Display ---
    v.push_back(SettingInfo::Enum(StrId::STR_SLEEP_SCREEN, &CrossPointSettings::sleepScreen,
                                  {StrId::STR_DARK, StrId::STR_LIGHT, StrId::STR_CUSTOM, StrId::STR_COVER,
                                   StrId::STR_NONE_OPT, StrId::STR_COVER_CUSTOM, StrId::STR_QUICK_RESUME},
                                  "sleepScreen", StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(StrId::STR_SLEEP_COVER_MODE, &CrossPointSettings::sleepScreenCoverMode,
                                  {StrId::STR_FIT, StrId::STR_CROP}, "sleepScreenCoverMode", StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(StrId::STR_SLEEP_COVER_FILTER, &CrossPointSettings::sleepScreenCoverFilter,
                                  {StrId::STR_NONE_OPT, StrId::STR_FILTER_CONTRAST, StrId::STR_INVERTED},
                                  "sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(StrId::STR_QUICK_RESUME_TIMEOUT, &CrossPointSettings::quickResumeSleepScreen,
                                  {StrId::STR_STATE_OFF, StrId::STR_STATE_ON}, "quickResumeSleepScreen",
                                  StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(StrId::STR_HIDE_BATTERY, &CrossPointSettings::hideBatteryPercentage,
                                  {StrId::STR_NEVER, StrId::STR_IN_READER, StrId::STR_ALWAYS}, "hideBatteryPercentage",
                                  StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(
        StrId::STR_REFRESH_FREQ, &CrossPointSettings::refreshFrequency,
        {StrId::STR_PAGES_1, StrId::STR_PAGES_5, StrId::STR_PAGES_10, StrId::STR_PAGES_15, StrId::STR_PAGES_30},
        "refreshFrequency", StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Enum(
        StrId::STR_UI_THEME, &CrossPointSettings::uiTheme,
        {StrId::STR_THEME_CLASSIC, StrId::STR_THEME_LYRA, StrId::STR_THEME_LYRA_EXTENDED, StrId::STR_THEME_ROUNDEDRAFF},
        "uiTheme", StrId::STR_CAT_DISPLAY));
    v.push_back(SettingInfo::Toggle(StrId::STR_SUNLIGHT_FADING_FIX, &CrossPointSettings::fadingFix, "fadingFix",
                                    StrId::STR_CAT_DISPLAY));
    // --- Reader ---
    // Built-in font-family entry. Replaced per-call with a registry-aware
    // version when SD fonts are installed.
    // --- Reader ---
    // Built-in font-family entry. Replaced per-call with a registry-aware
    // version when SD fonts are installed.
    v.push_back(SettingInfo::Enum(StrId::STR_FONT_FAMILY, &CrossPointSettings::fontFamily,
                                  {StrId::STR_NOTO_SERIF, StrId::STR_NOTO_SANS}, "fontFamily", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(StrId::STR_FONT_SIZE, &CrossPointSettings::fontSize,
                                  {StrId::STR_SMALL, StrId::STR_MEDIUM, StrId::STR_LARGE, StrId::STR_X_LARGE},
                                  "fontSize", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(StrId::STR_LINE_SPACING, &CrossPointSettings::lineSpacing,
                                  {StrId::STR_TIGHT, StrId::STR_NORMAL, StrId::STR_WIDE}, "lineSpacing",
                                  StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Value(StrId::STR_SCREEN_MARGIN, &CrossPointSettings::screenMargin, {5, 40, 5},
                                   "screenMargin", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(
        StrId::STR_PARA_ALIGNMENT, &CrossPointSettings::paragraphAlignment,
        {StrId::STR_JUSTIFY, StrId::STR_ALIGN_LEFT, StrId::STR_CENTER, StrId::STR_ALIGN_RIGHT, StrId::STR_BOOK_S_STYLE},
        "paragraphAlignment", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Toggle(StrId::STR_EMBEDDED_STYLE, &CrossPointSettings::embeddedStyle, "embeddedStyle",
                                    StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Toggle(StrId::STR_FOCUS_READING, &CrossPointSettings::focusReadingEnabled,
                                    "focusReadingEnabled", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Toggle(StrId::STR_HYPHENATION, &CrossPointSettings::hyphenationEnabled,
                                    "hyphenationEnabled", StrId::STR_CAT_READER));
#if defined(ENABLE_CHINESE_VERSION) || defined(ENABLE_JAPANESE_VERSION)
    v.push_back(SettingInfo::Toggle(StrId::STR_PUNCT_COMPRESSION, &CrossPointSettings::punctCompressionEnabled,
                                    "punctCompressionEnabled", StrId::STR_CAT_READER));
#endif
    v.push_back(SettingInfo::Enum(
        StrId::STR_ORIENTATION, &CrossPointSettings::orientation,
        {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_ORIENTATION_INVERTED, StrId::STR_LANDSCAPE_CCW},
        "orientation", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(StrId::STR_WRITING_MODE, &CrossPointSettings::writingMode,
                                  {StrId::STR_WRITING_HORIZONTAL, StrId::STR_WRITING_VERTICAL_RL}, "writingMode",
                                  StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Toggle(StrId::STR_EXTRA_SPACING, &CrossPointSettings::extraParagraphSpacing,
                                    "extraParagraphSpacing", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Toggle(StrId::STR_TEXT_AA, &CrossPointSettings::textAntiAliasing, "textAntiAliasing",
                                    StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(StrId::STR_FAKE_BOLD, &CrossPointSettings::fakeBold,
                                  {StrId::STR_FAKE_BOLD_OFF, StrId::STR_FAKE_BOLD_ON, StrId::STR_FAKE_BOLD_EXTRA},
                                  "fakeBold", StrId::STR_CAT_READER));
    v.push_back(SettingInfo::Enum(
        StrId::STR_IMAGES, &CrossPointSettings::imageRendering,
        {StrId::STR_IMAGES_DISPLAY, StrId::STR_IMAGES_PLACEHOLDER, StrId::STR_IMAGES_SUPPRESS, StrId::STR_IMAGES_LARGE},
        "imageRendering", StrId::STR_CAT_READER));
    // --- Controls ---
    v.push_back(SettingInfo::Enum(StrId::STR_SIDE_BTN_LAYOUT, &CrossPointSettings::sideButtonLayout,
                                  {StrId::STR_PREV_NEXT, StrId::STR_NEXT_PREV, StrId::STR_DISABLED}, "sideButtonLayout",
                                  StrId::STR_CAT_CONTROLS));
    v.push_back(SettingInfo::Toggle(StrId::STR_FRONT_BTN_FOLLOW_ORIENTATION,
                                    &CrossPointSettings::frontButtonFollowOrientation, "frontButtonFollowOrientation",
                                    StrId::STR_CAT_CONTROLS));
    v.push_back(SettingInfo::Enum(StrId::STR_LONG_PRESS_BEHAVIOR, &CrossPointSettings::longPressButtonBehavior,
                                  {StrId::STR_LONG_PRESS_BEHAVIOR_OFF, StrId::STR_LONG_PRESS_BEHAVIOR_SKIP,
                                   StrId::STR_LONG_PRESS_BEHAVIOR_ORIENTATION},
                                  "longPressButtonBehavior", StrId::STR_CAT_CONTROLS));
    v.push_back(SettingInfo::Enum(StrId::STR_LONG_PRESS_MENU, &CrossPointSettings::longPressMenuFunction,
                                  {StrId::STR_KOSYNC, StrId::STR_DISABLED, StrId::STR_BOOKMARK_OPTION},
                                  "longPressMenuFunction", StrId::STR_CAT_CONTROLS));
    v.push_back(SettingInfo::Enum(
        StrId::STR_SHORT_PWR_BTN, &CrossPointSettings::shortPwrBtn,
        {StrId::STR_IGNORE, StrId::STR_SLEEP, StrId::STR_PAGE_TURN, StrId::STR_FORCE_REFRESH, StrId::STR_FOOTNOTES},
        "shortPwrBtn", StrId::STR_CAT_CONTROLS));
    v.push_back(SettingInfo::Toggle(StrId::STR_PWR_BTN_FOOTNOTE_BACK, &CrossPointSettings::pwrBtnFootnoteBack,
                                    "pwrBtnFootnoteBack", StrId::STR_CAT_CONTROLS));
    // --- System ---
    v.push_back(SettingInfo::Value(
        StrId::STR_TIME_TO_SLEEP, &CrossPointSettings::sleepTimeoutMinutes,
        {CrossPointSettings::MIN_SLEEP_TIMEOUT_MINUTES, CrossPointSettings::MAX_SLEEP_TIMEOUT_MINUTES, 1},
        "sleepTimeoutMinutes", StrId::STR_CAT_SYSTEM));
    v.push_back(SettingInfo::Toggle(StrId::STR_SHOW_HIDDEN_FILES, &CrossPointSettings::showHiddenFiles,
                                    "showHiddenFiles", StrId::STR_CAT_SYSTEM));
    v.push_back(SettingInfo::Toggle(StrId::STR_REMOVE_READ_FROM_RECENTS,
                                    &CrossPointSettings::removeReadBooksFromRecents, "removeReadBooksFromRecents",
                                    StrId::STR_CAT_SYSTEM));
    v.push_back(SettingInfo::Toggle(StrId::STR_MOVE_FINISHED_TO_READ, &CrossPointSettings::moveFinishedToReadFolder,
                                    "moveFinishedToReadFolder", StrId::STR_CAT_SYSTEM));
    // Reading Analytics suite
    // Reading Analytics suite
    v.push_back(SettingInfo::Enum(StrId::STR_DAILY_GOAL, &CrossPointSettings::dailyGoalTarget,
                                  {StrId::STR_MIN_15, StrId::STR_MIN_30, StrId::STR_MIN_45, StrId::STR_MIN_60},
                                  "dailyGoalTarget", StrId::STR_CAT_SYSTEM));
    v.push_back(SettingInfo::Toggle(StrId::STR_ENABLE_ACHIEVEMENTS, &CrossPointSettings::achievementsEnabled,
                                    "achievementsEnabled", StrId::STR_CAT_SYSTEM));
    v.push_back(SettingInfo::Toggle(StrId::STR_ACHIEVEMENT_POPUPS, &CrossPointSettings::achievementPopups,
                                    "achievementPopups", StrId::STR_CAT_SYSTEM));
    // --- ryOS Cloud Sync (web-only, uses KOReaderCredentialStore) ---
    v.push_back(SettingInfo::DynamicString(
        StrId::STR_KOREADER_USERNAME, [] { return KOREADER_STORE.getUsername(); },
        [](const std::string& v) {
          KOREADER_STORE.setCredentials(v, KOREADER_STORE.getPassword());
          KOREADER_STORE.saveToFile();
        },
        "koUsername", StrId::STR_KOREADER_SYNC));
    v.push_back(SettingInfo::DynamicString(
        StrId::STR_KOREADER_PASSWORD, [] { return KOREADER_STORE.getPassword(); },
        [](const std::string& v) {
          KOREADER_STORE.setCredentials(KOREADER_STORE.getUsername(), v);
          KOREADER_STORE.saveToFile();
        },
        "koPassword", StrId::STR_KOREADER_SYNC));
    v.push_back(SettingInfo::DynamicString(
        StrId::STR_SYNC_SERVER_URL,
        [] {
          const auto& url = KOREADER_STORE.getServerUrl();
          return url.empty() ? std::string(KOReaderCredentialStore::getDefaultServerUrl()) : url;
        },
        [](const std::string& v) {
          KOREADER_STORE.setServerUrl(v);
          KOREADER_STORE.saveToFile();
        },
        "koServerUrl", StrId::STR_KOREADER_SYNC));
    v.push_back(SettingInfo::DynamicEnum(
        StrId::STR_DOCUMENT_MATCHING, {StrId::STR_FILENAME, StrId::STR_BINARY},
        [] { return static_cast<uint8_t>(KOREADER_STORE.getMatchMethod()); },
        [](uint8_t v) {
          KOREADER_STORE.setMatchMethod(static_cast<DocumentMatchMethod>(v));
          KOREADER_STORE.saveToFile();
        },
        "koMatchMethod", StrId::STR_KOREADER_SYNC));
    // --- Status Bar Settings (web-only, uses StatusBarSettingsActivity) ---
    v.push_back(SettingInfo::Toggle(StrId::STR_CHAPTER_PAGE_COUNT, &CrossPointSettings::statusBarChapterPageCount,
                                    "statusBarChapterPageCount", StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Toggle(StrId::STR_BOOK_PROGRESS_PERCENTAGE,
                                    &CrossPointSettings::statusBarBookProgressPercentage,
                                    "statusBarBookProgressPercentage", StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Enum(StrId::STR_PROGRESS_BAR, &CrossPointSettings::statusBarProgressBar,
                                  {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE}, "statusBarProgressBar",
                                  StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(
        SettingInfo::Enum(StrId::STR_PROGRESS_BAR_THICKNESS, &CrossPointSettings::statusBarProgressBarThickness,
                          {StrId::STR_PROGRESS_BAR_THIN, StrId::STR_PROGRESS_BAR_MEDIUM, StrId::STR_PROGRESS_BAR_THICK},
                          "statusBarProgressBarThickness", StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Enum(StrId::STR_TITLE, &CrossPointSettings::statusBarTitle,
                                  {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE}, "statusBarTitle",
                                  StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Toggle(StrId::STR_BATTERY, &CrossPointSettings::statusBarBattery, "statusBarBattery",
                                    StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Enum(StrId::STR_XTC_STATUS_BAR, &CrossPointSettings::xtcStatusBarMode,
                                  {StrId::STR_HIDE, StrId::STR_BOTTOM, StrId::STR_TOP}, "xtcStatusBarMode",
                                  StrId::STR_CUSTOMISE_STATUS_BAR));
    v.push_back(SettingInfo::Enum(StrId::STR_CLOCK, &CrossPointSettings::statusBarClock,
                                  {StrId::STR_HIDE, StrId::STR_DIR_LEFT, StrId::STR_DIR_RIGHT}, "statusBarClock",
                                  StrId::STR_CUSTOMISE_STATUS_BAR));
    // Clock timezone/format/sync — device UI is DateTimeSettingsActivity only
    // (Settings > System > Date & Time). Keep these in getSettingsList for
    // JSON/web persistence, but category STR_DATE_AND_TIME so they do not
    // appear as duplicate rows on the System tab.
    // Clock timezone/format/sync — device UI is DateTimeSettingsActivity only
    // (Settings > System > Date & Time). Keep these in getSettingsList for
    // JSON/web persistence, but category STR_DATE_AND_TIME so they do not
    // appear as duplicate rows on the System tab.
    v.push_back(SettingInfo::Value(StrId::STR_CLOCK_UTC_OFFSET, &CrossPointSettings::clockUtcOffsetQ, {0, 104, 1},
                                   "clockUtcOffsetQ", StrId::STR_DATE_AND_TIME));
    v.push_back(SettingInfo::Enum(StrId::STR_CLOCK_FORMAT, &CrossPointSettings::clockFormat,
                                  {StrId::STR_CLOCK_FORMAT_24H, StrId::STR_CLOCK_FORMAT_12H}, "clockFormat",
                                  StrId::STR_DATE_AND_TIME));
    v.push_back(SettingInfo::Toggle(StrId::STR_CLOCK_SYNCED, &CrossPointSettings::clockHasBeenSynced,
                                    "clockHasBeenSynced", StrId::STR_DATE_AND_TIME));
    // Only show tilt page turn setting when the QMI8658 IMU is present (X3)
    if (halTiltSensor.isAvailable()) {
      // Insert after the short power button setting (end of Controls section)
      auto it =
          std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_SHORT_PWR_BTN; });
      if (it != v.end()) {
        v.insert(it + 1, SettingInfo::Enum(StrId::STR_TILT_PAGE_TURN, &CrossPointSettings::tiltPageTurn,
                                           {StrId::STR_STATE_OFF, StrId::STR_NORMAL, StrId::STR_INVERTED},
                                           "tiltPageTurn", StrId::STR_CAT_CONTROLS));
      }
    }
    return v;
  }();

  std::vector<SettingInfo> v = baseList;
  if (registry && registry->getFamilyCount() > 0) {
    auto it = std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_FONT_FAMILY; });
    if (it != v.end()) {
      *it = buildFontFamilySetting(registry);
    }
  }
  return v;
}
