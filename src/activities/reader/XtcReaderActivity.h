/**
 * XtcReaderActivity.h
 *
 * XTC ebook reader activity for CrossPoint Reader
 * Displays pre-rendered XTC pages on e-ink display
 */

#pragma once

#include <Xtc.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "EndOfBookOptions.h"
#include "activities/Activity.h"

class XtcReaderActivity final : public Activity {
  std::shared_ptr<Xtc> xtc;

  uint32_t currentPage = 0;
  // Refresh cadence counter, seeded from SETTINGS in onEnter() (0 here would make the first
  // paint of the book a slow HALF refresh, see ReaderUtils::displayWithRefreshCycle).
  int pagesUntilFullRefresh = 0;
  // Next-book suggestion menu for the End-of-Book screen
  EndOfBookOptions endOfBookOptions;

  // Full-page decode buffer: allocated once in onEnter(), reused each render, freed in onExit().
  std::unique_ptr<uint8_t[]> pageBuffer;
  size_t pageBufferCapacity = 0;

  enum class StatusBarOverlayPosition { Bottom, Top };
  struct StatusBarInfo {
    int currentPage;
    int pageCount;
    std::string title;
  };

  void renderPage();
  // Opens chapter selection when the book has chapters (short-press Confirm); no-op otherwise
  void openChapterSelection();
  void renderStatusBarOverlay(StatusBarOverlayPosition position) const;
  StatusBarInfo getStatusBarInfo() const;
  void saveProgress() const;
  void loadProgress();

 public:
  explicit XtcReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Xtc> xtc,
                             int initialRefreshCountdown)
      : Activity("XtcReader", renderer, mappedInput),
        xtc(std::move(xtc)),
        pagesUntilFullRefresh(initialRefreshCountdown) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool handleForcedRefresh() override {
    {
      RenderLock lock(*this);
      pagesUntilFullRefresh = 1;
    }
    requestUpdate();
    return true;
  }
  ScreenshotInfo getScreenshotInfo() const override;
};
