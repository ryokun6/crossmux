#include "FontDownloadActivity.h"

#ifdef ENABLE_CHINESE_VERSION
#include <atomic>
#endif

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_rom_crc.h>

#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {
// Download worker stack. 8KB left MaxAlloc ~36KB after spawn and
// create_ssl_handle failed; 5KB is enough for TLS locals + SD write on this
// path (WeRead uses 4KB) and keeps ~3KB more contiguous DRAM for mbedTLS.
constexpr uint32_t kDownloadTaskStackBytes = 5120;
constexpr UBaseType_t kDownloadTaskPriority = 1;
// Contiguous DRAM a handshake needs (mbedTLS dynamic RX/TX + HTTP RX/TX).
// Falling below it means the arena is fragmented, so defrag via silent-restart
// rather than letting create_ssl_handle fail.
//
// Was 28KB, tuned when mbedTLS pinned 16K in + 4K out for the whole session.
// CONFIG_MBEDTLS_DYNAMIC_BUFFER now allocates the record buffer per record and
// frees it between reads, so the largest single request is one incoming record
// (bounded by CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN = 16KB) coexisting with
// HttpDownloader's 4KB HTTP RX buffer — 20KB covers that worst case. Device logs
// back this off the other end too: mid-transfer this path runs happily at
// 13-27KB MaxAlloc, so a 28KB floor was rebooting to defrag an arena that could
// already have finished the download. Do not push it below the 16KB record
// bound: the silent restart is the last thing standing between a fragmented
// arena and a hard create_ssl_handle failure, and too low trades a reboot the
// user barely notices for a download that just fails.
constexpr uint32_t MIN_MAX_ALLOC_FOR_TLS = 20 * 1024;
constexpr const char* MANIFEST_TMP = "/fonts_manifest.tmp";

#ifdef ENABLE_CHINESE_VERSION
std::atomic<bool> chineseFontPromptShownThisBoot{false};
#endif
}  // namespace

FontDownloadActivity::FontDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const Purpose purpose, const bool resumedAfterDefrag)
    : Activity("FontDownload", renderer, mappedInput),
      purpose_(purpose),
      fontInstaller_(sdFontSystem.registry()),
      resumedAfterDefrag_(resumedAfterDefrag) {}

#ifdef ENABLE_CHINESE_VERSION
bool FontDownloadActivity::wasChineseFontPromptShownThisBoot() {
  return chineseFontPromptShownThisBoot.load(std::memory_order_relaxed);
}
#endif

// --- Lifecycle ---

void FontDownloadActivity::onEnter() {
  Activity::onEnter();

  switch (purpose_) {
    case Purpose::Manage:
      startWifiSelection();
      return;
    case Purpose::PromptThenManage: {
      auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(
          renderer, mappedInput, tr(STR_CHINESE_FONT_INCOMPLETE), tr(STR_DOWNLOAD_FULL_CHINESE_FONT),
          ConfirmationActivity::BodyPlacement::PopupTitle);
      if (!confirmation) {
        LOG_ERR("FONT", "OOM allocating ConfirmationActivity (%zu bytes)", sizeof(ConfirmationActivity));
        finish();
        return;
      }
#ifdef ENABLE_CHINESE_VERSION
      chineseFontPromptShownThisBoot.store(true, std::memory_order_relaxed);
#endif
      startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
        if (result.isCancelled) {
          finish();
          return;
        }
        startWifiSelection();
      });
      return;
    }
  }
}

