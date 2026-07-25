#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
// Every token is stored NUL-terminated so cStr() can feed the C string APIs used
// during measurement without a temporary copy.
class WordList {
 public:
  size_t size() const { return offsets_.empty() ? 0 : offsets_.size() - 1; }
  bool empty() const { return size() == 0; }

  std::string_view operator[](const size_t index) const {
    return {buffer_.data() + offsets_[index], tokenLength(index)};
  }
  const char* cStr(const size_t index) const { return buffer_.data() + offsets_[index]; }
  std::string_view back() const { return (*this)[size() - 1]; }

  WordListView view() const { return {buffer_.data(), offsets_.data(), size()}; }

  void push_back(const std::string_view token) {
    if (offsets_.empty()) offsets_.push_back(0);
    buffer_.append(token);
    buffer_.push_back('\0');
    offsets_.push_back(static_cast<uint32_t>(buffer_.size()));
  }

  void clear() {
    buffer_.clear();
    offsets_.clear();
  }

  // Reserves room for extraTokens more tokens holding extraBytes of text.
  // Grows geometrically: reserve() sizes a block exactly, so asking for only
  // what the current word needs would re-allocate on every word.
  void reserveMore(const size_t extraTokens, const size_t extraBytes) {
    const size_t neededOffsets = size() + extraTokens + 1;
    if (neededOffsets > offsets_.capacity()) {
      offsets_.reserve(std::max(neededOffsets, offsets_.capacity() * 2));
    }
    const size_t neededBytes = buffer_.size() + extraBytes;
    if (neededBytes > buffer_.capacity()) {
      buffer_.reserve(std::max(neededBytes, buffer_.capacity() * 2));
    }
  }

  // Rewrites one token. `token` must not alias this list's storage.
  void replace(const size_t index, const std::string_view token) {
    const size_t oldLength = tokenLength(index);
    buffer_.replace(offsets_[index], oldLength, token);
    if (token.size() == oldLength) return;
    const auto delta = static_cast<int32_t>(token.size()) - static_cast<int32_t>(oldLength);
    for (size_t i = index + 1; i < offsets_.size(); ++i) {
      offsets_[i] = static_cast<uint32_t>(static_cast<int32_t>(offsets_[i]) + delta);
    }
  }

  // Splits token `index` at `byteOffset` into a prefix — optionally gaining a
  // visible hyphen — and a remainder inserted directly after it.
  void splitAt(const size_t index, const size_t byteOffset, const bool appendHyphenToPrefix) {
    // The remainder needs its own terminator; the inserted hyphen belongs to the
    // prefix, so it goes in front of that terminator.
    const char inserted[2] = {appendHyphenToPrefix ? '-' : '\0', '\0'};
    const size_t insertedBytes = appendHyphenToPrefix ? 2 : 1;
    const size_t splitAtByte = offsets_[index] + byteOffset;
    buffer_.insert(splitAtByte, inserted, insertedBytes);
    offsets_.insert(offsets_.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                    static_cast<uint32_t>(splitAtByte + insertedBytes));
    for (size_t i = index + 2; i < offsets_.size(); ++i) {
      offsets_[i] += static_cast<uint32_t>(insertedBytes);
    }
  }

  // Drops the first `count` tokens, keeping both allocations for the next page.
  void eraseFront(const size_t count) {
    if (count == 0) return;
    if (count >= size()) {
      clear();
      return;
    }
    const uint32_t base = offsets_[count];
    buffer_.erase(0, base);
    offsets_.erase(offsets_.begin(), offsets_.begin() + static_cast<std::ptrdiff_t>(count));
    for (auto& offset : offsets_) {
      offset -= base;
    }
  }

 private:
  size_t tokenLength(const size_t index) const { return offsets_[index + 1] - offsets_[index] - 1; }

  std::string buffer_;
  std::vector<uint32_t> offsets_;
};
