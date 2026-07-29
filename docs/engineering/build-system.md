# Build System & Build Flags

> Deep reference for [AGENTS.md](../../AGENTS.md). Covers PlatformIO usage, the
> build environments, the critical build flags that change firmware behavior, and
> personal local overrides.

## Build System: PlatformIO

**PlatformIO is BOTH a VS Code extension AND a CLI tool**:

1. **VS Code Extension** (Recommended):
   * Extension ID: `platformio.platformio-ide` (see `.vscode/extensions.json`)
   * Provides: Toolbar buttons, IntelliSense, integrated build/upload/monitor
   * Configuration: `.vscode/c_cpp_properties.json`, `.vscode/tasks.json`
   * Usage: Click Build (✓), Upload (→), or Monitor (🔌) buttons

2. **CLI Tool** (`pio` command):
   * **Installation**: Python package (typically `pip install platformio`)
   * **Windows Location**: `C:\Users\<user>\AppData\Local\Programs\Python\Python3xx\Scripts\pio.exe`
   * **Verify**: `which pio` (Git Bash) or `where.exe pio` (cmd)
   * **Usage**: `pio run`, `pio run -t upload`, etc.

**Configuration Files**:
* `platformio.ini`: Main build configuration (committed to git)
* `platformio.local.ini`: Local overrides (gitignored, create if needed)
* `partitions.csv`: ESP32 flash partition layout

**`custom_sdkconfig` (mbedTLS / TLS):** `[base]` sets asymmetric TLS record
buffers to 16KB in / 4KB out and turns on `CONFIG_MBEDTLS_DYNAMIC_BUFFER`, so
mbedTLS allocates RX/TX per record and frees cert state after the handshake
instead of pinning ~32KB for the whole session — without it an X3 font download
has ~5KB `MaxAlloc` mid-transfer and cannot allocate a 2KB read buffer.
Every HTTPS client attaches the CA bundle **except** GitHub's asset-serving CDN
hosts — `release-assets.githubusercontent.com` and
`objects.githubusercontent.com` — which `shouldAttachCrtBundle()` in
`src/network/HttpDownloader.cpp` exempts by exact host match (it parses the
authority so a crafted path or userinfo cannot spoof it).
Verification there was re-tested on X3 after `CONFIG_MBEDTLS_DYNAMIC_BUFFER` and
`KEEP_PEER_CERTIFICATE=n` landed — the theory being that those freed enough heap
to afford it — and it still fails: `release-assets.githubusercontent.com` returns
`PK verify failed with error 0x4290` on every attempt, i.e.
`RSA_PUBLIC_FAILED` + `MPI_ALLOC_FAILED`, an allocation failure during the RSA
verify rather than a trust failure (the same log line confirms `Certificate
matched`). Attempts
start at `Free=46968 MaxAlloc=32756` and the handshake drives the heap to
`MinFree=1824 MaxAlloc=9716`, against an idle post-boot ceiling of
`MaxAlloc=34804` — so this is a heap-size wall, not fragmentation, and no
`MIN_MAX_ALLOC_FOR_TLS` floor can gate around it.

The cause is chain-specific, which is why the exemption is scoped to those two
hosts. The asset hosts serve RSA-2048 leaf ← RSA-2048 intermediate ←
**RSA-4096** root. The C3's RSA accelerator caps at 3072 bits
(`SOC_RSA_MAX_BIT_LEN` in `soc_caps.h`), so `CONFIG_MBEDTLS_LARGE_KEY_SOFTWARE_MPI`
routes the 4096-bit verify through software modexp, needing one contiguous
~2564-byte MPI block against `MinFree=1824`. RSA-2048 needs ~1284 bytes and fits.
`github.com` **and `api.github.com` are not exempt and must keep the bundle**:
they serve an all-EC chain (P-256 leaf ← P-256 Sectigo E36 ← P-384 Sectigo Root
E46) that verifies cheaply — the measured `hop=0 github.com verify OK, 302` is
that chain passing. Do not widen the exemption back to a substring match on
`github.com` (which also matches `api.github.com`), and do not remove it without
new evidence. It is host-based and applies on **both**
X3 and X4 by decision, not by omission: one image serves both variants via
runtime detection, so gating TLS trust per device would mean a security posture
that silently differs by board plus a verify path that cannot be exercised on the
other board.

