#include "WeReadBookActivity.h"

#include <I18n.h>

#include <memory>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "WeReadBestMarksActivity.h"
#include "WeReadChaptersActivity.h"
#include "WeReadNotesActivity.h"
#include "WeReadReviewsActivity.h"
#include "WeReadSimilarActivity.h"

namespace {

enum class BookMenuItem : uint8_t {
  Notes = 0,
  BestMarks,
  Chapters,
  Reviews,
  Similar,
};

constexpr int kBookMenuCount = 5;

StrId titleFor(int idx) {
  switch (static_cast<BookMenuItem>(idx)) {
    case BookMenuItem::Notes:
      return StrId::STR_WEREAD_BOOK_NOTES;
    case BookMenuItem::BestMarks:
      return StrId::STR_WEREAD_BOOK_BESTMARKS;
    case BookMenuItem::Chapters:
      return StrId::STR_WEREAD_BOOK_CHAPTERS;
    case BookMenuItem::Reviews:
      return StrId::STR_WEREAD_BOOK_REVIEWS;
    case BookMenuItem::Similar:
      return StrId::STR_WEREAD_BOOK_SIMILAR;
  }
  return StrId::STR_WEREAD_BOOK_NOTES;
}

}  // namespace

WeReadBookActivity::WeReadBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                                       std::string title)
    : Activity("WeReadBook", renderer, mappedInput), bookId_(std::move(bookId)), title_(std::move(title)) {}

void WeReadBookActivity::onEnter() {
  Activity::onEnter();
  selected = 0;
  requestUpdate();
}

void WeReadBookActivity::onExit() { Activity::onExit(); }

void WeReadBookActivity::loop() {
  buttonNavigator.onNext([this] {
    selected = ButtonNavigator::nextIndex(selected, kBookMenuCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selected = ButtonNavigator::previousIndex(selected, kBookMenuCount);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onSelect();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Back to shelf rather than WeRead menu — shelf is what surfaced this
    // book in the first place, and search/recommend paths also benefit from
    // a consistent "back goes one level up".
    activityManager.goToWeReadShelf();
  }
}

void WeReadBookActivity::onSelect() {
  auto handler = [this](const ActivityResult&) { requestUpdate(); };
  switch (static_cast<BookMenuItem>(selected)) {
    case BookMenuItem::Notes:
      startActivityForResult(std::make_unique<WeReadNotesActivity>(renderer, mappedInput, bookId_, title_), handler);
      break;
    case BookMenuItem::BestMarks:
      startActivityForResult(std::make_unique<WeReadBestMarksActivity>(renderer, mappedInput, bookId_, title_),
                             handler);
      break;
    case BookMenuItem::Chapters:
      startActivityForResult(std::make_unique<WeReadChaptersActivity>(renderer, mappedInput, bookId_, title_), handler);
      break;
    case BookMenuItem::Reviews:
      startActivityForResult(std::make_unique<WeReadReviewsActivity>(renderer, mappedInput, bookId_, title_), handler);
      break;
    case BookMenuItem::Similar:
      startActivityForResult(std::make_unique<WeReadSimilarActivity>(renderer, mappedInput, bookId_, title_), handler);
      break;
  }
}

void WeReadBookActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  // Show the book title in the header, truncating happens naturally in
  // drawHeader. Subtitle is left out so the title gets full width.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, title_.c_str());

  const int listY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listH = sh - listY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, listY, sw, listH}, kBookMenuCount, selected,
      [](int i) { return std::string(I18n::getInstance().get(titleFor(i))); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
