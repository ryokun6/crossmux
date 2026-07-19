#include "AppsMenuActivity.h"

#include <I18n.h>

#include <algorithm>
#include <string>

#include "../../components/UITheme.h"
#include "../../util/PaginationDots.h"

namespace {

// Single source of truth for the Apps menu — add a new app here, then provide the
// matching `goTo<App>()` in ActivityManager. The enum-and-switch pattern this replaces
// required edits in five places per new app; this requires one.
struct AppEntry {
  StrId titleId;
  UIIcon icon;
  void (ActivityManager::*open)();
};

constexpr AppEntry kAppEntries[] = {
    {StrId::STR_AGENT_MONITOR_TITLE, UIIcon::Wifi, &ActivityManager::goToAgentMonitor},
    {StrId::STR_READING_STATS, UIIcon::Library, &ActivityManager::goToReadingStatsMenu},
#ifdef ENABLE_CHINESE_VERSION
    {StrId::STR_WEREAD_TITLE, UIIcon::WeRead, &ActivityManager::goToWeRead},
#endif
    {StrId::STR_STANDBY_TITLE, UIIcon::Standby, &ActivityManager::goToStandby},
    {StrId::STR_OPDS_BROWSER, UIIcon::Library, &ActivityManager::goToBrowser},
};

constexpr int kAppCount = static_cast<int>(sizeof(kAppEntries) / sizeof(kAppEntries[0]));

}  // namespace

void AppsMenuActivity::onEnter() {
  Activity::onEnter();
  selected = 0;
  requestUpdate();
}

void AppsMenuActivity::onExit() { Activity::onExit(); }

void AppsMenuActivity::loop() {
  buttonNavigator.onNext([this] {
    selected = ButtonNavigator::nextIndex(selected, kAppCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selected = ButtonNavigator::previousIndex(selected, kAppCount);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selected >= 0 && selected < kAppCount) {
      (activityManager.*kAppEntries[selected].open)();
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goHome();
  }
}

void AppsMenuActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, tr(STR_APPS_TITLE));

  const int listY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listH = sh - listY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Halved inter-row gap (8 -> 4 on LYRA) keeps the home-tile look but tightens the list.
  const int spacing = metrics.menuSpacing / 2;
  const int rowStep = metrics.menuRowHeight + spacing;
  // Number of rows that fit: n rows occupy n*rowHeight + (n-1)*spacing <= listH.
  const int perPage = std::max(1, (listH + spacing) / rowStep);
  const int totalPages = (kAppCount + perPage - 1) / perPage;
  const int page = selected / perPage;
  const int pageStart = page * perPage;
  const int pageCount = std::min(perPage, kAppCount - pageStart);

  // Render only the current page's slice through the shared menu (top-aligned in rect).
  GUI.drawButtonMenu(
      renderer, Rect{0, listY, sw, listH}, pageCount, selected - pageStart,
      [pageStart](int i) { return std::string(I18n::getInstance().get(kAppEntries[i + pageStart].titleId)); },
      [pageStart](int i) { return kAppEntries[i + pageStart].icon; }, spacing);

  // Standby-style page dots, sitting above the list bottom so they stay clear of the
  // button hints below (the last menu row never reaches this far down).
  if (totalPages > 1) {
    const int dotsY = listY + listH - 8;
    drawPaginationDots(renderer, sw, dotsY, totalPages, page);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
