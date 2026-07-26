#include "WeReadCacheBookActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <string>

#include "../../../components/UITheme.h"
#include "../../../fontIds.h"
#include "../../ActivityManager.h"
#include "WeReadBookCacheFlow.h"
#include "WeReadKeyStore.h"

WeReadCacheBookActivity::WeReadCacheBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 std::string bookId, std::string title)
    : Activity("WeReadCacheBook", renderer, mappedInput), bookId_(std::move(bookId)), title_(std::move(title)) {}

void WeReadCacheBookActivity::onEnter() {
  Activity::onEnter();
  LOG_DBG("WRCACHE", "onEnter bookId=%s", bookId_.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    lastErr_ = WeReadClient::Err::NoWifi;
    phase_ = Phase::Error;
    requestUpdate();
    return;
  }
  if (!WeReadKeyStore::has()) {
    lastErr_ = WeReadClient::Err::NoApiKey;
    phase_ = Phase::Error;
    requestUpdate();
    return;
  }

  step_ = 0;
  spawnFetchForCurrentStep();
  requestUpdate();
}

void WeReadCacheBookActivity::onExit() {
  stopFetchTask();
  ctx_.reset();
  fontUnload_.reset();
  Activity::onExit();
}

void WeReadCacheBookActivity::stopFetchTask() {
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
    LOG_ERR("WRCACHE", "fetch task still running on exit — forcing delete");
    vTaskDelete(taskHandle_);
    taskHandle_ = nullptr;
    ctx_->running.store(false);
  } else {
    taskHandle_ = nullptr;
  }
}

void WeReadCacheBookActivity::spawnFetchForCurrentStep() {
  // Guard lives here rather than in onEnter so retry-after-preflight-failure
  // reaches it too. A resident SD reader font pins ~75KB of the ~224KB heap
  // (hardware-constraints.md rule 10) and these are five chained HTTPS POSTs.
  // Re-emplacing per step would reload the font between steps, so only engage
  // it once; nesting is safe if the launching screen already holds one.
  if (!fontUnload_) fontUnload_.emplace(renderer);

  stopFetchTask();
  ctx_.reset();

  auto ctx = std::shared_ptr<Context>(new (std::nothrow) Context());
  if (!ctx) {
    LOG_ERR("WRCACHE", "Context OOM");
    lastErr_ = WeReadClient::Err::Http;
    phase_ = Phase::Error;
    return;
  }
  ctx->request = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  ctx->response = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  auto filterDoc = std::unique_ptr<JsonDocument>(new (std::nothrow) JsonDocument());
  if (!ctx->request || !ctx->response || !filterDoc) {
    LOG_ERR("WRCACHE", "JsonDocument OOM");
    lastErr_ = WeReadClient::Err::Http;
    phase_ = Phase::Error;
    return;
  }

  WeReadBookCacheFlow::buildRequest(step_, bookId_, *ctx->request, *filterDoc);
  ctx->apiName = WeReadBookCacheFlow::stepApiName(step_);
  ctx->filter = std::move(filterDoc);
  ctx->state.store(0);

  auto* taskArg = new (std::nothrow) std::shared_ptr<Context>(ctx);
  if (!taskArg) {
    LOG_ERR("WRCACHE", "taskArg OOM");
    lastErr_ = WeReadClient::Err::Http;
    phase_ = Phase::Error;
    return;
  }
  ctx->running.store(true);
  BaseType_t rc = xTaskCreate(&fetchTrampoline, "WRCacheFetch", 4096, taskArg, 1, &taskHandle_);
  if (rc != pdPASS) {
    LOG_ERR("WRCACHE", "xTaskCreate failed");
    ctx->running.store(false);
    delete taskArg;
    lastErr_ = WeReadClient::Err::Http;
    phase_ = Phase::Error;
    return;
  }

  ctx_ = std::move(ctx);
  phase_ = Phase::Fetching;
}

