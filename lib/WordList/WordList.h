#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>

#include "WordListView.h"

// Packed token storage for paragraph layout, replacing std::vector<std::string>.
//
// CJK text tokenizes to one token per Han character. A std::vector<std::string>
// spends 24 bytes of element storage on every 3-byte token (32-bit target), so a
// long paragraph both wastes RAM and, worse, asks the allocator for one block
// twice the size of the live one when the vector doubles — with -fno-exceptions
// a failed growth aborts instead of failing softly. Packing the bytes into a
// single buffer plus a uint32_t offset table costs 8 bytes per Han token and
// caps the largest single request at the offset table.
//
// Growth uses malloc/realloc so OOM returns false instead of abort()ing. Callers
// in the chapter indexer must check tryPushBack / reserveMore and fail the parse.
//
// Every token is stored NUL-terminated so cStr() can feed the C string APIs used
// during measurement without a temporary copy.
class WordList {
 public:
  size_t size() const { return offsetCount_ == 0 ? 0 : offsetCount_ - 1; }
  bool empty() const { return size() == 0; }

  std::string_view operator[](const size_t index) const {
    return {buffer_.get() + offsets_[index], tokenLength(index)};
  }
  const char* cStr(const size_t index) const { return buffer_.get() + offsets_[index]; }
  std::string_view back() const { return (*this)[size() - 1]; }

  WordListView view() const { return {buffer_.get(), offsets_.get(), size()}; }

  // Soft-fail growth. Returns false on OOM; list unchanged.
  bool tryPushBack(const std::string_view token) {
    if (offsetCount_ == 0) {
      if (!ensureOffsets(2)) return false;
      offsets_[0] = 0;
      offsetCount_ = 1;
    }
    const size_t needBytes = bufferSize_ + token.size() + 1;
    if (!ensureBuffer(needBytes)) return false;
    if (!ensureOffsets(offsetCount_ + 1)) return false;

    if (!token.empty()) {
      std::memcpy(buffer_.get() + bufferSize_, token.data(), token.size());
    }
    bufferSize_ += token.size();
    buffer_[bufferSize_++] = '\0';
    offsets_[offsetCount_++] = static_cast<uint32_t>(bufferSize_);
    return true;
  }

  // Convenience for tests / non-critical paths. On OOM the token is dropped
  // (never aborts). Prefer tryPushBack in indexing/layout hot paths.
  void push_back(const std::string_view token) { (void)tryPushBack(token); }

  void clear() {
    bufferSize_ = 0;
    offsetCount_ = 0;
  }

  // Reserves room for extraTokens more tokens holding extraBytes of text.
  // Returns false on OOM; existing contents are preserved.
  bool reserveMore(const size_t extraTokens, const size_t extraBytes) {
    const size_t neededOffsets = size() + extraTokens + 1;
    if (neededOffsets > offsetCap_ && !ensureOffsets(std::max(neededOffsets, offsetCap_ == 0 ? 8 : offsetCap_ * 2))) {
      return false;
    }
    const size_t neededBytes = bufferSize_ + extraBytes;
    if (neededBytes > bufferCap_ && !ensureBuffer(std::max(neededBytes, bufferCap_ == 0 ? 64 : bufferCap_ * 2))) {
      return false;
    }
    return true;
  }

  // Rewrites one token. `token` must not alias this list's storage.
  // Returns false on OOM when growing.
  bool tryReplace(const size_t index, const std::string_view token) {
    const size_t oldLength = tokenLength(index);
    if (token.size() > oldLength) {
      const size_t delta = token.size() - oldLength;
      if (!ensureBuffer(bufferSize_ + delta)) return false;
      // Shift tail right to make room.
      const size_t moveFrom = offsets_[index] + oldLength;
      const size_t moveLen = bufferSize_ - moveFrom;
      std::memmove(buffer_.get() + moveFrom + delta, buffer_.get() + moveFrom, moveLen);
      bufferSize_ += delta;
      for (size_t i = index + 1; i < offsetCount_; ++i) {
        offsets_[i] = static_cast<uint32_t>(offsets_[i] + delta);
      }
    } else if (token.size() < oldLength) {
      const size_t delta = oldLength - token.size();
      const size_t moveFrom = offsets_[index] + oldLength;
      const size_t moveLen = bufferSize_ - moveFrom;
      std::memmove(buffer_.get() + moveFrom - delta, buffer_.get() + moveFrom, moveLen);
      bufferSize_ -= delta;
      for (size_t i = index + 1; i < offsetCount_; ++i) {
        offsets_[i] = static_cast<uint32_t>(offsets_[i] - delta);
      }
    }
    if (!token.empty()) {
      std::memcpy(buffer_.get() + offsets_[index], token.data(), token.size());
    }
    return true;
  }

