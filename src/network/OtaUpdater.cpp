#include "OtaUpdater.h"

// clang-format off
// HttpDownloader.h pulls Arduino/SdFat, whose macros collide with lwip's
// ip4_addr.h unless seen before esp_http_client (which includes lwip). Pin this
// order; clang-format would otherwise sort the local header last and break the
// build.
#include "HttpDownloader.h"
#include <Logging.h>
#include <ReleaseJsonParser.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
// clang-format on

#include <WiFi.h>

#include <cstring>
#include <string>

#include "FirmwareFlasher.h"

namespace {
constexpr char latestReleaseUrl[] = "https://api.github.com/repos/ryokun6/crossmux/releases/latest";
constexpr char firmwareAssetName[] =
#ifdef ENABLE_CHINESE_VERSION
#ifdef CHINESE_UI_SIMPLIFIED
    "firmware-sc.bin";
#else
    "firmware-tc.bin";
#endif
#elif defined(ENABLE_JAPANESE_VERSION)
    "firmware-ja.bin";
#elif defined(ENABLE_KOREAN_VERSION)
    "firmware-ko.bin";
#else
    "firmware.bin";
#endif

// The staged image lands on SD, not in a spare partition: partitions.csv has no
// free room (dual 6.25 MiB app slots plus the rest fill the 16 MiB map), and the
// largest SKU image is already 93% of one app slot.
constexpr char stagedFirmwarePath[] = "/firmware_ota.tmp";

// SHA-256 read buffer. 512 bytes matches the SD sector size (so SdFat serves each
// read straight from its existing sector cache with no extra buffering) and lives
// on the caller's stack — the alternative, a heap block, would be claimed at the
// worst moment for the arena, right after a multi-MB TLS transfer.
constexpr size_t HASH_CHUNK = 512;

// Removes the staged download on every exit path — mismatch, cancel, flash
// failure, or success. A 6 MB orphan on the user's card is a real cost.
struct StagedFileCleanup {
  ~StagedFileCleanup() {
    if (Storage.exists(stagedFirmwarePath)) {
      Storage.remove(stagedFirmwarePath);
    }
  }
};
}  // namespace

OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() {
  LOG_DBG("OTA", "Checking for update (current: %s)", CROSSPOINT_VERSION);

  // Stream the ~32KB release JSON straight into the parser as it arrives.
  // Buffering the whole body in a std::string would add a growing allocation
  // on top of the TLS session's heap during the fetch; with -fno-exceptions an
  // OOM there aborts. fetchUrl handles the verified-https GET, redirects, and
  // User-Agent (see HttpDownloader).
  ReleaseJsonParser releaseParser(firmwareAssetName);
  if (HttpDownloader::fetchUrl(latestReleaseUrl, [&releaseParser](const uint8_t* data, size_t len) {
        releaseParser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      }) != HttpDownloader::OK) {
    LOG_ERR("OTA", "Release check fetch failed");
    return HTTP_ERROR;
  }

  LOG_DBG("OTA", "Parser results: tag=%s firmware=%s", releaseParser.foundTag() ? "yes" : "no",
          releaseParser.foundFirmware() ? "yes" : "no");

  if (!releaseParser.foundTag()) {
    LOG_ERR("OTA", "No tag_name in release JSON");
    return JSON_PARSE_ERROR;
  }

  if (!releaseParser.foundFirmware()) {
    LOG_ERR("OTA", "No %s asset found", firmwareAssetName);
    return NO_UPDATE;
  }

  latestVersion = releaseParser.getTagName();
  otaUrl = releaseParser.getFirmwareUrl();
  otaSize = releaseParser.getFirmwareSize();
  totalSize = otaSize;
  updateAvailable = true;

  // The digest arrives over the CA-verified api.github.com connection this
  // function just used, which is what makes it an authenticity anchor for the
  // asset fetch (that hop skips CA verify — see shouldAttachCrtBundle in
  // HttpDownloader.cpp). Absence is recorded, not tolerated: installUpdate()
  // refuses to flash without one.
  static_assert(DIGEST_SIZE == ReleaseJsonParser::DIGEST_SIZE, "digest buffer size must match the parser");
  otaDigestValid = releaseParser.foundFirmwareDigest();
  if (otaDigestValid) {
    memcpy(otaDigest, releaseParser.getFirmwareDigest(), sizeof(otaDigest));
  } else {
    memset(otaDigest, 0, sizeof(otaDigest));
  }

  LOG_DBG("OTA", "Found update: tag=%s size=%zu digest=%s", latestVersion.c_str(), otaSize,
          otaDigestValid ? "yes" : "no");
  LOG_DBG("OTA", "Firmware URL: %s", otaUrl.c_str());
  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == CROSSPOINT_VERSION) {
    return false;
  }

  int currentMajor, currentMinor, currentPatch;
  int latestMajor, latestMinor, latestPatch;

  const auto currentVersion = CROSSPOINT_VERSION;

  // semantic version check (only match on 3 segments)
  sscanf(latestVersion.c_str(), "%d.%d.%d", &latestMajor, &latestMinor, &latestPatch);
  sscanf(currentVersion, "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

  /*
   * Compare major versions.
   * If they differ, return true if latest major version greater than current major version
   * otherwise return false.
   */
  if (latestMajor != currentMajor) return latestMajor > currentMajor;

  /*
   * Compare minor versions.
   * If they differ, return true if latest minor version greater than current minor version
   * otherwise return false.
   */
  if (latestMinor != currentMinor) return latestMinor > currentMinor;

  /*
   * Check patch versions.
   */
  if (latestPatch != currentPatch) return latestPatch > currentPatch;

  // If we reach here, it means all segments are equal.
  // One final check, if we're on an RC build (contains "-rc"), we should consider the latest version as newer even if
  // the segments are equal, since RC builds are pre-release versions.
  if (strstr(currentVersion, "-rc") != nullptr) {
    return true;
  }

  return false;
}

