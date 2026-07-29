#pragma once
#include <Epub.h>
#include <Epub/FootnoteEntry.h>
#include <Epub/Section.h>

#ifdef ENABLE_CHINESE_VERSION
#include <atomic>
#endif
#include <optional>

#include "BookmarkEntry.h"
#include "EpubReaderMenuActivity.h"
#include "ProgressMapper.h"
#include "activities/Activity.h"

class EpubReaderActivity final : public Activity {
  std::shared_ptr<Epub> epub;
  std::unique_ptr<Section> section = nullptr;
  int currentSpineIndex = 0;
  int nextPageNumber = 0;
  std::optional<uint16_t> pendingPageJump;
  // Set when navigating to a footnote href with a fragment (e.g. #note1).
  // Cleared on the next render after the new section loads and resolves it to a page.
  std::string pendingAnchor;
  // Refresh cadence counter, seeded from SETTINGS in onEnter() (0 here would make the first
  // paint of the book a slow HALF refresh, see ReaderUtils::displayWithRefreshCycle).
  int pagesUntilFullRefresh = 0;
  // Idle-time glyph prewarm: after a page settles, scan the LIKELY next page
  // (scan mode draws nothing) and load its missing glyphs from SD during idle,
  // so the next turn's in-render prewarm is a cache hit instead of ~100 ms of
  // SD reads on the page-turn critical path. One attempt per position.
  int idlePrewarmSpine = -1;
  int idlePrewarmPage = -1;
  unsigned long lastRenderCompleteMs = 0;
  int cachedSpineIndex = 0;
  int cachedChapterTotalPageCount = 0;
  unsigned long lastPageTurnTime = 0UL;
  unsigned long pageTurnDuration = 0UL;
  // Signals that the next render should reposition within the newly loaded section
  // based on a cross-book percentage jump.
  bool pendingPercentJump = false;
  // Normalized 0.0-1.0 progress within the target spine item, computed from book percentage.
  float pendingSpineProgress = 0.0f;
  bool pendingScreenshot = false;
  bool pendingSyncSaveError = false;
  bool pendingSyncLaunchError = false;
#ifdef ENABLE_CHINESE_VERSION
  std::atomic<uint32_t> pendingMissingChineseCodepoint_{0};
  char wereadBookId_[64] = {};
  bool clearInitialProgressAfterSave_ = false;
  bool maybeOfferCompleteChineseFont();
#endif
  bool skipNextButtonCheck = false;  // Skip button processing for one frame after subactivity exit
  bool automaticPageTurnActive = false;
  bool showBookmarkMessage = false;
  bool showDictionaryMessage = false;
  unsigned long dictionaryMessageTime = 0UL;
  bool ignoreNextConfirmRelease = false;
  bool currentPageBookmarked = false;
  bool bookmarkRemoved = false;  // true when last toggle removed (controls popup text)
  std::vector<BookmarkEntry> cachedBookmarks;
  // Tracks whether this book is currently removed from Recent Books by the
  // removeReadBooksFromRecents feature (set at End-of-Book, cleared if paged back in).
  bool recentsEntryRemoved = false;
  unsigned long bookmarkMessageTime = 0UL;
  // Set when the reader is left at end-of-book and SETTINGS.moveFinishedToReadFolder is on.
  // Consumed in onExit() to relocate the finished book into /Read/.
  bool pendingReadFolderMove = false;
  // After createSectionFile fails (typically inflate MaxAlloc cliff), do not retry
  // indexing on every requestUpdate / page-turn — that paints an error loop.
  bool sectionBuildFailed = false;

  // Footnote support
  std::vector<FootnoteEntry> currentPageFootnotes;
  struct SavedPosition {
    int spineIndex;
    int pageNumber;
  };
  static constexpr int MAX_FOOTNOTE_DEPTH = 3;
  SavedPosition savedPositions[MAX_FOOTNOTE_DEPTH] = {};
  int footnoteDepth = 0;

  void renderContents(std::unique_ptr<Page> page, int orientedMarginTop, int orientedMarginRight,
                      int orientedMarginBottom, int orientedMarginLeft);
  void renderStatusBar() const;
  // Heap floor for optional render-adjacent work (idle prewarm). Page
  // deserialization and glyph caching allocate through throwing paths that
  // abort() on OOM; skip deferrable work below it.
  static constexpr size_t RENDER_MIN_FREE_HEAP = 24 * 1024;
  static constexpr size_t RENDER_MIN_MAX_ALLOC = 24 * 1024;
  void silentIndexNextChapterIfNeeded(uint16_t viewportWidth, uint16_t viewportHeight);
  bool saveProgress(int spineIndex, int currentPage, int pageCount);
  // Jump to a percentage of the book (0-100), mapping it to spine and page.
  void jumpToPercent(int percent);
  void onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action);
  // Returns true if sync acted (launched sync, showed credentials hint, or surfaced a
  // save error). CrossMux always acts on no-credentials by opening the hint screen.
  void launchKOReaderSync();
#ifdef ENABLE_CHINESE_VERSION
  bool launchWeReadSync();
#endif
  void applyOrientation(uint8_t orientation);
  void applyWritingMode(uint8_t writingMode);
  uint8_t effectiveWritingMode() const;
  void toggleAutoPageTurn(uint8_t selectedPageTurnOption);
  void pageTurn(bool isForwardTurn);
  void loadCachedBookmarks();
  void addBookmark();
  void updateBookmarkFlag();
  void openDictionaryWordSelect();

  // Footnote navigation
  void navigateToHref(const std::string& href, bool savePosition = false);
  void restoreSavedPosition();

 public:
  explicit EpubReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Epub> epub)
      : Activity("EpubReader", renderer, mappedInput), epub(std::move(epub)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool isReaderActivity() const override { return true; }
  ScreenshotInfo getScreenshotInfo() const override;
  CrossPointPosition getCurrentPosition() const;
};