void FontDownloadActivity::startWifiSelection() {
  WiFi.mode(WIFI_STA);
  // autoConnect=true: after a defrag reboot this reconnects the last SSID without
  // a full scan UI, which is what left MaxAlloc too small for TLS.
  auto wifiSelection = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true);
  if (!wifiSelection) {
    LOG_ERR("FONT", "OOM allocating WifiSelectionActivity (%zu bytes)", sizeof(WifiSelectionActivity));
    finish();
    return;
  }
  startActivityForResult(std::move(wifiSelection),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void FontDownloadActivity::onExit() {
  stopDownloadTask();
  Storage.remove(MANIFEST_TMP);
  Activity::onExit();

  // Restore the user's SD font if we unloaded it for HTTPS headroom.
  sdFontSystem.ensureLoaded(renderer);

  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void FontDownloadActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    finish();
    return;
  }

  // Resident SD reader fonts fragment the internal heap; drop them before TLS.
  sdFontSystem.unloadAll(renderer);
  WiFi.scanDelete();

  const uint32_t maxAlloc = ESP.getMaxAllocHeap();
  LOG_INF("FONT", "Post-WiFi Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(maxAlloc));
  if (!resumedAfterDefrag_ && maxAlloc < MIN_MAX_ALLOC_FOR_TLS) {
    // Tear Wi‑Fi down before reboot so onExit doesn't silentRestart() to home
    // and clobber the font-download resume target.
    LOG_INF("FONT", "MaxAlloc below TLS floor — silent-restart to defrag heap");
    WiFi.disconnect(true);
    delay(30);
    WiFi.mode(WIFI_OFF);
    silentRestartToFontDownload();
    return;
  }

  {
    RenderLock lock(*this);
    state_ = LOADING_MANIFEST;
  }
  requestUpdateAndWait();

  if (!fetchAndParseManifest()) {
    {
      RenderLock lock(*this);
      state_ = ERROR;
    }
    return;
  }

  {
    RenderLock lock(*this);
    state_ = FAMILY_LIST;
    selectedIndex_ = 0;
  }
}

// --- Manifest fetching ---

bool FontDownloadActivity::fetchAndParseManifest() {
  // Manifest staged on SD so the TLS buffers and the JSON document never
  // coexist. Re-used as-is after a defrag restart so the retry costs no second
  // fonts.json round trip.
  if (!(resumedAfterDefrag_ && Storage.exists(MANIFEST_TMP))) {
    auto result = HttpDownloader::downloadToFile(FONT_MANIFEST_URL, MANIFEST_TMP, nullptr);
    if (result != HttpDownloader::OK) {
      LOG_ERR("FONT", "Failed to fetch manifest from %s", FONT_MANIFEST_URL);
      errorMessage_ = "Failed to fetch font list";
      Storage.remove(MANIFEST_TMP);
      return false;
    }
  }

  HalFile manifestFile;
  if (!Storage.openFileForRead("FONT", MANIFEST_TMP, manifestFile)) {
    LOG_ERR("FONT", "Failed to open temp manifest");
    Storage.remove(MANIFEST_TMP);
    errorMessage_ = "Failed to read font list";
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, manifestFile);
  manifestFile.close();
  // Keep MANIFEST_TMP so a failed .cpfont attempt can silent-restart and
  // reload the list without another fonts.json HTTPS (heap stays cleaner).

  if (err) {
    LOG_ERR("FONT", "Manifest parse error: %s", err.c_str());
    errorMessage_ = "Invalid font manifest";
    return false;
  }

  int version = doc["version"] | 0;
  if (version != FONTS_MANIFEST_VERSION) {
    LOG_ERR("FONT", "Unsupported manifest version: %d", version);
    errorMessage_ = "Unsupported manifest version";
    return false;
  }

  baseUrl_ = doc["baseUrl"] | "";
  families_.clear();
  fontInstaller_.refreshRegistry();

  JsonArray familiesArr = doc["families"].as<JsonArray>();
  families_.reserve(familiesArr.size());

  for (JsonObject fObj : familiesArr) {
    ManifestFamily family;
    family.name = fObj["name"] | "";
    family.description = fObj["description"] | "";

    for (JsonVariant s : fObj["styles"].as<JsonArray>()) {
      family.styles.push_back(s.as<std::string>());
    }

    family.totalSize = 0;
    for (JsonObject fileObj : fObj["files"].as<JsonArray>()) {
      ManifestFile file;
      file.name = fileObj["name"] | "";
      file.size = fileObj["size"] | 0;

      if (!fileObj["crc32"].is<uint32_t>()) {
        LOG_ERR("FONT", "Malformed manifest file entry: missing or invalid crc32 for %s", file.name.c_str());
        errorMessage_ = "Invalid font manifest";
        return false;
      }
      file.crc32 = fileObj["crc32"].as<uint32_t>();

      family.totalSize += file.size;
      family.files.push_back(std::move(file));
    }

    family.installed = fontInstaller_.isFamilyInstalled(family.name.c_str());

    // Detect updates by comparing manifest file sizes with files on disk.
    // Not a checksum, but a size mismatch reliably indicates a rebuild in practice.
    if (family.installed) {
      for (const auto& file : family.files) {
        char path[128];
        FontInstaller::buildFontPath(family.name.c_str(), file.name.c_str(), path, sizeof(path));
        HalFile f;
        if (Storage.openFileForRead("FONT", path, f)) {
          size_t actual = f.fileSize();
          f.close();
          if (actual != file.size) {
            family.hasUpdate = true;
            break;
          }
        } else {
          // File missing on disk but family dir exists — treat as update
          family.hasUpdate = true;
          break;
        }
      }
    }

    families_.push_back(std::move(family));
  }

  LOG_DBG("FONT", "Manifest loaded: %zu families", families_.size());
  return true;
}

// --- Download ---

void FontDownloadActivity::stopDownloadTask() {
  if (!downloadTaskRunning_.load()) {
    downloadTask_ = nullptr;
    return;
  }
  cancelRequested_ = true;
  // Worker checks cancel between 3s HTTP op timeouts; wait for clean exit so
  // we don't vTaskDelete mid-HalFile write (SdFat is not thread-safe).
  for (int i = 0; i < 40 && downloadTaskRunning_.load(); ++i) {
    delay(100);
  }
  if (downloadTaskRunning_.load() && downloadTask_ != nullptr) {
    LOG_ERR("FONT", "download task still running on exit — forcing delete");
    vTaskDelete(downloadTask_);
    downloadTask_ = nullptr;
    downloadTaskRunning_.store(false);
  }
}

void FontDownloadActivity::startDownloadJob(DownloadJob job) {
  if (downloadTaskRunning_.load()) {
    LOG_ERR("FONT", "download already running");
    return;
  }
  WiFi.scanDelete();
  const uint32_t maxAlloc = ESP.getMaxAllocHeap();
  LOG_INF("FONT", "Pre-job Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(maxAlloc));
  if (maxAlloc < MIN_MAX_ALLOC_FOR_TLS) {
    // Defrag and reload the cached manifest (kept on SD until onExit).
    LOG_INF("FONT", "MaxAlloc %u < %u — silent-restart before retry", static_cast<unsigned>(maxAlloc),
            static_cast<unsigned>(MIN_MAX_ALLOC_FOR_TLS));
    WiFi.disconnect(true);
    delay(30);
    WiFi.mode(WIFI_OFF);
    silentRestartToFontDownload();
    return;
  }

  downloadJob_ = job;
  cancelRequested_ = false;
  {
    RenderLock lock(*this);
    state_ = DOWNLOADING;
    fileProgress_ = 0;
    fileTotal_ = 0;
  }
  requestUpdate();

  downloadTaskRunning_.store(true);
  // Heap: task TCB + stack in internal DRAM — required so loop() can poll
  // Cancel while HTTPS is blocked (sync download starved gpio.update()).
  const BaseType_t rc = xTaskCreate(&downloadTaskTrampoline, "FontDL", kDownloadTaskStackBytes, this,
                                    kDownloadTaskPriority, &downloadTask_);
  if (rc != pdPASS) {
    LOG_ERR("FONT", "xTaskCreate FontDL failed");
    downloadTask_ = nullptr;
    downloadTaskRunning_.store(false);
    RenderLock lock(*this);
    state_ = ERROR;
    errorMessage_ = "Failed to start download";
  }
}

void FontDownloadActivity::downloadTaskTrampoline(void* arg) {
  auto* self = static_cast<FontDownloadActivity*>(arg);
  self->runDownloadJob();
  self->downloadTask_ = nullptr;
  self->downloadTaskRunning_.store(false);
  self->requestUpdate(true);
  vTaskDelete(nullptr);
}

void FontDownloadActivity::runDownloadJob() {
  switch (downloadJob_) {
    case DownloadJob::OneFamily:
      if (downloadingFamilyIndex_ >= 0 && downloadingFamilyIndex_ < static_cast<int>(families_.size())) {
        downloadFamily(families_[downloadingFamilyIndex_]);
      }
      break;
    case DownloadJob::AllMissing:
      downloadAll();
      break;
    case DownloadJob::AllUpdates:
      updateAll();
      break;
    case DownloadJob::None:
      break;
  }
  downloadJob_ = DownloadJob::None;
}

void FontDownloadActivity::downloadAll() {
  for (size_t i = 0; i < families_.size(); i++) {
    if (families_[i].installed) continue;
    downloadFamily(families_[i]);
    if (state_ == ERROR || cancelRequested_) return;
  }

  {
    RenderLock lock(*this);
    state_ = COMPLETE;
  }
}

void FontDownloadActivity::updateAll() {
  for (size_t i = 0; i < families_.size(); i++) {
    if (!families_[i].hasUpdate) continue;
    downloadFamily(families_[i]);
    if (state_ == ERROR || cancelRequested_) return;
  }

  {
    RenderLock lock(*this);
    state_ = COMPLETE;
  }
}

bool FontDownloadActivity::showDownloadAllRow() const {
  for (const auto& f : families_) {
    if (!f.installed) return true;
  }
  return false;
}

bool FontDownloadActivity::showUpdateAllRow() const {
  for (const auto& f : families_) {
    if (f.hasUpdate) return true;
  }
  return false;
}

int FontDownloadActivity::specialRowCount() const {
  return (showDownloadAllRow() ? 1 : 0) + (showUpdateAllRow() ? 1 : 0);
}

bool FontDownloadActivity::isDownloadAllRow(int index) const { return showDownloadAllRow() && index == 0; }

bool FontDownloadActivity::isUpdateAllRow(int index) const {
  return showUpdateAllRow() && index == (showDownloadAllRow() ? 1 : 0);
}

int FontDownloadActivity::listItemCount() const {
  return families_.empty() ? 0 : static_cast<int>(families_.size()) + specialRowCount();
}

size_t FontDownloadActivity::totalDownloadSize() const {
  size_t total = 0;
  for (const auto& f : families_) {
    if (!f.installed) total += f.totalSize;
  }
  return total;
}

size_t FontDownloadActivity::totalUpdateSize() const {
  size_t total = 0;
  for (const auto& f : families_) {
    if (f.hasUpdate) total += f.totalSize;
  }
  return total;
}

// Standard CRC32 matching zlib/Python zlib.crc32().
bool FontDownloadActivity::computeFileCrc32(const char* path, uint32_t& outCrc) {
  HalFile f;
  if (!Storage.openFileForRead("FONT", path, f)) {
    return false;
  }
  constexpr size_t BUF_SIZE = 128;
  uint8_t buf[BUF_SIZE];
  uint32_t crc = 0;
  while (f.available()) {
    const int n = f.read(buf, BUF_SIZE);
    if (n <= 0) break;
    crc = esp_rom_crc32_le(crc, buf, static_cast<uint32_t>(n));
  }
  outCrc = crc;
  return true;
}

void FontDownloadActivity::downloadFamily(ManifestFamily& family) {
  {
    RenderLock lock(*this);
    state_ = DOWNLOADING;
    downloadingFamilyIndex_ = static_cast<int>(&family - families_.data());
    fileProgress_ = 0;
    fileTotal_ = 0;
  }
  requestUpdate(true);

  if (cancelRequested_) {
    RenderLock lock(*this);
    state_ = FAMILY_LIST;
    return;
  }

  if (!fontInstaller_.ensureFamilyDir(family.name.c_str())) {
    RenderLock lock(*this);
    state_ = ERROR;
    errorMessage_ = "Failed to create font directory";
    return;
  }

  for (size_t i = 0; i < family.files.size(); i++) {
    if (cancelRequested_) {
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      RenderLock lock(*this);
      state_ = FAMILY_LIST;
      return;
    }

    const auto& file = family.files[i];

    {
      RenderLock lock(*this);
      fileProgress_ = 0;
      fileTotal_ = file.size;
    }
    requestUpdate(true);
    // E-ink refresh fragments the arena; drop Wi‑Fi scan debris and give the
    // heap a moment before github→CDN TLS. CDN PK verify 0x4290 is MPI OOM.
    WiFi.scanDelete();
    delay(100);
    LOG_INF("FONT", "Pre-file Free=%u MaxAlloc=%u (%s)", static_cast<unsigned>(ESP.getFreeHeap()),
            static_cast<unsigned>(ESP.getMaxAllocHeap()), file.name.c_str());

    char destPath[128];
    FontInstaller::buildFontPath(family.name.c_str(), file.name.c_str(), destPath, sizeof(destPath));

    std::string url = baseUrl_ + file.name;

    // Redraw at most every 5% (plus 100%). Cancel is polled from loop() on the
    // main task — do not touch mappedInput here (download worker).
    int lastReportedPct = -1;
    auto result = HttpDownloader::downloadToFile(
        url, destPath,
        [this, &lastReportedPct](size_t downloaded, size_t total) {
          fileProgress_ = downloaded;
          fileTotal_ = total;
          const int pct = total > 0 ? static_cast<int>((downloaded * 100ULL) / total) : 0;
          if (pct == lastReportedPct || (pct != 100 && pct / 5 == lastReportedPct / 5 && lastReportedPct >= 0)) {
            return;
          }
          lastReportedPct = pct;
          LOG_INF("FONT", "progress %d%% (%zu/%zu)", pct, downloaded, total);
          requestUpdate(true);
        },
        &cancelRequested_);

    if (result == HttpDownloader::ABORTED) {
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      {
        RenderLock lock(*this);
        state_ = FAMILY_LIST;
      }
      return;
    }

    if (result != HttpDownloader::OK) {
      LOG_ERR("FONT", "Download failed: %s (%d) Free=%u MaxAlloc=%u", file.name.c_str(), result,
              static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      if (ESP.getMaxAllocHeap() < MIN_MAX_ALLOC_FOR_TLS && Storage.exists(MANIFEST_TMP)) {
        LOG_INF("FONT", "Defrag after failed file TLS");
        WiFi.disconnect(true);
        delay(30);
        WiFi.mode(WIFI_OFF);
        silentRestartToFontDownload();
        return;
      }
      RenderLock lock(*this);
      state_ = ERROR;
      errorMessage_ = "Download failed: " + file.name;
      return;
    }

    uint32_t actualCrc = 0;
    if (!computeFileCrc32(destPath, actualCrc)) {
      LOG_ERR("FONT", "Failed to open file for CRC check: %s", destPath);
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      RenderLock lock(*this);
      state_ = ERROR;
      errorMessage_ = "Failed to compute checksum: " + file.name;
      return;
    }
    if (actualCrc != file.crc32) {
      LOG_ERR("FONT", "CRC32 mismatch for %s: got %08x expected %08x", file.name.c_str(), actualCrc, file.crc32);
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      RenderLock lock(*this);
      state_ = ERROR;
      errorMessage_ = "Checksum mismatch: " + file.name;
      return;
    }
    LOG_DBG("FONT", "Downloaded %s (size=%zu crc32=%08x)", file.name.c_str(), file.size, actualCrc);

    if (!fontInstaller_.validateCpfontFile(destPath)) {
      LOG_ERR("FONT", "Invalid .cpfont: %s", destPath);
      fontInstaller_.deleteFamily(family.name.c_str());
      family.installed = false;
      family.hasUpdate = false;
      RenderLock lock(*this);
      state_ = ERROR;
      errorMessage_ = "Invalid font file: " + file.name;
      return;
    }
    currentFileIndex_++;
  }

  fontInstaller_.refreshRegistry();
  family.installed = true;
  family.hasUpdate = false;

  {
    RenderLock lock(*this);
    state_ = COMPLETE;
  }
}

void FontDownloadActivity::promptDeleteSelectedFamily() {
  const int pendingDeleteFamilyIndex = familyIndexFromList(selectedIndex_);
  if (pendingDeleteFamilyIndex < 0 || pendingDeleteFamilyIndex >= static_cast<int>(families_.size())) {
    return;
  }

  std::string heading = tr(STR_DELETE);
  const auto& family = families_[pendingDeleteFamilyIndex];
  std::string body = family.name;
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, body),
                         [this](const ActivityResult& result) { onDeleteConfirmationResult(result); });
}

void FontDownloadActivity::onDeleteConfirmationResult(const ActivityResult& result) {
  if (result.isCancelled) {
    requestUpdate();
    return;
  }

  auto& family = families_[familyIndexFromList(selectedIndex_)];

  if (fontInstaller_.deleteFamily(family.name.c_str()) != FontInstaller::Error::OK) {
    RenderLock lock(*this);
    state_ = ERROR;
    errorMessage_ = "Failed to delete font";
  } else {
    fontInstaller_.refreshRegistry();
    family.installed = false;
    family.hasUpdate = false;
  }

  requestUpdate();
}

bool FontDownloadActivity::isSelectedFamilyDeletable() const {
  if (isDownloadAllRow(selectedIndex_) || isUpdateAllRow(selectedIndex_)) return false;
  if (selectedIndex_ < specialRowCount() || selectedIndex_ >= listItemCount()) return false;
  const auto& family = families_[familyIndexFromList(selectedIndex_)];
  return family.installed && !family.hasUpdate;
}

// --- Input handling ---

void FontDownloadActivity::loop() {
  if (state_ == FAMILY_LIST) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }

    const int listSize = listItemCount();
    const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

    buttonNavigator_.onNextRelease([this, listSize] {
      selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, listSize);
      requestUpdate();
    });

    buttonNavigator_.onPreviousRelease([this, listSize] {
      selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, listSize);
      requestUpdate();
    });

    buttonNavigator_.onNextContinuous([this, listSize, pageItems] {
      selectedIndex_ = ButtonNavigator::nextPageIndex(selectedIndex_, listSize, pageItems);
      requestUpdate();
    });

    buttonNavigator_.onPreviousContinuous([this, listSize, pageItems] {
      selectedIndex_ = ButtonNavigator::previousPageIndex(selectedIndex_, listSize, pageItems);
      requestUpdate();
    });

    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      if (!families_.empty()) {
        if (isDownloadAllRow(selectedIndex_)) {
          currentFileIndex_ = 0;
          currentFileTotal_ = 0;
          for (const auto& f : families_) {
            if (!f.installed) currentFileTotal_ += f.files.size();
          }
          startDownloadJob(DownloadJob::AllMissing);
        } else if (isUpdateAllRow(selectedIndex_)) {
          currentFileIndex_ = 0;
          currentFileTotal_ = 0;
          for (const auto& f : families_) {
            if (f.hasUpdate) currentFileTotal_ += f.files.size();
          }
          startDownloadJob(DownloadJob::AllUpdates);
        } else {
          auto& family = families_[familyIndexFromList(selectedIndex_)];
          if (!family.installed || family.hasUpdate) {
            currentFileIndex_ = 0;
            currentFileTotal_ = family.files.size();
            downloadingFamilyIndex_ = familyIndexFromList(selectedIndex_);
            startDownloadJob(DownloadJob::OneFamily);
          } else {
            promptDeleteSelectedFamily();
            return;
          }
        }
        return;
      }
    }
  } else if (state_ == DOWNLOADING) {
    // Main loop keeps running while FontDL task blocks on HTTPS — Cancel works.
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.isPressed(MappedInputManager::Button::Back)) {
      if (!cancelRequested_) {
        LOG_INF("FONT", "Cancel requested");
        cancelRequested_ = true;
      }
    }
  } else if (state_ == COMPLETE) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      {
        RenderLock lock(*this);
        state_ = FAMILY_LIST;
      }
      requestUpdate();
    }
  } else if (state_ == ERROR) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      {
        RenderLock lock(*this);
        state_ = FAMILY_LIST;
      }
      requestUpdate();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      if (downloadingFamilyIndex_ >= 0 && downloadingFamilyIndex_ < static_cast<int>(families_.size())) {
        currentFileIndex_ = 0;
        currentFileTotal_ = families_[downloadingFamilyIndex_].files.size();
        startDownloadJob(DownloadJob::OneFamily);
        return;
      } else {
        {
          RenderLock lock(*this);
          state_ = FAMILY_LIST;
        }
        requestUpdate();
      }
    }
  }
}

