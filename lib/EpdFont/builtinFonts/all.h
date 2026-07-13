#pragma once

#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
// Simplified Chinese build: SC-keyed CJK headers from GenSen Rounded TW
// (subset by Simplified codepoints). Traditional EPUB codepoints remap at
// runtime (TcToScRemap.h).
#include <builtinFonts/notosans_sc_8.h>
#include <builtinFonts/notosans_sc_10.h>
#include <builtinFonts/notosans_sc_12.h>
#include <builtinFonts/notosans_sc_14.h>
#include <builtinFonts/notosans_sc_16.h>
#include <builtinFonts/notosans_sc_18.h>
#else
// Traditional Chinese build: TC-keyed CJK headers from GenSen Rounded TW.
// Simplified codepoints remap at runtime (ScToTcRemap.h).
#include <builtinFonts/notosans_cjk_8.h>
#include <builtinFonts/notosans_cjk_10.h>
#include <builtinFonts/notosans_cjk_12.h>
#include <builtinFonts/notosans_cjk_14.h>
#include <builtinFonts/notosans_cjk_16.h>
#include <builtinFonts/notosans_cjk_18.h>
#endif
#elif defined(ENABLE_JAPANESE_VERSION)
// Japanese build: GenSen Rounded 2 JP Regular (Japanese orthography; no Han remap).
#include <builtinFonts/notosans_ja_8.h>
#include <builtinFonts/notosans_ja_10.h>
#include <builtinFonts/notosans_ja_12.h>
#include <builtinFonts/notosans_ja_14.h>
#include <builtinFonts/notosans_ja_16.h>
#include <builtinFonts/notosans_ja_18.h>
#elif defined(ENABLE_KOREAN_VERSION)
// Korean build: Resource Han Rounded KR Regular (no Han remap).
#include <builtinFonts/notosans_ko_8.h>
#include <builtinFonts/notosans_ko_10.h>
#include <builtinFonts/notosans_ko_12.h>
#include <builtinFonts/notosans_ko_14.h>
#include <builtinFonts/notosans_ko_16.h>
#include <builtinFonts/notosans_ko_18.h>
#else
#include <builtinFonts/notoserif_12_bold.h>
#include <builtinFonts/notoserif_12_bolditalic.h>
#include <builtinFonts/notoserif_12_italic.h>
#include <builtinFonts/notoserif_12_regular.h>
#include <builtinFonts/notoserif_14_bold.h>
#include <builtinFonts/notoserif_14_bolditalic.h>
#include <builtinFonts/notoserif_14_italic.h>
#include <builtinFonts/notoserif_14_regular.h>
#include <builtinFonts/notoserif_16_bold.h>
#include <builtinFonts/notoserif_16_bolditalic.h>
#include <builtinFonts/notoserif_16_italic.h>
#include <builtinFonts/notoserif_16_regular.h>
#include <builtinFonts/notoserif_18_bold.h>
#include <builtinFonts/notoserif_18_bolditalic.h>
#include <builtinFonts/notoserif_18_italic.h>
#include <builtinFonts/notoserif_18_regular.h>
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/notosans_12_bold.h>
#include <builtinFonts/notosans_12_bolditalic.h>
#include <builtinFonts/notosans_12_italic.h>
#include <builtinFonts/notosans_12_regular.h>
#include <builtinFonts/notosans_14_bold.h>
#include <builtinFonts/notosans_14_bolditalic.h>
#include <builtinFonts/notosans_14_italic.h>
#include <builtinFonts/notosans_14_regular.h>
#include <builtinFonts/notosans_16_bold.h>
#include <builtinFonts/notosans_16_bolditalic.h>
#include <builtinFonts/notosans_16_italic.h>
#include <builtinFonts/notosans_16_regular.h>
#include <builtinFonts/notosans_18_bold.h>
#include <builtinFonts/notosans_18_bolditalic.h>
#include <builtinFonts/notosans_18_italic.h>
#include <builtinFonts/notosans_18_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>
#endif
