#include "WeReadHighlightDetailActivity.h"

#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include "../../../components/UITheme.h"
#include "../../../fontIds.h"

namespace {

void appendCodepoint(std::string& dst, uint32_t cp) {
  if (cp < 0x80) {
    dst.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    dst.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    dst.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    dst.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    dst.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    dst.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    dst.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    dst.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    dst.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    dst.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Find the byte offset of the last ASCII space in [0, line.size()). Returns
// std::string::npos if not found.
size_t findLastSpace(const std::string& line) { return line.find_last_of(' '); }

// CJK-aware word wrap. Splits CJK at every codepoint and Latin at spaces.
// Falls back to a hard break when a single Latin run exceeds maxWidth.
std::vector<std::string> wrapHighlight(const GfxRenderer& renderer, int fontId, const std::string& text, int maxWidth) {
  std::vector<std::string> out;
  if (text.empty() || maxWidth <= 0) return out;

  std::string current;
  current.reserve(text.size());

  auto flushLine = [&](bool trimLeadingSpace) {
    std::string line = current;
    if (trimLeadingSpace && !line.empty() && line.front() == ' ') line.erase(0, 1);
    out.push_back(std::move(line));
    current.clear();
  };

  const auto* p = reinterpret_cast<const unsigned char*>(text.c_str());
  bool atLineStart = true;
  while (true) {
    uint32_t cp = utf8NextCodepoint(&p);
    if (cp == 0) break;

    if (cp == '\n') {
      flushLine(false);
      atLineStart = true;
      continue;
    }
    // Collapse leading whitespace at line start to avoid stray indents after
    // a soft wrap.
    if (atLineStart && cp == ' ') continue;
    atLineStart = false;

    std::string candidate = current;
    appendCodepoint(candidate, cp);
    if (renderer.getTextWidth(fontId, candidate.c_str()) <= maxWidth) {
      current = std::move(candidate);
      continue;
    }

    // Doesn't fit: decide where to break.
    if (current.empty()) {
      // Even a single codepoint exceeds the width — push it anyway so we make
      // forward progress and don't infinite-loop.
      current = std::move(candidate);
      flushLine(false);
      atLineStart = true;
      continue;
    }

    const bool cpIsCjk = utf8IsCjkBreakable(cp);
    const bool cpIsSpace = (cp == ' ');
    // Last char of current as codepoint to decide if previous boundary is a
    // natural break (CJK char on either side, or trailing space).
    const unsigned char lastByte = static_cast<unsigned char>(current.back());
    const bool prevIsAscii = lastByte < 0x80;
    // Cheap heuristic: CJK ends in a multibyte sequence (high bit set). For an
    // exact codepoint check we'd have to scan back, but for line-break purposes
    // this is enough — Latin words don't contain raw high-bit bytes outside
    // multibyte sequences here.
    const bool prevIsCjkLike = !prevIsAscii;

    if (cpIsCjk || prevIsCjkLike || cpIsSpace) {
      // Natural break point: end the line here, start the next with cp (unless
      // cp is whitespace, in which case we drop it).
      flushLine(false);
      if (!cpIsSpace) {
        appendCodepoint(current, cp);
      } else {
        atLineStart = true;
      }
      continue;
    }

    // Inside a Latin word: break at the last space if any; otherwise hard-break
    // (long URL or single token wider than the viewport).
    size_t sp = findLastSpace(current);
    if (sp == std::string::npos) {
      // Hard break before cp.
      flushLine(false);
      appendCodepoint(current, cp);
    } else {
      std::string carry = current.substr(sp + 1);
      current.erase(sp);
      flushLine(false);
      current = std::move(carry);
      appendCodepoint(current, cp);
    }
  }

  if (!current.empty()) out.push_back(std::move(current));
  return out;
}

}  // namespace

WeReadHighlightDetailActivity::WeReadHighlightDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                             std::string bookTitle, ItemSource source,
                                                             int initialIndex)
    : Activity("WeReadHighlightDetail", renderer, mappedInput),
      bookTitle_(std::move(bookTitle)),
      source_(source),
      currentIndex_(initialIndex) {}

const std::string& WeReadHighlightDetailActivity::currentText() const {
  static const std::string kEmpty;
  if (!source_.getText || currentIndex_ < 0 || currentIndex_ >= source_.itemCount) return kEmpty;
  return source_.getText(source_.ctx, currentIndex_);
}

int WeReadHighlightDetailActivity::currentCount() const {
  if (!source_.getCount || currentIndex_ < 0 || currentIndex_ >= source_.itemCount) return -1;
  return source_.getCount(source_.ctx, currentIndex_);
}

int WeReadHighlightDetailActivity::maxScrollOffset() const {
  return std::max(0, static_cast<int>(lines_.size()) - visibleLines_);
}

void WeReadHighlightDetailActivity::rewrap() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int contentWidth = sw - 2 * metrics.contentSidePadding;
  lines_ = wrapHighlight(renderer, UI_12_FONT_ID, currentText(), contentWidth);
}

