#pragma once

#include <string>
#include <vector>

#include "../../../util/ButtonNavigator.h"
#include "../../Activity.h"

class WeReadHighlightDetailActivity final : public Activity {
 public:
  // Non-owning view of the parent list Activity's row collection. Lifetime of
  // the referenced data must extend across this Detail Activity — true by
  // construction because the parent stays on the activity stack while Detail
  // is on top (startActivityForResult does not destroy the parent).
  struct ItemSource {
    void* ctx = nullptr;
    int itemCount = 0;
    const std::string& (*getText)(void* ctx, int idx) = nullptr;
    // Returns -1 when this item carries no "N 人划线" badge (used by Notes).
    int (*getCount)(void* ctx, int idx) = nullptr;
  };

  WeReadHighlightDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookTitle,
                                ItemSource source, int initialIndex);
  ~WeReadHighlightDetailActivity() override = default;

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string bookTitle_;
  ItemSource source_;
  int currentIndex_ = 0;
  std::vector<std::string> lines_;
  int scrollOffset_ = 0;
  int visibleLines_ = 0;
  // Latched on Press, consumed on Release — only a tap that *started* at the
  // respective boundary crosses to the next/previous item.
  bool pressStartedAtTop_ = false;
  bool pressStartedAtEnd_ = false;
  ButtonNavigator buttonNavigator;

  const std::string& currentText() const;
  int currentCount() const;
  int maxScrollOffset() const;
  void rewrap();
  void switchTo(int newIndex);
};
