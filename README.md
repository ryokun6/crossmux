# ryOS CrossMux

ryOS CrossMux is reading-first firmware for the Xteink X3 and X4. It is a fork of
[CrossMux](https://github.com/0x1abin/crossmux), built on
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).

This fork focuses on Chinese and CJK books: vertical EPUB layout, broader font
coverage, reliable SD-card fonts, and faster 4-level grayscale text. It keeps
reading stats, WeRead, and standby faces. The old game and toy apps are not part
of the firmware.

Current firmware version: **1.4.4**

![ryOS CrossMux running on an Xteink device](./docs/images/cover.jpg)

## What this fork adds

### Vertical CJK EPUB reading

Choose `Writing Mode > Vertical (RTL)` in Reader Settings or the in-book menu.
The layout engine then:

- lays out columns from right to left
- uses vertical presentation forms for CJK punctuation
- stacks repeated ellipses and dashes one character cell at a time
- rotates Latin runs and keeps short numeric references readable
- applies tate-chu-yoko layout to compact horizontal runs
- moves paragraph spacing and block margins along the column axis
- reverses page controls to follow the reading direction

Vertical mode activates only for EPUBs whose language metadata is tagged
Chinese, Japanese, or Korean. Other books stay horizontal even if the global
setting is vertical.

### Chinese firmware

Two Chinese SKUs ship alongside international:

| Env | Locale | UI | OTA asset |
| --- | --- | --- | --- |
| `gh_release_tc` | `zh-TW` | Traditional (繁體中文) | `firmware-tc.bin` |
| `gh_release_sc` | `zh-CN` | Simplified (简体中文, from Traditional YAML via OpenCC t2s) | `firmware-sc.bin` |

Both include English + Chinese UI strings, CJK line-breaking, WeRead, dual-slot
OTA from `ryokun6/crossmux`, and embedded CJK bitmap fonts (GenSen TW for TC;
Source Han Sans CN for SC). TC remaps Simplified EPUB codepoints → Traditional
glyphs; SC remaps Traditional → Simplified.

### Better SD-card fonts

`.cpfont` families can live in either `/.fonts/` or `/fonts/` on the SD card.
The loader indexes large CJK families on demand, prewarms upcoming page glyphs,
and falls back to the regular style when a CJK bold or italic glyph is absent.

The repo also includes an EB Garamond plus Source Han Serif TC builder. Its
current character set covers base CJK ideographs, compatibility ideographs,
Hiragana, Katakana, Greek, EPUB symbols, and a small book-derived supplement.
See [SD-card fonts](./docs/sd-card-fonts.md).

### Faster grayscale text

Text anti-aliasing uses the display's four grayscale levels. The fork renders
the two grayscale planes in narrow strips and writes them directly to the
display, instead of keeping two extra full-screen buffers in RAM. Fast glyph
blitting and strip rejection reduce repeated work on pages with SD fonts and
vertical columns.

### Cache and image handling

Section cache rebuilds write to a `.tmp` sidecar before replacing the active
cache. If an SD card refuses to truncate or rename a stale section file, the
reader can keep using the completed sidecar instead of failing the chapter
rebuild.

The `Large only` image mode now drops inline icons, em-sized separators, and
small standalone images while preserving full figures.

### A smaller Apps menu

The firmware ships only reading-related apps:

- OPDS Browser
- Reading Stats, including history, heatmap, profile, and achievements
- WeRead in the Chinese build
- Standby faces, including Sloppy Clock and AirPage, plus Chinese Calendar in
  the Chinese build

Sudoku, Gomoku, Minesweeper, 2048, Chinese Chess, Game of Life, and the avatar
generator are intentionally excluded.

## Reader features

ryOS CrossMux keeps the main CrossPoint reader:

- EPUB 2 and EPUB 3 rendering
- chapter navigation, footnotes, bookmarks, and go-to-percent
- embedded styles, images, kerning, hyphenation, and focus reading
- auto page turn, orientation control, screenshots, and QR display
- KOReader progress sync
- `.epub`, `.txt`, `.xtc`, `.xtch`, and `.bmp` files
- recent books, folder browsing, cache management, and long-press delete
- installable SD-card font families with regular, bold, italic, and bold-italic
  styles
- international UI translations and RTL interface support

