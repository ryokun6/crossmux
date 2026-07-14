#pragma once

#include <Utf8.h>

#include <cstdint>
#include <string>
#include <vector>

// CJK line-break prohibition (禁則 / kinsoku) classification.
// Shared by ParsedText tokenization, horizontal line breaking, and vertical
// column breaking. Tables follow CSS Text "normal" plus JLReq TF guidance
// (small kana + prolonged sound mark as line-start prohibited on JA builds).
// Hanging punctuation (懸掛) is intentionally not handled here.
//
// Vertical-rl uses the same helpers after punctuation is remapped to
// presentation forms (FE1x / FE3x); those codepoints are included below.

namespace CjkKinsoku {

// Characters that must not begin a line (行頭禁則).
inline bool isLineStartProhibited(const uint32_t cp) {
  switch (cp) {
    // ASCII closers / stops (common in mixed CJK text)
    case '.':
    case ',':
    case ':':
    case ';':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case '%':
    // Latin-1 / general punctuation closers
    case 0x00B0:  // °
    case 0x00BB:  // »
    case 0x2019:  // ’
    case 0x201D:  // ”
    case 0x2020:  // †
    case 0x2021:  // ‡
    case 0x2030:  // ‰
    case 0x203C:  // ‼
    case 0x2047:  // ⁇
    case 0x2048:  // ⁈
    case 0x2049:  // ⁉
    case 0x2103:  // ℃
    // Hyphens / dash-like (must not start a line)
    case 0x2010:  // ‐
    case 0x2013:  // –
    case 0x2014:  // —
    case 0x2015:  // ―
    case 0x2025:  // ‥
    case 0x2026:  // …
    case 0x301C:  // 〜
    case 0x30A0:  // ゠
    case 0x30FB:  // ・
    // CJK closers / stops
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0x3005:  // 々
    case 0x3009:  // 〉
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0x3011:  // 】
    case 0x3015:  // 〕
    case 0x3017:  // 〗
    case 0x3019:  // 〙
    case 0x301B:  // 〛
    case 0x303B:  // 〻
    // Fullwidth closers / stops
    case 0xFF01:  // ！
    case 0xFF05:  // ％
    case 0xFF09:  // ）
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF1A:  // ：
    case 0xFF1B:  // ；
    case 0xFF1F:  // ？
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
    case 0xFF61:  // ｡
    case 0xFF64:  // ､
    case 0xFF65:  // ･
    // Vertical presentation forms (closers / stops)
    case 0xFE10:  // ︐
    case 0xFE11:  // ︑
    case 0xFE12:  // ︒
    case 0xFE13:  // ︓
    case 0xFE14:  // ︔
    case 0xFE15:  // ︕
    case 0xFE16:  // ︖
    case 0xFE19:  // ︙
    case 0xFE30:  // ︰
    case 0xFE31:  // ︱
    case 0xFE32:  // ︲
    case 0xFE36:  // ︶
    case 0xFE38:  // ︸
    case 0xFE3A:  // ︺
    case 0xFE3C:  // ︼
    case 0xFE3E:  // ︾
    case 0xFE40:  // ﹀
    case 0xFE42:  // ﹂
    case 0xFE44:  // ﹄
    case 0xFE48:  // ﹈
#if defined(ENABLE_JAPANESE_VERSION)
    // Small kana + prolonged sound mark (JLReq / CSS strict-as-normal for readers)
    case 0x3041:  // ぁ
    case 0x3043:  // ぃ
    case 0x3045:  // ぅ
    case 0x3047:  // ぇ
    case 0x3049:  // ぉ
    case 0x3063:  // っ
    case 0x3083:  // ゃ
    case 0x3085:  // ゅ
    case 0x3087:  // ょ
    case 0x308E:  // ゎ
    case 0x3095:  // ゕ
    case 0x3096:  // ゖ
    case 0x30A1:  // ァ
    case 0x30A3:  // ィ
    case 0x30A5:  // ゥ
    case 0x30A7:  // ェ
    case 0x30A9:  // ォ
    case 0x30C3:  // ッ
    case 0x30E3:  // ャ
    case 0x30E5:  // ュ
    case 0x30E7:  // ョ
    case 0x30EE:  // ヮ
    case 0x30F5:  // ヵ
    case 0x30F6:  // ヶ
    case 0x30FC:  // ー
    case 0x31F0:  // ㇰ
    case 0x31F1:  // ㇱ
    case 0x31F2:  // ㇲ
    case 0x31F3:  // ㇳ
    case 0x31F4:  // ㇴ
    case 0x31F5:  // ㇵ
    case 0x31F6:  // ㇶ
    case 0x31F7:  // ㇷ
    case 0x31F8:  // ㇸ
    case 0x31F9:  // ㇹ
    case 0x31FA:  // ㇺ
    case 0x31FB:  // ㇻ
    case 0x31FC:  // ㇼ
    case 0x31FD:  // ㇽ
    case 0x31FE:  // ㇾ
    case 0x31FF:  // ㇿ
    case 0xFF67:  // ｧ
    case 0xFF68:  // ｨ
    case 0xFF69:  // ｩ
    case 0xFF6A:  // ｪ
    case 0xFF6B:  // ｫ
    case 0xFF6C:  // ｬ
    case 0xFF6D:  // ｭ
    case 0xFF6E:  // ｮ
    case 0xFF6F:  // ｯ
    case 0xFF70:  // ｰ
#endif
      return true;
    default:
      return false;
  }
}

// Characters that must not end a line (行末禁則).
inline bool isLineEndProhibited(const uint32_t cp) {
  switch (cp) {
    case '(':
    case '[':
    case '{':
    case '$':
    case 0x00A3:  // £
    case 0x00A5:  // ¥
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
    case 0xFF04:  // ＄
    case 0xFF08:  // （
    case 0xFF3B:  // ［
    case 0xFF5B:  // ｛
    case 0xFFE1:  // ￡
    case 0xFFE5:  // ￥
    case 0xFFE6:  // ￦
    case 0xFE35:  // ︵
    case 0xFE37:  // ︷
    case 0xFE39:  // ︹
    case 0xFE3B:  // ︻
    case 0xFE3D:  // ︽
    case 0xFE3F:  // ︿
    case 0xFE41:  // ﹁
    case 0xFE43:  // ﹃
    case 0xFE47:  // ﹇
      return true;
    default:
      return false;
  }
}

inline bool isAsciiDigit(const uint32_t cp) { return cp >= '0' && cp <= '9'; }

inline bool isDigitSuffixUnit(const uint32_t cp) {
  switch (cp) {
    case '%':
    case 0x00B0:  // °
    case 0x2030:  // ‰
    case 0x2103:  // ℃
    case 0x2109:  // ℉
    case 0xFF05:  // ％
      return true;
    default:
      return false;
  }
}

inline bool isCurrencyPrefix(const uint32_t cp) {
  switch (cp) {
    case '$':
    case 0x00A3:  // £
    case 0x00A5:  // ¥
    case 0xFF04:  // ＄
    case 0xFFE1:  // ￡
    case 0xFFE5:  // ￥
    case 0xFFE6:  // ￦
      return true;
    default:
      return false;
  }
}

inline bool isEllipsisOrDash(const uint32_t cp) {
  switch (cp) {
    case 0x2013:  // –
    case 0x2014:  // —
    case 0x2015:  // ―
    case 0x2025:  // ‥
    case 0x2026:  // …
    case 0x22EE:  // ⋮
    case 0x22EF:  // ⋯
    case 0x2E3A:  // ⸺ TWO-EM DASH
    case 0x2E3B:  // ⸻ THREE-EM DASH
    case 0xFE19:  // ︙
    case 0xFE30:  // ︰
    case 0xFE31:  // ︱
    case 0xFE32:  // ︲
      return true;
    default:
      return false;
  }
}

// Pairs that must not be split across lines (分離禁則).
inline bool isInseparablePair(const uint32_t leftCp, const uint32_t rightCp) {
  if (isEllipsisOrDash(leftCp) && isEllipsisOrDash(rightCp)) return true;
  if (isAsciiDigit(leftCp) && isDigitSuffixUnit(rightCp)) return true;
  if (isCurrencyPrefix(leftCp) && isAsciiDigit(rightCp)) return true;
  return false;
}

// True when a line/column break after `leftCp` (next line starts with `rightCp`) is allowed.
inline bool isLegalBreakBetween(const uint32_t leftCp, const uint32_t rightCp) {
  if (isLineEndProhibited(leftCp)) return false;
  if (isLineStartProhibited(rightCp)) return false;
  if (isInseparablePair(leftCp, rightCp)) return false;
  if (utf8IsCombiningMark(rightCp)) return false;
  return true;
}

// Intra-CJK soft-break opportunity (used when splitting multi-codepoint tokens).
inline bool hasCjkBreakOpportunityBetween(const uint32_t leftCp, const uint32_t rightCp) {
  if (!utf8IsCjkBreakable(leftCp) && !utf8IsCjkBreakable(rightCp)) return false;
  return isLegalBreakBetween(leftCp, rightCp);
}

inline uint32_t firstCodepoint(const std::string& text) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(text.c_str());
  return utf8NextCodepoint(&ptr);
}

