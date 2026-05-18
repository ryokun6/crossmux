#pragma once

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

/**
 * Reading dashboard — non-list layout: hours, days, day-average, vs-last-period
 * compare, top-3 most-read books, prefer time/category words. Pulls
 * /readdata/detail in `monthly` mode by default (matches the SKILL.md default).
 */
class WeReadStatsActivity final : public WeReadFetchActivity {
 public:
  WeReadStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadStatsActivity() override = default;

 protected:
  const char* apiName() const override { return "/readdata/detail"; }
  void buildRequest(JsonDocument& body) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return 0; }  // not navigable as a list
  const char* headerTitle() const override;
  void renderContent(Rect contentRect) override;

 private:
  WeReadModels::StatsSummary stats_;
};
