# ryOS CrossMux Development Guide

Project: Open-source e-reader firmware for Xteink X4 (ESP32-C3)
Mission: Provide a lightweight, high-performance reading experience focused on EPUB rendering on constrained hardware.

> **This file is a map, not a manual.** It holds the identity, the
> non-negotiable invariants, and a quick reference — then points to the deep
> engineering docs in [`docs/engineering/`](docs/engineering/index.md). Read the
> linked doc that matches your task instead of carrying everything in context.
> The detailed docs are the system of record; keep them current when behavior
> changes.
>
> Links below are relative to the repository root.
>
> **Maintainers** — when merging upstream changes to this file, follow
> [docs/engineering/upstream-merge-policy.md](docs/engineering/upstream-merge-policy.md):
> keep this map thin; route deep content into `docs/engineering/`.

## AI Agent Identity and Cognitive Rules
* Role: Senior Embedded Systems Engineer (ESP-IDF/Arduino-ESP32 specialized).
* Primary Constraint: 380KB RAM is the hard ceiling. Stability is non-negotiable.
* Evidence-Based Reasoning: Before proposing a change, you MUST cite the specific file path and line numbers that justify the modification.
* Anti-Hallucination: Do not assume the existence of libraries or ESP-IDF functions. If you are unsure of an API's availability for the ESP32-C3 RISC-V target, check the open-x4-sdk or official docs first.
* No Unfounded Claims: Do not claim performance gains or memory savings without explaining the technical mechanism (e.g., DRAM vs IRAM usage).
* Resource Justification: You must justify any new heap allocation (new, malloc, std::vector) or explain why a stack/static alternative was rejected.
* Verification: After suggesting a fix, instruct the user on how to verify it (e.g., monitoring heap via Serial or checking a specific cache file).

## Golden Rules — Non-Negotiable Invariants

These are the highest-frequency-violation rules. Each links to the doc with the
full reasoning, examples, and edge cases.

1. **380KB RAM is the ceiling.** Justify every heap allocation; prefer stack/static; `.reserve()` before `push_back` loops; mark constants `constexpr`. → [hardware-constraints.md](docs/engineering/hardware-constraints.md)
2. **Never bare `new`.** With `-fno-exceptions` a failed `new` calls `abort()`, not `nullptr`. Use `makeUniqueNoThrow<T>()` from `lib/Memory/Memory.h` (or `new (std::nothrow)` only when a C API takes ownership); always null-check and `LOG_ERR` on OOM. → [memory-and-allocation.md](docs/engineering/memory-and-allocation.md)
3. **All user-facing text uses `tr()`.** Never hardcode UI strings (logs may be hardcoded). → [ui-and-input.md](docs/engineering/ui-and-input.md)
4. **Use HAL classes, never the SDK directly** (`Storage`, `HalDisplay`, `HalGPIO`). → [architecture-and-patterns.md](docs/engineering/architecture-and-patterns.md)
5. **No `file.close()` on local `FsFile`** — `DESTRUCTOR_CLOSES_FILE=1` handles scope exit (close member files in `onExit()`, and before delete/reopen). → [build-system.md](docs/engineering/build-system.md)
6. **`memcpy` for unaligned reads.** RISC-V faults on raw `reinterpret_cast` of `uint8_t*` to wider types. ISRs need `IRAM_ATTR`; never call a mutex from an ISR. → [esp32-pitfalls.md](docs/engineering/esp32-pitfalls.md)
7. **Use `MappedInputManager::Button::*` logical buttons**, never raw `HalGPIO::BTN_*` (except in ButtonRemapActivity). → [ui-and-input.md](docs/engineering/ui-and-input.md)
8. **All rendering through the `GUI`/UITheme macro.** Never hardcode 800/480 — use `renderer.getScreenWidth()/getScreenHeight()`. → [ui-and-input.md](docs/engineering/ui-and-input.md)
9. **Free in `onExit()` what you alloc in `onEnter()`.** `vTaskDelete()` tasks before activity destruction; activities are heap-allocated and deleted on exit. → [architecture-and-patterns.md](docs/engineering/architecture-and-patterns.md)
10. **Bump the cache format version BEFORE changing a binary layout** (`book.bin`, `section.bin`); document it in `docs/file-formats.md`. → [cache-management.md](docs/engineering/cache-management.md)
11. **Edit sources, not generated files** (`*.generated.h`, `I18n*` generated headers). → [generated-files.md](docs/engineering/generated-files.md)
12. **Verify repo context before any git op; ask before committing**; never stage `.gitignore`d files. → [git-workflow.md](docs/engineering/git-workflow.md)

