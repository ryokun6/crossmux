#pragma once

#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

class WeReadRecommendActivity final : public WeReadFetchActivity {
 public:
  WeReadRecommendActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadRecommendActivity() override = default;

 protected:
  const char* apiName() const override { return "/book/recommend"; }
  void buildRequest(JsonDocument& body) override;
  void buildResponseFilter(JsonDocument& filter) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return static_cast<int>(rows_.size()); }
  const char* headerTitle() const override;
  void renderContent(Rect contentRect) override;
  void onConfirm(int index) override;

 private:
  std::vector<WeReadModels::RecommendRow> rows_;
};
