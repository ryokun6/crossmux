#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "CjkKinsoku.h"

// CJK punctuation compression (標點擠壓 / 約物詰め).
// Shrinks layout advances between adjacent punctuation and at line/column
// edges. Glyph bitmaps stay full-em; only advances (and thus paint positions)
// change. Applied before kinsoku repair so break decisions see compressed
// widths; line-edge trims run after repair on the final run bounds.
//
// Enabled for TC / SC / JA only. Korean and Latin builds compile to Profile::Off.
// Hanging punctuation outside the content box is intentionally out of scope.

namespace CjkPunctCompression {

enum class Class : uint8_t {
  None = 0,
  OpenBracket,
  CloseBracket,
  PauseStop,  // 、，。：；！？ and vertical FE remaps
  MiddleDot,  // ・‧· etc.
};

enum class Profile : uint8_t {
  Off = 0,
  TraditionalChinese,  // conservative: no PauseStop adjacent / line-end PauseStop
  SimplifiedChinese,   // full CLREQ adjacent + line-edge
  Japanese,            // same trim table as SimplifiedChinese
};

inline Profile currentProfile() {
#if defined(ENABLE_CHINESE_VERSION) && defined(CHINESE_UI_SIMPLIFIED)
  return Profile::SimplifiedChinese;
#elif defined(ENABLE_CHINESE_VERSION)
  return Profile::TraditionalChinese;
#elif defined(ENABLE_JAPANESE_VERSION)
  return Profile::Japanese;
#else
  return Profile::Off;
#endif
}

inline bool isSupportedSku() { return currentProfile() != Profile::Off; }

inline Class classify(const uint32_t cp) {
  // Open brackets ≈ kinsoku line-end prohibited openers (currency excluded).
  switch (cp) {
    case '(':
    case '[':
    case '{':
    case 0x00AB:  // «
    case 0x2018:  // ‘
    case 0x201C:  // “
    case 0x3008:  // 〈
    case 0x300A:  // 《
    case 0x300C:  // 「
    case 0x300E:  // 『
    case 0x3010:  // 【
    case 0x3014:  // 〔
    case 0x3016:  // 〖
    case 0x3018:  // 〘
    case 0x301A:  // 〚
    case 0xFF08:  // （
    case 0xFF3B:  // ［
    case 0xFF5B:  // ｛
    case 0xFE35:  // ︵
    case 0xFE37:  // ︷
    case 0xFE39:  // ︹
    case 0xFE3B:  // ︻
    case 0xFE3D:  // ︽
    case 0xFE3F:  // ︿
    case 0xFE41:  // ﹁
    case 0xFE43:  // ﹃
    case 0xFE47:  // ﹇
      return Class::OpenBracket;
    default:
      break;
  }

  // Middle dots / interpuncts (compressible; not treated as PauseStop).
  switch (cp) {
    case 0x00B7:  // ·
    case 0x2022:  // •
    case 0x2027:  // ‧
    case 0x30FB:  // ・
    case 0xFF65:  // ･
      return Class::MiddleDot;
    default:
      break;
  }

  // Pause / stop marks (點號).
  switch (cp) {
    case '.':
    case ',':
    case ':':
    case ';':
    case '!':
    case '?':
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0xFF01:  // ！
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF1A:  // ：
    case 0xFF1B:  // ；
    case 0xFF1F:  // ？
    case 0xFF61:  // ｡
    case 0xFF64:  // ､
    case 0xFE10:  // ︐
    case 0xFE11:  // ︑
    case 0xFE12:  // ︒
    case 0xFE13:  // ︓
    case 0xFE14:  // ︔
    case 0xFE15:  // ︕
    case 0xFE16:  // ︖
      return Class::PauseStop;
    default:
      break;
  }

  // Closing brackets (kinsoku line-start closers minus PauseStop / MiddleDot / dashes).
  switch (cp) {
    case ')':
    case ']':
    case '}':
    case 0x00BB:  // »
    case 0x2019:  // ’
    case 0x201D:  // ”
    case 0x3009:  // 〉
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0x3011:  // 】
    case 0x3015:  // 〕
    case 0x3017:  // 〗
    case 0x3019:  // 〙
    case 0x301B:  // 〛
    case 0xFF09:  // ）
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
    case 0xFE36:  // ︶
    case 0xFE38:  // ︸
    case 0xFE3A:  // ︺
    case 0xFE3C:  // ︼
    case 0xFE3E:  // ︾
    case 0xFE40:  // ﹀
    case 0xFE42:  // ﹂
    case 0xFE44:  // ﹄
    case 0xFE48:  // ﹈
      return Class::CloseBracket;
    default:
      return Class::None;
  }
}

inline bool involvesPauseStop(const Class a, const Class b) { return a == Class::PauseStop || b == Class::PauseStop; }

// Adjacent pair trim in pixels (0 if no compression). Typically half-em; middle-dot
// pairs use quarter-em. Ellipsis/dash inseparable pairs are never compressed.
inline int adjacentTrimPx(const Class left, const Class right, const Profile profile, const int emPx) {
  if (profile == Profile::Off || left == Class::None || right == Class::None || emPx <= 0) {
    return 0;
  }
  // TC: skip PauseStop-involved adjacent pairs (CLREQ 繁體不適用 / little demand).
  if (profile == Profile::TraditionalChinese && involvesPauseStop(left, right)) {
    return 0;
  }
  const int half = std::max(1, emPx / 2);
  const int quarter = std::max(1, emPx / 4);
  if (left == Class::MiddleDot || right == Class::MiddleDot) {
    return quarter;
  }
  return half;
}

inline int lineStartTrimPx(const Class first, const Profile profile, const int emPx) {
  if (profile == Profile::Off || emPx <= 0) return 0;
  // All Chinese/Japanese profiles: trim opening bracket at run start.
  if (first == Class::OpenBracket) return std::max(1, emPx / 2);
  return 0;
}

inline int lineEndTrimPx(const Class last, const Profile profile, const int emPx) {
  if (profile == Profile::Off || emPx <= 0) return 0;
  if (last == Class::CloseBracket) return std::max(1, emPx / 2);
  // SC/JA only: line-end PauseStop trim (GB/T 5.1.10); TC 繁體不適用.
  if (last == Class::PauseStop && profile != Profile::TraditionalChinese) {
    return std::max(1, emPx / 2);
  }
  return 0;
}

// Never reduce a mark below half-em.
inline uint16_t clampAdvanceAfterTrim(const uint16_t advance, const int trim, const int emPx) {
  if (trim <= 0 || advance == 0) return advance;
  const int minAdv = std::max(1, emPx / 2);
  const int next = static_cast<int>(advance) - trim;
  return static_cast<uint16_t>(std::max(minAdv, next));
}

// Apply adjacent half-em (or quarter-em) trims to advance widths. Skips pairs that
// are glued via wordContinues (Latin NBSP-style groups) and inseparable kinsoku pairs.
// Line/column-start OpenBracket hangs via paint-origin shift in ParsedText using the
// glyph's left bearing (leading blank); the opener's advance stays full.
inline void applyAdjacentPunctCompression(const std::vector<std::string>& words, const std::vector<bool>& continuesVec,
                                          std::vector<uint16_t>& advances, const int emPx, const Profile profile) {
  if (profile == Profile::Off || advances.size() < 2 || emPx <= 0) return;
  const size_t n = std::min(words.size(), advances.size());
  for (size_t i = 0; i + 1 < n; ++i) {
    if (i + 1 < continuesVec.size() && continuesVec[i + 1]) {
      continue;
    }
    const uint32_t leftCp = CjkKinsoku::lastCodepoint(words[i]);
    const uint32_t rightCp = CjkKinsoku::firstCodepoint(words[i + 1]);
    if (CjkKinsoku::isInseparablePair(leftCp, rightCp)) {
      continue;
    }
    const Class left = classify(leftCp);
    const Class right = classify(rightCp);
    const int trim = adjacentTrimPx(left, right, profile, emPx);
    if (trim <= 0) continue;
    // Prefer trimming the left mark's trailing advance so the next xpos moves left.
    advances[i] = clampAdvanceAfterTrim(advances[i], trim, emPx);
  }
}

struct EdgeTrim {
  int startTrim = 0;
  int endTrim = 0;
};

inline EdgeTrim lineEdgeTrimPx(const std::vector<std::string>& words, const size_t runStart, const size_t runEnd,
                               const Profile profile, const int emPx) {
  EdgeTrim out;
  if (profile == Profile::Off || runEnd <= runStart || emPx <= 0 || runEnd > words.size()) {
    return out;
  }
  out.startTrim = lineStartTrimPx(classify(CjkKinsoku::firstCodepoint(words[runStart])), profile, emPx);
  out.endTrim = lineEndTrimPx(classify(CjkKinsoku::lastCodepoint(words[runEnd - 1])), profile, emPx);
  return out;
}

}  // namespace CjkPunctCompression
