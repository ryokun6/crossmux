#include "DateTimeSettingsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstdio>
#include <memory>
#include <string>

#include "ClockOffsetActivity.h"
#include "ClockSyncActivity.h"
#include "CrossPointSettings.h"
#include "DateTimeEditActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
enum MenuItem {
  ITEM_TIMEZONE = 0,
  ITEM_FORMAT,
  ITEM_SYNC,
  ITEM_SET_DATE_TIME,  // X3 only
  ITEM_COUNT
};

constexpr int BASE_MENU_ITEMS = ITEM_SYNC + 1;
constexpr int FULL_MENU_ITEMS = ITEM_COUNT;

const StrId menuNames[FULL_MENU_ITEMS] = {
    StrId::STR_CLOCK_UTC_OFFSET,
    StrId::STR_CLOCK_FORMAT,
    StrId::STR_CLOCK_SYNC_NOW,
    StrId::STR_SET_DATE_AND_TIME,
};

constexpr int CLOCK_FORMAT_ITEMS = 2;
const StrId clockFormatNames[CLOCK_FORMAT_ITEMS] = {StrId::STR_CLOCK_FORMAT_24H, StrId::STR_CLOCK_FORMAT_12H};

std::string formatUtcOffset(const uint8_t biasedQ) {
  uint8_t biased = biasedQ;
  if (biased > 104) biased = 48;
  const int totalMinutes = (static_cast<int>(biased) - 48) * 15;
  const bool neg = totalMinutes < 0;
  const int absMinutes = neg ? -totalMinutes : totalMinutes;
  const int hours = absMinutes / 60;
  const int mins = absMinutes % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "UTC%c%d:%02d", neg ? '-' : '+', hours, mins);
  return buf;
}
}  // namespace

void DateTimeSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  visibleItemCount = halClock.isAvailable() ? FULL_MENU_ITEMS : BASE_MENU_ITEMS;

  if (SETTINGS.clockUtcOffsetQ > 104) {
    SETTINGS.clockUtcOffsetQ = 48;
  }
  if (SETTINGS.clockFormat >= CLOCK_FORMAT_ITEMS) {
    SETTINGS.clockFormat = 0;
  }

  requestUpdate();
}

void DateTimeSettingsActivity::onExit() { Activity::onExit(); }

void DateTimeSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });
}

void DateTimeSettingsActivity::handleSelection() {
  switch (selectedIndex) {
    case ITEM_TIMEZONE:
      startActivityForResult(std::make_unique<ClockOffsetActivity>(renderer, mappedInput), nullptr);
      return;
    case ITEM_FORMAT:
      SETTINGS.clockFormat = (SETTINGS.clockFormat + 1) % CLOCK_FORMAT_ITEMS;
      SETTINGS.saveToFile();
      break;
    case ITEM_SYNC:
      startActivityForResult(std::make_unique<ClockSyncActivity>(renderer, mappedInput), nullptr);
      return;
    case ITEM_SET_DATE_TIME:
      startActivityForResult(std::make_unique<DateTimeEditActivity>(renderer, mappedInput), nullptr);
      return;
    default:
      return;
  }
}

void DateTimeSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DATE_AND_TIME));

  char currentTime[20] = {};
  if (halClock.formatDateTime(currentTime, sizeof(currentTime), SETTINGS.clockUtcOffsetQ)) {
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing,
                              currentTime);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing,
                              tr(STR_NOT_SET));
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2 + 20;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, visibleItemCount, selectedIndex,
      [](int index) { return std::string(I18N.get(menuNames[index])); }, nullptr, nullptr,
      [](int index) -> std::string {
        switch (index) {
          case ITEM_TIMEZONE:
            return formatUtcOffset(SETTINGS.clockUtcOffsetQ);
          case ITEM_FORMAT: {
            const uint8_t fmt = SETTINGS.clockFormat < CLOCK_FORMAT_ITEMS ? SETTINGS.clockFormat : 0;
            return std::string(I18N.get(clockFormatNames[fmt]));
          }
          case ITEM_SYNC:
            return SETTINGS.clockHasBeenSynced ? tr(STR_CLOCK_SYNCED) : tr(STR_NOT_SET);
          case ITEM_SET_DATE_TIME:
            return tr(STR_SET_DATE);
          default:
            return {};
        }
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
