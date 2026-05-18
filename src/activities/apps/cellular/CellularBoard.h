#pragma once

#include <cstdint>

// Conway's Game of Life board on a 54×85 toroidal (wrap-around) grid.
//
// Sized to fit the inset content area (480 - 2×24 px wide, ~680 px tall)
// at 8 px per cell. Two packed bit-buffers (≈574 B each ≈ 1.15 KB total).
class CellularBoard {
 public:
  static constexpr int COLS = 54;
  static constexpr int ROWS = 85;
  static constexpr int CELL_COUNT = COLS * ROWS;
  static constexpr int BYTES = (CELL_COUNT + 7) / 8;

  CellularBoard();

  // Read a cell. Coordinates wrap toroidally; callers may pass any int.
  bool get(int r, int c) const;

  // Reset all cells and gen/pop to zero, then seed each cell with a random
  // independent draw at `pctAlive` (0..100, clamped).
  void randomFill(uint8_t pctAlive);

  // Advance one generation under Conway's rules (B3/S23) with toroidal edges.
  void step();

  uint32_t generation() const { return gen_; }
  uint16_t population() const { return pop_; }

 private:
  static int idx(int r, int c) { return r * COLS + c; }
  static int wrapRow(int r);
  static int wrapCol(int c);
  static bool readBit(const uint8_t* buf, int i);
  static void writeBit(uint8_t* buf, int i, bool v);

  uint8_t cur_[BYTES];
  uint8_t nxt_[BYTES];
  uint32_t gen_ = 0;
  uint16_t pop_ = 0;
};
