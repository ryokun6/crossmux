#pragma once

#include <string>
#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

class WeReadChaptersActivity final : public WeReadFetchActivity {
 public:
  WeReadChaptersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                         std::string bookTitle);
  ~WeReadChaptersActivity() override = default;

 protected:
  const char* apiName() const override { return "/book/chapterinfo"; }
  void buildRequest(JsonDocument& body) override;
  void buildResponseFilter(JsonDocument& filter) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return static_cast<int>(rows_.size()); }
  const char* headerTitle() const override { return bookTitle_.c_str(); }
  void renderContent(Rect contentRect) override;
  void onBack() override;

 private:
  std::string bookId_;
  std::string bookTitle_;
  std::vector<WeReadModels::ChapterRow> rows_;
};
