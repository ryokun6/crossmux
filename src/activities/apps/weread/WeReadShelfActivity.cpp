#include "WeReadShelfActivity.h"

#include <I18n.h>

#include <cstdio>
#include <functional>
#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"

WeReadShelfActivity::WeReadShelfActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : WeReadFetchActivity("WeReadShelf", renderer, mappedInput) {}

void WeReadShelfActivity::buildRequest(JsonDocument& /*body*/) {
  // /shelf/sync takes no business params (user identified via Bearer token).
}

void WeReadShelfActivity::buildResponseFilter(JsonDocument& filter) {
  // Keep only what parseResponse reads. The full /shelf/sync response is
  // ~30-50 KB for a typical shelf (cover URLs, intro text, payType, etc.);
  // without this filter the JsonDocument blows the heap on a fragmented
  // E-Ink device. The `[0]` index applies the inner filter to EVERY array
  // element per ArduinoJson 7's filter semantics.
  filter["books"][0]["bookId"] = true;
  filter["books"][0]["title"] = true;
  filter["books"][0]["author"] = true;
  filter["books"][0]["category"] = true;
  filter["books"][0]["readUpdateTime"] = true;
  filter["books"][0]["finishReading"] = true;
  filter["books"][0]["isTop"] = true;
  filter["books"][0]["secret"] = true;
  filter["albums"][0]["albumInfo"]["albumId"] = true;
  filter["albums"][0]["albumInfo"]["name"] = true;
  filter["albums"][0]["albumInfo"]["authorName"] = true;
  filter["albums"][0]["albumInfo"]["finishStatus"] = true;
  filter["albums"][0]["albumInfoExtra"]["lectureReadUpdateTime"] = true;
  filter["mp"] = true;
}

const char* WeReadShelfActivity::headerTitle() const { return tr(STR_WEREAD_MENU_SHELF); }

void WeReadShelfActivity::parseResponse(JsonDocument& resp) {
  books_.clear();
  ebookCount_ = 0;
  albumCount_ = 0;
  hasMpEntry_ = false;

  JsonArrayConst booksJson = resp["books"].as<JsonArrayConst>();
  if (!booksJson.isNull()) {
    books_.reserve(booksJson.size());
    for (JsonVariantConst b : booksJson) {
      WeReadModels::BookCard card;
      card.bookId = b["bookId"] | "";
      card.title = b["title"] | "";
      card.author = b["author"] | "";
      card.category = b["category"] | "";
      card.readUpdateTime = b["readUpdateTime"] | 0u;
      card.finishReading = b["finishReading"] | 0;
      card.isTop = b["isTop"] | 0;
      card.secret = b["secret"] | 0;
      card.isAlbum = 0;
      if (!card.bookId.empty()) {
        books_.push_back(std::move(card));
        ebookCount_++;
      }
    }
  }

  JsonArrayConst albumsJson = resp["albums"].as<JsonArrayConst>();
  if (!albumsJson.isNull()) {
    for (JsonVariantConst a : albumsJson) {
      JsonVariantConst info = a["albumInfo"];
      WeReadModels::BookCard card;
      // Use albumId as the visible bookId so the row stays addressable; we
      // disable Confirm-into-book for audio rows (no Notes/Highlights API).
      card.bookId = info["albumId"] | "";
      card.title = info["name"] | "";
      card.author = info["authorName"] | "";
      card.category = info["finishStatus"] | "";
      card.readUpdateTime = a["albumInfoExtra"]["lectureReadUpdateTime"] | 0u;
      card.isAlbum = 1;
      if (!card.bookId.empty()) {
        books_.push_back(std::move(card));
        albumCount_++;
      }
    }
  }

  // mp 字段非空时书架顶部有一条「文章收藏」入口 — 只在统计里出现，不进列表。
  JsonVariantConst mp = resp["mp"];
  hasMpEntry_ = !mp.isNull();
}

void WeReadShelfActivity::renderContent(Rect contentRect) {
  if (books_.empty()) {
    // Server returned 0 books — show a plain empty-shelf message.
    GUI.drawPopup(renderer, tr(STR_WEREAD_SHELF_EMPTY));
    return;
  }

  // Subtitle uses a static buffer so we don't allocate every redraw.
  static char subtitleBuf[64];
  std::snprintf(subtitleBuf, sizeof(subtitleBuf), "%d · %d 有声 · %s",
                ebookCount_, albumCount_, hasMpEntry_ ? "含文章收藏" : "无文章收藏");

  GUI.drawList(
      renderer, contentRect, static_cast<int>(books_.size()), selected,
      [this](int i) {
        const auto& b = books_[i];
        std::string title;
        if (b.isAlbum) title += "[听] ";
        if (b.isTop) title += "★ ";
        title += b.title;
        return title;
      },
      [this](int i) {
        const auto& b = books_[i];
        std::string sub;
        if (!b.author.empty()) sub = b.author;
        if (!b.category.empty()) {
          if (!sub.empty()) sub += " · ";
          sub += b.category;
        }
        return sub;
      });
}

void WeReadShelfActivity::onConfirm(int index) {
  if (index < 0 || index >= static_cast<int>(books_.size())) return;
  const auto& b = books_[index];
  if (b.isAlbum) {
    // /book/* endpoints don't accept albumId — Book Activity would 404.
    return;
  }
  activityManager.goToWeReadBook(b.bookId, b.title);
}
