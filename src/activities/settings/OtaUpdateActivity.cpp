#include "OtaUpdateActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/OtaUpdater.h"

namespace {
// Same floor as FontDownloadActivity (see the derivation there) — OTA begin also
// needs mbedTLS + upgrade buf. Lowered from 28KB now that
// CONFIG_MBEDTLS_DYNAMIC_BUFFER sizes the record buffer per record instead of
// pinning 16K in + 4K out, which leaves a 16KB record as the largest single
// allocation; 20KB covers it with margin. Observed handshakes on this path open
// with 38-53KB MaxAlloc, so the old floor only ever fired on arenas that were
// still workable.
constexpr uint32_t MIN_MAX_ALLOC_FOR_TLS = 20 * 1024;
}  // namespace

void OtaUpdateActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    LOG_ERR("OTA", "WiFi connection failed, exiting");
    finish();
    return;
  }

  LOG_DBG("OTA", "WiFi connected, checking for update");

  sdFontSystem.unloadAll(renderer);
  WiFi.scanDelete();

  const uint32_t maxAlloc = ESP.getMaxAllocHeap();
  LOG_INF("OTA", "Post-WiFi Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(maxAlloc));
  if (!resumedAfterDefrag_ && maxAlloc < MIN_MAX_ALLOC_FOR_TLS) {
    LOG_INF("OTA", "MaxAlloc below TLS floor — silent-restart to defrag heap");
    WiFi.disconnect(true);
    delay(30);
    WiFi.mode(WIFI_OFF);
    silentRestartToOtaUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state = CHECKING_FOR_UPDATE;
  }
  requestUpdateAndWait();

  const auto res = updater.checkForUpdate();
  if (res != OtaUpdater::OK) {
    LOG_DBG("OTA", "Update check failed: %d", res);
    {
      RenderLock lock(*this);
      state = FAILED;
    }
    return;
  }

  // Built for both outcomes: being up to date is exactly when a user wants to
  // reinstall or move to another language build.
  buildSkuRows();

  if (!updater.isUpdateNewer()) {
    LOG_DBG("OTA", "No new update available");
    {
      RenderLock lock(*this);
      state = NO_UPDATE;
    }
    return;
  }

  {
    RenderLock lock(*this);
    state = WAITING_CONFIRMATION;
  }
}

const char* OtaUpdateActivity::skuLabel(const OtaUpdater::Sku sku) {
  switch (sku) {
    case OtaUpdater::Sku::TRADITIONAL_CHINESE:
      return tr(STR_FW_TRADITIONAL_CHINESE);
    case OtaUpdater::Sku::SIMPLIFIED_CHINESE:
      return tr(STR_FW_SIMPLIFIED_CHINESE);
    case OtaUpdater::Sku::JAPANESE:
      return tr(STR_FW_JAPANESE);
    case OtaUpdater::Sku::KOREAN:
      return tr(STR_FW_KOREAN);
    case OtaUpdater::Sku::INTERNATIONAL:
      break;
  }
  return tr(STR_FW_INTERNATIONAL);
}

void OtaUpdateActivity::buildSkuRows() {
  skuRowCount_ = 0;
  for (size_t i = 0; i < OtaUpdater::SKU_COUNT; i++) {
    if (updater.getSkuAsset(static_cast<OtaUpdater::Sku>(i)).present()) {
      skuRows_[skuRowCount_++] = static_cast<uint8_t>(i);
    }
  }
}

void OtaUpdateActivity::enterSkuSelection() {
  if (skuRowCount_ == 0) return;

  // Open on the running build, so the common case — reinstalling what is already
  // there — is one press away and a language switch is a deliberate move off it.
  selectedSkuRow_ = 0;
  for (uint8_t row = 0; row < skuRowCount_; row++) {
    if (rowSku(row) == OtaUpdater::runningSku()) selectedSkuRow_ = row;
  }

  RenderLock lock(*this);
  skuReturnState_ = state;
  state = SKU_SELECTION;
}

