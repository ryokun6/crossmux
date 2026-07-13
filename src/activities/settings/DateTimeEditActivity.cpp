#include "DateTimeEditActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

namespace {
constexpr int FIELD_COUNT = 5;
constexpr int MIN_DAY = 1;
constexpr int MIN_MONTH = 1;
constexpr int MAX_MONTH = 12;
constexpr int MIN_YEAR = 2024;
constexpr int MAX_YEAR = 2099;

bool isLeapYear(const int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

unsigned getDaysInMonth(const int year, const unsigned month) {
  static constexpr unsigned DAYS_PER_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    return isLeapYear(year) ? 29U : 28U;
  }
  if (month < 1 || month > 12) {
    return 31;
  }
  return DAYS_PER_MONTH[month - 1];
}

std::string formatTwoDigits(const unsigned value) {
  char buffer[4];
  snprintf(buffer, sizeof(buffer), "%02u", value);
  return buffer;
}

unsigned wrapValue(const unsigned value, const int delta, const unsigned minValue, const unsigned maxValue) {
  const int range = static_cast<int>(maxValue - minValue + 1);
  int offset = static_cast<int>(value - minValue) + delta;
  offset %= range;
  if (offset < 0) {
    offset += range;
  }
  return minValue + static_cast<unsigned>(offset);
}
}  // namespace

void DateTimeEditActivity::onEnter() {
  Activity::onEnter();

  UtcDateTime utc{};
  if (halClock.getUtcDateTime(utc)) {
    const time_t epoch = HalClock::utcToEpoch(utc);
    const time_t localEpoch = epoch + HalClock::biasedOffsetToSeconds(SETTINGS.clockUtcOffsetQ);
    UtcDateTime local{};
    HalClock::epochToUtc(localEpoch, local);
    year = std::clamp(static_cast<int>(local.year), MIN_YEAR, MAX_YEAR);
    month = static_cast<unsigned>(std::clamp(static_cast<int>(local.month), MIN_MONTH, MAX_MONTH));
    day = static_cast<unsigned>(std::clamp(static_cast<int>(local.day), MIN_DAY, static_cast<int>(getDaysInMonth(year, month))));
    hour = local.hour;
    minute = local.minute;
  }

  selectedField = 0;
  requestUpdate();
}

void DateTimeEditActivity::adjustSelectedField(const int delta) {
  if (selectedField == 0) {
    day = wrapValue(day, delta, MIN_DAY, getDaysInMonth(year, month));
  } else if (selectedField == 1) {
    month = wrapValue(month, delta, MIN_MONTH, MAX_MONTH);
    day = std::min(day, getDaysInMonth(year, month));
  } else if (selectedField == 2) {
    year = std::clamp(year + delta, MIN_YEAR, MAX_YEAR);
    day = std::min(day, getDaysInMonth(year, month));
  } else if (selectedField == 3) {
    hour = wrapValue(hour, delta, 0U, 23U);
  } else {
    minute = wrapValue(minute, delta, 0U, 59U);
  }
  requestUpdate();
}

void DateTimeEditActivity::saveAndFinish() {
  UtcDateTime local{};
  local.year = static_cast<uint16_t>(year);
  local.month = static_cast<uint8_t>(month);
  local.day = static_cast<uint8_t>(day);
  local.hour = static_cast<uint8_t>(hour);
  local.minute = static_cast<uint8_t>(minute);
  local.second = 0;

  const time_t localEpoch = HalClock::utcToEpoch(local);
  const time_t utcEpoch = localEpoch - HalClock::biasedOffsetToSeconds(SETTINGS.clockUtcOffsetQ);
  UtcDateTime utc{};
  HalClock::epochToUtc(utcEpoch, utc);

  if (halClock.setUtcDateTime(utc)) {
    SETTINGS.clockHasBeenSynced = 1;
    SETTINGS.saveToFile();
  }
  finish();
}

void DateTimeEditActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    saveAndFinish();
    return;
  }

  buttonNavigator.onRelease({MappedInputManager::Button::Down}, [this] {
    selectedField = ButtonNavigator::nextIndex(selectedField, FIELD_COUNT);
    requestUpdate();
  });

  buttonNavigator.onRelease({MappedInputManager::Button::Up}, [this] {
    selectedField = ButtonNavigator::previousIndex(selectedField, FIELD_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustSelectedField(-1); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustSelectedField(1); });
}

void DateTimeEditActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int sidePadding = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = metrics.listWithSubtitleRowHeight * FIELD_COUNT;

  char headerDate[20];
  snprintf(headerDate, sizeof(headerDate), "%04d-%02u-%02u %02u:%02u", year, month, day, hour, minute);
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_SET_DATE_AND_TIME), headerDate);

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, listHeight}, FIELD_COUNT, selectedField,
      [](int index) {
        if (index == 0) return std::string(tr(STR_DAY));
        if (index == 1) return std::string(tr(STR_MONTH));
        if (index == 2) return std::string(tr(STR_YEAR));
        if (index == 3) return std::string(tr(STR_HOUR));
        return std::string(tr(STR_MINUTE));
      },
      [this](int index) {
        if (index == 0) return formatTwoDigits(day);
        if (index == 1) return formatTwoDigits(month);
        if (index == 2) return std::to_string(year);
        if (index == 3) return formatTwoDigits(hour);
        return formatTwoDigits(minute);
      },
      [](int) { return UIIcon::Recent; }, nullptr, false);

  const int hintTop = contentTop + listHeight + metrics.verticalSpacing;
  const int hintWidth = pageWidth - sidePadding * 2;
  const std::string hint = renderer.truncatedText(UI_10_FONT_ID, tr(STR_SET_DATE_HINT), hintWidth);
  renderer.drawText(UI_10_FONT_ID, sidePadding, hintTop, hint.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