  void replace(const size_t index, const std::string_view token) { (void)tryReplace(index, token); }

  // Splits token `index` at `byteOffset` into a prefix — optionally gaining a
  // visible hyphen — and a remainder inserted directly after it.
  bool trySplitAt(const size_t index, const size_t byteOffset, const bool appendHyphenToPrefix) {
    const char inserted[2] = {appendHyphenToPrefix ? '-' : '\0', '\0'};
    const size_t insertedBytes = appendHyphenToPrefix ? 2 : 1;
    if (!ensureBuffer(bufferSize_ + insertedBytes)) return false;
    if (!ensureOffsets(offsetCount_ + 1)) return false;

    const size_t splitAtByte = offsets_[index] + byteOffset;
    std::memmove(buffer_.get() + splitAtByte + insertedBytes, buffer_.get() + splitAtByte, bufferSize_ - splitAtByte);
    std::memcpy(buffer_.get() + splitAtByte, inserted, insertedBytes);
    bufferSize_ += insertedBytes;

    // Shift offset entries right and insert the new boundary.
    for (size_t i = offsetCount_; i > index + 1; --i) {
      offsets_[i] = offsets_[i - 1];
    }
    offsets_[index + 1] = static_cast<uint32_t>(splitAtByte + insertedBytes);
    offsetCount_++;
    for (size_t i = index + 2; i < offsetCount_; ++i) {
      offsets_[i] = static_cast<uint32_t>(offsets_[i] + insertedBytes);
    }
    return true;
  }

  void splitAt(const size_t index, const size_t byteOffset, const bool appendHyphenToPrefix) {
    (void)trySplitAt(index, byteOffset, appendHyphenToPrefix);
  }

  // Drops the first `count` tokens, keeping both allocations for the next page.
  void eraseFront(const size_t count) {
    if (count == 0) return;
    if (count >= size()) {
      clear();
      return;
    }
    const uint32_t base = offsets_[count];
    const size_t newSize = bufferSize_ - base;
    std::memmove(buffer_.get(), buffer_.get() + base, newSize);
    bufferSize_ = newSize;
    const size_t newOffsetCount = offsetCount_ - count;
    for (size_t i = 0; i < newOffsetCount; ++i) {
      offsets_[i] = offsets_[i + count] - base;
    }
    offsetCount_ = newOffsetCount;
  }

 private:
  size_t tokenLength(const size_t index) const { return offsets_[index + 1] - offsets_[index] - 1; }

  bool ensureBuffer(const size_t need) {
    if (need <= bufferCap_) return true;
    size_t ncap = bufferCap_ == 0 ? 64 : bufferCap_;
    while (ncap < need) {
      ncap *= 2;
    }
    char* old = buffer_.release();
    char* p = static_cast<char*>(std::realloc(old, ncap));
    if (p == nullptr) {
      buffer_.reset(old);  // realloc failed: original block still valid
      return false;
    }
    buffer_.reset(p);
    bufferCap_ = ncap;
    return true;
  }

  bool ensureOffsets(const size_t need) {
    if (need <= offsetCap_) return true;
    size_t ncap = offsetCap_ == 0 ? 8 : offsetCap_;
    while (ncap < need) {
      ncap *= 2;
    }
    uint32_t* old = offsets_.release();
    auto* p = static_cast<uint32_t*>(std::realloc(old, ncap * sizeof(uint32_t)));
    if (p == nullptr) {
      offsets_.reset(old);
      return false;
    }
    offsets_.reset(p);
    offsetCap_ = ncap;
    return true;
  }

  // Custom deleters so unique_ptr can free realloc'd blocks.
  struct FreeDeleter {
    void operator()(void* p) const { std::free(p); }
  };

  std::unique_ptr<char[], FreeDeleter> buffer_;
  size_t bufferSize_ = 0;
  size_t bufferCap_ = 0;
  std::unique_ptr<uint32_t[], FreeDeleter> offsets_;
  size_t offsetCount_ = 0;
  size_t offsetCap_ = 0;
};