Wireless tools include file transfer, the EPUB Optimizer, web settings, fast
WebSocket uploads, WebDAV, Calibre wireless connection, OPDS browsing, and
network OTA from the latest `ryokun6/crossmux` GitHub release. OTA selects
`firmware.bin`, `firmware-tc.bin`, or `firmware-sc.bin` to match the installed build. Firmware can
also be installed through USB, the web flasher, or `SD Card Firmware Update`.

## X3 and X4 support

One firmware image runs on both devices. It detects the hardware at boot and
adapts the panel size, controls, battery source, and available peripherals.

- X4: 800 x 480 SSD1677 display
- X3: 792 x 528 UC81xx display, DS3231 clock, fuel gauge, and tilt page turn

There is no separate X3 build. Build either language variant and flash the same
`firmware.bin` to the matching target in the web flasher. See
[device variants](./docs/engineering/device-variants.md) for the detection and
recovery details.

## Before flashing

> **USB-locked device warning**
>
> The Xteink Unlocker officially supports CrossPoint and CrossInk. ryOS
> CrossMux is a community fork. Flashing it to a USB-locked device can leave the
> device permanently stuck without a supported recovery path. Do not install
> this fork on a locked unit unless you already have a verified way to recover
> it.

Units bought directly from xteink.com are normally not USB-locked. If a browser
cannot see the serial device, try another data-capable cable, USB port, and
Chromium-based browser before assuming the device is locked.

## Build and install

### Requirements

- [pioarduino](https://github.com/pioarduino/pioarduino) or its VS Code plugin
- Python 3.8 or newer
- Git with submodule support
- a data-capable USB-C cable

### Clone

```bash
git clone --recursive https://github.com/ryokun6/crossmux.git
cd crossmux
```

If the repo was cloned without submodules:

```bash
git submodule update --init --recursive
```

### Build

```bash
# International firmware
pio run -e gh_release

# Chinese firmware
pio run -e gh_release_tc
```

Build outputs:

```text
.pio/build/gh_release/firmware.bin
.pio/build/gh_release_tc/firmware.bin
```

### Flash over USB

```bash
# International
pio run -e gh_release -t upload

# Chinese
pio run -e gh_release_tc -t upload
```

You can also open the
[CrossPoint web flasher](https://crosspointreader.com/#flash-tools), select the
physical device, choose `Custom .bin`, and upload the matching build output.
The target selector patches the image for the device bootloader. It does not
select a different firmware build.

To return to official firmware, flash an official
[CrossPoint release](https://github.com/crosspoint-reader/crosspoint-reader/releases).

## Install custom fonts

You can install `.cpfont` files without rebuilding the firmware:

1. On the device, open `Settings > System > Manage Fonts`.
2. Or upload fonts through the file-transfer web interface.
3. Or copy a family directory to `/.fonts/FamilyName/` or
   `/fonts/FamilyName/` on the SD card.
4. Select the family under `Settings > Reader > Reader Font Family`.

The hidden `/.fonts/` directory wins if the same family exists in both font
roots. Conversion commands, Unicode presets, and the CJK font builder are
documented in [docs/sd-card-fonts.md](./docs/sd-card-fonts.md).

## Known limits

- The built-in Chinese font is best at the default 14 pt Medium size. The 12 pt
  Small font has a smaller ideograph set, while Large and Extra Large are
  intended mainly for English books.
- Built-in CJK text has one weight. Install an SD-card family for distinct bold
  and italic Latin styles.
- Vertical mode depends on correct `zh`, `ja`, or `ko` EPUB language metadata.
- The desktop simulator currently models X4 geometry only. X3 display and
  peripheral testing needs real hardware.

## Development

Useful checks before opening a pull request:

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
pio run -e gh_release_tc
```

The ESP32-C3 has about 380 KB of usable RAM. Reader caches live on the SD card
under `/.crosspoint/`, and code changes should avoid adding persistent heap
pressure.

Start here:

- [User guide](./USER_GUIDE.md)
- [Development guide](./docs/contributing/README.md)
- [Architecture](./docs/contributing/architecture.md)
- [Chinese build](./docs/engineering/chinese-build.md)
- [Cache management](./docs/engineering/cache-management.md)
- [Binary file formats](./docs/file-formats.md)
- [Web server](./docs/webserver.md)
- [Desktop and WebAssembly simulator](./simulator/README.md)

## Credits and license

ryOS CrossMux builds on work from
[CrossMux](https://github.com/0x1abin/crossmux),
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader),
and [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader).

The project is not affiliated with Xteink or any device manufacturer.

Licensed under the [MIT License](./LICENSE).
