#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class OtaUpdater {
 public:
  using ProgressCallback = void (*)(void* ctx);

  // The install path has three long phases with very different characteristics:
  // a multi-MB HTTPS download to SD, a SHA-256 pass over the staged file, and
  // the partition erase/write. The UI needs to distinguish them (hashing 6 MB is
  // not instant and would otherwise look hung), so installUpdate() reports
  // transitions through StageCallback.
  enum class Stage : uint8_t {
    DOWNLOADING,
    VERIFYING,
    FLASHING,
  };
  using StageCallback = void (*)(void* ctx, Stage stage);

  enum OtaUpdaterError {
    OK = 0,
    NO_UPDATE,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    UPDATE_OLDER_ERROR,
    INTERNAL_UPDATE_ERROR,
    OOM_ERROR,
    DIGEST_MISSING,   // release JSON carried no usable sha256 digest — fail closed
    DIGEST_MISMATCH,  // staged file does not hash to the published digest
    STORAGE_ERROR,    // SD write failed / ran out of space while staging
    FLASH_ERROR,      // staged image failed validation or the partition write
  };

  size_t getOtaSize() const { return otaSize; }

  size_t getProcessedSize() const { return processedSize; }

  size_t getTotalSize() const { return totalSize; }

  OtaUpdater() = default;
  bool isUpdateNewer() const;
  const std::string& getLatestVersion() const;
  OtaUpdaterError checkForUpdate();
  // Downloads the release asset to SD, verifies it against the digest published
  // by the release API, then flashes it via firmware_flash::flashFromSdPath.
  // The staged file is removed on every exit path. Caller restarts on OK.
  OtaUpdaterError installUpdate(ProgressCallback onProgress = nullptr, void* ctx = nullptr,
                                StageCallback onStage = nullptr);

 private:
  // Kept in sync with ReleaseJsonParser::DIGEST_SIZE by a static_assert in the
  // .cpp; duplicated here so the JSON parser header stays out of every TU that
  // only needs the activity-facing API.
  static constexpr size_t DIGEST_SIZE = 32;

  static void flashProgressTrampoline(size_t written, size_t total, void* ctx);
  OtaUpdaterError verifyStagedDigest(const char* path) const;

  bool updateAvailable = false;
  std::string latestVersion;
  std::string otaUrl;
  size_t otaSize = 0;
  size_t processedSize = 0;
  size_t totalSize = 0;
  // Expected SHA-256 of the asset, from the release JSON. Fixed-size, no heap.
  uint8_t otaDigest[DIGEST_SIZE] = {};
  bool otaDigestValid = false;

  // Live only for the duration of installUpdate(), so the C-style flash
  // progress trampoline can reach the caller's callback.
  ProgressCallback progressCb = nullptr;
  void* progressCtx = nullptr;
};
