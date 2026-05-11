#pragma once

#include <cstdint>

#include "ChineseChessBoard.h"

enum class ChineseChessAiLevel : uint8_t { Easy = 0, Medium = 1, Hard = 2 };

class ChineseChessAI {
 public:
  // Pick a move for `aiSide` on the given board.
  // Returns a move with from = INVALID_IDX if no legal move exists.
  // Caller must verify it is the AI's turn and the game is not over.
  // Time-bounded internally; never blocks longer than the level's budget.
  static ChineseChessBoard::Move chooseMove(const ChineseChessBoard& board, ChineseChessBoard::Side aiSide,
                                            ChineseChessAiLevel level);
};