inline uint32_t lastCodepoint(const std::string& text) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(text.c_str());
  uint32_t last = 0;
  while (*ptr) {
    last = utf8NextCodepoint(&ptr);
  }
  return last;
}

// Repair a candidate line/column break so the next run does not start with
// 行頭禁則 and the current run does not end with 行末禁則. Used by both
// horizontal line breaking and vertical-rl column packing.
//
// Repair only retreats the break. It must never absorb a token that did not fit:
// doing so would exceed the line/column measure. If the run contains only one
// token, keeping the bounded break is the only safe fallback.
inline size_t repairBreakIndex(const std::vector<std::string>& words, const std::vector<bool>& continuesVec,
                               const size_t runStart, size_t breakAt) {
  if (breakAt <= runStart || breakAt > words.size()) {
    return breakAt;
  }

  auto retreatOne = [&]() {
    if (breakAt <= runStart + 1) return false;
    --breakAt;
    while (breakAt > runStart + 1 && breakAt < words.size() && continuesVec[breakAt]) {
      --breakAt;
    }
    return breakAt > runStart;
  };

  while (breakAt < words.size() && isLineStartProhibited(firstCodepoint(words[breakAt]))) {
    if (!retreatOne()) break;
  }

  while (breakAt > runStart && breakAt <= words.size() && isLineEndProhibited(lastCodepoint(words[breakAt - 1]))) {
    if (!retreatOne()) break;
  }

  while (breakAt > runStart && breakAt < words.size() &&
         isInseparablePair(lastCodepoint(words[breakAt - 1]), firstCodepoint(words[breakAt]))) {
    if (!retreatOne()) break;
  }

  return breakAt;
}

}  // namespace CjkKinsoku