const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }

void OtaUpdater::flashProgressTrampoline(size_t written, size_t total, void* ctx) {
  auto* self = static_cast<OtaUpdater*>(ctx);
  self->processedSize = written;
  self->totalSize = total;
  if (self->progressCb) self->progressCb(self->progressCtx);
}

// Streams the staged file through mbedtls_sha256 and compares against the digest
// the release API published. mbedtls_sha256 is the same primitive the structural
// image check uses (FirmwareFlasher.cpp:129-132); the chunked read mirrors the
// font CRC32 loop (FontDownloadActivity.cpp:394-409) so a 6 MB file never needs
// to be resident.
OtaUpdater::OtaUpdaterError OtaUpdater::verifyStagedDigest(const char* path) const {
  if (!otaDigestValid) {
    // Fail closed. Degrading to "no verification" would defeat the point of
    // staging: the asset hop runs without CA verification, so the digest is the
    // only thing tying these bytes to the release.
    LOG_ERR("OTA", "No sha256 digest in release JSON — refusing to flash");
    return DIGEST_MISSING;
  }

  HalFile file;
  if (!Storage.openFileForRead("OTA", path, file) || !file) {
    LOG_ERR("OTA", "verify: cannot open staged file");
    return STORAGE_ERROR;
  }

  const size_t fileSize = file.fileSize();
  if (otaSize > 0 && fileSize != otaSize) {
    LOG_ERR("OTA", "verify: size %u != expected %u", static_cast<unsigned>(fileSize), static_cast<unsigned>(otaSize));
    file.close();
    return DIGEST_MISMATCH;
  }

  uint8_t buf[HASH_CHUNK];
  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, /*is224=*/0);
  size_t remaining = fileSize;
  while (remaining > 0) {
    const size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
    const int got = file.read(buf, want);
    if (got <= 0 || static_cast<size_t>(got) != want) {
      LOG_ERR("OTA", "verify: short read with %u bytes left", static_cast<unsigned>(remaining));
      mbedtls_sha256_free(&shaCtx);
      file.close();
      return STORAGE_ERROR;
    }
    mbedtls_sha256_update(&shaCtx, buf, want);
    remaining -= want;
  }

  uint8_t computed[DIGEST_SIZE];
  mbedtls_sha256_finish(&shaCtx, computed);
  mbedtls_sha256_free(&shaCtx);
  file.close();

  if (memcmp(computed, otaDigest, sizeof(computed)) != 0) {
    LOG_ERR("OTA", "verify: sha256 mismatch (expected %02x%02x…, got %02x%02x…)", otaDigest[0], otaDigest[1],
            computed[0], computed[1]);
    return DIGEST_MISMATCH;
  }
  LOG_INF("OTA", "verify: sha256 OK (%u bytes)", static_cast<unsigned>(fileSize));
  return OK;
}

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate(ProgressCallback onProgress, void* ctx, StageCallback onStage) {
  if (!isUpdateNewer()) {
    return UPDATE_OLDER_ERROR;
  }

  // Why SD staging instead of esp_https_ota: esp_https_ota_begin() rejects a
  // config with no CA attached (ESP_ERR_INVALID_ARG from
  // is_server_verification_enabled) unless CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP is
  // set, and it is not — so on X3 it necessarily verifies the asset CDN's
  // RSA-4096 root and dies with 0x4290 MPI_ALLOC_FAILED (see
  // shouldAttachCrtBundle in HttpDownloader.cpp for the measurements).
  // HttpDownloader's downloadToFile is the X3-proven path and is already tuned
  // for this exact hazard, so the download goes there and authenticity is
  // restored out-of-band: the sha256 digest below came from a CA-verified
  // api.github.com fetch, and the bytes cannot be substituted without breaking
  // it. Both boards take this identical path; X4 gains the digest cross-check.
  //
  // CONFIG_SECURE_BOOT is not enabled (the sdkconfig has only
  // SECURE_BOOT_V2_RSA_SUPPORTED/_PREFERRED, no CONFIG_SECURE_SIGNED_APPS_*), so
  // the image's own checksum + SHA256 trailer — what flashFromSdPath validates —
  // proves only that the file is internally consistent, never who produced it.
  // That is defense-in-depth on top of the digest, not a substitute for it.

  // Checked here as well as in verifyStagedDigest so a release without a usable
  // digest costs the user nothing instead of a ~6 MB download that can only be
  // thrown away.
  if (!otaDigestValid) {
    LOG_ERR("OTA", "No sha256 digest for %s — refusing to download unverifiable firmware", otaUrl.c_str());
    return DIGEST_MISSING;
  }

  progressCb = onProgress;
  progressCtx = ctx;
  StagedFileCleanup cleanup;

  /* For better timing and connectivity, we disable power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_NONE);

  // Same X3 MaxAlloc pressure as HttpDownloader: free scan debris, then log the
  // contiguous block size that decides whether the mbedTLS buffers fit.
  WiFi.scanDelete();
  LOG_INF("OTA", "Install prep Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(ESP.getMaxAllocHeap()));

  if (onStage) onStage(ctx, Stage::DOWNLOADING);
  processedSize = 0;
  totalSize = otaSize;

  // Report on whole-percent change only. Firing per chunk wakes the render task,
  // whose framebuffer work contends with TLS on the same internal arena, and
  // e-ink cannot repaint faster than a percent tick anyway.
  int lastReportedPct = -1;
  const auto downloadResult = HttpDownloader::downloadToFile(
      otaUrl, stagedFirmwarePath, [this, &lastReportedPct](size_t downloaded, size_t total) {
        processedSize = downloaded;
        if (total > 0) totalSize = total;
        if (!progressCb || totalSize == 0) return;
        const int pct = static_cast<int>(static_cast<uint64_t>(downloaded) * 100 / totalSize);
        if (pct == lastReportedPct) return;
        lastReportedPct = pct;
        progressCb(progressCtx);
      });

  /* Return back to default power saving for WiFi in case of failing */
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (downloadResult != HttpDownloader::OK) {
    LOG_ERR("OTA", "staging download failed: %d (Free=%u MaxAlloc=%u)", downloadResult,
            static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
    // FILE_ERROR is a short SD write, which in practice means the card ran out
    // of room for a ~6 MB image. HalStorage exposes no free-space query, so the
    // failed write *is* the pre-flight check. downloadToFile already deleted the
    // partial file; the cleanup guard covers the rest.
    return downloadResult == HttpDownloader::FILE_ERROR ? STORAGE_ERROR : HTTP_ERROR;
  }

  if (onStage) onStage(ctx, Stage::VERIFYING);
  const OtaUpdaterError verifyResult = verifyStagedDigest(stagedFirmwarePath);
  if (verifyResult != OK) {
    return verifyResult;  // staged file removed by the cleanup guard
  }

  // Wi-Fi down before the erase/write loop: it holds LWIP/mbedTLS debris that
  // the flash path has no use for, and the partition writes are long enough that
  // a live radio is pure risk.
  WiFi.disconnect(true);
  delay(30);
  WiFi.mode(WIFI_OFF);
  LOG_INF("OTA", "Pre-flash Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(ESP.getMaxAllocHeap()));

  if (onStage) onStage(ctx, Stage::FLASHING);
  processedSize = 0;
  // alreadyValidated=false: keep the structural pass (header, segment table, XOR
  // checksum, SHA256 trailer) as defense-in-depth even though the digest already
  // matched — it is cheap next to the write and catches an SD read that goes bad
  // between hashing and flashing.
  const auto flashResult =
      firmware_flash::flashFromSdPath(stagedFirmwarePath, &OtaUpdater::flashProgressTrampoline, this,
                                      /*alreadyValidated=*/false);
  if (flashResult != firmware_flash::Result::OK) {
    LOG_ERR("OTA", "flash failed: %s", firmware_flash::resultName(flashResult));
    return FLASH_ERROR;
  }

  LOG_INF("OTA", "Update completed");
  return OK;
}
