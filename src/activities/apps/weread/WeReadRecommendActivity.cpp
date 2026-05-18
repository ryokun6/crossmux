#include "WeReadRecommendActivity.h"

#include <I18n.h>

#include <cstdio>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"

WeReadRecommendActivity::WeReadRecommendActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : WeReadFetchActivity("WeReadRecommend", renderer, mappedInput) {}

const char* WeReadRecommendActivity::headerTitle() const { return tr(STR_WEREAD_MENU_RECOMMEND); }

void WeReadRecommendActivity::buildRequest(JsonDocument& body) {
  body["count"] = 12;  // skill doc default; server caps at small values
}

void WeReadRecommendActivity::buildResponseFilter(JsonDocument& filter) {
  filter["books"][0]["bookId"] = true;
  filter["books"][0]["title"] = true;
  filter["books"][0]["author"] = true;
  filter["books"][0]["category"] = true;
  filter["books"][0]["reason"] = true;
  filter["books"][0]["newRating"] = true;
  filter["books"][0]["readingCount"] = true;
  filter["books"][0]["searchIdx"] = true;
}

void WeReadRecommendActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst books = resp["books"].as<JsonArrayConst>();
  if (books.isNull()) return;
  rows_.reserve(books.size());
  for (JsonVariantConst b : books) {
    WeReadModels::RecommendRow row;
    row.bookId = b["bookId"] | "";
    row.title = b["title"] | "";
    row.author = b["author"] | "";
    row.reason = b["reason"] | "";
    row.category = b["category"] | "";
    row.newRating = b["newRating"] | 0;
    row.readingCount = b["readingCount"] | 0;
    row.searchIdx = b["searchIdx"] | 0;
    if (!row.bookId.empty()) rows_.push_back(std::move(row));
  }
}

void WeReadRecommendActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_RESULTS));
    return;
  }
  static char subtitleBuf[100];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected,
      [this](int i) { return rows_[i].title; },
      [this](int i) {
        const auto& r = rows_[i];
        // Prefer the recommend reason — it's the whole point of this list.
        if (!r.reason.empty()) {
          std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%s · %s", r.author.c_str(), r.reason.c_str());
        } else if (r.newRating > 0) {
          std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%s · 评分 %.1f", r.author.c_str(), r.newRating / 10.0);
        } else {
          std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%s", r.author.c_str());
        }
        return std::string(subtitleBuf);
      });
}

void WeReadRecommendActivity::onConfirm(int index) {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return;
  const auto& r = rows_[index];
  if (r.bookId.empty()) return;
  activityManager.goToWeReadBook(r.bookId, r.title);
}
