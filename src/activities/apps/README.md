# Apps

The `apps/` directory holds non-reader sub-applications. They share the home-screen "Apps" tile, `AppsMenuActivity`, and the conventions below.

Shipped Apps menu entries:

- `weread/` — WeRead offline / QR login (Chinese builds only; `ENABLE_CHINESE_VERSION`, not on Emscripten)
- `reading-stats/` — reading statistics
- `standby/` — standby / clock screen
- ryOS Books — shortcut to the core book catalog browser (OPDS); opens via `goToBrowser()`

Games and toys (sudoku, gomoku, minesweeper, 2048, cellular, avatar, chinese-chess) are **not** included in firmware. Their `AppId` bit positions remain reserved in `hiddenAppsMask` for settings compatibility.

Reader, file browser, settings, etc. are core features under `activities/<feature>/`, not under `apps/`. OPDS browser code lives under `activities/browser/` / `activities/settings/`; the Apps menu only exposes a launcher row.

---

## Directory layout

```
apps/
├── AppsMenuActivity.{h,cpp}   # dispatcher — see "Adding a new app"
├── reading-stats/
├── standby/
└── weread/                    # gated by ENABLE_CHINESE_VERSION (excluded on Emscripten)
```

**File naming** — keep the app-name prefix (`ReadingStatsMenuActivity.cpp`). Class names stay globally unique without namespaces.

---

## Adding a new app

The dispatcher is table-driven. A new app needs the app files, a navigation method, and one catalog entry:

### 1. Create the app

```
apps/<myapp>/
  MyAppActivity.{h,cpp}   # extends Activity
```

Back must return to the Apps menu:

```cpp
if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
  activityManager.goToApps();
}
```

### 2. Add `goToMyApp()` in `ActivityManager.{h,cpp}`

### 3. Assign an ID and append one row to `kAppEntries`

In `apps/AppsMenuActivity.cpp`:

```cpp
enum class AppId : uint8_t {
  ReadingStats = 0,
  WeRead = 1,
  // 2–7 reserved (upstream game IDs; not catalogued)
  Standby = 8,
  OpdsBrowser = 9,
  MyApp = 10,  // new, never-reused bit ID
  Count = 11,
};

constexpr AppEntry kAppEntries[] = {
#if defined(ENABLE_CHINESE_VERSION) && !defined(__EMSCRIPTEN__)
    {AppId::WeRead,        StrId::STR_WEREAD_TITLE,  UIIcon::WeRead,  &ActivityManager::goToWeRead},
#endif
    {AppId::ReadingStats,  StrId::STR_READING_STATS, UIIcon::Library, &ActivityManager::goToReadingStatsMenu},
    {AppId::Standby,       StrId::STR_STANDBY_TITLE, UIIcon::Standby, &ActivityManager::goToStandby},
    {AppId::OpdsBrowser,   StrId::STR_OPDS_BROWSER,  UIIcon::Library, &ActivityManager::goToBrowser},
    {AppId::MyApp,         StrId::STR_MYAPP_TITLE,   UIIcon::MyApp,   &ActivityManager::goToMyApp},
};
```

The ID is the persisted bit position in `hiddenAppsMask`: allocate the next unused value, never reuse or change existing values, and keep conditional-app IDs outside their `#ifdef`. New bits default to visible. The menu, launcher, and App Visibility settings all read this same table.

### 4. Add i18n key + `UIIcon` + Lyra `iconForName` case

- **i18n**: add `STR_MYAPP_TITLE` to `lib/I18n/translations/english.yaml`, then regenerate i18n.
- **Icon**: add `MyApp` to `UIIcon` in `src/components/themes/BaseTheme.h`, a 32×32 bitmap under `src/components/icons/`, and a `case UIIcon::MyApp:` in Lyra `iconForName`.

### 5. Conditional apps (e.g. WeRead)

Use `#if defined(ENABLE_CHINESE_VERSION) && !defined(__EMSCRIPTEN__)` (or the appropriate flag) at every reference site, plus `build_src_filter` in `platformio.ini`:

- base: `-<activities/apps/<app>/>`
- gated env: `+<activities/apps/<app>/>`

Do not add inner ifdefs inside the app's own sources.

---

## UI conventions

- **Renderer**: the Apps menu uses `GUI.drawButtonMenu`, not `GUI.drawList` (32px icons, UI_12, home-tile style). Inter-row gap is `metrics.menuSpacing / 2`.
- **Pagination**: page slice via offset callbacks + `drawPaginationDots` above the button hints.
- **Header**: each app draws its own header via `GUI.drawHeader(... tr(STR_<APP>_TITLE))`.
- **Hints**: `STR_BACK / STR_SELECT / STR_DIR_UP / STR_DIR_DOWN` for menu rows.

## Navigation flow

```
Home  ──Confirm "Apps"──▶  AppsMenu  ──Confirm row──▶  <App>
  ▲                            │
  └──────Back──────────────────┘    ◀──Back──  <App>
```

Every sub-app's Back button must call `activityManager.goToApps()`.

## Resource budget

Same 380KB RAM ceiling as the reader: alloc in `onEnter()`, free in `onExit()`; no large stack buffers; debounce SPIFFS writes.
