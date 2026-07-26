#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// Non-owning view over a packed word list (see WordList.h): one contiguous byte
// buffer holding every token, plus count + 1 start offsets.
//
// Tokens are stored NUL-terminated inside the buffer, so cStr() hands a token to
// the C string APIs used for glyph metrics and BiDi probing without copying it.
//
// This header deliberately depends on nothing: the EPUB layer owns the word list
// but the font and renderer layers below it must be able to read one.
struct WordListView {
  const char* bytes = nullptr;
  const uint32_t* offsets = nullptr;
  size_t count = 0;

  size_t size() const { return count; }
  bool empty() const { return count == 0; }

  std::string_view operator[](const size_t index) const {
    return {bytes + offsets[index], offsets[index + 1] - offsets[index] - 1};
  }
  const char* cStr(const size_t index) const { return bytes + offsets[index]; }

  // Yields each token as a C string so range algorithms can walk the list
  // without materializing std::string copies.
  class CStrIterator {
   public:
    CStrIterator(const WordListView* list, const size_t index) : list_(list), index_(index) {}
    const char* operator*() const { return list_->cStr(index_); }
    CStrIterator& operator++() {
      ++index_;
      return *this;
    }
    bool operator!=(const CStrIterator& other) const { return index_ != other.index_; }

   private:
    const WordListView* list_;
    size_t index_;
  };

  CStrIterator begin() const { return {this, 0}; }
  CStrIterator end() const { return {this, count}; }
};
