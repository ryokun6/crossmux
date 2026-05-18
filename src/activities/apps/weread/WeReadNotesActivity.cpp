#include "WeReadNotesActivity.h"

#include <I18n.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "WeReadHighlightDetailActivity.h"

WeReadNotesActivity::WeReadNotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                                         std::string bookTitle)
    : WeReadFetchActivity("WeReadNotes", renderer, mappedInput),
      bookId_(std::move(bookId)),
      bookTitle_(std::move(bookTitle)) {}

void WeReadNotesActivity::buildRequest(JsonDocument& body) { body["bookId"] = bookId_; }

void WeReadNotesActivity::buildResponseFilter(JsonDocument& filter) {
  filter["updated"][0]["bookmarkId"] = true;
  filter["updated"][0]["markText"] = true;
  filter["updated"][0]["range"] = true;
  filter["updated"][0]["chapterUid"] = true;
  filter["updated"][0]["createTime"] = true;
}

void WeReadNotesActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst updated = resp["updated"].as<JsonArrayConst>();
  if (updated.isNull()) return;
  rows_.reserve(updated.size());
  for (JsonVariantConst b : updated) {
    WeReadModels::BookmarkRow r;
    r.bookmarkId = b["bookmarkId"] | "";
    r.markText = b["markText"] | "";
    r.range = b["range"] | "";
    r.chapterUid = b["chapterUid"] | 0u;
    r.createTime = b["createTime"] | 0u;
    if (!r.markText.empty()) rows_.push_back(std::move(r));
  }
}

void WeReadNotesActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_NOTES));
    return;
  }

  static char subtitleBuf[32];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected,
      [this](int i) {
        // Use the markText itself as the row title; truncation handled by
        // drawList based on listRowHeight + listWithSubtitleRowHeight.
        return rows_[i].markText;
      },
      [this](int i) {
        // Subtitle: date stamp (per CLAUDE.md / SKILL.md, format timestamps).
        const uint32_t t = rows_[i].createTime;
        if (t == 0) return std::string{};
        const time_t raw = static_cast<time_t>(t);
        struct tm timeinfo{};
        gmtime_r(&raw, &timeinfo);
        std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                      timeinfo.tm_mday);
        return std::string(subtitleBuf);
      });
}

const std::string& WeReadNotesActivity::detailItemText(void* ctx, int idx) {
  return static_cast<WeReadNotesActivity*>(ctx)->rows_[idx].markText;
}

int WeReadNotesActivity::detailItemCount(void* /*ctx*/, int /*idx*/) {
  // Personal notes have no popularity count; sentinel suppresses the badge.
  return -1;
}

void WeReadNotesActivity::onConfirm(int index) {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return;
  WeReadHighlightDetailActivity::ItemSource src{
      .ctx = this,
      .itemCount = static_cast<int>(rows_.size()),
      .getText = &WeReadNotesActivity::detailItemText,
      .getCount = &WeReadNotesActivity::detailItemCount,
  };
  auto handler = [this](const ActivityResult&) { requestUpdate(); };
  startActivityForResult(
      std::make_unique<WeReadHighlightDetailActivity>(renderer, mappedInput, bookTitle_, src, index), handler);
}

void WeReadNotesActivity::onBack() { finish(); }
