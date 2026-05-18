#include "WeReadMenuActivity.h"

#include <I18n.h>
#include <WiFi.h>

#include <memory>
#include <string>

#include "../../../WifiCredentialStore.h"
#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "../../network/WifiSelectionActivity.h"
#include "WeReadKeyStore.h"
#include "WeReadSetupActivity.h"

namespace {

// Menu items in display order. Action stubs return to the menu with a
// "coming soon" banner until the corresponding feature Activity lands.
enum class MenuItem : uint8_t {
  Shelf = 0,
  Search,
  ForYou,
  Stats,
  Setup,
};

constexpr int kMenuItemCount = 5;

StrId titleFor(int idx) {
  switch (static_cast<MenuItem>(idx)) {
    case MenuItem::Shelf:
      return StrId::STR_WEREAD_MENU_SHELF;
    case MenuItem::Search:
      return StrId::STR_WEREAD_MENU_SEARCH;
    case MenuItem::ForYou:
      return StrId::STR_WEREAD_MENU_RECOMMEND;
    case MenuItem::Stats:
      return StrId::STR_WEREAD_MENU_STATS;
    case MenuItem::Setup:
      return StrId::STR_WEREAD_MENU_SETUP;
  }
  return StrId::STR_WEREAD_MENU_SHELF;
}

StrId subtitleFor(int idx, bool keyOk) {
  if (static_cast<MenuItem>(idx) == MenuItem::Setup) {
    return keyOk ? StrId::STR_WEREAD_KEY_SET : StrId::STR_WEREAD_KEY_NOT_SET;
  }
  // Default subtitle for other rows; rendering layer falls back to empty if
  // the key resolves to an empty translation.
  return StrId::STR_WEREAD_NONE;
}

}  // namespace

WeReadMenuActivity::WeReadMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WeReadMenu", renderer, mappedInput) {}

void WeReadMenuActivity::onEnter() {
  Activity::onEnter();
  selected = 0;
  banner = Banner::None;
  refreshGates();

  // If we don't have Wi-Fi yet AND we haven't tried auto-connecting once
  // already this Activity instance AND a previously-used network is on file,
  // hand off to WifiSelectionActivity. It will silently connect to the last
  // known SSID (skipping its own scan UI) and return when done.
  if (!wifiOk && !autoConnectAttempted_) {
    autoConnectAttempted_ = true;
    WIFI_STORE.loadFromFile();
    const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();
    if (!lastSsid.empty() && WIFI_STORE.findCredential(lastSsid) != nullptr) {
      launchAutoConnect();
      return;
    }
  }
  requestUpdate();
}

void WeReadMenuActivity::launchAutoConnect() {
  auto handler = [this](const ActivityResult&) {
    // The WifiResult.connected flag would work too, but WiFi.status() is the
    // authoritative source — re-poll it via refreshGates so we stay aligned
    // with what FetchActivity preflights will see.
    refreshGates();
    requestUpdate();
  };
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true), handler);
}

void WeReadMenuActivity::onExit() { Activity::onExit(); }

void WeReadMenuActivity::refreshGates() {
  wifiOk = (WiFi.status() == WL_CONNECTED);
  keyOk = WeReadKeyStore::has();
}

void WeReadMenuActivity::loop() {
  buttonNavigator.onNext([this] {
    selected = ButtonNavigator::nextIndex(selected, kMenuItemCount);
    banner = Banner::None;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selected = ButtonNavigator::previousIndex(selected, kMenuItemCount);
    banner = Banner::None;
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onSelect();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goToApps();
  }
}

void WeReadMenuActivity::onSelect() {
  const auto item = static_cast<MenuItem>(selected);

  if (item == MenuItem::Setup) {
    auto handler = [this](const ActivityResult&) {
      // Setup Activity returns here regardless of save/cancel — re-check gates.
      refreshGates();
      requestUpdate();
    };
    startActivityForResult(std::make_unique<WeReadSetupActivity>(renderer, mappedInput), handler);
    return;
  }

  // All non-Setup actions need both Wi-Fi and an API key.
  refreshGates();
  if (!wifiOk) {
    banner = Banner::NoWifi;
    requestUpdate();
    return;
  }
  if (!keyOk) {
    // Send the user directly into setup rather than nagging.
    auto handler = [this](const ActivityResult&) {
      refreshGates();
      requestUpdate();
    };
    startActivityForResult(std::make_unique<WeReadSetupActivity>(renderer, mappedInput), handler);
    return;
  }

  switch (item) {
    case MenuItem::Shelf:
      activityManager.goToWeReadShelf();
      break;
    case MenuItem::Search:
      activityManager.goToWeReadSearch();
      break;
    case MenuItem::ForYou:
      activityManager.goToWeReadRecommend();
      break;
    case MenuItem::Stats:
      activityManager.goToWeReadStats();
      break;
    case MenuItem::Setup:
      break;  // handled above
  }
}

void WeReadMenuActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, tr(STR_WEREAD_TITLE));

  const int listY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listH = sh - listY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const bool keyOkLocal = keyOk;
  GUI.drawList(
      renderer, Rect{0, listY, sw, listH}, kMenuItemCount, selected,
      [](int i) { return std::string(I18n::getInstance().get(titleFor(i))); },
      [keyOkLocal](int i) { return std::string(I18n::getInstance().get(subtitleFor(i, keyOkLocal))); });

  // Banner pops over the menu — drawn last so it sits on top.
  if (banner == Banner::NoWifi) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_WIFI));
  } else if (banner == Banner::ComingSoon) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_COMING_SOON));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
