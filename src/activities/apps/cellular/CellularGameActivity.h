#pragma once

#include <cstdint>

#include "../../Activity.h"
#include "CellularBoard.h"

// Conway's Game of Life on a 54×85 toroidal grid. Minimal UX: on entry the
// field auto-seeds with a random configuration but stays paused. The Left
// button toggles Run/Pause; the Right button re-seeds randomly; Back exits.
// The grid sits inside the standard 24 px side padding so its left/right
// edges stay inside the device's safe display area.
class CellularGameActivity final : public Activity {
 public:
  CellularGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Cellular", renderer, mappedInput) {}
  ~CellularGameActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Layout (480 × 800 portrait). Side padding matches Sudoku's CONTENT_X so
  // the grid stays inside the safe area; GRID_TOP matches Sudoku's GRID_Y.
  static constexpr int CONTENT_X = 24;
  static constexpr int TITLE_BAR_H = 36;
  static constexpr int GRID_TOP = 60;
  static constexpr int CELL_PX = 8;
  static constexpr int GRID_W = CellularBoard::COLS * CELL_PX;  // 432
  static constexpr uint16_t STEP_INTERVAL_MS = 800;
  static constexpr uint8_t RANDOM_DENSITY_PCT = 25;

  CellularBoard board_;
  bool running_ = false;  // Start paused — Left starts it.
  uint32_t lastStepMs_ = 0;

  void handleInput();
  void drawTitleBar();
  void drawGrid();
  void drawFooter();
};
