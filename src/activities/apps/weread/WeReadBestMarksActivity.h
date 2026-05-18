#pragma once

#include <string>
#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

class WeReadBestMarksActivity final : public WeReadFetchActivity {
 public:
  WeReadBestMarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                          std::string bookTitle);
  ~WeReadBestMarksActivity() override = default;

 protected:
  const char* apiName() const override { return "/book/bestbookmarks"; }
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
  std::vector<WeReadModels::BestMarkRow> rows_;

  // Trampolines exposed to WeReadHighlightDetailActivity::ItemSource so the
  // Detail view can browse rows_ without copying.
  static const std::string& detailItemText(void* ctx, int idx);
  static int detailItemCount(void* ctx, int idx);
};
