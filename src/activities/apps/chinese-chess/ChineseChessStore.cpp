#include "ChineseChessStore.h"

#include <HalStorage.h>
#include <Logging.h>

namespace {

constexpr const char* kSavePath = "/.crosspoint/chinese-chess.bin";
constexpr const char* kStatsPath = "/.crosspoint/chinese-chess_stats.bin";
constexpr const char* kDir = "/.crosspoint";

constexpr uint8_t SAVE_VERSION = 2;
constexpr uint8_t STATS_VERSION = 1;

bool ensureDir() {
  if (Storage.exists(kDir)) return true;
  return Storage.mkdir(kDir);
}

}  // namespace

bool ChineseChessStore::hasInProgress() { return Storage.exists(kSavePath); }

bool ChineseChessStore::load(ChineseChessSaveSlot& out) {
  HalFile f;
  if (!Storage.openFileForRead("XQI", kSavePath, f)) {
    return false;
  }
  uint8_t version = 0;
  if (f.read(&version, 1) != 1 || version != SAVE_VERSION) {
    LOG_ERR("XQI", "Save version mismatch (got %u)", static_cast<unsigned>(version));
    return false;
  }
  uint8_t mode = 0;
  if (f.read(&mode, 1) != 1) return false;
  out.mode = static_cast<ChineseChessMode>(mode);
  uint8_t levelByte = 0;
  if (f.read(&levelByte, 1) != 1) return false;
  out.aiLevel = (levelByte <= static_cast<uint8_t>(ChineseChessAiLevel::Hard))
                    ? static_cast<ChineseChessAiLevel>(levelByte)
                    : ChineseChessAiLevel::Medium;
  if (f.read(&out.cursorR, 1) != 1) return false;
  if (f.read(&out.cursorC, 1) != 1) return false;
  if (f.read(&out.selR, 1) != 1) return false;
  if (f.read(&out.selC, 1) != 1) return false;
  uint8_t hasSelByte = 0;
  if (f.read(&hasSelByte, 1) != 1) return false;
  out.hasSelection = (hasSelByte != 0);
  if (f.read(reinterpret_cast<uint8_t*>(&out.redElapsedSec), sizeof(out.redElapsedSec)) !=
      static_cast<int>(sizeof(out.redElapsedSec))) {
    return false;
  }
  if (f.read(reinterpret_cast<uint8_t*>(&out.blackElapsedSec), sizeof(out.blackElapsedSec)) !=
      static_cast<int>(sizeof(out.blackElapsedSec))) {
    return false;
  }
  if (!out.board.readFrom(f)) {
    LOG_ERR("XQI", "Save board read failed");
    return false;
  }
  return true;
}

bool ChineseChessStore::save(const ChineseChessSaveSlot& in) {
  if (!ensureDir()) {
    LOG_ERR("XQI", "Cannot create dir %s", kDir);
    return false;
  }
  HalFile f;
  if (!Storage.openFileForWrite("XQI", kSavePath, f)) {
    return false;
  }
  const uint8_t version = SAVE_VERSION;
  if (f.write(&version, 1) != 1) return false;
  const uint8_t mode = static_cast<uint8_t>(in.mode);
  if (f.write(&mode, 1) != 1) return false;
  const uint8_t levelByte = static_cast<uint8_t>(in.aiLevel);
  if (f.write(&levelByte, 1) != 1) return false;
  if (f.write(&in.cursorR, 1) != 1) return false;
  if (f.write(&in.cursorC, 1) != 1) return false;
  if (f.write(&in.selR, 1) != 1) return false;
  if (f.write(&in.selC, 1) != 1) return false;
  const uint8_t hasSelByte = in.hasSelection ? 1 : 0;
  if (f.write(&hasSelByte, 1) != 1) return false;
  if (f.write(reinterpret_cast<const uint8_t*>(&in.redElapsedSec), sizeof(in.redElapsedSec)) !=
      sizeof(in.redElapsedSec)) {
    return false;
  }
  if (f.write(reinterpret_cast<const uint8_t*>(&in.blackElapsedSec), sizeof(in.blackElapsedSec)) !=
      sizeof(in.blackElapsedSec)) {
    return false;
  }
  if (!in.board.writeTo(f)) {
    LOG_ERR("XQI", "Save board write failed");
    return false;
  }
  f.flush();
  return true;
}

