#pragma once

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <memory>
#include <string>

#include "../../../components/themes/BaseTheme.h"  // for struct Rect (used in virtual signatures)
#include "../../../util/ButtonNavigator.h"
#include "../../Activity.h"
#include "WeReadClient.h"

/**
 * Shared scaffolding for WeRead list Activities.
 *
 * Lifecycle:
 *   onEnter() → preflight (Wi-Fi + API key) → spawnFetch() → background task
 *     runs WeReadClient::post() with subclass-supplied apiName/body → main
 *     loop() polls the shared context and calls parseResponse() when ready.
 *
 * Cross-thread state lives in a shared_ptr<Context>; the Activity may destruct
 * while the task is mid-call without UAF — the task keeps its own shared_ptr
 * and writes results only via the context, never via `this`. shared_ptr is
 * justified here despite the CLAUDE.md preference against it: this is a cold
 * setup path (one per Activity entry), and the alternative (manual flag +
 * busy-wait in destructor) would block Back for up to one HTTP timeout.
 */
class WeReadFetchActivity : public Activity {
 public:
  WeReadFetchActivity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadFetchActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 protected:
  // ---- Subclass hooks --------------------------------------------------------

  // Must return a stable string literal (e.g. "/shelf/sync"). Caller copies it.
  virtual const char* apiName() const = 0;

  // Fill the request JsonDocument with business params. api_name and
  // skill_version are injected by WeReadClient — do NOT add them here.
  virtual void buildRequest(JsonDocument& body) = 0;

  // Parse the response into subclass-owned containers (typically a
  // std::vector<RowModel> stored as a member).
  virtual void parseResponse(JsonDocument& resp) = 0;

  // Optional: populate `filter` to instruct ArduinoJson to keep only the
  // fields you mark `true` and discard everything else. Critical for
  // memory-heavy endpoints like /shelf/sync (full response with cover URLs
  // + intro text easily pushes the JsonDocument over 40 KB and triggers
  // OOM on a fragmented heap). Default: no filter — the response is parsed
  // verbatim. See WeReadShelfActivity for an example. Top-level fields the
  // base class itself reads (`errcode`, `errmsg`, `upgrade_info`) are merged
  // in automatically — subclasses do not need to add them.
  virtual void buildResponseFilter(JsonDocument& /*filter*/) {}

  // Number of rows to navigate / render. Called only when state == Ready.
  virtual int itemCount() const = 0;

  // Header label drawn at the top.
  virtual const char* headerTitle() const = 0;

  // Draw the list content into `contentRect`. Loading/error popups are drawn
  // on top by the base class.
  virtual void renderContent(Rect contentRect) = 0;

  // Confirm / Back hooks for the row at `selected`. Default: Back returns to
  // the WeRead menu; Confirm is a no-op.
  virtual void onConfirm(int /*index*/) {}
  virtual void onBack();

  // Re-fire the fetch (e.g. for a manual refresh via long-press). Discards
  // any in-flight response.
  void requestRefresh();

  // State exposed to subclass renderers
  enum class State : int { Idle = 0, Loading = 1, Ready = 2, Error = 3 };
  State currentState() const;
  WeReadClient::Err lastError() const { return lastErr_; }

  // Selected index for list-style subclasses. Kept here so the base can
  // hand it to the ButtonNavigator without subclasses re-implementing nav.
  int selected = 0;
  ButtonNavigator buttonNavigator;

 private:
  struct Context {
    std::atomic<int> state{static_cast<int>(State::Idle)};
    int err = 0;
    std::unique_ptr<JsonDocument> request;
    std::unique_ptr<JsonDocument> response;
    std::unique_ptr<JsonDocument> filter;  // optional — null = no filtering
    std::string apiName;
  };

  std::shared_ptr<Context> ctx_;
  TaskHandle_t taskHandle_ = nullptr;
  WeReadClient::Err lastErr_ = WeReadClient::Err::Ok;

  bool wifiOk_ = false;
  bool keyOk_ = false;

  void spawnFetch();
  void consumeResultIfReady();
  static void fetchTrampoline(void* arg);

  // Cached so render() can show a Wi-Fi / no-key banner without re-querying.
  bool preflightFailed() const { return !wifiOk_ || !keyOk_; }
  const char* preflightMessage() const;
};
