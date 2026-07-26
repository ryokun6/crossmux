#include "WeReadFetchActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <string>

#include "../../../components/UITheme.h"
#include "../../ActivityManager.h"
#include "WeReadKeyStore.h"

WeReadFetchActivity::WeReadFetchActivity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity(std::move(name), renderer, mappedInput) {}

void WeReadFetchActivity::onEnter() {
  Activity::onEnter();

  // Every WeRead screen talks TLS through WeReadClient, and a resident SD
  // reader font pins ~75KB of the ~224KB heap — the same starvation that left
  // the web server unable to allocate a socket (hardware-constraints.md rule
  // 10). Unconditional, including the cache hit below: refresh, opening a book
  // and bulk sync all reach the network from here, and reloading the font
  // mid-screen would cost more than holding it unloaded. These lists render
  // with UI fonts only, so the unload is invisible.
  fontUnload_.emplace(renderer);

  wifiOk_ = (WiFi.status() == WL_CONNECTED);
  keyOk_ = WeReadKeyStore::has();
  selected = 0;
  offlineReady_ = false;
  lastErr_ = WeReadClient::Err::Ok;

  // Cache-first: read SD before touching the network. Falls through to a
  // live fetch when the subclass has no cached copy yet — the fetch path
  // then persists via the subclass's parseResponse → saveXxx hook so the
  // next entry hits the cache.
  if (tryLoadFromCache()) {
    offlineReady_ = true;
    requestUpdate();
    return;
  }

  if (!preflightFailed()) {
    spawnFetch();
  }
  requestUpdate();
}

void WeReadFetchActivity::onExit() {
  stopFetchTask();
  ctx_.reset();
  fontUnload_.reset();
  Activity::onExit();
}

void WeReadFetchActivity::stopFetchTask() {
  if (!ctx_ || !ctx_->running.load()) {
    taskHandle_ = nullptr;
    return;
  }
  ctx_->cancel.store(true);
  // Worker checks cancel around the HTTP POST; wait for clean exit so we
  // don't vTaskDelete mid-TLS / JsonDocument work (matches FontDownload).
  for (int i = 0; i < 40 && ctx_->running.load(); ++i) {
    delay(100);
  }
  if (ctx_->running.load() && taskHandle_ != nullptr) {
    LOG_ERR("WEREAD", "fetch task still running on exit — forcing delete");
    vTaskDelete(taskHandle_);
    taskHandle_ = nullptr;
    ctx_->running.store(false);
  } else {
    taskHandle_ = nullptr;
  }
}

WeReadFetchActivity::State WeReadFetchActivity::currentState() const {
  if (offlineReady_) return State::Ready;
  if (!ctx_) return State::Idle;
  return static_cast<State>(ctx_->state.load());
}

void WeReadFetchActivity::onBack() { activityManager.goToWeRead(); }

const char* WeReadFetchActivity::preflightMessage() const {
  if (!wifiOk_) return tr(STR_WEREAD_NO_WIFI);
  if (!keyOk_) return tr(STR_WEREAD_NO_API_KEY);
  return "";
}

void WeReadFetchActivity::requestRefresh() {
  // Manual refresh: force a network round-trip even if we have cached data.
  // The dedicated "重新同步本书" / "全量同步" entry points are the user-
  // visible refresh affordances; this stays as the error-state retry path.
  offlineReady_ = false;
  lastErr_ = WeReadClient::Err::Ok;
  wifiOk_ = (WiFi.status() == WL_CONNECTED);
  keyOk_ = WeReadKeyStore::has();
  if (preflightFailed()) {
    // Same teardown spawnFetch() would do first — otherwise a retry while
    // offline leaves the previous Context and any in-flight HTTP worker live.
    stopFetchTask();
    ctx_.reset();
  } else {
    spawnFetch();
  }
  requestUpdate();
}

