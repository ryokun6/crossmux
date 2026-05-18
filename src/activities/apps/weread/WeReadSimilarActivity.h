#pragma once

#include <string>
#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

class WeReadSimilarActivity final : public WeReadFetchActivity {
 public:
  WeReadSimilarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                        std::string bookTitle);
  ~WeReadSimilarActivity() override = default;

 protected:
  const char* apiName() const override { return "/book/similar"; }
  void buildRequest(JsonDocument& body) override;
  void buildResponseFilter(JsonDocument& filter) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return static_cast<int>(rows_.size()); }
  const char* headerTitle() const override { return bookTitle_.c_str(); }
  void renderContent(Rect contentRect) override;
  void onBack() override;
  void onConfirm(int index) override;

 private:
  std::string bookId_;
  std::string bookTitle_;
  std::vector<WeReadModels::SearchRow> rows_;
};