bool ChineseChessStore::clear() {
  if (!Storage.exists(kSavePath)) return true;
  return Storage.remove(kSavePath);
}

ChineseChessStats ChineseChessStore::loadStats() {
  ChineseChessStats out;
  if (!Storage.exists(kStatsPath)) return out;
  HalFile f;
  if (!Storage.openFileForRead("XQI", kStatsPath, f)) return out;
  uint8_t version = 0;
  if (f.read(&version, 1) != 1 || version != STATS_VERSION) {
    LOG_ERR("XQI", "Stats version mismatch (got %u)", static_cast<unsigned>(version));
    return ChineseChessStats{};
  }
  if (f.read(reinterpret_cast<uint8_t*>(&out.bestTimeSec), sizeof(out.bestTimeSec)) != sizeof(out.bestTimeSec) ||
      f.read(reinterpret_cast<uint8_t*>(&out.redWins), sizeof(out.redWins)) != sizeof(out.redWins) ||
      f.read(reinterpret_cast<uint8_t*>(&out.blackWins), sizeof(out.blackWins)) != sizeof(out.blackWins) ||
      f.read(reinterpret_cast<uint8_t*>(&out.draws), sizeof(out.draws)) != sizeof(out.draws) ||
      f.read(reinterpret_cast<uint8_t*>(&out.startedCount), sizeof(out.startedCount)) != sizeof(out.startedCount)) {
    LOG_ERR("XQI", "Stats truncated; resetting");
    return ChineseChessStats{};
  }
  return out;
}

bool ChineseChessStore::saveStats(const ChineseChessStats& stats) {
  if (!ensureDir()) return false;
  HalFile f;
  if (!Storage.openFileForWrite("XQI", kStatsPath, f)) return false;
  const uint8_t version = STATS_VERSION;
  if (f.write(&version, 1) != 1) return false;
  if (f.write(reinterpret_cast<const uint8_t*>(&stats.bestTimeSec), sizeof(stats.bestTimeSec)) !=
      sizeof(stats.bestTimeSec)) {
    return false;
  }
  if (f.write(reinterpret_cast<const uint8_t*>(&stats.redWins), sizeof(stats.redWins)) != sizeof(stats.redWins)) {
    return false;
  }
  if (f.write(reinterpret_cast<const uint8_t*>(&stats.blackWins), sizeof(stats.blackWins)) != sizeof(stats.blackWins)) {
    return false;
  }
  if (f.write(reinterpret_cast<const uint8_t*>(&stats.draws), sizeof(stats.draws)) != sizeof(stats.draws)) {
    return false;
  }
  if (f.write(reinterpret_cast<const uint8_t*>(&stats.startedCount), sizeof(stats.startedCount)) !=
      sizeof(stats.startedCount)) {
    return false;
  }
  f.flush();
  return true;
}

void ChineseChessStore::recordStart() {
  ChineseChessStats stats = loadStats();
  if (stats.startedCount < 0xFFFF) stats.startedCount++;
  saveStats(stats);
}

void ChineseChessStore::recordWin(ChineseChessBoard::Side winner, uint16_t timeSec) {
  ChineseChessStats stats = loadStats();
  if (winner == ChineseChessBoard::Side::Red && stats.redWins < 0xFFFF) stats.redWins++;
  if (winner == ChineseChessBoard::Side::Black && stats.blackWins < 0xFFFF) stats.blackWins++;
  if (stats.bestTimeSec == 0 || (timeSec > 0 && timeSec < stats.bestTimeSec)) {
    stats.bestTimeSec = timeSec;
  }
  saveStats(stats);
}

void ChineseChessStore::recordDraw() {
  ChineseChessStats stats = loadStats();
  if (stats.draws < 0xFFFF) stats.draws++;
  saveStats(stats);
}