// --- Rendering ---

std::string FontDownloadActivity::formatSize(size_t bytes) {
  char buf[32];
  if (bytes >= 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024) {
    snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(bytes) / 1024.0);
  } else {
    snprintf(buf, sizeof(buf), "%zu B", bytes);
  }
  return buf;
}

void FontDownloadActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FONT_BROWSER));

  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const auto centerY = (pageHeight - lineHeight) / 2;

  if (state_ == LOADING_MANIFEST) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_LOADING_FONT_LIST));
  } else if (state_ == FAMILY_LIST) {
    if (families_.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_NO_FONTS_AVAILABLE));
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    } else {
      GUI.drawList(
          renderer,
          Rect{0, contentTop, pageWidth, pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing},
          listItemCount(), selectedIndex_,
          [this](int index) -> std::string {
            if (isDownloadAllRow(index)) {
              return std::string(tr(STR_DOWNLOAD_ALL)) + " (" + formatSize(totalDownloadSize()) + ")";
            }
            if (isUpdateAllRow(index)) {
              return std::string(tr(STR_UPDATE_ALL)) + " (" + formatSize(totalUpdateSize()) + ")";
            }
            return families_[familyIndexFromList(index)].name;
          },
          [this](int index) -> std::string {
            if (isDownloadAllRow(index) || isUpdateAllRow(index)) return "";
            return families_[familyIndexFromList(index)].description;
          },
          nullptr,
          [this](int index) -> std::string {
            if (isDownloadAllRow(index) || isUpdateAllRow(index)) return "";
            const auto& f = families_[familyIndexFromList(index)];
            if (f.hasUpdate) return tr(STR_UPDATE_AVAILABLE);
            if (f.installed) return tr(STR_INSTALLED);
            return "";
          },
          true,
          [this](int index) -> bool {
            if (isDownloadAllRow(index) || isUpdateAllRow(index)) return false;
            const auto& f = families_[familyIndexFromList(index)];
            return f.installed && !f.hasUpdate;
          });

      const auto labels = mappedInput.mapLabels(tr(STR_BACK),
                                                isSelectedFamilyDeletable()      ? tr(STR_DELETE)
                                                : isUpdateAllRow(selectedIndex_) ? tr(STR_UPDATE)
                                                                                 : tr(STR_DOWNLOAD),
                                                tr(STR_DIR_UP), tr(STR_DIR_DOWN));
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    }
  } else if (state_ == DOWNLOADING) {
    const auto& family = families_[downloadingFamilyIndex_];

    std::string statusText = std::string(tr(STR_DOWNLOADING)) + " " + family.name + " (" +
                             std::to_string(currentFileIndex_ + 1) + "/" + std::to_string(currentFileTotal_) + ")";
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, statusText.c_str());

    float progress = 0;
    if (fileTotal_ > 0) {
      progress = static_cast<float>(fileProgress_) / static_cast<float>(fileTotal_);
    }

    // Same layout as OTA: bar, percent (from drawProgressBar), then size line.
    int y = centerY + metrics.verticalSpacing;
    GUI.drawProgressBar(
        renderer,
        Rect{metrics.contentSidePadding, y, pageWidth - metrics.contentSidePadding * 2, metrics.progressBarHeight},
        static_cast<int>(progress * 100), 100);
    y += metrics.progressBarHeight + metrics.verticalSpacing;
    y += lineHeight + metrics.verticalSpacing;
    renderer.drawCenteredText(UI_10_FONT_ID, y, (formatSize(fileProgress_) + " / " + formatSize(fileTotal_)).c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state_ == COMPLETE) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_FONT_INSTALLED), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state_ == ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, tr(STR_FONT_INSTALL_FAILED), true,
                              EpdFontFamily::BOLD);
    if (!errorMessage_.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + metrics.verticalSpacing, errorMessage_.c_str());
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