void OtaUpdateActivity::confirmSkuSelection() {
  // Rejects an image too large for this device's app slot here rather than after a
  // ~6 MB download, which is the failure worth catching early when moving from a
  // small SKU to a large one.
  const auto res = updater.selectSkuForInstall(rowSku(selectedSkuRow_));
  if (res != OtaUpdater::OK) {
    setFailure(res);
    return;
  }
  RenderLock lock(*this);
  state = SKU_CONFIRMATION;
}

void OtaUpdateActivity::setFailure(const OtaUpdater::OtaUpdaterError error) {
  RenderLock lock(*this);
  state = FAILED;
  switch (error) {
    case OtaUpdater::DIGEST_MISSING:
    case OtaUpdater::DIGEST_MISMATCH:
      // The download completed but the bytes are not the published release, so
      // say so rather than leaving the user to retry into the same wall.
      errorMessage_ = tr(STR_UPDATE_VERIFY_FAILED);
      break;
    case OtaUpdater::STORAGE_ERROR:
      errorMessage_ = tr(STR_UPDATE_NO_SD_SPACE);
      break;
    case OtaUpdater::IMAGE_TOO_LARGE:
      // Reachable by moving from a small SKU to a large one on a device whose
      // app slot is smaller than this build assumes.
      errorMessage_ = tr(STR_FIRMWARE_TOO_LARGE);
      break;
    case OtaUpdater::FLASH_ERROR:
      errorMessage_ = tr(STR_FIRMWARE_WRITE_FAILED);
      break;
    default:
      errorMessage_ = nullptr;
      break;
  }
}

