#pragma once

#include <string>
#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

class WeReadNotesActivity final : public WeReadFetchActivity {
 public:
  WeReadNotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                      std::string bookTitle);
  ~WeReadNotesActivity() override = default;

 protected:
  const char* apiName() const override { return "/book/bookmarklist"; }
  void buildRequest(JsonDocument& body) override;
  void buildResponseFilter(JsonDocument& filter) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return static_cast<int>(rows_.size()); }
  const char* headerTitle() const override { return bookTitle_.c_str(); }
  void renderContent(Rect contentRect) override;
  void onConfirm(int index) override;
  void onBack() override;

 private:
  std::string bookId_;
  std::string bookTitle_;
  std::vector<WeReadModels::BookmarkRow> rows_;

  // Trampolines for WeReadHighlightDetailActivity::ItemSource. Personal notes
  // carry no popularity count, so detailItemCount always returns -1.
  static const std::string& detailItemText(void* ctx, int idx);
  static int detailItemCount(void* ctx, int idx);
};
