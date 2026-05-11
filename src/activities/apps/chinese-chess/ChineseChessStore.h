#pragma once

#include <cstdint>

#include "ChineseChessAI.h"
#include "ChineseChessBoard.h"

struct ChineseChessStats {
  uint16_t bestTimeSec = 0;  // 0 = no record
  uint16_t redWins = 0;
  uint16_t blackWins = 0;
  uint16_t draws = 0;
  uint16_t startedCount = 0;
};

enum class ChineseChessMode : uint8_t { TwoPlayer = 0, VsAi = 1 };

struct ChineseChessSaveSlot {
  ChineseChessBoard board;
  ChineseChessMode mode = ChineseChessMode::TwoPlayer;
  ChineseChessAiLevel aiLevel = ChineseChessAiLevel::Medium;  // ignored for TwoPlayer
  uint8_t cursorR = 9;
  uint8_t cursorC = 4;
  uint8_t selR = ChineseChessBoard::INVALID_IDX;
  uint8_t selC = ChineseChessBoard::INVALID_IDX;
  bool hasSelection = false;
  uint16_t redElapsedSec = 0;
  uint16_t blackElapsedSec = 0;
};

class ChineseChessStore {
 public:
  static bool hasInProgress();
  static bool load(ChineseChessSaveSlot& out);
  static bool save(const ChineseChessSaveSlot& in);
  static bool clear();

  static ChineseChessStats loadStats();
  static bool saveStats(const ChineseChessStats& stats);
  static void recordStart();
  static void recordWin(ChineseChessBoard::Side winner, uint16_t timeSec);
  static void recordDraw();
};