void OtaUpdateActivity::onEnter() {
  Activity::onEnter();

  // Turn on WiFi immediately
  LOG_DBG("OTA", "Turning on WiFi...");
  WiFi.mode(WIFI_STA);

  // Launch WiFi selection subactivity
  LOG_DBG("OTA", "Launching WifiSelectionActivity...");
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void OtaUpdateActivity::onExit() {
  Activity::onExit();

  sdFontSystem.ensureLoaded(renderer);

  // Success path reboots via the SHUTTING_DOWN state's plain ESP.restart()
  // (loop() above) so the new firmware boots normally. Back-out paths land
  // here with wifi still active; silent-restart to free the LWIP/mbedTLS
  // fragmentation, same as the other wifi activities.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void OtaUpdateActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const bool choosingBuild = state == SKU_SELECTION || state == SKU_CONFIRMATION;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 choosingBuild ? tr(STR_FW_BUILD) : tr(STR_UPDATE));
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - height) / 2;

  float updaterProgress = 0;
  if (state == UPDATE_IN_PROGRESS || state == FLASHING) {
    LOG_DBG("OTA", "Update progress: %d / %d", updater.getProcessedSize(), updater.getTotalSize());
    updaterProgress = updater.getTotalSize() > 0
                          ? static_cast<float>(updater.getProcessedSize()) / static_cast<float>(updater.getTotalSize())
                          : 0.0f;
    // Only update every 2% at the most
    if (static_cast<int>(updaterProgress * 50) == lastUpdaterPercentage / 2) {
      return;
    }
    lastUpdaterPercentage = static_cast<int>(updaterProgress * 100);
  }

  if (state == SKU_SELECTION) {
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const size_t slotSize = OtaUpdater::getOtaSlotSize();
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, skuRowCount_, selectedSkuRow_,
        [this](int row) { return std::string(skuLabel(rowSku(row))); }, nullptr, nullptr,
        [this, slotSize](int row) {
          const OtaUpdater::Sku sku = rowSku(row);
          if (sku == OtaUpdater::runningSku()) return std::string(tr(STR_FW_CURRENT));
          const size_t size = updater.getSkuAsset(sku).size;
          if (slotSize > 0 && size > slotSize) return std::string(tr(STR_FW_TOO_LARGE_SHORT));
          char buf[16];
          snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(size) / (1024.0 * 1024.0));
          return std::string(buf);
        },
        true,
        [this, slotSize](int row) {
          const size_t size = updater.getSkuAsset(rowSku(row)).size;
          return slotSize > 0 && size > slotSize;
        });

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == SKU_CONFIRMATION) {
    const OtaUpdater::Sku sku = rowSku(selectedSkuRow_);
    const bool switching = sku != OtaUpdater::runningSku();
    int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

    renderer.drawCenteredText(UI_10_FONT_ID, y, switching ? tr(STR_FW_SWITCH_PROMPT) : tr(STR_FW_REINSTALL_PROMPT),
                              true, EpdFontFamily::BOLD);
    y += height + metrics.verticalSpacing * 2;
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, skuLabel(sku));
    y += height + metrics.verticalSpacing;
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y,
                      (std::string(tr(STR_NEW_VERSION)) + updater.getLatestVersion()).c_str());

    // Only a language switch has consequences worth listing; a same-build
    // reinstall writes the identical image and changes nothing.
    if (switching) {
      y += height + metrics.verticalSpacing * 2;
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, tr(STR_FW_SWITCH_LANGUAGE));
      y += height + metrics.verticalSpacing;
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, tr(STR_FW_SWITCH_REPAGINATE));
      y += height + metrics.verticalSpacing;
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, tr(STR_FW_SWITCH_KEEPS));
    }

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_FW_INSTALL), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == CHECKING_FOR_UPDATE) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_CHECKING_UPDATE));
  } else if (state == WAITING_CONFIRMATION) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_NEW_UPDATE), true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, top + height + metrics.verticalSpacing,
                      (std::string(tr(STR_CURRENT_VERSION)) + CROSSPOINT_VERSION).c_str());
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, top + height * 2 + metrics.verticalSpacing * 2,
                      (std::string(tr(STR_NEW_VERSION)) + updater.getLatestVersion()).c_str());

    // Confirm installs the running SKU's asset (the plain update); the third
    // button opens the build list for anyone who wants to switch language at the
    // same time.
    const auto labels =
        mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_UPDATE), skuRowCount_ > 1 ? tr(STR_FW_CHANGE_BUILD) : "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == UPDATE_IN_PROGRESS || state == FLASHING) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, state == FLASHING ? tr(STR_UPDATING) : tr(STR_DOWNLOADING));

    int y = top + height + metrics.verticalSpacing;
    GUI.drawProgressBar(
        renderer,
        Rect{metrics.contentSidePadding, y, pageWidth - metrics.contentSidePadding * 2, metrics.progressBarHeight},
        static_cast<int>(updaterProgress * 100), 100);

    y += metrics.progressBarHeight + metrics.verticalSpacing;
    // Percent label is drawn by BaseTheme::drawProgressBar; this slot is left intentionally empty
    // so the bytes line below stays at the same Y it was at when the activity drew its own percent.
    y += height + metrics.verticalSpacing;
    // Writing the OTA partition is the only phase where losing power leaves the
    // device without a bootable app, so the warning is scoped to it.
    if (state == FLASHING) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_FIRMWARE_UPDATE_DO_NOT_POWER_OFF));
    } else {
      renderer.drawCenteredText(
          UI_10_FONT_ID, y,
          (std::to_string(updater.getProcessedSize()) + " / " + std::to_string(updater.getTotalSize())).c_str());
    }
  } else if (state == VERIFYING) {
    // Hashing ~6 MB off the SD card takes long enough that a static "Updating…"
    // would read as a hang.
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_VERIFYING_UPDATE));
  } else if (state == NO_UPDATE) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_NO_UPDATE), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, top + height + metrics.verticalSpacing,
                              (std::string(tr(STR_CURRENT_VERSION)) + CROSSPOINT_VERSION).c_str());
    // The dead end this screen used to be is where reinstalling and switching
    // language build now live, since both are same-version installs.
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), skuRowCount_ > 0 ? tr(STR_FW_REINSTALL) : "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_UPDATE_FAILED), true, EpdFontFamily::BOLD);
    if (errorMessage_ != nullptr) {
      renderer.drawCenteredText(UI_10_FONT_ID, top + height + metrics.verticalSpacing, errorMessage_);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == FINISHED) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_UPDATE_COMPLETE), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, top + height + metrics.verticalSpacing, tr(STR_POWER_ON_HINT));
  }

  renderer.displayBuffer();
}

