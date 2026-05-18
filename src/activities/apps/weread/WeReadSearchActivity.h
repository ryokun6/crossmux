#pragma once

#include <string>
#include <vector>

#include "WeReadFetchActivity.h"
#include "WeReadModels.h"

/**
 * Search Activity — pops the keyboard before its first fetch, then issues
 * /store/search with scope=10 (e-books only). Pressing Confirm on a result
 * navigates to WeReadBookActivity for that book.
 *
 * The keyword flow is layered on top of WeReadFetchActivity: we defer the
 * initial spawnFetch() until the keyboard returns by overriding onEnter()
 * before chaining to base.
 */
class WeReadSearchActivity final : public WeReadFetchActivity {
 public:
  WeReadSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadSearchActivity() override = default;

  void onEnter() override;
  void loop() override;

 protected:
  const char* apiName() const override { return "/store/search"; }
  void buildRequest(JsonDocument& body) override;
  void buildResponseFilter(JsonDocument& filter) override;
  void parseResponse(JsonDocument& resp) override;
  int itemCount() const override { return static_cast<int>(rows_.size()); }
  const char* headerTitle() const override;
  void renderContent(Rect contentRect) override;
  void onConfirm(int index) override;

 private:
  std::string keyword_;
  std::string headerCache_;
  std::vector<WeReadModels::SearchRow> rows_;
  bool awaitingKeyboard_ = false;
  bool keyboardLaunched_ = false;

  void launchKeyboard();
};
