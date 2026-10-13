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

## Tool initialization

Use pioarduino 6.2.0 with the pinned ESP32 platform. Before building with a new
PlatformIO tool directory, run from the repository root:

```bash
python -m pip install pioarduino==6.2.0
python scripts/patch_pioarduino_cache.py --prepare-platform
```

The platform's IDF installer otherwise replaces the Core's running SCons
package, causing missing Python modules during a clean build. Preparation
removes SCons from that installer's tool list; Core still supplies it. It does
not change compiler optimization flags. CI performs the same preparation.
Nightly and CI use a separate `PLATFORMIO_CORE_DIR` per build job so concurrent
runner jobs cannot change each other's installed toolchains. The directory name
includes `.platformio`: pioarduino's framework-restoration path check requires
that component when switching from a custom SDK back to prebuilt TinyUSB.
On Ubuntu, the bundled cppcheck executable also requires `libpcre3`.

The custom SDK bootstrap uses non-LTO objects; the final application retains
its existing LTO configuration. Keep the verified C3 optimization options and
partition layout unchanged; sufficient Flash headroom is not a reason to tune
additional compiler options.

X4 Pro, X4 Classic, Murphy M4, Waveshare 3.97 and PaperMono use application LTO (`-flto=auto`, removing the packaged `-fno-lto`)
to keep the complete reader inside the existing 6.25 MiB OTA partitions. Their
prebuilt `dio_opi` core continues to supply the TinyUSB CDC/MSC component graph.

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

**X4 SSD1677 display implications**:
- The application does not override display-driver configuration. The SDK's
  active X4 board profile selects the SSD1677 and its in-spec 20 MHz SPI clock.
- Refresh waveforms come from the SDK's active board config. X4 FAST refreshes
  use the stock absolute sequence (`0xFC`), which includes the temperature and
  power sequencing needed to avoid the persistent ghosting seen with the
  weaker incremental `0x1C` path.
- X3 is runtime-selected before display initialization and uses its UC81xx
  driver and SPI configuration unchanged. X4 Pro probes its SSD1677/UC81xx
  controller once before display initialization. Sticky retains its
  board-specific SSD1677 waveform config.

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

### Fixed local integration builds (2026-10-02)

The SDK/Simulator/Reader pins are `98b4e427`, `20e73803`, and `38280863`.
Use the real source exports recorded by `sync-upstream start --local-rehearsal`;
record their complete Git tree fingerprints with validation artifacts. The SDK
fork includes all six ReadPico fixes through `e3550ec`. Production dependency
commits remain unchanged during rehearsal; the ignored local PlatformIO config
selects reviewed exports. No commit, push, PR or flashing is part of this stage.

Validate `default`, `gh_release`, `readpico`, `readpico_nightly`, and `metalio_eink4`,
and all six existing simulator environments. Only the explicit ReadPico profiles
use high-density metrics. Keep host tests, whole-page visuals, Flash/static RAM,
and physical-device acceptance as separate results.

The integrated SD catalog retains at most 48KiB of row/container data and checks
32KiB free-heap plus 4KiB contiguous headroom before growth. Installed discovery
is bounded to 32 entries, reuses root lookup instead of keeping every seen name,
and rejects excess picker/history growth with a log. Manifest/title/description
inputs are bounded; JSON rows accept 768-byte fields, 64 bundle files and 4KiB
total captured text, with 32 nesting/path segments and 128-byte keys. Browse
responses remain on SD (up to 1MiB), while API responses retain the upstream
48KiB cap and fallible Arduino String reserve. TLS is released before downloads
and on exit. These limits protect C3 shared code; simulator heap numbers are
synthetic and do not verify hardware runtime headroom.
