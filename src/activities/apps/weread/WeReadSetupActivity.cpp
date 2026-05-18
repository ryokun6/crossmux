#include "WeReadSetupActivity.h"

#include <I18n.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "../../util/KeyboardEntryActivity.h"
#include "WeReadKeyStore.h"

namespace {

constexpr int kIdxSet = 0;
constexpr int kIdxClear = 1;

}  // namespace

WeReadSetupActivity::WeReadSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("WeReadSetup", renderer, mappedInput) {}

void WeReadSetupActivity::onEnter() {
  Activity::onEnter();
  selected = 0;
  banner = Banner::None;
  refresh();
  requestUpdate();
}

void WeReadSetupActivity::onExit() { Activity::onExit(); }

void WeReadSetupActivity::refresh() { keyPresent = WeReadKeyStore::has(); }

int WeReadSetupActivity::itemCount() const { return keyPresent ? 2 : 1; }

void WeReadSetupActivity::loop() {
  const int count = itemCount();
  buttonNavigator.onNext([this, count] {
    selected = ButtonNavigator::nextIndex(selected, count);
    banner = Banner::None;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, count] {
    selected = ButtonNavigator::previousIndex(selected, count);
    banner = Banner::None;
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onSelect();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void WeReadSetupActivity::onSelect() {
  if (selected == kIdxSet) {
    auto handler = [this](const ActivityResult& result) {
      if (result.isCancelled) {
        requestUpdate();
        return;
      }
      const auto& kb = std::get<KeyboardResult>(result.data);
      if (!WeReadKeyStore::isWellFormed(kb.text)) {
        banner = Banner::Invalid;
        requestUpdate();
        return;
      }
      if (WeReadKeyStore::save(kb.text)) {
        banner = Banner::Saved;
        refresh();
        selected = 0;
      } else {
        banner = Banner::Invalid;
      }
      requestUpdate();
    };
    // URL layout has the '-' and '.' under one tap and lowercase letters by
    // default — closest match for a "wrk-xxxx" key shape.
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, std::string(tr(STR_WEREAD_API_KEY_PROMPT)),
                                                std::string(WeReadKeyStore::load()), 128, InputType::Url),
        handler);
  } else if (selected == kIdxClear && keyPresent) {
    WeReadKeyStore::clear();
    banner = Banner::Cleared;
    refresh();
    selected = 0;
    requestUpdate();
  }
}

void WeReadSetupActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, tr(STR_WEREAD_MENU_SETUP));

  const int listY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listH = sh - listY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const bool present = keyPresent;
  GUI.drawList(
      renderer, Rect{0, listY, sw, listH}, itemCount(), selected,
      [present](int i) {
        if (i == kIdxSet) {
          return std::string(I18n::getInstance().get(present ? StrId::STR_WEREAD_KEY_REPLACE
                                                             : StrId::STR_WEREAD_KEY_SET_ACTION));
        }
        return std::string(tr(STR_WEREAD_KEY_CLEAR));
      },
      [present](int i) {
        if (i == kIdxSet) {
          return std::string(
              I18n::getInstance().get(present ? StrId::STR_WEREAD_KEY_SET : StrId::STR_WEREAD_KEY_NOT_SET));
        }
        return std::string{};
      });

  switch (banner) {
    case Banner::Saved:
      GUI.drawPopup(renderer, tr(STR_WEREAD_KEY_SAVED));
      break;
    case Banner::Cleared:
      GUI.drawPopup(renderer, tr(STR_WEREAD_KEY_WAS_CLEARED));
      break;
    case Banner::Invalid:
      GUI.drawPopup(renderer, tr(STR_WEREAD_API_KEY_INVALID));
      break;
    case Banner::None:
      break;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
