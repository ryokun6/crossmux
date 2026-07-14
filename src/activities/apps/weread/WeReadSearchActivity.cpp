#include "WeReadSearchActivity.h"

#include <I18n.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "../../util/KeyboardEntryActivity.h"

WeReadSearchActivity::WeReadSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : WeReadFetchActivity("WeReadSearch", renderer, mappedInput) {
  headerCache_ = tr(STR_WEREAD_MENU_SEARCH);
}

void WeReadSearchActivity::onEnter() {
  Activity::onEnter();  // skip base preflight — we'll re-trigger via launchKeyboard
  selected = 0;
  if (keyword_.empty() && !keyboardLaunched_) {
    launchKeyboard();
  }
  requestUpdate();
}

void WeReadSearchActivity::launchKeyboard() {
  keyboardLaunched_ = true;
  awaitingKeyboard_ = true;
  auto handler = [this](const ActivityResult& result) {
    awaitingKeyboard_ = false;
    if (result.isCancelled) {
      activityManager.goToWeRead();
      return;
    }
    const auto& kb = std::get<KeyboardResult>(result.data);
    keyword_ = kb.text;
    headerCache_ = std::string(tr(STR_WEREAD_MENU_SEARCH)) + ": " + keyword_;
    // Now that we have a keyword, run the base preflight + fetch via the
    // public entry point — onEnter() does both.
    WeReadFetchActivity::onEnter();
  };
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, std::string(tr(STR_WEREAD_SEARCH_PROMPT)),
                                              keyword_, 63, InputType::Text),
      handler);
}

void WeReadSearchActivity::loop() {
  // When awaiting the keyboard result, do not let the base run its own input
  // handling — it would consume Back/Confirm meant for the keyboard child.
  if (awaitingKeyboard_) return;
  WeReadFetchActivity::loop();
}

void WeReadSearchActivity::buildRequest(JsonDocument& body) {
  body["keyword"] = keyword_;
  body["scope"] = 10;  // e-books per skill doc default for "搜書"
}

void WeReadSearchActivity::buildResponseFilter(JsonDocument& filter) {
  // Drop intro/cover/publisher (each potentially KBs) — we only render
  // title/author/rating/readingCount.
  filter["results"][0]["books"][0]["searchIdx"] = true;
  filter["results"][0]["books"][0]["readingCount"] = true;
  filter["results"][0]["books"][0]["newRating"] = true;
  filter["results"][0]["books"][0]["newRatingCount"] = true;
  filter["results"][0]["books"][0]["bookInfo"]["bookId"] = true;
  filter["results"][0]["books"][0]["bookInfo"]["title"] = true;
  filter["results"][0]["books"][0]["bookInfo"]["author"] = true;
  filter["results"][0]["books"][0]["bookInfo"]["category"] = true;
  filter["results"][0]["books"][0]["bookInfo"]["soldout"] = true;
}

const char* WeReadSearchActivity::headerTitle() const { return headerCache_.c_str(); }

void WeReadSearchActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst results = resp["results"].as<JsonArrayConst>();
  if (results.isNull()) return;
  // scope=10 returns a single group; iterate defensively to cope with scope=0
  // mixed groups too.
  for (JsonVariantConst group : results) {
    JsonArrayConst books = group["books"].as<JsonArrayConst>();
    if (books.isNull()) continue;
    for (JsonVariantConst b : books) {
      JsonVariantConst info = b["bookInfo"];
      WeReadModels::SearchRow row;
      row.bookId = info["bookId"] | "";
      row.title = info["title"] | "";
      row.author = info["author"] | "";
      row.category = info["category"] | "";
      row.soldout = info["soldout"] | 0;
      row.newRating = b["newRating"] | 0;
      row.newRatingCount = b["newRatingCount"] | 0;
      row.readingCount = b["readingCount"] | 0;
      row.searchIdx = b["searchIdx"] | 0;
      if (!row.bookId.empty()) rows_.push_back(std::move(row));
    }
  }
}

void WeReadSearchActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_RESULTS));
    return;
  }
  static char subtitleBuf[80];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected,
      [this](int i) {
        const auto& r = rows_[i];
        std::string title;
        if (r.soldout) title += tr(STR_WEREAD_SOLDOUT_PREFIX);
        title += r.title;
        return title;
      },
      [this](int i) {
        const auto& r = rows_[i];
        if (r.newRating > 0) {
          char ratingBuf[24];
          std::snprintf(ratingBuf, sizeof(ratingBuf), tr(STR_WEREAD_RATING_FMT), r.newRating / 10.0);
          char readingBuf[24];
          std::snprintf(readingBuf, sizeof(readingBuf), tr(STR_WEREAD_READING_COUNT_FMT), r.readingCount);
          std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%s · %s · %s", r.author.c_str(), ratingBuf, readingBuf);
        } else {
          char readingBuf[24];
          std::snprintf(readingBuf, sizeof(readingBuf), tr(STR_WEREAD_READING_COUNT_FMT), r.readingCount);
          std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%s · %s", r.author.c_str(), readingBuf);
        }
        return std::string(subtitleBuf);
      });
}

void WeReadSearchActivity::onConfirm(int index) {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return;
  const auto& r = rows_[index];
  if (r.bookId.empty()) return;
  activityManager.goToWeReadBook(r.bookId, r.title);
}