## Quick Reference

**Platform detection** (run once per session): `uname -s` → `MINGW64_NT-*` (Windows Git Bash) / `Linux` / `Darwin` (macOS).

**Singletons**:
```cpp
#define SETTINGS CrossPointSettings::getInstance()  // User settings
#define APP_STATE CrossPointState::getInstance()    // Runtime state
#define GUI UITheme::getInstance()                   // Current theme
#define Storage HalStorage::getInstance()            // SD card I/O
#define I18N I18n::getInstance()                     // Internationalization
```

**Core commands**:
```bash
pio run                              # Build (default env)
pio run -t upload                    # Build + flash
pio run -e gh_release_tc             # Traditional Chinese (zh-TW)
pio run -e gh_release_sc             # Simplified Chinese (zh-CN)
pio run -e gh_release_ja             # Japanese (ja-JP)
pio run -e gh_release_ko             # Korean (ko-KR)
pio check                            # Static analysis (cppcheck)
./bin/clang-format-fix               # Format (CI uses clang-format 21+)
python3 scripts/debugging_monitor.py # Enhanced serial monitor
```

**Desktop simulator** (from the repo root; configure/build SKUs sequentially
because i18n generation uses shared files):
```bash
cmake -S simulator -B simulator/build_intl -DSIMULATOR_INTERNATIONAL_VERSION=ON
cmake --build simulator/build_intl -j2
cmake -S simulator -B simulator/build_tc
cmake --build simulator/build_tc -j2
cmake -S simulator -B simulator/build_sc -DSIMULATOR_CHINESE_UI_SIMPLIFIED=ON
cmake --build simulator/build_sc -j2
cmake -S simulator -B simulator/build_ja -DSIMULATOR_JAPANESE_VERSION=ON
cmake --build simulator/build_ja -j2
cmake -S simulator -B simulator/build_ko -DSIMULATOR_KOREAN_VERSION=ON
cmake --build simulator/build_ko -j2
```

Run at explicit 1× scale:
```bash
SKU=tc  # intl, tc, sc, ja, or ko
nohup ./simulator/build_${SKU}/crosspoint_simulator --scale 1 \
  --sd-root ./simulator/sd_root >/tmp/crosspoint-simulator-${SKU}.log 2>&1 &
```
Use a background shell or a separate terminal application so the simulator
stays alive; never attach it to a transient foreground command. Run only one
simulator at a time against a given `sd_root`. See
[`simulator/README.md`](simulator/README.md) for prerequisites and controls.

## The Map — where to look next