void OtaUpdateActivity::runInstall() {
  {
    RenderLock lock(*this);
    state = UPDATE_IN_PROGRESS;
  }
  requestUpdateAndWait();
  const auto res = updater.installUpdate(
      [](void* ctx) {
        // immediate=true notifies the render task directly. The default deferred path only
        // sets a flag consumed at the end of ActivityManager::loop(), which never runs while
        // installUpdate() blocks this task.
        static_cast<OtaUpdateActivity*>(ctx)->requestUpdate(true);
      },
      this,
      [](void* ctx, OtaUpdater::Stage stage) {
        auto* self = static_cast<OtaUpdateActivity*>(ctx);
        {
          RenderLock lock(*self);
          switch (stage) {
            case OtaUpdater::Stage::DOWNLOADING:
              self->state = UPDATE_IN_PROGRESS;
              break;
            case OtaUpdater::Stage::VERIFYING:
              self->state = VERIFYING;
              break;
            case OtaUpdater::Stage::FLASHING:
              self->state = FLASHING;
              break;
          }
          // Each phase restarts its progress at 0%, so drop the throttle
          // baseline or the first frame of the new phase is skipped.
          self->lastUpdaterPercentage = UNINITIALIZED_PERCENTAGE;
        }
        self->requestUpdate(true);
      });

  if (res != OtaUpdater::OK) {
    LOG_DBG("OTA", "Update failed: %d", res);
    setFailure(res);
    requestUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state = FINISHED;
  }
  requestUpdateAndWait();
  // Hold the completion screen briefly so the user sees it, then restart.
  delay(3000);
  {
    RenderLock lock(*this);
    state = SHUTTING_DOWN;
  }
}

void OtaUpdateActivity::loop() {
  if (state == WAITING_CONFIRMATION) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      LOG_DBG("OTA", "New update available, starting download...");
      runInstall();
      return;
    }

    if (skuRowCount_ > 1 && mappedInput.wasPressed(MappedInputManager::Button::NavPrevious)) {
      enterSkuSelection();
      requestUpdate();
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }

    return;
  }

  if (state == SKU_SELECTION) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      // Leaving the list abandons any build picked in it, so the "Update" button
      // on the screen behind still means the running SKU's asset.
      updater.resetInstallTarget();
      {
        RenderLock lock(*this);
        state = skuReturnState_;
      }
      requestUpdate();
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      confirmSkuSelection();
      requestUpdate();
      return;
    }

    const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);
    buttonNavigator.onNextRelease([this] {
      selectedSkuRow_ = ButtonNavigator::nextIndex(selectedSkuRow_, skuRowCount_);
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this] {
      selectedSkuRow_ = ButtonNavigator::previousIndex(selectedSkuRow_, skuRowCount_);
      requestUpdate();
    });
    buttonNavigator.onNextContinuous([this, pageItems] {
      selectedSkuRow_ = ButtonNavigator::nextPageIndex(selectedSkuRow_, skuRowCount_, pageItems);
      requestUpdate();
    });
    buttonNavigator.onPreviousContinuous([this, pageItems] {
      selectedSkuRow_ = ButtonNavigator::previousPageIndex(selectedSkuRow_, skuRowCount_, pageItems);
      requestUpdate();
    });
    return;
  }

  if (state == SKU_CONFIRMATION) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      LOG_INF("OTA", "Installing %s by user choice", OtaUpdater::skuAssetName(rowSku(selectedSkuRow_)));
      runInstall();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      {
        RenderLock lock(*this);
        state = SKU_SELECTION;
      }
      requestUpdate();
    }
    return;
  }

  if (state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state == NO_UPDATE) {
    if (skuRowCount_ > 0 && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      enterSkuSelection();
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state == SHUTTING_DOWN) {
    ESP.restart();
  }
}
