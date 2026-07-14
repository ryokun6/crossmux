# SD Card Fonts

ryOS CrossMux supports loading additional fonts from the SD card, including fonts
with extended Unicode coverage (CJK, Cyrillic, Greek, etc.).

## Installing Fonts

There are three ways to install fonts:

### Option 1: Download from device (recommended)

1. Connect your ryOS CrossMux reader to Wi-Fi
2. Go to **Settings > System > Manage Fonts**
3. Browse available font families and tap to download
4. Downloaded fonts appear immediately in **Settings > Reader > Font Family**

### Option 2: Upload via web browser

1. Start **File Transfer** and connect through **Join Network** or **Create Hotspot**
2. Open the web interface URL shown on the reader
3. Navigate to the **Fonts** tab
4. Upload `.cpfont` files using the upload form

### Option 3: Manual SD card copy

1. Download font files from the
   [crosspoint-fonts repository](https://github.com/crosspoint-reader/crosspoint-fonts)
2. Copy font family folders to one of two locations on your SD card:

   - `/.fonts/` — hidden directory (preferred; keeps the SD root tidy
     when mounted on a desktop)
   - `/fonts/` — visible directory (use this if your OS hides dot-files
     and you'd rather see the folder in your file manager)

   Both roots are always scanned at boot and the results are merged: a
   family installed in `/fonts/` shows up even when `/.fonts/` also
   exists, and vice versa. The two roots only collide if the same family
   name appears in both — in that case the copy in `/.fonts/` wins and
   the duplicate in `/fonts/` is ignored.

       SD Card Root/
       ├── .fonts/                     ← Hidden root (preferred)
       │   └── Literata/
       │       ├── Literata_12.cpfont
       │       ├── Literata_14.cpfont
       │       ├── Literata_16.cpfont
       │       └── Literata_18.cpfont
       └── fonts/                      ← Visible root (equally valid)
           └── Merriweather/
               ├── Merriweather_12.cpfont
               └── ...

3. Insert the SD card and power on your ryOS CrossMux reader

## Available Pre-Built Fonts

The current list of pre-built fonts is maintained in the
[crosspoint-fonts repository](https://github.com/crosspoint-reader/crosspoint-fonts).

## Converting Custom Fonts

To convert your own TrueType/OpenType fonts:

### Prerequisites

    pip install freetype-py fonttools

### Single font (one style)

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py \
      MyFont-Regular.ttf \
      --intervals latin-ext \
      --sizes 12,14,16,18 \
      --style regular \
      --name MyFont \
      --output-dir ./MyFont/

### Multi-style font

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py \
      --regular MyFont-Regular.ttf \
      --bold MyFont-Bold.ttf \
      --italic MyFont-Italic.ttf \
      --bolditalic MyFont-BoldItalic.ttf \
      --intervals latin-ext \
      --sizes 12,14,16,18 \
      --name MyFont \
      --output-dir ./MyFont/

### Available Unicode interval presets

| Preset | Coverage |
|--------|----------|
| `ascii` | U+0020–U+007E (Basic Latin) |
| `latin1` | U+0080–U+00FF (Latin-1 Supplement) |
| `latin-ext` | European languages (Latin + Extended-A/B + punctuation + ligatures) |
| `greek` | Greek + Extended Greek |
| `cyrillic` | Cyrillic + Supplement |
| `hebrew` | Hebrew + Alphabetic Presentation Forms |
| `georgian` | Georgian + Georgian Supplement |
| `armenian` | Armenian |
| `ethiopic` | Ethiopic + Extended |
| `vietnamese` | Vietnamese subset (ơ/ư and combining marks) |
| `punctuation` | General punctuation (U+2000–U+206F) |
| `cjk` | CJK Unified Ideographs + Hiragana + Katakana + Fullwidth |
| `hangul` | Korean Hangul syllables + Jamo + Compatibility Jamo |
| `cherokee` | Cherokee (historic + supplement block) |
| `tifinagh` | Tifinagh |
| `symbols` | Math, currency, arrows, box-drawing, misc symbols, dingbats |
| `reading` | Literary fiction coverage: Latin, Greek, Cyrillic, math/symbol blocks, supplemental punctuation, and CJK quote marks |
| `builtin` | Matches the firmware's built-in font conversion intervals |

Combine presets with commas: `--intervals latin-ext,greek,cyrillic`

You can also specify arbitrary Unicode ranges directly:
`--intervals latin-ext,(0x2100-0x214F)`

To list all presets with codepoint counts:

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py --list-presets

### Additional options

`--force-autohint` — force FreeType's auto-hinter instead of the font's native hinting (useful when a font's built-in hints produce poor results at small sizes).

### CJK `.cpfont` size notes

Full-range `--intervals cjk` on a **complete** pan-CJK OTF can produce
**15–30 MB** per size. Prefer a language-region subset (Adobe Source Han
regional SubsetOTF, or your own `pyftsubset` pass) and let
`fontconvert_sdcard.py` keep only glyphs present in that face.

### EB Garamond + Source Han Serif (locale families)

The repo ships a builder that composites EB Garamond (Latin) with Adobe
regional Source Han Serif SubsetOTF fallbacks (CJK / Hangul). Sources are
vendored under `lib/EpdFont/scripts/source_fonts/`:

```
source_fonts/
├── EBGaramond/                 # Regular / Bold / Italic / BoldItalic + OFL
├── SourceHanSerifTW/           # TW Regular + Bold + LICENSE
├── SourceHanSerifCN/
├── SourceHanSerifJP/
└── SourceHanSerifKR/
```

There is **no second-pass character trim**: every glyph in the regional face
that falls inside the locale intervals is rasterized. Firmware SC↔TC remaps
(`ScToTcRemap` / `TcToScRemap`) still apply at runtime for cross-orthography
EPUB text on Chinese SKUs.

```bash
# From repo root; use the project venv if you have one:
pip install -r lib/EpdFont/scripts/requirements.txt
# or: PYTHON=.venv/bin/python …

bash lib/EpdFont/scripts/build-ebgaramond-cjk-sd.sh          # all locales
bash lib/EpdFont/scripts/build-ebgaramond-cjk-sd.sh tc       # one locale
LOCALES=sc,ja bash lib/EpdFont/scripts/build-ebgaramond-cjk-sd.sh
```

| Locale arg | Family folder | Source Han Serif | Intervals extras |
|---|---|---|---|
| `tc` | `EBGaramondSHS-TC` | `source_fonts/SourceHanSerifTW` | Bopomofo |
| `sc` | `EBGaramondSHS-SC` | `source_fonts/SourceHanSerifCN` | — |
| `ja` | `EBGaramondSHS-JA` | `source_fonts/SourceHanSerifJP` | — |
| `ko` | `EBGaramondSHS-KO` | `source_fonts/SourceHanSerifKR` | `hangul` |

Output: `lib/EpdFont/scripts/output/EBGaramondSHS-{TC,SC,JA,KO}/` with
`{name}_{12,14,16,18}.cpfont`. Install by copying (or symlinking) a family
folder to `/.fonts/<name>/` on the SD card.

Simulator convenience (local only; `simulator/sd_root*` is gitignored):

```bash
# Example: TC → shared sd_root
mkdir -p simulator/sd_root/.fonts
ln -sfn ../../../lib/EpdFont/scripts/output/EBGaramondSHS-TC \
  simulator/sd_root/.fonts/EBGaramondSHS-TC
# SC / JA / KO → simulator/sd_root_{sc,ja,ko}/.fonts/…
```

Then set `sdFontFamilyName` in that root’s `.crosspoint/settings.json`
(e.g. `"EBGaramondSHS-TC"`) and `fontSize` to `1` for MEDIUM.

Override source locations if needed:

```bash
SOURCE_FONTS_DIR=/path/to/source_fonts \
bash lib/EpdFont/scripts/build-ebgaramond-cjk-sd.sh tc
```

| `.cpfont` style | EB Garamond (primary) | Source Han Serif (fallback) |
|---|---|---|
| regular | Regular | Regular (CJK + Latin) |
| bold | Bold | Bold (CJK + Latin) |
| italic | Italic | — (Latin only; CJK falls back to regular at runtime) |
| bolditalic | BoldItalic | Bold (CJK + Latin; no italic SHS face) |

CJK bitmaps are stored for regular (SHS Regular) and bold/bolditalic (SHS Bold);
italic Han still falls back to regular at runtime. Latin always comes from EB Garamond.

### Missing-glyph fallback (Latin-only SD fonts)

If the selected SD font lacks a codepoint (typical: Latin family + Chinese EPUB),
the reader uses the **builtin system font** at the current size for that glyph —
layout advances and bitmaps both. On `gh_release_tc` the builtins carry CJK; on
the global build they are Latin-only, so missing glyphs remain blank/tofu.

This also avoids an indexing hang: absent codepoints are no longer mapped onto
U+FFFD in the SD advance table (which previously caused thousands of SPI seeks).

Install custom fonts via the web interface or manual SD card copy.