`CONFIG_ESP_TLS_INSECURE` / `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY` are
therefore load-bearing: with no CA attached, esp-tls falls back to
`MBEDTLS_SSL_VERIFY_NONE` instead of failing setup. Fonts still CRC32-check, so
the exemption costs authenticity, not integrity. OTA restores authenticity
out-of-band: `OtaUpdater` no longer uses `esp_https_ota_*` (which rejects a
config with no CA outright unless `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP` is set, and
it is not, so on X3 it necessarily hit the same `0x4290`). It stages the asset to
SD through `HttpDownloader::downloadToFile()`, then verifies the staged file's
SHA-256 against the per-asset `digest` published by `api.github.com` over a
CA-verified connection, and refuses to flash on a missing or mismatched digest.
Two constraints the same block created, documented at
the top of `src/network/HttpDownloader.cpp`:
`CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE=n` makes `mbedtls_ssl_get_peer_cert()`
return NULL, which Arduino's `verify_ssl_dn()` dereferences unchecked, so
`WiFiClientSecure::verify()` / `setFingerprint()` must not be adopted; and
`CONFIG_MBEDTLS_DYNAMIC_FREE_CA_CERT` nulls `conf->ca_chain` after each
handshake, so building a fresh client per hop/request is load-bearing and
connection pooling is foreclosed. Changing these lines rebuilds the
Arduino-ESP32 prebuilt libs on the next `pio run` (slow once, then cached).
`custom_component_remove` drops RainMaker/Insights so that hybrid rebuild does
not require their embedded server certs.

### Choosing a language build over OTA

Settings > Check for updates can install **any** of the five language assets
(`firmware.bin`, `firmware-{tc,sc,ja,ko}.bin`), not only the one matching the
running SKU, and can reinstall the version already running. Both reach the same
staged-download-then-verify path; only the target asset and the version gate
differ.

- **Asset discovery.** `ReleaseJsonParser` still latches the running SKU's asset
  (that drives the ordinary "something newer is out" flow, unchanged), and now
  also fires `setAssetCallback` once per asset in the array with that asset's
  name, URL, size and digest together. `OtaUpdater::recordAsset` keeps the five
  it recognizes in `skuAssets[]`. The callback exists so the parser does not have
  to hold five 512-byte URL buffers it mostly has no use for; it hands out its
  own scratch and the consumer copies what it wants.
- **Digest binding.** Because name, URL and digest arrive in one call describing
  one asset object, the digest verified in `verifyStagedDigest()` is always the
  digest published for the file that was downloaded.
  `selectSkuForInstall()` copies URL and digest from the same slot, so switching
  target can never pair one image with another's hash. The parser also clears its
  digest scratch on asset-object *entry* as well as on commit.
- **Reinstall.** `installUpdate()` skips `isUpdateNewer()` only when
  `selectSkuForInstall()` has run, which happens exclusively from the build-list
  confirmation screen. A same-version install therefore takes a deliberate
  Confirm → pick build → Install, and no path reaches a forced flash from a
  single press.
- **Slot fit.** `firmware_flash::nextSlotSize()` reports the destination app
  slot; both `selectSkuForInstall()` and `installUpdate()` reject an asset larger
  than it and surface `IMAGE_TOO_LARGE`. This matters going from a small SKU to a
  large one — `firmware-tc.bin` is ~6.26 MB against a 6.25 MiB (6 553 600 B)
  slot, so the margin is tens of KB, and discovering it after the download is a
  wasted 6 MB transfer.
- **What a switch changes on the card.** Nothing is deleted. `settings.json`
  survives; its `langSku` marker no longer matches, so the UI language resets to
  the new build's default while every other setting is kept (see
  [../i18n.md](../i18n.md)). Section caches invalidate on their own because
  `SECTION_FILE_VERSION` is per-flavor (see
  [cache-management.md](cache-management.md)), so books repaginate on next open;
  `progress.bin` holds spine index plus page number and is untouched, so the
  chapter is exact and the page within it can shift.

**`scripts/copy_upstream_mbedtls.py` — required for any `CONFIG_MBEDTLS_*` to
work.** pioarduino's hybrid rebuild copies `build/esp-idf/<component>/lib<component>.a`
into `framework-arduinoespressif32-libs`, walking only one directory deep. The
mbedTLS *submodule* targets build a level lower
(`build/esp-idf/mbedtls/mbedtls/library/lib{mbedtls,mbedcrypto,mbedx509}.a`,
plus everest/p256-m) and were silently left stale, so options that change
upstream mbedTLS code had no effect while `libesp-tls.a` *was* rebuilt against
them — the two halves disagree, which shows up either as a bogus TLS memory
profile or as an undefined-reference link error. This script copies the missing
archives (upstream `libmbedtls.a` is packaged as `libmbedtls_2.a` to avoid
colliding with the component archive) and fails the build if they are absent.
It must stay a `pre:` script: it hooks `checkprogsize`, and SCons runs
post-actions in registration order, so registering before the platform does is
what gets it in ahead of the platform's own copy-then-`rmtree`.

