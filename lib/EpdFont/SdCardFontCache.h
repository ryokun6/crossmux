#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

// Temporary stub until the inactive-OTA SD font cache is ported onto the
// fork SdCardFont stack. Upstream Boot/TextSettings compile against this.
namespace SdCardFontCache {

enum class Result : uint8_t {
  Ok = 0,
  AlreadyCached,
  TooLarge,
  Oom,
  OpenFailed,
  InvalidFont,
  ReadFailed,
  EraseFailed,
  WriteFailed,
  VerifyFailed,
  NotSafe,
};

inline const char* resultName(Result r) {
  switch (r) {
    case Result::Ok: return "ok";
    case Result::AlreadyCached: return "already_cached";
    case Result::TooLarge: return "too_large";
    case Result::Oom: return "oom";
    case Result::OpenFailed: return "open_failed";
    case Result::InvalidFont: return "invalid_font";
    case Result::ReadFailed: return "read_failed";
    case Result::EraseFailed: return "erase_failed";
    case Result::WriteFailed: return "write_failed";
    case Result::VerifyFailed: return "verify_failed";
    case Result::NotSafe: return "not_safe";
  }
  return "unknown";
}

inline bool isValidFor(const char* /*path*/, size_t* payloadSize = nullptr) {
  if (payloadSize) *payloadSize = 0;
  return false;
}

template <typename... Args>
inline Result preload(Args&&...) {
  return Result::NotSafe;
}

}  // namespace SdCardFontCache
