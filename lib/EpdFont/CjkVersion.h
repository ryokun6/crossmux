#pragma once

// Shared gate for CJK typography / embedded CJK bitmap fonts.
// Locale-specific SKUs also define ENABLE_CHINESE_VERSION,
// ENABLE_JAPANESE_VERSION, or ENABLE_KOREAN_VERSION.
#if defined(ENABLE_CHINESE_VERSION) || defined(ENABLE_JAPANESE_VERSION) || defined(ENABLE_KOREAN_VERSION)
#ifndef ENABLE_CJK_VERSION
#define ENABLE_CJK_VERSION 1
#endif
#endif
