#pragma once

#ifdef ENABLE_CHINESE_VERSION
// CN-only build: ship NotoSansSC at 5 sizes (Regular) with CJK glyphs for
// ~3,500 common chars + 通用规范汉字表 一级 + CJK punctuation. The
// international Latin/Cyrillic/etc. font headers are not included because
// they would push the firmware past the 6.25 MB app partition and the
// NotoSansSC fonts already cover Latin glyphs via the Noto family.
// 5 sizes match the Latin built-in slot lineup: UI 10/12pt, reader 12/14/16/18pt.
#include <builtinFonts/chinese_chess_16.h>
#include <builtinFonts/cn/notosanssc_8_regular.h>
#include <builtinFonts/cn/notosanssc_10_regular.h>
#include <builtinFonts/cn/notosanssc_12_regular.h>
#include <builtinFonts/cn/notosanssc_14_regular.h>
#include <builtinFonts/cn/notosanssc_16_regular.h>
#include <builtinFonts/cn/notosanssc_18_regular.h>
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
#include <builtinFonts/opendyslexic_10_bold.h>
#include <builtinFonts/opendyslexic_10_bolditalic.h>
#include <builtinFonts/opendyslexic_10_italic.h>
#include <builtinFonts/opendyslexic_10_regular.h>
#include <builtinFonts/opendyslexic_12_bold.h>
#include <builtinFonts/opendyslexic_12_bolditalic.h>
#include <builtinFonts/opendyslexic_12_italic.h>
#include <builtinFonts/opendyslexic_12_regular.h>
#include <builtinFonts/opendyslexic_14_bold.h>
#include <builtinFonts/opendyslexic_14_bolditalic.h>
#include <builtinFonts/opendyslexic_14_italic.h>
#include <builtinFonts/opendyslexic_14_regular.h>
#include <builtinFonts/opendyslexic_8_bold.h>
#include <builtinFonts/opendyslexic_8_bolditalic.h>
#include <builtinFonts/opendyslexic_8_italic.h>
#include <builtinFonts/opendyslexic_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>
#endif