void WeReadCacheBookActivity::fetchTrampoline(void* arg) {
  auto* wrapper = static_cast<std::shared_ptr<Context>*>(arg);
  std::shared_ptr<Context> ctx = *wrapper;
  delete wrapper;

  if (!ctx->cancel.load()) {
    WeReadClient::Err err = WeReadClient::post(ctx->apiName.c_str(), *ctx->request, *ctx->response,
                                               /*httpTimeoutMs=*/10000, ctx->filter.get());
    ctx->filter.reset();
    if (!ctx->cancel.load()) {
      ctx->err = static_cast<int>(err);
      ctx->state.store(err == WeReadClient::Err::Ok ? 1 : 2);
    }
  }
  // vTaskDelete(nullptr) does not return — local destructors will not run.
  // Drop the shared_ptr explicitly so Context doesn't leak forever.
  ctx->running.store(false);
  ctx.reset();
  vTaskDelete(nullptr);
}

void WeReadCacheBookActivity::consumeResult() {
  if (!ctx_) return;
  const int s = ctx_->state.load();
  if (s == 1) {
    const bool savedOk = WeReadBookCacheFlow::parseAndSave(step_, bookId_, *ctx_->response);
    stopFetchTask();
    ctx_.reset();
    if (!savedOk) {
      LOG_ERR("WRCACHE", "SD write failed at step %d", step_);
      lastErr_ = WeReadClient::Err::Http;
      phase_ = Phase::Error;
      requestUpdate();
      return;
    }
    ++step_;
  } else if (s == 2) {
    const WeReadClient::Err err = static_cast<WeReadClient::Err>(ctx_->err);
    stopFetchTask();
    ctx_.reset();
    if (WeReadBookCacheFlow::isFatalErr(err)) {
      lastErr_ = err;
      phase_ = Phase::Error;
      requestUpdate();
      return;
    }
    // Non-fatal: write an empty .bin so the offline reader sees "no rows"
    // rather than "not cached", then advance.
    LOG_DBG("WRCACHE", "step %d %s (non-fatal); writing empty cache", step_, WeReadClient::errorName(err));
    WeReadBookCacheFlow::saveEmpty(step_, bookId_);
    ++step_;
  } else {
    return;
  }

  if (step_ >= WeReadBookCacheFlow::kTotalSteps) {
    LOG_DBG("WRCACHE", "%s all %d steps done, finishing", bookId_.c_str(), WeReadBookCacheFlow::kTotalSteps);
    finish();  // returns to parent Activity via its resultHandler
    return;
  }
  spawnFetchForCurrentStep();
  requestUpdate();
}

void WeReadCacheBookActivity::loop() {
  consumeResult();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (phase_ == Phase::Error && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (WiFi.status() != WL_CONNECTED) {
      lastErr_ = WeReadClient::Err::NoWifi;
      requestUpdate();
      return;
    }
    spawnFetchForCurrentStep();
    requestUpdate();
  }
}

void WeReadCacheBookActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, title_.c_str());

  const int contentY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentH = sh - contentY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (phase_ == Phase::Error) {
    const char* msg = (lastErr_ == WeReadClient::Err::NoWifi)     ? tr(STR_WEREAD_NO_WIFI)
                      : (lastErr_ == WeReadClient::Err::NoApiKey) ? tr(STR_WEREAD_NO_API_KEY)
                      : (lastErr_ == WeReadClient::Err::Server)   ? tr(STR_WEREAD_SERVER_ERROR)
                      : (lastErr_ == WeReadClient::Err::Upgrade)  ? tr(STR_WEREAD_UPGRADE_REQUIRED)
                                                                  : tr(STR_WEREAD_CACHE_FAILED);
    GUI.drawPopup(renderer, msg);
  } else {
    static char statusBuf[128];
    const int curStep = step_ + 1;
    std::snprintf(statusBuf, sizeof(statusBuf), tr(STR_WEREAD_CACHE_PROGRESS_FMT), curStep,
                  WeReadBookCacheFlow::kTotalSteps, WeReadBookCacheFlow::stepName(step_));
    const int centerY = contentY + contentH / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, centerY - metrics.verticalSpacing, statusBuf);
    const int barY = centerY + metrics.verticalSpacing;
    GUI.drawProgressBar(
        renderer,
        Rect{metrics.contentSidePadding, barY, sw - metrics.contentSidePadding * 2, metrics.progressBarHeight},
        static_cast<size_t>(step_), static_cast<size_t>(WeReadBookCacheFlow::kTotalSteps));
  }

  const char* btn2 = (phase_ == Phase::Error) ? tr(STR_RETRY) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), btn2, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
