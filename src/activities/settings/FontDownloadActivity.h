#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <string>
#include <vector>

#include "FontInstaller.h"
#include "SdCardFont.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// JSON schema version of the fonts.json manifest. The canonical version for
// the build tooling lives in lib/EpdFont/scripts/cpfont_version.py. This
// firmware-side copy must be bumped manually when the firmware is updated to
// support a new manifest schema.
#define FONTS_MANIFEST_VERSION 1

#ifndef FONT_MANIFEST_URL
// Manifest + .cpfont assets are published by .github/workflows/release-fonts.yml
// to this repo under the "sd-fonts-m<META>-b<BIN>" tag. The tag pattern must
// stay in sync with the workflow; it derives its version numbers from
// lib/EpdFont/scripts/cpfont_version.py.
#define FONT_MANIFEST_URL_STRINGIFY_INNER(x) #x
#define FONT_MANIFEST_URL_STRINGIFY(x) FONT_MANIFEST_URL_STRINGIFY_INNER(x)
#define FONT_MANIFEST_URL                                                                         \
  "https://github.com/ryokun6/crossmux/releases/download/sd-fonts-m" FONT_MANIFEST_URL_STRINGIFY( \
      FONTS_MANIFEST_VERSION) "-b" FONT_MANIFEST_URL_STRINGIFY(CPFONT_VERSION) "/fonts.json"
#endif

class FontDownloadActivity : public Activity {
 public:
  // Manage: settings entry. PromptThenManage: first-boot Chinese font flash preload.
  // resumedAfterDefrag: silent-restart resume path — skip the MaxAlloc defrag
  // reboot (already done) and auto-reconnect Wi‑Fi before fetching fonts.json.
  enum class Purpose : uint8_t { Manage, PromptThenManage };

  explicit FontDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                Purpose purpose = Purpose::Manage, bool resumedAfterDefrag = false);

#ifdef ENABLE_CHINESE_VERSION
  static bool wasChineseFontPromptShownThisBoot();
#endif

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override {
<<<<<<< HEAD
    return state_ == LOADING_MANIFEST || state_ == DOWNLOADING || state_ == COMPLETE || state_ == ERROR;
=======
    return state_ == LOADING_MANIFEST || state_ == DOWNLOADING ||
           // The download is synchronous and blocks the main loop until it
           // completes, so activityManager.preventAutoSleep() is never polled
           // during downloading.
           state_ == COMPLETE || state_ == ERROR;
>>>>>>> upstream/master
  }
  bool skipLoopDelay() override { return true; }

 private:
  enum State {
    WIFI_SELECTION,
    LOADING_MANIFEST,
    FAMILY_LIST,
    DOWNLOADING,
    COMPLETE,
    ERROR,
  };

  enum class DownloadJob : uint8_t {
    None,
    OneFamily,
    AllMissing,
    AllUpdates,
  };

  struct ManifestFile {
    std::string name;
    size_t size = 0;
    uint32_t crc32 = 0;
  };

  struct ManifestFamily {
    std::string name;
    std::string description;
    std::vector<std::string> styles;
    std::vector<ManifestFile> files;
    size_t totalSize = 0;
    bool installed = false;
    bool hasUpdate = false;
  };

  State state_ = WIFI_SELECTION;
  Purpose purpose_;
  FontInstaller fontInstaller_;
  ButtonNavigator buttonNavigator_;
  bool resumedAfterDefrag_ = false;

  // Manifest data
  std::string baseUrl_;
  std::vector<ManifestFamily> families_;
  int selectedIndex_ = 0;

  // Download progress
  size_t currentFileIndex_ = 0;
  size_t currentFileTotal_ = 0;
  size_t fileProgress_ = 0;
  size_t fileTotal_ = 0;
  int downloadingFamilyIndex_ = 0;
  std::string errorMessage_;
  bool cancelRequested_ = false;

  // Download runs on a worker task so loop() can poll Cancel while HTTP is
  // blocked. 8KB stack: TLS + HttpDownloader + SD write (same ballpark as
  // other network fetch tasks; 4KB was too tight for github→CDN).
  TaskHandle_t downloadTask_ = nullptr;
  std::atomic<bool> downloadTaskRunning_{false};
  DownloadJob downloadJob_ = DownloadJob::None;

  void startWifiSelection();
  void onWifiSelectionComplete(bool success);
  bool fetchAndParseManifest();
  void downloadFamily(ManifestFamily& family);
  void downloadAll();
  void updateAll();
  void startDownloadJob(DownloadJob job);
  void stopDownloadTask();
  static void downloadTaskTrampoline(void* arg);
  void runDownloadJob();
  static bool computeFileCrc32(const char* path, uint32_t& outCrc);
  bool showDownloadAllRow() const;
  bool showUpdateAllRow() const;
  int specialRowCount() const;
  bool isDownloadAllRow(int index) const;
  bool isUpdateAllRow(int index) const;
  bool isSelectedFamilyDeletable() const;
  void promptDeleteSelectedFamily();
  void onDeleteConfirmationResult(const ActivityResult& result);
  int familyIndexFromList(int listIndex) const { return listIndex - specialRowCount(); }
  int listItemCount() const;
  size_t totalDownloadSize() const;
  size_t totalUpdateSize() const;
  static std::string formatSize(size_t bytes);
};
