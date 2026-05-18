#include "WeReadReviewsActivity.h"

#include <I18n.h>

#include <cstdio>
#include <string>

#include "../../../components/UITheme.h"

WeReadReviewsActivity::WeReadReviewsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             std::string bookId, std::string bookTitle)
    : WeReadFetchActivity("WeReadReviews", renderer, mappedInput),
      bookId_(std::move(bookId)),
      bookTitle_(std::move(bookTitle)) {}

void WeReadReviewsActivity::buildRequest(JsonDocument& body) {
  body["bookId"] = bookId_;
  body["reviewListType"] = 0;  // 0 = 全部, 1 = 推荐, 2 = 不行, 3 = 最新
}

void WeReadReviewsActivity::buildResponseFilter(JsonDocument& filter) {
  // Drop htmlContent (largest field) and unused metadata; keep only the row
  // model fields. Note the deeply-nested .review.review path matches the
  // shape parseResponse reads.
  filter["reviews"][0]["idx"] = true;
  filter["reviews"][0]["review"]["review"]["reviewId"] = true;
  filter["reviews"][0]["review"]["review"]["content"] = true;
  filter["reviews"][0]["review"]["review"]["star"] = true;
  filter["reviews"][0]["review"]["review"]["isFinish"] = true;
  filter["reviews"][0]["review"]["review"]["createTime"] = true;
  filter["reviews"][0]["review"]["review"]["chapterName"] = true;
  filter["reviews"][0]["review"]["review"]["author"]["name"] = true;
}

void WeReadReviewsActivity::parseResponse(JsonDocument& resp) {
  rows_.clear();
  JsonArrayConst reviews = resp["reviews"].as<JsonArrayConst>();
  if (reviews.isNull()) return;
  rows_.reserve(reviews.size());
  for (JsonVariantConst r : reviews) {
    WeReadModels::PublicReviewRow row;
    // Note the deeply-nested path: reviews[].review.review.* per skill doc
    JsonVariantConst inner = r["review"]["review"];
    row.reviewId = inner["reviewId"] | "";
    row.content = inner["content"] | "";
    row.starPercent = inner["star"] | 0;
    row.createTime = inner["createTime"] | 0u;
    row.isFinish = inner["isFinish"] | 0;
    row.chapterName = inner["chapterName"] | "";
    row.authorName = inner["author"]["name"] | "";
    row.idx = r["idx"] | 0;
    if (!row.content.empty() || !row.authorName.empty()) rows_.push_back(std::move(row));
  }
}

namespace {
// 100 = 5星, 80 = 4星 ... 0 = 无评分
const char* starText(int starPercent) {
  switch (starPercent) {
    case 100:
      return "★★★★★";
    case 80:
      return "★★★★";
    case 60:
      return "★★★";
    case 40:
      return "★★";
    case 20:
      return "★";
    default:
      return "";
  }
}
}  // namespace

void WeReadReviewsActivity::renderContent(Rect contentRect) {
  if (rows_.empty()) {
    GUI.drawPopup(renderer, tr(STR_WEREAD_NO_REVIEWS));
    return;
  }
  static char subtitleBuf[80];
  GUI.drawList(
      renderer, contentRect, static_cast<int>(rows_.size()), selected,
      [this](int i) {
        // Title: starred reviewer name. Content goes in subtitle so it can
        // line-wrap into the listWithSubtitleRowHeight = 50px row.
        const auto& r = rows_[i];
        std::string title = starText(r.starPercent);
        if (!title.empty()) title += "  ";
        title += r.authorName.empty() ? std::string("匿名读者") : r.authorName;
        return title;
      },
      [this](int i) {
        const auto& r = rows_[i];
        // Truncate content to ~70 chars (≈ one row); full text view = V2.
        std::string excerpt = r.content;
        constexpr size_t kMaxLen = 70;
        if (excerpt.size() > kMaxLen) {
          excerpt.resize(kMaxLen);
          excerpt += "…";
        }
        return excerpt;
      });
}

void WeReadReviewsActivity::onBack() { finish(); }
