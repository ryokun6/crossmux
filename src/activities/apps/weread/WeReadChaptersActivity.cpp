#include "WeReadChaptersActivity.h"

#include <I18n.h>

#include <cstdio>
#include <string>

#include "../../../components/UITheme.h"

WeReadChaptersActivity::WeReadChaptersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               std::string bookId, std::string bookTitle)
    : WeReadFetchActivity("WeReadChapters", renderer, mappedInput),
      bookId_(std::move(bookId)),
      bookTitle_(std::move(bookTitle)) {}

void WeReadChaptersActivity::buildRequest(JsonDocument& body) { body["bookId"] = bookId_; }

void WeReadChaptersActivity::buildResponseFilter(JsonDocument& filter) {
  filter["chapters"][0]["title"] = true;
  filter["chapters"][0]["chapterUid"] = true;
  filter["chapters"][0]["chapterIdx"] = true;
  filter["chapters"][0]["wordCount"] = true;
  filter["chapters"][0]["level"] = true;
  filter["chapters"][0]["paid"] = true;
}

void WeReadChaptersActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst chapters = resp["chapters"].as<JsonArrayConst>();
  if (chapters.isNull()) return;
  rows_.reserve(chapters.size());
  for (JsonVariantConst c : chapters) {
    WeReadModels::ChapterRow r;
    r.title = c["title"] | "";
    r.chapterUid = c["chapterUid"] | 0u;
    r.chapterIdx = c["chapterIdx"] | 0u;
    r.wordCount = c["wordCount"] | 0;
    r.level = c["level"] | 1;
    r.paid = c["paid"] | 1;
    if (!r.title.empty()) rows_.push_back(std::move(r));
  }
}

void WeReadChaptersActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_CHAPTERS));
    return;
  }

  static char subtitleBuf[40];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected,
      [this](int i) {
        // Indent by level so the hierarchy is readable; max 4 indent steps.
        const auto& r = rows_[i];
        std::string title;
        const int indent = std::min<int>(r.level > 1 ? r.level - 1 : 0, 4);
        for (int k = 0; k < indent; k++) title += "  ";
        if (!r.paid) title += "🔒 ";
        title += r.title;
        return title;
      },
      [this](int i) {
        const auto& r = rows_[i];
        if (r.wordCount <= 0) return std::string{};
        std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%d 字", r.wordCount);
        return std::string(subtitleBuf);
      });
}

void WeReadChaptersActivity::onBack() { finish(); }
