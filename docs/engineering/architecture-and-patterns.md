# Architecture & Common Patterns

> Deep reference for [CLAUDE.md](../../CLAUDE.md). For the big-picture runtime
> lifecycle, activity model, and dataflow diagrams, see
> [../contributing/architecture.md](../contributing/architecture.md). This file
> covers the agent-facing details: directory layout, the HAL, and the reusable
> code patterns (singletons, activity lifecycle, FreeRTOS tasks, fonts).

## Directory Structure
* lib/: Internal libraries (Epub engine, GfxRenderer, UITheme, I18n)
  * lib/hal/: Hardware Abstraction Layer (HalDisplay, HalGPIO, HalStorage)
  * lib/I18n/: Internationalization (translations in `translations/*.yaml`, generated string tables)
* src/activities/: UI logic using the Activity Lifecycle (onEnter, loop, onExit)
* open-x4-sdk/: Low-level SDK (EInkDisplay, InputManager, BatteryMonitor, SDCardManager)
* .crosspoint/: SD-based binary cache for EPUB metadata and pre-rendered layout sections

## Hardware Abstraction Layer (HAL)

**CRITICAL**: Always use HAL classes, NOT SDK classes directly.

| HAL Class | Wraps SDK Class | Purpose | Singleton Macro |
|-----------|----------------|---------|-----------------|
| `HalDisplay` | `EInkDisplay` | E-ink display control | *(none)* |
| `HalGPIO` | `InputManager` | Button input handling | *(none)* |
| `HalStorage` | `SDCardManager` | SD card file I/O | `Storage` |

**Location**: [lib/hal/](../../lib/hal/)

**Why HAL?**
- Provides consistent error logging per module
- Abstracts SDK implementation details
- Centralizes resource management

**Example - HalStorage**:
```cpp
#include <HalStorage.h>

// Use Storage singleton (defined via macro)
FsFile file;
if (Storage.openFileForRead("MODULE", "/path/to/file.bin", file)) {
  // Read from file
  // No file.close() needed — DESTRUCTOR_CLOSES_FILE=1 handles it at scope exit
}
```

**Usage**: See example above. Uses `FsFile` (SdFat), NOT Arduino `File`. Do NOT add `file.close()` for local variables (see [build-system.md](build-system.md) → DESTRUCTOR_CLOSES_FILE).

---

## Common Patterns

### Singleton Access
**Available Singletons**:
```cpp
#define SETTINGS CrossPointSettings::getInstance()  // User settings
#define APP_STATE CrossPointState::getInstance()    // Runtime state
#define GUI UITheme::getInstance()                   // Current theme
#define Storage HalStorage::getInstance()            // SD card I/O
#define I18N I18n::getInstance()                     // Internationalization
```

### Activity Lifecycle and Memory Management

**Source**: [src/main.cpp:132-143](../../src/main.cpp)

**CRITICAL**: Activities are **heap-allocated** and **deleted on exit**.

```cpp
// main.cpp navigation pattern
void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;  // Activity deleted here!
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  currentActivity = activity;  // Heap-allocated activity
  currentActivity->onEnter();
}
```

**Memory Implications**:
- Activity navigation = `delete` old activity + `new` create next activity
- Any memory allocated in `onEnter()` MUST be freed in `onExit()`
- FreeRTOS tasks MUST be deleted in `onExit()` before activity destruction
- Member `FsFile` handles MUST be closed in `onExit()` (local `FsFile` variables auto-close via destructor)

**Activity Pattern**:
```cpp
void onEnter()  { Activity::onEnter(); /* alloc: buffer, tasks */ render(); }
void loop()     { mappedInput.update(); /* handle input */ }
void onExit()   { /* free: vTaskDelete, free buffer, close member FsFiles */ Activity::onExit(); }
```

**Critical**: Free resources in reverse order. Delete tasks BEFORE activity destruction.

### FreeRTOS Task Guidelines

**Source**: [src/activities/util/KeyboardEntryActivity.cpp:45-50](../../src/activities/util/KeyboardEntryActivity.cpp)

**Pattern**: See Activity Lifecycle above. `xTaskCreate(&taskTrampoline, "Name", stackSize, this, 1, &handle)`

**Stack Sizing** (in BYTES, not words):
- **2048**: Simple rendering (most activities)
- **4096**: Network, EPUB parsing
- Monitor: `uxTaskGetStackHighWaterMark()` if crashes

**Rules**: Always `vTaskDelete()` in `onExit()` before destruction. Use mutex if shared state.

### Global Font Loading

**Source**: [src/main.cpp:40-115](../../src/main.cpp)

**All fonts are loaded as global static objects** at firmware startup:
- Noto Serif: 12, 14, 16, 18pt (4 styles each: regular, bold, italic, bold-italic)
- Noto Sans: 12, 14, 16, 18pt (4 styles each)
- OpenDyslexic: 8, 10, 12, 14pt (4 styles each)
- Ubuntu UI fonts: 10, 12pt (2 styles)

**Total**: ~80+ global `EpdFont` and `EpdFontFamily` objects

**Compilation Flag**:
```cpp
#ifndef OMIT_FONTS
  // Most fonts loaded here
#endif
```

**Implications**:
- Fonts stored in **Flash** (marked as `static const` in `lib/EpdFont/builtinFonts/`)
- Font rendering data cached in **DRAM** when first used
- `OMIT_FONTS` can reduce binary size for minimal builds
- Font IDs defined in [src/fontIds.h](../../src/fontIds.h)

**Usage**:
```cpp
#include "fontIds.h"

renderer.insertFont(FONT_UI_MEDIUM, ui12FontFamily);
renderer.drawText(FONT_UI_MEDIUM, x, y, "Hello", true);
```
