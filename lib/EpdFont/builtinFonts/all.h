#pragma once

// Ubuntu UI faces are shared by every image. UITheme.cpp includes them directly
// as well; including them here keeps reader translation units consistent.
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#if defined(ENABLE_CJK_VERSION)
// ryOS per-SKU embedded reader faces. TC/SC use GenSen Rounded TW (SC is keyed
// by Simplified codepoints; the other script remaps at glyph lookup). JA uses
// GenSen Rounded 2 JP. KO uses Resource Han Rounded KR.
#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
#include <builtinFonts/notosans_sc_8.h>
#include <builtinFonts/notosans_sc_10.h>
#include <builtinFonts/notosans_sc_12.h>
#include <builtinFonts/notosans_sc_14.h>
#include <builtinFonts/notosans_sc_16.h>
#include <builtinFonts/notosans_sc_18.h>
#else
#include <builtinFonts/notosans_cjk_8.h>
#include <builtinFonts/notosans_cjk_10.h>
#include <builtinFonts/notosans_cjk_12.h>
#include <builtinFonts/notosans_cjk_14.h>
#include <builtinFonts/notosans_cjk_16.h>
#include <builtinFonts/notosans_cjk_18.h>
#endif
#elif defined(ENABLE_JAPANESE_VERSION)
#include <builtinFonts/notosans_ja_8.h>
#include <builtinFonts/notosans_ja_10.h>
#include <builtinFonts/notosans_ja_12.h>
#include <builtinFonts/notosans_ja_14.h>
#include <builtinFonts/notosans_ja_16.h>
#include <builtinFonts/notosans_ja_18.h>
#elif defined(ENABLE_KOREAN_VERSION)
#include <builtinFonts/notosans_ko_8.h>
#include <builtinFonts/notosans_ko_10.h>
#include <builtinFonts/notosans_ko_12.h>
#include <builtinFonts/notosans_ko_14.h>
#include <builtinFonts/notosans_ko_16.h>
#include <builtinFonts/notosans_ko_18.h>
#endif
#else
// Unified firmware, including X4 Pro: compact international reader faces plus
// 8/10/12pt CJK fallbacks. Larger families ship as SD .cpfont files so the
// 6.25 MB app slot still fits. The 8/10/12 CJK headers kept here are the fork's
// GenSen bitmaps (same symbol names as upstream's trimmed pool).
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/notosans_18_bold.h>
#include <builtinFonts/notosans_18_regular.h>
#include <builtinFonts/notosans_cjk_8.h>
#include <builtinFonts/notosans_cjk_10.h>
#include <builtinFonts/notosans_cjk_12.h>
#ifdef CROSSMUX_UI_PROFILE_HIGH_DPI
#include <builtinFonts/notosans_12_regular.h>
#include <builtinFonts/notosans_cjk_14.h>
#include <builtinFonts/notosans_cjk_16.h>
#endif
#endif
