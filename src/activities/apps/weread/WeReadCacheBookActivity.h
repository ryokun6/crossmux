#pragma once

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "../../Activity.h"
#include "ScopedSdFontUnload.h"
#include "WeReadClient.h"

/**
 * Single-book 5-step cache UI.
 *
 * Step definitions, request building, response parsing, and SD persistence
 * all live in `WeReadBookCacheFlow` — this Activity only owns the lifecycle:
 *   - one FreeRTOS task per step (HTTP POST)
 *   - main-loop state polling
 *   - per-step progress UI
 *   - auto-finish() on the last successful (or non-fatally skipped) step
 *
 * Error UX: Confirm retries the current step; Back exits the Activity.
 *
 * Threading: identical pattern to `WeReadFetchActivity` — the task owns a
 * `std::shared_ptr<Context>` for the duration of one POST. The Activity may
 * be destructed mid-fetch; the task drops its reference before vTaskDelete.
 */
class WeReadCacheBookActivity final : public Activity {
 public:
  WeReadCacheBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId,
                          std::string title);
  ~WeReadCacheBookActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Phase : uint8_t { Fetching, Error };

  struct Context {
    std::atomic<int> state{0};  // 0=loading, 1=ready, 2=error
    int err = 0;
    std::unique_ptr<JsonDocument> request;
    std::unique_ptr<JsonDocument> response;
    std::unique_ptr<JsonDocument> filter;
    std::string apiName;
  };

  std::string bookId_;
  std::string title_;

  Phase phase_ = Phase::Fetching;
  int step_ = 0;
  WeReadClient::Err lastErr_ = WeReadClient::Err::Ok;

  std::shared_ptr<Context> ctx_;
  TaskHandle_t taskHandle_ = nullptr;

  // Engaged once the five-step fetch actually starts; released on destruction.
  std::optional<ScopedSdFontUnload> fontUnload_;

  // Build request body + filter for `step_`, spawn the worker task.
  void spawnFetchForCurrentStep();

  // Drain a Ready/Error result from `ctx_` and advance the step. On the last
  // step (success or non-fatal skip) this calls finish() — control returns
  // to the parent Activity via its result handler.
  void consumeResult();

  static void fetchTrampoline(void* arg);
};