| Topic | Read this when… | Doc |
|---|---|---|
| Hardware & RAM budget | Allocations, strings/vectors, SPIFFS writes, the Resource Protocol | [docs/engineering/hardware-constraints.md](docs/engineering/hardware-constraints.md) |
| Memory & allocation | `new`/`malloc`/`makeUniqueNoThrow`, smart pointers, RAII | [docs/engineering/memory-and-allocation.md](docs/engineering/memory-and-allocation.md) |
| ESP32-C3 pitfalls | ISRs/IRAM, `string_view`, alignment, templates, JSON | [docs/engineering/esp32-pitfalls.md](docs/engineering/esp32-pitfalls.md) |
| Build system & flags | PlatformIO, build envs, critical flags, local overrides | [docs/engineering/build-system.md](docs/engineering/build-system.md) |
| Architecture & patterns | HAL, singletons, activity lifecycle, FreeRTOS, fonts | [docs/engineering/architecture-and-patterns.md](docs/engineering/architecture-and-patterns.md) |
| Coding standards | Naming, header guards, error handling | [docs/engineering/coding-standards.md](docs/engineering/coding-standards.md) |
| UI, orientation & input | Rendering, button mapping, UITheme, `tr()` | [docs/engineering/ui-and-input.md](docs/engineering/ui-and-input.md) |
| Generated files | HTML, i18n, fonts produced by build scripts | [docs/engineering/generated-files.md](docs/engineering/generated-files.md) |
| Testing & debugging | Build/monitor commands, crash playbook, verification, CI | [docs/engineering/testing-and-debugging.md](docs/engineering/testing-and-debugging.md) |
| Git workflow | Repo detection, branching, commits | [docs/engineering/git-workflow.md](docs/engineering/git-workflow.md) |
| Cache management | Cache structure, invalidation, format versioning | [docs/engineering/cache-management.md](docs/engineering/cache-management.md) |
| Chinese builds | `gh_release_tc` (zh-TW) / `gh_release_sc` (zh-CN), embedded CJK fonts | [docs/engineering/chinese-build.md](docs/engineering/chinese-build.md) |
| Japanese / Korean builds | `gh_release_ja` / `gh_release_ko`, GenSen JP + Resource Han Rounded KR | [docs/engineering/japanese-korean-build.md](docs/engineering/japanese-korean-build.md) |
| Device variants (X3/X4) | Building/flashing for X3, runtime device detection (one binary), per-device hardware differences | [docs/engineering/device-variants.md](docs/engineering/device-variants.md) |
| System overview & dataflow | Runtime lifecycle, activity model, pipeline diagrams | [docs/contributing/architecture.md](docs/contributing/architecture.md) |
| Binary file formats | Byte-level cache/notes/font formats | [docs/file-formats.md](docs/file-formats.md) |
| i18n system | Translation workflow in depth | [docs/i18n.md](docs/i18n.md) |
| Web server API | HTTP/WebSocket endpoints | [docs/webserver-endpoints.md](docs/webserver-endpoints.md) |
| Scope & governance | Whether a feature belongs in the project | [SCOPE.md](SCOPE.md), [GOVERNANCE.md](GOVERNANCE.md) |

---

Philosophy: We are building a dedicated e-reader, not a Swiss Army knife. If a feature adds RAM pressure without significantly improving the reading experience, it is Out of Scope.

## Cursor Cloud specific instructions

This is embedded ESP32-C3 firmware: the cloud VM has **no device**, so you can
build/lint/static-analyze the firmware and run the **native host** unit tests,
but you cannot flash or serial-monitor (`pio run -t upload`, `pio device
monitor`, `scripts/debugging_monitor.py` need hardware). Device-side runtime
behavior (real e-ink refresh, hardware buttons, heap under load) still needs a
physical X3/X4. For UI flows that do not require hardware, use the **desktop
simulator** (see Quick Reference above and
[`simulator/README.md`](simulator/README.md)).

### Computer-use testing with the simulator

When driving the simulator via computer use (GUI automation):

- **Use the keyboard to navigate — do not click.** Mouse clicks on the eink
  area or the device-shell button bumps do **not** deliver button input to the
  firmware under computer use. Always send key events instead.
- Map keys to logical buttons (full table in
  [`simulator/README.md`](simulator/README.md)): Arrow keys for cursor /
  page, Enter for confirm/select, Backspace for back/cancel, Esc for power
  (exits the sim — avoid unless intentional), F1 for the host settings
  overlay.
- Focus the simulator window first (one click to focus is fine), then drive
  navigation exclusively with keyboard events.
- Launch at explicit 1× scale in a background shell; only one simulator per
  `sd_root` at a time.

The startup update script already refreshes the moving parts (checks out the
`open-x4-sdk` submodule, installs PlatformIO Core, installs `requirements.txt`).
System toolchain deps are baked into the VM image, not the update script:
`cmake`, `ninja-build`, `python3-venv` (PlatformIO's internal `penv` fails to
build without it), the `gcc-14`/`libstdc++-14-dev` host compiler (the native
gtest suite links `-lstdc++`), and `clang-format-21`.

The four CI-equivalent checks (see `.github/workflows/ci.yml`) all pass headless:

- Build firmware: `pio run` → `.pio/build/default/firmware.bin`. Standard build
  envs and flags are in `docs/engineering/build-system.md`.
- Static analysis: `pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high`.
- Native unit tests: `cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j`
  (or `pio run -t unit-tests`).
- Formatting: `bin/clang-format-fix` **requires clang-format 21 first on PATH** —
  run `PATH="/usr/lib/llvm-21/bin:$PATH" ./bin/clang-format-fix`; the bare
  `clang-format` on the image is not v21.

Non-obvious: PlatformIO installs to `~/.local/bin` (already on the default login
PATH). The first `pio run` downloads the ESP32 toolchain + libraries into
`~/.platformio` (a few minutes); subsequent builds are fast.
