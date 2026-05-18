#include "WeReadBestMarksActivity.h"

#include <I18n.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../../../components/UITheme.h"
#include "WeReadHighlightDetailActivity.h"

WeReadBestMarksActivity::WeReadBestMarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 std::string bookId, std::string bookTitle)
    : WeReadFetchActivity("WeReadBestMarks", renderer, mappedInput),
      bookId_(std::move(bookId)),
      bookTitle_(std::move(bookTitle)) {}

void WeReadBestMarksActivity::buildRequest(JsonDocument& body) {
  body["bookId"] = bookId_;
  body["chapterUid"] = 0;  // 0 = all chapters per skill doc
}

void WeReadBestMarksActivity::buildResponseFilter(JsonDocument& filter) {
  filter["items"][0]["bookmarkId"] = true;
  filter["items"][0]["markText"] = true;
  filter["items"][0]["range"] = true;
  filter["items"][0]["chapterUid"] = true;
  filter["items"][0]["totalCount"] = true;
}

void WeReadBestMarksActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst items = resp["items"].as<JsonArrayConst>();
  if (items.isNull()) return;
  rows_.reserve(items.size());
  for (JsonVariantConst it : items) {
    WeReadModels::BestMarkRow r;
    r.bookmarkId = it["bookmarkId"] | "";
    r.markText = it["markText"] | "";
    r.range = it["range"] | "";
    r.chapterUid = it["chapterUid"] | 0u;
    r.totalCount = it["totalCount"] | 0;
    if (!r.markText.empty()) rows_.push_back(std::move(r));
  }
}

void WeReadBestMarksActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_BESTMARKS));
    return;
  }
  static char countBuf[24];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected, [this](int i) { return rows_[i].markText; },
      [this](int i) {
        std::snprintf(countBuf, sizeof(countBuf), tr(STR_WEREAD_HIGHLIGHT_COUNT_FMT), rows_[i].totalCount);
        return std::string(countBuf);
      });
}

const std::string& WeReadBestMarksActivity::detailItemText(void* ctx, int idx) {
  return static_cast<WeReadBestMarksActivity*>(ctx)->rows_[idx].markText;
}

int WeReadBestMarksActivity::detailItemCount(void* ctx, int idx) {
  return static_cast<WeReadBestMarksActivity*>(ctx)->rows_[idx].totalCount;
}

void WeReadBestMarksActivity::onConfirm(int index) {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return;
  WeReadHighlightDetailActivity::ItemSource src{
      .ctx = this,
      .itemCount = static_cast<int>(rows_.size()),
      .getText = &WeReadBestMarksActivity::detailItemText,
      .getCount = &WeReadBestMarksActivity::detailItemCount,
  };
  auto handler = [this](const ActivityResult&) { requestUpdate(); };
  startActivityForResult(
      std::make_unique<WeReadHighlightDetailActivity>(renderer, mappedInput, bookTitle_, src, index), handler);
}

void WeReadBestMarksActivity::onBack() { finish(); }