void WeReadFetchActivity::spawnFetch() {
  stopFetchTask();
  ctx_.reset();

  auto ctx = std::shared_ptr<Context>(new (std::nothrow) Context());
  if (!ctx) {
    LOG_ERR("WEREAD", "spawnFetch: Context OOM");
    return;
  }
  ctx->request = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  ctx->response = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  if (!ctx->request || !ctx->response) {
    LOG_ERR("WEREAD", "spawnFetch: JsonDocument OOM");
    return;
  }
  ctx->apiName = apiName();
  buildRequest(*ctx->request);

  // Build the response filter. The subclass populates whitelisted fields;
  // we then merge in the top-level fields that the base class itself reads
  // (errcode, upgrade_info) so they survive filtering. If the subclass
  // adds nothing, drop the filter doc so deserializeJson runs unfiltered.
  auto filterDoc = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  if (filterDoc) {
    buildResponseFilter(*filterDoc);
    if (filterDoc->size() > 0) {
      (*filterDoc)["errcode"] = true;
      (*filterDoc)["errmsg"] = true;
      (*filterDoc)["upgrade_info"] = true;
      ctx->filter = std::move(filterDoc);
    }
  }
  ctx->state.store(static_cast<int>(State::Loading));

  // Heap-allocate a shared_ptr copy for the task. Task takes ownership and
  // frees the wrapper inside fetchTrampoline.
  auto* taskArg = new (std::nothrow) std::shared_ptr<Context>(ctx);
  if (!taskArg) {
    LOG_ERR("WEREAD", "spawnFetch: taskArg OOM");
    return;
  }

  ctx->running.store(true);
  // 4 KB stack is enough for TLS + ArduinoJson parse on this codebase
  // (matches the pattern at KeyboardEntryActivity.cpp:45-50).
  BaseType_t rc = xTaskCreate(&fetchTrampoline, "WeReadFetch", 4096, taskArg, 1, &taskHandle_);
  if (rc != pdPASS) {
    LOG_ERR("WEREAD", "spawnFetch: xTaskCreate failed (%d)", static_cast<int>(rc));
    ctx->running.store(false);
    delete taskArg;
    return;
  }
  ctx_ = std::move(ctx);
}

void WeReadFetchActivity::fetchTrampoline(void* arg) {
  // Take ownership of the wrapper, copy out the shared_ptr, free the wrapper.
  auto* wrapper = static_cast<std::shared_ptr<Context>*>(arg);
  std::shared_ptr<Context> ctx = *wrapper;
  delete wrapper;

  if (!ctx->cancel.load()) {
    WeReadClient::Err err = WeReadClient::post(ctx->apiName.c_str(), *ctx->request, *ctx->response,
                                               /*httpTimeoutMs=*/10000, ctx->filter.get());
    // Filter is only needed during deserialize — free it before exposing the
    // response to the main thread to release a few KB sooner.
    ctx->filter.reset();
    if (!ctx->cancel.load()) {
      ctx->err = static_cast<int>(err);
      ctx->state.store(err == WeReadClient::Err::Ok ? static_cast<int>(State::Ready) : static_cast<int>(State::Error));
    }
  }
  // CRITICAL: vTaskDelete(nullptr) never returns, so local destructors do
  // NOT run on stack unwind. Without an explicit reset the local shared_ptr
  // leaks the Context (and its JsonDocument response — tens of KB) forever.
  // Cache flows that fire fetch-tasks back-to-back accumulate the leak fast.
  ctx->running.store(false);
  ctx.reset();
  vTaskDelete(nullptr);
}

void WeReadFetchActivity::consumeResultIfReady() {
  if (!ctx_) return;
  const int s = ctx_->state.load();
  if (s == static_cast<int>(State::Ready) && ctx_->response) {
    parseResponse(*ctx_->response);
    // Free the raw JSON buffer right away — parsed POD lives in subclass.
    ctx_->response.reset();
    ctx_->request.reset();
    requestUpdate();
  } else if (s == static_cast<int>(State::Error) && ctx_->response) {
    lastErr_ = static_cast<WeReadClient::Err>(ctx_->err);
    ctx_->response.reset();
    ctx_->request.reset();
    requestUpdate();
  }
}

void WeReadFetchActivity::loop() {
  consumeResultIfReady();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  // List navigation is enabled only once results are in.
  if (currentState() == State::Ready) {
    const int count = itemCount();
    if (count > 0) {
      buttonNavigator.onNext([this, count] {
        selected = ButtonNavigator::nextIndex(selected, count);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this, count] {
        selected = ButtonNavigator::previousIndex(selected, count);
        requestUpdate();
      });
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (count > 0 && selected >= 0 && selected < count) onConfirm(selected);
    }
  }

  if (currentState() == State::Error && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    requestRefresh();
  }
}

void WeReadFetchActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, headerTitle());

  const int contentY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentH = sh - contentY - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const Rect contentRect{0, contentY, sw, contentH};

  if (preflightFailed()) {
    GUI.drawPopup(renderer, preflightMessage());
  } else {
    switch (currentState()) {
      case State::Idle:
      case State::Loading:
        GUI.drawPopup(renderer, tr(STR_WEREAD_LOADING));
        break;
      case State::Error: {
        const char* msg = (lastErr_ == WeReadClient::Err::Upgrade)   ? tr(STR_WEREAD_UPGRADE_REQUIRED)
                          : (lastErr_ == WeReadClient::Err::Server)  ? tr(STR_WEREAD_SERVER_ERROR)
                          : (lastErr_ == WeReadClient::Err::NoCache) ? tr(STR_WEREAD_CACHE_NOT_AVAILABLE)
                                                                     : tr(STR_WEREAD_HTTP_ERROR);
        GUI.drawPopup(renderer, msg);
        break;
      }
      case State::Ready:
        renderContent(contentRect);
        break;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
