#include "CellularBoard.h"

#include <esp_random.h>

#include <cstring>

CellularBoard::CellularBoard() {
  std::memset(cur_, 0, sizeof(cur_));
  // nxt_ is scratch and fully rewritten by step(); no need to zero it here.
}

int CellularBoard::wrapRow(int r) {
  int m = r % ROWS;
  return (m < 0) ? m + ROWS : m;
}

int CellularBoard::wrapCol(int c) {
  int m = c % COLS;
  return (m < 0) ? m + COLS : m;
}

bool CellularBoard::readBit(const uint8_t* buf, int i) { return (buf[i >> 3] >> (i & 7)) & 1u; }

void CellularBoard::writeBit(uint8_t* buf, int i, bool v) {
  const uint8_t mask = static_cast<uint8_t>(1u << (i & 7));
  if (v) {
    buf[i >> 3] = static_cast<uint8_t>(buf[i >> 3] | mask);
  } else {
    buf[i >> 3] = static_cast<uint8_t>(buf[i >> 3] & ~mask);
  }
}

bool CellularBoard::get(int r, int c) const { return readBit(cur_, idx(wrapRow(r), wrapCol(c))); }

void CellularBoard::randomFill(uint8_t pctAlive) {
  if (pctAlive > 100) pctAlive = 100;
  std::memset(cur_, 0, sizeof(cur_));
  uint16_t pop = 0;
  // Compare an 8-bit random byte to (pctAlive * 256 / 100). One byte of
  // randomness per cell keeps the distribution uniform within ±1%, which is
  // plenty for visual seeding.
  const uint32_t threshold = (static_cast<uint32_t>(pctAlive) * 256u) / 100u;
  for (int i = 0; i < CELL_COUNT;) {
    const uint32_t r = esp_random();
    for (int b = 0; b < 4 && i < CELL_COUNT; b++, i++) {
      const uint32_t byte = (r >> (b * 8)) & 0xFFu;
      if (byte < threshold) {
        writeBit(cur_, i, true);
        pop = static_cast<uint16_t>(pop + 1);
      }
    }
  }
  pop_ = pop;
  gen_ = 0;
}

void CellularBoard::step() {
  uint16_t pop = 0;
  // Hot path: ~4600 cells × 8 neighbour reads. `cur_` is read-only during the
  // sweep, results land in `nxt_`, then we memcpy back at the end.
  for (int r = 0; r < ROWS; r++) {
    const int rm = (r == 0) ? ROWS - 1 : r - 1;
    const int rp = (r == ROWS - 1) ? 0 : r + 1;
    for (int c = 0; c < COLS; c++) {
      const int cm = (c == 0) ? COLS - 1 : c - 1;
      const int cp = (c == COLS - 1) ? 0 : c + 1;
      int n = 0;
      n += readBit(cur_, idx(rm, cm)) ? 1 : 0;
      n += readBit(cur_, idx(rm, c)) ? 1 : 0;
      n += readBit(cur_, idx(rm, cp)) ? 1 : 0;
      n += readBit(cur_, idx(r, cm)) ? 1 : 0;
      n += readBit(cur_, idx(r, cp)) ? 1 : 0;
      n += readBit(cur_, idx(rp, cm)) ? 1 : 0;
      n += readBit(cur_, idx(rp, c)) ? 1 : 0;
      n += readBit(cur_, idx(rp, cp)) ? 1 : 0;

      const bool alive = readBit(cur_, idx(r, c));
      const bool next = alive ? (n == 2 || n == 3) : (n == 3);
      writeBit(nxt_, idx(r, c), next);
      if (next) pop = static_cast<uint16_t>(pop + 1);
    }
  }
  std::memcpy(cur_, nxt_, sizeof(cur_));
  pop_ = pop;
  gen_++;
}