void WeReadHighlightDetailActivity::switchTo(int newIndex) {
  if (newIndex < 0 || newIndex >= source_.itemCount) return;
  currentIndex_ = newIndex;
  scrollOffset_ = 0;
  rewrap();
  requestUpdate();
}

void WeReadHighlightDetailActivity::onEnter() {
  Activity::onEnter();
  if (currentIndex_ < 0) currentIndex_ = 0;
  if (source_.itemCount > 0 && currentIndex_ >= source_.itemCount) currentIndex_ = source_.itemCount - 1;
  rewrap();
  scrollOffset_ = 0;
  requestUpdate();
}

void WeReadHighlightDetailActivity::loop() {
  buttonNavigator.onPreviousPress([this] {
    pressStartedAtTop_ = (scrollOffset_ == 0);
    if (scrollOffset_ > 0) {
      --scrollOffset_;
      requestUpdate();
    }
  });
  buttonNavigator.onPreviousContinuous([this] {
    if (scrollOffset_ > 0) {
      --scrollOffset_;
      requestUpdate();
    }
  });
  buttonNavigator.onPreviousRelease([this] {
    if (pressStartedAtTop_ && currentIndex_ > 0) {
      switchTo(currentIndex_ - 1);
    }
    pressStartedAtTop_ = false;
  });

  buttonNavigator.onNextPress([this] {
    const int maxOffset = maxScrollOffset();
    pressStartedAtEnd_ = (visibleLines_ > 0 && scrollOffset_ >= maxOffset);
    if (!pressStartedAtEnd_) {
      ++scrollOffset_;
      requestUpdate();
    }
  });
  buttonNavigator.onNextContinuous([this] {
    const int maxOffset = maxScrollOffset();
    if (visibleLines_ > 0 && scrollOffset_ < maxOffset) {
      ++scrollOffset_;
      requestUpdate();
    }
  });
  buttonNavigator.onNextRelease([this] {
    if (pressStartedAtEnd_ && currentIndex_ + 1 < source_.itemCount) {
      switchTo(currentIndex_ + 1);
    }
    pressStartedAtEnd_ = false;
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void WeReadHighlightDetailActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, bookTitle_.c_str(),
                 tr(STR_WEREAD_HIGHLIGHT_DETAIL_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  // Reserve a small strip above the button hints for the "N 人划线" label when
  // applicable. Keeps the line-height math stable regardless of orientation.
  const int displayCount = currentCount();
  const int countStripHeight = (displayCount >= 0) ? 22 : 0;
  const int contentBottom = sh - metrics.buttonHintsHeight - metrics.verticalSpacing - countStripHeight;
  const int contentHeight = contentBottom - contentTop;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  visibleLines_ = (lineHeight > 0) ? (contentHeight / lineHeight) : 0;

  const int textX = metrics.contentSidePadding;
  int y = contentTop;
  const int firstLine = scrollOffset_;
  const int lastLine = std::min(firstLine + visibleLines_, static_cast<int>(lines_.size()));
  for (int i = firstLine; i < lastLine; ++i) {
    renderer.drawText(UI_12_FONT_ID, textX, y, lines_[i].c_str(), true);
    y += lineHeight;
  }

  // Scrollbar on the right when content overflows.
  if (static_cast<int>(lines_.size()) > visibleLines_ && visibleLines_ > 0) {
    const int scrollAreaY = contentTop;
    const int scrollAreaH = contentHeight;
    const int total = static_cast<int>(lines_.size());
    const int barH = std::max(8, (scrollAreaH * visibleLines_) / total);
    const int maxOffset = total - visibleLines_;
    const int barY = scrollAreaY + (maxOffset > 0 ? ((scrollAreaH - barH) * scrollOffset_) / maxOffset : 0);
    const int barX = sw - 5;
    renderer.drawLine(barX, scrollAreaY, barX, scrollAreaY + scrollAreaH, true);
    renderer.fillRect(barX - 4, barY, 4, barH, true);
  }

  // Bottom count label (only when this item has a non-negative count badge).
  if (displayCount >= 0) {
    static char countBuf[24];
    std::snprintf(countBuf, sizeof(countBuf), tr(STR_WEREAD_HIGHLIGHT_COUNT_FMT), displayCount);
    const int labelW = renderer.getTextWidth(SMALL_FONT_ID, countBuf);
    const int labelX = sw - metrics.contentSidePadding - labelW;
    const int labelY = sh - metrics.buttonHintsHeight - metrics.verticalSpacing - 16;
    renderer.drawText(SMALL_FONT_ID, labelX, labelY, countBuf, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
