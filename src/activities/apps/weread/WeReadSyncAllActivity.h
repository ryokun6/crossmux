#pragma once

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../Activity.h"
#include "ScopedSdFontUnload.h"
#include "WeReadClient.h"

/**
 * Bulk shelf-sync. Triggered from the WeRead Menu "立即缓存" entry.
 *
 * Inlined two-axis state machine: outer = book index, inner = step within
 * the per-book 5-step flow defined in `WeReadBookCacheFlow`. The Activity
 * owns one FreeRTOS fetch task at a time and never pushes a child Activity,
 * so the UI stays on a single screen — no e-ink flicker between books.
 *
 * Re-running after a partial sync simply restarts from book 0. There's no
 * "skip already cached" short-circuit because the cache files written on
 * non-fatal errors look identical to a success on disk; explicit re-run
 * always overwrites.
 *
 * Cancellation: Back at any point. onExit() signals cancel, waits for the
 * in-flight fetch task, then force-deletes if still running (FontDownload
 * pattern). Already-completed books remain on SD; the in-flight book's files
 * may be incomplete but will be overwritten by the next visit.
 */
class WeReadSyncAllActivity final : public Activity {
 public:
  WeReadSyncAllActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadSyncAllActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Phase : uint8_t { FetchingShelf, SyncingBook, Done, Error };

  struct Context {
    std::atomic<int> state{0};  // 0=loading, 1=ready, 2=error
    std::atomic<bool> cancel{false};
    std::atomic<bool> running{false};
    int err = 0;
    std::unique_ptr<JsonDocument> request;
    std::unique_ptr<JsonDocument> response;
    std::unique_ptr<JsonDocument> filter;
    std::string apiName;
  };

  Phase phase_ = Phase::FetchingShelf;
  WeReadClient::Err lastErr_ = WeReadClient::Err::Ok;

  std::shared_ptr<Context> ctx_;
  TaskHandle_t taskHandle_ = nullptr;

  // Engaged once the bulk sync actually starts; released on destruction.
  std::optional<ScopedSdFontUnload> fontUnload_;

  // (bookId, title) for every ebook on the shelf. Albums are skipped:
  // /book/* endpoints reject albumIds.
  std::vector<std::pair<std::string, std::string>> booksToSync_;
  int bookIdx_ = 0;
  int stepIdx_ = 0;
  int syncedCount_ = 0;

  // Spawn /shelf/sync and transition to FetchingShelf.
  void spawnShelfFetch();

  // Spawn the current (bookIdx_, stepIdx_) step's fetch task. Sets
  // Phase::SyncingBook.
  void spawnBookStepFetch();

  // Signal cancel, wait for the worker, force-delete if still running.
  void stopFetchTask();

  // Main-loop drain. Dispatches by phase_ and advances the state machine.
  void consumeResult();

  // Parse /shelf/sync and populate booksToSync_ + save shelf.bin.
  void parseShelfResponseAndSave();

  // Used by both shelf fetch and per-step fetch — single trampoline that
  // just calls WeReadClient::post and forwards status via the shared Context.
  static void fetchTrampoline(void* arg);
};
