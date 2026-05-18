#include "CellularGameActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <cstdio>

#include "../../../components/UITheme.h"
#include "../../../fontIds.h"
#include "../GameUi.h"

namespace {
constexpr int kStatusFont = UI_12_FONT_ID;
}  // namespace

void CellularGameActivity::onEnter() {
  Activity::onEnter();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  board_.randomFill(RANDOM_DENSITY_PCT);
  running_ = false;
  requestUpdate();
}

void CellularGameActivity::onExit() { Activity::onExit(); }

void CellularGameActivity::loop() {
  if (running_) {
    const uint32_t now = millis();
    if (now - lastStepMs_ >= STEP_INTERVAL_MS) {
      board_.step();
      lastStepMs_ = now;
      requestUpdate();
    }
  }
  handleInput();
}

void CellularGameActivity::handleInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goToApps();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    running_ = !running_;
    if (running_) lastStepMs_ = millis();
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    board_.randomFill(RANDOM_DENSITY_PCT);
    requestUpdate();
  }
}

void CellularGameActivity::render(RenderLock&&) {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.clearScreen();
  drawTitleBar();
  drawGrid();
  drawFooter();
  // Always FAST: a full refresh in any state — even entry or re-seed —
  // causes the visible "blink" we want to avoid.
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void CellularGameActivity::drawTitleBar() {
  const int w = renderer.getScreenWidth();
  // Separator line spans the full width — the title bar is treated as a
  // notification strip and is exempt from the side safe-area inset that
  // applies to the grid below it.
  renderer.drawLine(0, TITLE_BAR_H, w - 1, TITLE_BAR_H, true);
  const int textH = renderer.getTextHeight(kStatusFont);
  const int y = gameCenterY(TITLE_BAR_H, textH);

  // Left: app title. Right: gen / pop / mode, flush right.
  renderer.drawText(kStatusFont, CONTENT_X, y, tr(STR_CELLULAR_TITLE));

  char tail[64];
  snprintf(tail, sizeof(tail), "%s %lu  ·  %s %u  ·  %s", tr(STR_CELLULAR_GEN),
           static_cast<unsigned long>(board_.generation()), tr(STR_CELLULAR_POP),
           static_cast<unsigned>(board_.population()),
           running_ ? tr(STR_CELLULAR_MODE_RUN) : tr(STR_CELLULAR_MODE_PAUSE));
  const int tw = renderer.getTextWidth(kStatusFont, tail);
  renderer.drawText(kStatusFont, w - CONTENT_X - tw, y, tail);
}

void CellularGameActivity::drawGrid() {
  const int sw = renderer.getScreenWidth();
  const int ox = (sw - GRID_W) / 2;
  const int oy = GRID_TOP;
  // Filled 8×8 black squares against the cleared white background. No outer
  // frame: the grid already sits inside the 24 px side padding and the
  // title-bar separator above visually frames it.
  for (int r = 0; r < CellularBoard::ROWS; r++) {
    for (int c = 0; c < CellularBoard::COLS; c++) {
      if (board_.get(r, c)) {
        renderer.fillRect(ox + c * CELL_PX, oy + r * CELL_PX, CELL_PX, CELL_PX, true);
      }
    }
  }
}

void CellularGameActivity::drawFooter() {
  // Args to mapLabels are (back, confirm, previous=Left, next=Right). Confirm
  // is intentionally empty — only Back / Run / Random are active.
  const char* leftLabel = running_ ? tr(STR_CELLULAR_MENU_PAUSE) : tr(STR_CELLULAR_MENU_RUN);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", leftLabel, tr(STR_CELLULAR_MENU_RANDOM));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
