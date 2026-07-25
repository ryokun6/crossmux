#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class OtaUpdater {
 public:
  using ProgressCallback = void (*)(void* ctx);

  // The language builds published per release. Each is a separate `firmware*.bin`
  // asset with its own embedded fonts and i18n table, not a runtime option, so
  // moving between them means flashing a different image. The X3/X4 split is
  // *not* here — one image runs on both boards (see docs/engineering/device-variants.md).
  enum class Sku : uint8_t {
    INTERNATIONAL,
    TRADITIONAL_CHINESE,
    SIMPLIFIED_CHINESE,
    JAPANESE,
    KOREAN,
  };
  static constexpr size_t SKU_COUNT = 5;

  // Release asset file name for a SKU, e.g. "firmware-tc.bin".
  static const char* skuAssetName(Sku sku);
  // The SKU this binary was built as, from the compile-time language flags.
  static Sku runningSku();

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
    IMAGE_TOO_LARGE,  // asset is bigger than this device's OTA app slot
  };

  // Kept in sync with ReleaseJsonParser::DIGEST_SIZE by a static_assert in the
  // .cpp; duplicated here so the JSON parser header stays out of every TU that
  // only needs the activity-facing API.
  static constexpr size_t DIGEST_SIZE = 32;

  // What a release publishes for one SKU. `url` empty means the release has no
  // asset for that SKU, which is normal for older tags predating a language.
  struct SkuAsset {
    std::string url;
    size_t size = 0;
    uint8_t digest[DIGEST_SIZE] = {};
    bool digestValid = false;

    bool present() const { return !url.empty(); }
  };

  const SkuAsset& getSkuAsset(Sku sku) const { return skuAssets[static_cast<size_t>(sku)]; }

  // Repoints the install at `sku` and drops the "is it newer" gate, because both
  // things this enables are same-version by nature: reinstalling the build already
  // running, and moving to another language build of the same release. Returns OK,
  // NO_UPDATE when the release publishes no asset for that SKU, or IMAGE_TOO_LARGE
  // when it cannot fit the app slot; on failure nothing is changed.
  OtaUpdaterError selectSkuForInstall(Sku sku);

  // Puts the target back to the running SKU's asset and re-arms the version gate.
  // Called when the user backs out of the build list, so an abandoned switch can
  // never end up behind the plain "Update" button.
  void resetInstallTarget();

  // Bytes available in the app slot the next flash would write, 0 if unknown.
  static size_t getOtaSlotSize();

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
  static void flashProgressTrampoline(size_t written, size_t total, void* ctx);
  // Records one asset from the release JSON into skuAssets when its name matches a
  // known SKU, ignoring everything else the release ships. Takes plain types so
  // the parser header stays out of this one.
  void recordAsset(const char* name, const char* url, size_t size, const uint8_t* digest, bool digestValid);
  void adoptTarget(const SkuAsset& asset);
  OtaUpdaterError verifyStagedDigest(const char* path) const;

  // One entry per Sku, filled while the release JSON streams past. Holding the
  // URLs as std::string costs ~5 short heap blocks instead of the 2.5 KB a
  // char[SKU_COUNT][512] table would always occupy; the alternative of
  // reconstructing URLs from the tag would hardcode GitHub's download path shape
  // when the response already hands us the real one.
  SkuAsset skuAssets[SKU_COUNT];

  bool updateAvailable = false;
  // Set only by selectSkuForInstall(): an explicit user choice of what to write,
  // which is what distinguishes a deliberate reinstall from "there is nothing new".
  bool userSelectedInstall = false;
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
