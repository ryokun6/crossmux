#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "../../Activity.h"
#include "util/ButtonNavigator.h"

struct ReadingStatsDetailContext {
  bool showSessionSummary = false;
};

class ReadingStatsDetailActivity final : public Activity {
  // Match GfxRenderer::BW_BUFFER_CHUNK_SIZE — never one contiguous 48KB malloc.
  static constexpr size_t kBaseScreenChunkSize = 8000;
  static constexpr size_t kBaseScreenMaxChunks = 8;  // 8 * 8KB >= 48KB framebuffer

  ButtonNavigator buttonNavigator;
  std::string bookPath;
  std::string resolvedCoverBmpPath;
  ReadingStatsDetailContext context;
  bool coverLoadPending = false;
  int selectedStatsItem = 0;
  bool waitForConfirmRelease = false;
  bool waitForBackRelease = false;
  bool baseScreenBufferStored = false;
  std::array<std::unique_ptr<uint8_t[]>, kBaseScreenMaxChunks> baseScreenChunks{};
  size_t baseScreenChunkCount = 0;
  size_t baseScreenStoredSize = 0;
  std::string baseScreenBookPath;
  std::string baseScreenCoverPath;
  int baseScreenScrollOffset = -1;
  int scrollOffset = 0;
  int maxScrollOffset = 0;

  void openAdjustment();
  void guardChildReturn();
  bool storeBaseScreenBuffer();
  bool restoreBaseScreenBuffer();
  void invalidateBaseScreenBuffer();
  void freeBaseScreenBuffer();

 public:
  explicit ReadingStatsDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                                      ReadingStatsDetailContext context = {})
      : Activity("ReadingStatsDetail", renderer, mappedInput),
        bookPath(std::move(bookPath)),
        context(std::move(context)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