## Build Environment
* **Standard**: C++20 (`-std=c++2a`). No Exceptions, No RTTI.
* **Logging**: ALWAYS use `LOG_INF`, `LOG_DBG`, or `LOG_ERR` from `Logging.h`. Raw Serial output is deprecated.
* **Environments** (in `platformio.ini`):
  * `default`: Development (LOG_LEVEL=2, serial enabled)
  * `gh_release`: Production (LOG_LEVEL=0)
  * `gh_release_rc`: Release candidate (LOG_LEVEL=1)
  * `slim`: Minimal build (no serial logging)
  * `gh_release_tc`: Traditional Chinese (zh-TW) release with GenSen TW CJK fonts (see [chinese-build.md](chinese-build.md))
  * `gh_release_sc`: Simplified Chinese (zh-CN) release with GenSen TW fonts (SC-keyed) + `CHINESE_UI_SIMPLIFIED`
  * `gh_release_ja`: Japanese (ja-JP) release with GenSen Rounded 2 JP fonts (see [japanese-korean-build.md](japanese-korean-build.md))
  * `gh_release_ko`: Korean (ko-KR) release with Resource Han Rounded KR fonts (see [japanese-korean-build.md](japanese-korean-build.md))

## Critical Build Flags
These flags in `platformio.ini` fundamentally affect firmware behavior:

```cpp
-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1  // Single framebuffer (saves 48KB RAM!)
-DFREEINK_SSD1677_CONFIG=crossPointX4Ssd1677Config  // CrossPoint's X4 waveform tuning
-DARDUINO_USB_MODE=1                 // Enable USB CDC
-DARDUINO_USB_CDC_ON_BOOT=1          // Serial available immediately at boot
-DXML_CONTEXT_BYTES=1024             // XML parser memory limit (EPUB parsing)
-DUSE_UTF8_LONG_NAMES=1              // SD card long filename support
-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1   // Avoid zlib name conflicts
-DXML_GE=0                           // Disable XML general entities (security)
-DDESTRUCTOR_CLOSES_FILE=1           // FsFile destructor auto-closes (SdFat)
```

**DESTRUCTOR_CLOSES_FILE implications**:
- SdFat's `FsBaseFile` destructor calls `close()` automatically when the object goes out of scope
- **Do NOT add explicit `file.close()` calls** for local `FsFile` variables — the destructor handles it
- Explicit `close()` is still required in these cases:
  1. **Close before delete**: Must close before `Storage.remove()` on the same path
  2. **Close before reopen**: Must close before reopening the same `FsFile` variable (e.g., write then reopen for read, or rewrite the same path)
  3. **Member variables**: `FsFile` members persist beyond any single function scope, so close at the intended release point (e.g., in `onExit()`)

**SINGLE_BUFFER_MODE implications**:
- Only ONE framebuffer exists (not double-buffered)
- Grayscale rendering requires temporary buffer allocation (`renderer.storeBwBuffer()`)
- Must call `renderer.restoreBwBuffer()` to free temporary buffers
- See [lib/GfxRenderer/GfxRenderer.cpp:439-440](../../lib/GfxRenderer/GfxRenderer.cpp) for malloc usage

**X4 SSD1677 tuning implications**:
- `crossPointX4Ssd1677Config()` is defined in `lib/hal/HalDisplay.cpp`, where the
  X4 display bus is also raised from the SDK profile's conservative 5 MHz to the
  SSD1677's in-spec 20 MHz limit before display initialization.
- FAST refreshes use the driver's incremental DU sequence (`0x1C`) instead of
  the SDK default absolute sequence (`0xFC`). This reduces BUSY time but may show
  more ghosting on panel samples that need the stronger stock waveform.
- X3 is runtime-selected before display initialization and uses its UC81xx
  driver and SPI configuration unchanged. Do not use this flag to introduce
  compile-time X3/X4 firmware variants.
- If hardware verification finds unacceptable persistent ghosting, keep the
  20 MHz bus setting and remove this config flag/override to restore `0xFC`.

---

## Local Development Configuration

### platformio.local.ini (Personal Overrides)

**Purpose**: Personal development settings that should NEVER be committed.

**Use Cases**:
- Serial port configuration (varies by machine)
- Debug flags for specific testing
- Local build optimizations
- Developer-specific paths

**Example** `platformio.local.ini`:
```ini
# platformio.local.ini (gitignored)
[env:default]
upload_port = COM7              # Windows: COMx, Linux: /dev/ttyUSBx
monitor_port = COM7

build_flags =
  ${base.build_flags}
  -DMY_DEBUG_FLAG=1             # Personal debug flags
  -DTEST_FEATURE_ENABLED=1
```

**Configuration Hierarchy**:
1. `platformio.ini` - **Committed**, shared project settings
2. `platformio.local.ini` - **Gitignored**, personal overrides
3. Local file extends/overrides base config

**Rules**:
- **NEVER commit** `platformio.local.ini`
- **NEVER put** personal info (serial ports, credentials) in main `platformio.ini`
- Use `${base.build_flags}` to extend (not replace) base flags

See also: [getting-started](../contributing/getting-started.md) for first-time toolchain setup, [testing-and-debugging.md](testing-and-debugging.md) for build/monitor commands.
