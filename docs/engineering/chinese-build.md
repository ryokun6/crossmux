# Chinese Builds (ENABLE_CHINESE_VERSION)

> Deep reference for [AGENTS.md](../../AGENTS.md). Guide to the Traditional
> (`gh_release_tc`) and Simplified (`gh_release_sc`) Chinese firmwares: how the
> flags gate resources, flash budget, locales, and regenerating CJK fonts.

Two Chinese release envs share `-DENABLE_CHINESE_VERSION` and add a locale axis:

| Env | Locale | UI strings | Fonts | Glyph remap | OTA asset | Version suffix |
|---|---|---|---|---|---|---|
| `gh_release_tc` | `zh-TW` | Traditional (`chinese.yaml`) | GenSen TW → `notosans_cjk_*.h` | SC→TC (`ScToTcRemap.h`) | `firmware-tc.bin` | `-tc` |
| `gh_release_sc` | `zh-CN` | Traditional YAML → OpenCC **tw2sp** (TW terms → Mainland Simplified) at gen_i18n | GenSen TW → `notosans_sc_*.h` (SC-keyed subset; same `notosans_cjk_*` symbols) | TC→SC (`TcToScRemap.h`) | `firmware-sc.bin` | `-sc` |

`CHINESE_UI_SIMPLIFIED` is set only on SC envs. International (`gh_release`) has
neither flag.

The `-DENABLE_CHINESE_VERSION` flag in [platformio.ini](../../platformio.ini)
gates every Chinese-only resource:

| Resource | Unified behavior |
|---|---|
| i18n string table (`gen_i18n.py`) | Emits **EN + ZH_TW** (TC) or synthesizes **EN + ZH_CN** via OpenCC **tw2sp** from the same Taiwan-terminology `chinese.yaml` when `CHINESE_UI_SIMPLIFIED` is set (檔案→文件, 網路→网络, …). Persisted codes are BCP47 `zh-TW` / `zh-CN` (`_locale`). |
| Built-in fonts ([lib/EpdFont/builtinFonts/all.h](../../lib/EpdFont/builtinFonts/all.h)) | Latin headers skipped. Six per-size CJK headers — TC files `notosans_cjk_*.h` or SC files `notosans_sc_*.h` — raw 2-bit bitmaps. Coverage tiers: 8/10/12pt ~3500 common; 14pt ~7000 + symbols; 16/18pt i18n-only. |
| `src/main.cpp` font globals | Each Latin `EpdFont`/`EpdFontFamily` aliases the matching-size CJK symbol (`notosans_cjk_*`). Bold/italic share Regular. |
| EPUB / TXT layout ([lib/Epub/Epub/ParsedText.cpp](../../lib/Epub/Epub/ParsedText.cpp), [CjkKinsoku.h](../../lib/Epub/Epub/CjkKinsoku.h), [CjkPunctCompression.h](../../lib/Epub/Epub/CjkPunctCompression.h), [TxtReaderActivity.cpp](../../src/activities/reader/TxtReaderActivity.cpp)) | CJK punctuation rules (行頭/行末/分離 禁则) for EPUB horizontal lines and vertical-rl columns, plus TXT horizontal wrap; full-width padding on EPUB. 標點擠壓 (punctuation compression) for TC/SC when Reader setting `punctCompressionEnabled` is on (default); see section cache versions 70/71 in [file-formats.md](../file-formats.md). |
| Activities (`src/activities/apps/weread/`) | Offline QR WeRead stack compiled in via `build_src_filter` on TC/SC (not JA/KO). Uses `freeink-sdk` SecureNet/wolfSSL. |
| First-boot language | `Language::ZH_TW` or `Language::ZH_CN` by SKU; international defaults to `EN`. |

**Flash budget** (default `partitions.csv`, dual A/B app slot = 6.25 MB) — each Chinese SKU separately:

| Section | Bytes |
|---|---|
| Code + non-font data | ~3.0 MB |
| 3 CJK font headers 8/10/12pt | ~1.1 MB |
| 1 CJK font header 14pt | ~1.62 MB |
| 2 CJK font headers 16/18pt | ~400 KB |
| i18n strings (EN + one Chinese) | ~16 KB |
| Hyphenation tries | English only on CJK (`ENABLE_CJK_VERSION`); intl keeps 10 langs |
| **Total (measured post-1.5.1 sync / #27)** | SC ~95.4% / TC ~98.1% of 6.25 MB app slot (WeRead + GenSen + wolfSSL) |
| **After EN-only hyphen** | TC **93.2%** (6,107,031 / 6,553,600); SC **90.5%** (5,932,941 / 6,553,600) |

These are same-checkout `.bin` measurements, including image padding. The two
changes save 180,960 B (176.7 KiB) without changing supported CJK characters,
glyph bitmaps or metrics. Sharing the 2,523-entry Unicode index removes
60,552 B of duplicate data; the image shrinks by 60,544 B after alignment.
No new runtime allocations are introduced. The remaining 355 KiB headroom
is still below the 512 KiB release budget; a further 160,768 B must be removed
to reach that target. Physical X3/X4 display validation remains outstanding.

Sticky smoke test (2026-09-17): flashed the 5,796,752-byte `sticky` image
with write/hash verification. Wake, calculator rendering, return to the reader
and EPUB page turns were observed in serial logs; the tester confirmed normal
calculator display. The reader used an SD font, so this does not validate the
built-in 12pt CJK reader fallback. Dedicated 8/10/12pt visual checks remain
outstanding. Firmware SHA-256:
`f1bec28a4d10e5e87af9f25c484bad379831fd92be777465b2559c53bb5088d6`.

X3 smoke test (2026-09-17): rebuilt and flashed the 6,190,080-byte
`gh_release` image to ESP32-C3 MAC `70:af:09:5f:81:3c`; write/hash verification
passed. Font manifest loading failed after reboot (also confirmed by the tester):
both automatic and TLS 1.2 handshakes timed out, with only 3,024 / 3,652 bytes
of free heap respectively. After failure, free/minimum/largest-block heap was
15,892 / 2,568 / 10,740 bytes. This download check failed; reading/standby
comparison and physical font display checks remain pending. Local evidence:
`build/font-shrink-x3-upload.log` and `build/font-shrink-x3-test.log`.
Firmware SHA-256:
`e9d00e976050318937830126da43171b71f5494abeecfe5011019fa1c7d1124c`.

A/B OTA remains unchanged: both app slots are 6,553,600 bytes. Release builds
must stay at or below 6,029,312 bytes to retain at least 512 KiB headroom.

> **Historical note (2026-05-19)**: the committed `notosans_cjk_*.h` headers
> had drifted from `cn_common_chars.txt` in earlier PRs — the headers held
> ~5000 CJK glyphs per size (including ~2000 Traditional Chinese variants
> the project never renders) while `cn_common_chars.txt` declared only the
> 3500 SC chars. A regen during the 老黄历 PR brought them back in sync at
> 3500 SC chars, freeing ~755 KB of dead-weight bitmap data (Flash dropped
> from ~91% to ~79%).
>
> **Update (2026-05-26)**: 14pt was promoted to the 7000 通用汉字 tier with
> extended symbol coverage (number forms / arrows / math / box drawing /
> geometric shapes / dingbats) so the reader-default MEDIUM size renders
> rare names, place names, and modern EPUB symbols. Flash returned to ~92%
> deliberately; the headroom is still ~500 KB. 8/10/12pt and 16/18pt sizes
> were intentionally left at their existing coverage to keep the budget
> in check.
>
> **Update (2026-07-24)**: 14pt returned to the common tier and the extended
> symbol ranges were removed. The common tier now preserves the complete
> 3500-char source pool before adding all required glyphs; the i18n tier now
> contains every CJK glyph scanned from `chinese.yaml` and feature files.
> Relative to the 6,502,659-byte RC baseline, the RC image is 736,664 bytes
> smaller while adding the previously missing required glyphs.
>
> **Update (2026-08-06)**: 14pt moved to the i18n-only tier. Its 747 required
> CJK glyphs keep every built-in UI and feature string renderable, while broad
> Chinese EPUB coverage now uses the downloadable SD-card font. This removes
> 587,322 bytes of raw bitmap and glyph metadata from the firmware. Regeneration
> also picked up 14 UI glyphs that had been added since the previous font build.

The unified build keeps LTO disabled. Combining LTO with the custom ESP-IDF core
rebuild previously allowed cache-critical SPI flash helpers to be linked into
Flash; calling them while cache was disabled caused an early-boot Cache Error.
Do not re-enable LTO without checking the final ELF and map for the affected
IRAM mappings.

The active size strategy is to embed only 8/10/12pt CJK subsets and externalize
large reading faces to `.cpfont`. This removes flash data without introducing a
decompression buffer or a new long-lived heap allocation.

## WeRead transport

The root headers below `src/activities/apps/weread/` are compile-time-only
selection points. The public build uses the UI in `webapi/` with
`lib/WeReadWebApi`; there is no runtime backend switch.

WeRead owns its transport in `lib/WeReadWebApi/src/WeReadHttpClient.*`;
the public `HttpDownloader` has no WeRead-specific session, Cookie, or protocol
logic. On device, the client uses wolfSSL through `freeink::SecureClient`, a
caller-owned 4 KB work buffer, and one best-effort persistent connection.
Chapter images are streamed to SD and scheduled in batches by HTTPS host, so
requests for the same host reuse that single connection. Redirects may change
the active host, but only `weread.qq.com` and its subdomains receive the WeRead
Cookie. The connection closes before EPUB packaging, and also closes early
when free heap falls below 20 KB or the largest block falls below 8 KB.

The pinned SDK owns the maximum-fragment request: `HAVE_MAX_FRAGMENT` remains
enabled and `SecureClient` requests 2 KiB before the handshake. There is no
application SNI wrapper. This preserves the effective request on the original
SDK path; it does not prove that the peer accepted it. HTTP buffer sizes do not
establish TLS record sizes. Cipher suites and hardware acceleration are unchanged.

Chapter shard merging and Base64 input reuse the Operation's 4 KiB buffer instead
of allocating a temporary 1 KiB buffer per chapter; Base64 output writes remain
unchanged. Downloads remain serial on C3 and S3. UI stage numbers and labels share
one mapping and drawing path; chapter progress refresh requests use 5% buckets,
while images retain 10% buckets. Unknown totals show waiting text rather than a
percentage. DEBUG-only diagnostic sampling is compiled out at lower log levels.
Timing includes synchronous SD I/O and must not be interpreted as pure CPU work.
See the [WeRead README](../../src/activities/apps/weread/README.md) for measurement
boundaries, cold-cache test records, and unverified hardware checks.

Device requests call `setInsecure()`: traffic is encrypted, but the CA and host
identity are not verified. This makes session credentials and downloaded
content vulnerable to a man-in-the-middle attacker. The native simulator takes
a different path, `WeReadHttpClient -> simulator esp_http_client shim -> curl`,
and uses the host trust store for certificate verification. WeRead uses an
unofficial Web protocol and may stop working when the service changes.

## WeRead progress sync

The reader exposes one **Sync Progress** menu action. When the current EPUB path
exactly matches a standard book in `shelf.bin`, the action uses WeRead; all other
EPUBs continue to use KOReader. WeRead public-account books (`MP_WXS_`), moved
files, and renamed files are intentionally treated as ordinary EPUBs. The
existing long-press KOReader shortcut is unchanged.

Manual WeRead sync starts with the saved Cookie and renews it only after an
authentication failure. Newly generated WeRead chapters retain invisible
raw-XHTML UTF-16 source anchors. These map the local page's visible-text offset
to WeRead's native `chapterUid + chapterOffset` coordinate without clamping the
offset to the `WRT2` word count; the inverse mapping restores a remote offset
through the reader's existing pagination LUT. Older generated books without
anchors retain the visible-offset approximation, and other EPUBs fall back to
the whole-book percentage derived from `WRT2` word counts. Different canonical
positions show the direction selector. A local upload uses an enter/report pair
and includes the whole seconds recorded by the local reading session that ended
when manual sync was opened. When the remote position is selected, its exact
chapter and offset are reported before being applied locally, so reporting time
cannot move cloud progress backwards. Upload is accepted only after a read-back
verifies the chapter and offset. A timed report is not automatically repeated
after an ambiguous network failure; the current sync screen retains it for an
explicit Retry, but no pending time is persisted after leaving the screen. An
expired session directs the user back to **Apps → WeRead** to sign in; it does
not open a QR flow from the reader.

For a new standard-book cache, the downloader fetches cloud progress after the
`WRT2` catalog and before any chapter or image. This request is best effort:
network, authentication, and protocol failures are logged but do not block the
book download. After the EPUB is atomically replaced, a successful result is
stored as the one-shot `WRP1` initial position. The reader consumes it only when
there is no valid local `progress.bin`; an existing local position always wins.
Public-account books skip this prefetch. See [file-formats.md](../file-formats.md)
for the `WRT2` and `WRP1` layouts and migration rules.

## Regenerating the CJK fonts

The generator finishes by running `share-cn-font-intervals.py`. It validates
that the 8/10/12pt interval triples (first, last, glyph offset) are identical,
then writes `notosans_cjk_common_intervals.h` and references its single
`inline constexpr` table from those fonts. It refuses mismatches before writing.
To share indices in existing generated headers without rerasterizing fonts or
refreshing character sets, run:

```bash
python3 lib/EpdFont/scripts/share-cn-font-intervals.py
python3 lib/EpdFont/scripts/share-cn-font-intervals.py --check
```

```bash
# 1. (One-time) install build deps into a venv
python3 -m venv /tmp/cn_font_venv
/tmp/cn_font_venv/bin/pip install -r lib/EpdFont/scripts/requirements.txt

# 2. Place GenSenRounded2TW-R.otf into the source dir (gitignored).
#    Source: https://github.com/ButTaiwan/gensen-font (or your licensed copy)
cp /path/to/GenSenRounded2TW-R.otf lib/EpdFont/builtinFonts/source/GenSenRounded2TW/

# 3. (Optional) regenerate the character lists. build-cn-builtin-fonts.sh
#    calls this for you in Step 0 unless SKIP_CHARSET=1 is set.
#    Writes BOTH cn_common_chars.txt (top-N + require-from) and
#    cn_i18n_chars.txt (require-from only — drives the 14/16/18pt subset).
PYTHON=/tmp/cn_font_venv/bin/python3 \
  python3 lib/EpdFont/scripts/build_cn_charset.py \
    --require-from lib/I18n/translations/chinese.yaml

# 4. Generate the 6 per-size CJK headers (TC and/or SC — same GenSen OTF)
PYTHON=/tmp/cn_font_venv/bin/python3 \
  bash lib/EpdFont/scripts/build-cn-builtin-fonts.sh
PYTHON=/tmp/cn_font_venv/bin/python3 \
  bash lib/EpdFont/scripts/build-sc-builtin-fonts.sh

# 5. Build
pio run -e gh_release_tc
# or
pio run -e gh_release_sc
```

`build_cn_charset.py` prints the highest-frequency casualties (chars just
above and below the cutoff) so you can verify the trim looks reasonable.

## Force-including feature-specific glyphs

Each feature that needs CJK glyphs absent from both the 3500 SC pool *and*
the natural `chinese.yaml` STR_ values ships a small dedicated text file and
adds it to the `REQUIRE_FROM` array in
[build-cn-builtin-fonts.sh](../../lib/EpdFont/scripts/build-cn-builtin-fonts.sh).
`build_cn_charset.py --require-from <file>` regex-scans the file for every
CJK Unified Ideograph (`[一-鿿]`) and force-includes them in **both**
`cn_common_chars.txt` (8/10/12pt) and `cn_i18n_chars.txt` (16/18pt) —
so glyphs added via this mechanism render at every CN font size. The
14pt tier draws from the static `chars_7000_common.txt` pool instead, so
a `REQUIRE_FROM` entry does not reach it. The one exception is
[cn_tc_extra_chars.txt](../../lib/EpdFont/scripts/cn_tc_extra_chars.txt),
which `build-cn-builtin-fonts.sh` folds into the 14pt charset directly (see
"Traditional coverage at 14pt" below). If you add a `cn_<feature>_chars.txt`
containing a glyph outside the 7000 pool, audit it against
`chars_7000_common.txt` and extend that pool too (otherwise the feature will
render at 8/10/12/16/18pt but show □ at 14pt).

Current example: [cn_almanac_chars.txt](../../lib/EpdFont/scripts/cn_almanac_chars.txt)
holds the chars `ChineseAlmanac.cpp` / `ChineseCalendarFace.cpp` need
beyond the standard pool:

```text
戊庚壬癸寅卯巳酉戌廿蛰闰初七八九十冬腊
```

Pattern when adding a new feature:

1. Create `lib/EpdFont/scripts/cn_<feature>_chars.txt` containing only the
   CJK chars the feature renders that aren't otherwise covered (one line,
   UTF-8, no separators required).
2. Append that path to `REQUIRE_FROM=(... cn_<feature>_chars.txt)` in
   `build-cn-builtin-fonts.sh`.
3. Re-run `bash build-cn-builtin-fonts.sh` and commit the regenerated
   `cn_common_chars.txt`, `cn_i18n_chars.txt`, and six `notosans_cjk_*.h`.

## Traditional coverage at 14pt

The 14pt reader tier is generated from `chars_7000_common.txt`, which is the
**Simplified** 通用规范汉字 list, converted with OpenCC `s2tw`. That conversion is
one-to-one, so it can only ever produce one Traditional form per Simplified
character. Every Simplified merge therefore lost its siblings: 复 yielded 復 and
never 複 or 覆, 面 never 麵, 系 never 係 or 繫. Scanning ten Traditional EPUBs
found 221 distinct glyphs rendering as □, 8417 occurrences in all — 複 alone
appeared 750 times. The smaller 8/10/12pt tiers were unaffected because they
come from `cn_common_chars.txt`, which preserves require-from characters.

`build-cn-builtin-fonts.sh` now recovers the siblings by walking the source
OTF's cmap and keeping any Traditional character whose `tw2s` conversion lands
back in the pool (+282 chars). The check `back != ch` excludes Simplified
characters, which reach the font at runtime through `ScToTcRemap.h` anyway.
This is derived from the converters rather than OpenCC's dictionary files,
whose path differs between the C++ binding pinned in `requirements.txt` and the
pure-Python package.

Expansion cannot reach a Traditional character that no Simplified character
maps to, and OpenCC declines some pairs entirely (`tw2s('甦')` returns 甦, not
苏). Those live in
[cn_tc_extra_chars.txt](../../lib/EpdFont/scripts/cn_tc_extra_chars.txt): the
pronouns 妳/牠/祢, older orthographic forms (卽 旣 眞), Japanese shinjitai quoted
in translated works, and names such as 淼. Prefer widening the expansion when a
character is a genuine variant; the extras list only covers what someone has
already hit. The two together bring those ten books to zero missing glyphs, at
a cost of ~190 KB of flash.

GenSen Rounded TW needs **per-size** metric nudges so Lyra layouts
(hardcoded `itemY+7` for UI_10, `getLineHeight` centering for UI_12 menus)
match Ubuntu-tuned spacing. Defaults in `build-cn-builtin-fonts.sh`:

| Size | `--baseline-adjust` | `--line-height-adjust` | Why |
|------|---------------------|------------------------|-----|
| 8 | -2 | +6 | High ink; match Latin `notosans_8` advanceY 23 |
| 10 | -2 | +7 | High ink; interpolate Latin 8/12 → advanceY 28 |
| 12 | +4 | +9 | Menu was low; match Latin `notosans_12` advanceY 34 |
| 14 | +1 | +11 | Match Latin reader advanceY 40 |
| 16 | +1 | +12 | Match Latin reader advanceY 45 |
| 18 | +1 | +13 | Match Latin reader advanceY 51 |

Override with `CN_BASELINE_ADJUST[_SIZE]` / `CN_LINE_HEIGHT_ADJUST[_SIZE]`.

### Traditional glyphs + Simplified remap

Bitmaps store **Taiwan Traditional** codepoints (OpenCC `s2tw` on the SC pool,
plus `--require-from` chars from `chinese.yaml` preserved verbatim so UI forms
like 啟/為/裡 are not rewritten to HK variants). UI strings in `chinese.yaml`
are Traditional (`_language_name: 繁體中文`); `EpdFont::getGlyph` remaps via
committed `lib/EpdFont/ScToTcRemap.h` (~10 KB, also `s2tw`) so Simplified
EPUB/codepoint input does not need duplicate bitmaps.

Anti-pattern (don't do this): hiding chars in a `#` YAML comment inside
`chinese.yaml` to abuse the regex scanner. It works (build_cn_charset.py
does scan comments) but couples a font detail to the i18n file's structure
and forces `gen_i18n.py` to know about it. Dedicated `cn_*_chars.txt`
files are self-documenting and orthogonal to i18n.

## Expanding character coverage (pool, not --top)

`--top` is capped by the source pool size (`chars_3500_common.txt` has 3500
chars by construction — it is the vendored 教育部《现代汉语常用字表》).
Passing `--top 5000` will fail with `--top is >= pool size`.

To enlarge the renderable character set:

1. **For a handful of specific chars**: add a feature-scoped
   `cn_<feature>_chars.txt` as described above. One-line file + one-line
   append to `REQUIRE_FROM`. No pool expansion needed.
2. **For broad coverage gain** (e.g. classical literature, GB2312 Lv2): drop a
   larger source list into `lib/EpdFont/scripts/` (e.g. a `gb2312_full.txt`
   union of Lv1+Lv2) and point `SOURCE_FILE` in `build_cn_charset.py` at it.
   This is a deliberate Flash-budget decision — every 1000 extra chars adds
   roughly **380 KB** to the 8/10/12pt headers combined.

The hard ceiling is the 6.25 MiB A/B-OTA slot; the release gate reserves at
least 512 KiB, so broad pool expansion must be measured in the built image.

## Incomplete built-in → SD font prompt

SC/TC firmware offers the guided font downloader when:

- Text settings change the built-in family to 16/18pt, or
- an EPUB page render hits a missing U+4E00–U+9FFF / U+F900–U+FAFF glyph
  (horizontal and vertical).

The render task records only the first such codepoint; `EpubReaderActivity::loop`
opens `FontDownloadActivity` (`Purpose::PromptThenManage`). One prompt per boot;
an active SD font is never rechecked. Confirming opens ordinary **Manage Fonts**.
Downloads never change `sdFontFamilyName` — pick the installed family later in
text settings. See [`docs/sd-card-fonts.md`](../sd-card-fonts.md).

## Known limitations

- **No bold/italic CJK glyphs**: the bitmaps come from a single GenSenRounded2TW-R subset. UI elements that pass `EpdFontFamily::Style::Bold` render the regular weight under CN.
- **Font-size dropdown affects rendered size**: each reader size (12/14/16/18pt) and UI size (10/12pt) and small font (8pt) has its own bitmap header. Switching size really does swap glyph bitmaps.
- **Rare characters render as □ in reader at SMALL**: the SMALL (12pt) bitmap uses the 3500-char pool, which covers all of modern SC but omits classical / scientific rarities, Traditional Chinese variants, and most niche surnames/place names. MEDIUM (14pt, reader default) now uses the 7000 通用汉字 pool and covers most modern names, place names, and regulated chars from the 2013 通用规范汉字表. The reader offers the complete-font downloader on the first detected missing CJK ideograph. Expand SMALL by adding a feature-scoped `cn_<feature>_chars.txt` or by enlarging the pool — see "Expanding character coverage" above. Bumping `--top` alone does nothing.
- **CJK in reader at LARGE/EXTRA_LARGE shows blank** for chars outside the i18n subset — by design, since 16/18pt reader sizes are tuned for English EPUB. Switching to LARGE/EXTRA_LARGE offers the same downloader; otherwise switch to MEDIUM to read Chinese.
- **`FontDecompressor` is bypassed for CJK** by design — bitmaps are stored raw because compressing 6 fonts × ~50 KB groups fragments the heap on boot.
- **UI language codes are `zh-TW` / `zh-CN`**: enum members `ZH_TW` / `ZH_CN`; Traditional YAML is the single Taiwan-terminology source (`_language_name: 繁體中文`); SC UI is synthesized via OpenCC **tw2sp** at gen_i18n.

## Files

| Path | Role |
|---|---|
| `lib/EpdFont/scripts/build_cn_charset.py` | Rank a pool by wordfreq Zipf, emit top-N + i18n force-includes. Pool capped at `chars_3500_common.txt` size (3500). |
| `lib/EpdFont/scripts/chars_3500_common.txt` | Source pool — 现代汉语常用字表, 3500 chars (committed). To expand coverage, swap this file (see "Expanding character coverage"). |
| `lib/EpdFont/scripts/cn_common_chars.txt` | Generated full subset, drives 8/10/12pt (committed). Single-line UTF-8, sorted by codepoint. |
| `lib/EpdFont/scripts/chars_7000_common.txt` | Static 7000 通用汉字 pool, drives 14pt (committed). One-line UTF-8, codepoint-sorted. Extracted from the 2013 国家通用规范汉字表-aligned vendor list; fully contains today's `cn_common_chars.txt`, `cn_i18n_chars.txt`, and `cn_almanac_chars.txt`. |
| `lib/EpdFont/scripts/cn_i18n_chars.txt` | Generated i18n-only subset, drives 16/18pt (committed). Contains every CJK char found in `--require-from` inputs. |
| `lib/EpdFont/scripts/build-cn-builtin-fonts.sh` | pyftsubset → fontconvert.py pipeline, six headers. Default re-runs `build_cn_charset.py`; set `SKIP_CHARSET=1` to reuse the current `cn_common_chars.txt`. The `REQUIRE_FROM=(...)` array at the top lists every file scanned for force-included CJK chars — add new feature-scoped `cn_*_chars.txt` files here. |
| `lib/EpdFont/scripts/cn_almanac_chars.txt` | Feature-scoped force-include for `ChineseAlmanac.cpp` / `ChineseCalendarFace.cpp` — ganzhi stems/branches + lunar-row vocab that aren't in the pool or any `chinese.yaml` STR_ value. Single-line UTF-8. |
| `lib/EpdFont/scripts/cn_weread_chars.txt` | Feature-scoped force-include for hardcoded WeRead shelf/search UI fragments. |
| `lib/EpdFont/scripts/cn_tc_extra_chars.txt` | Traditional-only characters the Simplified pool cannot reach (妳 牠 卽 淼 甦 …). Unlike the other force-include files this one also feeds 14pt directly — see "Traditional coverage at 14pt". |
| `lib/EpdFont/builtinFonts/notosans_cjk_{8,10,12,14,16,18}.h` | Generated bitmap headers (committed). Should always match `cn_common_chars.txt` (8/10/12pt), `chars_7000_common.txt` traditionalized plus its variant expansion and `cn_tc_extra_chars.txt` (14pt), and `cn_i18n_chars.txt` (16/18pt) — see consistency check below. |
| `lib/EpdFont/builtinFonts/source/GenSenRounded2TW/` | OTF source dir (gitignored). Drop `GenSenRounded2TW-R.otf` here. |
| `lib/EpdFont/ScToTcRemap.h` | Committed SC→TC lookup (~2600 pairs). Regenerated by `build_sc_to_tc_remap.py`. |
| `lib/I18n/translations/chinese.yaml` | Taiwan Traditional Chinese UI translations (`_language_name: 繁體中文`, `_language_code: ZH_TW`, `_locale: zh-TW`); SC builds synthesize `ZH_CN`/`zh-CN`/`简体中文` via OpenCC **tw2sp** (phrase-level Mainland terms). Fed to `--require-from` for font subsets. |
| `lib/EpdFont/scripts/build-sc-builtin-fonts.sh` | SC font pipeline (GenSen Rounded TW → SC-keyed `notosans_sc_*.h` + `TcToScRemap.h`). |
| `lib/EpdFont/scripts/sc_common_chars.txt` / `sc_i18n_chars.txt` | Simplified charset lists for the SC font build. |

## Consistency check

After regenerating, confirm the committed bitmap headers match the committed
charset files:
- 8/10/12pt headers should equal `len(cn_common_chars.txt unique CJK) + ~497`
  (ASCII/Latin-1/CJK-punct from `--unicodes`)
- 14pt header should equal `len(chars_7000_common.txt) + ~700`
  (extra ~200 comes from the LARGE tier's added symbol ranges)
- 16/18pt headers should equal `len(cn_i18n_chars.txt) + ~497`

```bash
python3 -c "
import re
from pathlib import Path
root = Path('.')
cjk = lambda s: {c for c in s if '\u4e00' <= c <= '\u9fff'}
scripts = root / 'lib/EpdFont/scripts'
pool = cjk((scripts / 'chars_3500_common.txt').read_text())
common = cjk((scripts / 'cn_common_chars.txt').read_text())
i18n = cjk((scripts / 'cn_i18n_chars.txt').read_text())
required = cjk((root / 'lib/I18n/translations/chinese.yaml').read_text())
required |= cjk((scripts / 'cn_almanac_chars.txt').read_text())
assert (len(pool), len(common), len(i18n)) == (3500, 3517, 802)
assert common == pool | required and i18n == required
for size, expected in [(8, 4014), (10, 4014), (12, 4014),
                       (14, 1299), (16, 1299), (18, 1299)]:
    header = (root / f'lib/EpdFont/builtinFonts/notosans_cjk_{size}.h').read_text()
    index_header = ((root / 'lib/EpdFont/builtinFonts/notosans_cjk_common_intervals.h').read_text()
                    if size <= 12 else header)
    intervals = re.search(r'Intervals\[\] = \{(.*?)\n\};', index_header, re.S).group(1)
    codepoints = set()
    for first, last in re.findall(r'\{\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+),', intervals):
        codepoints.update(range(int(first, 16), int(last, 16) + 1))
    expected_cjk = common if size <= 12 else i18n
    assert {ord(char) for char in expected_cjk} <= codepoints
    assert 'mode: 2-bit' in header
    assert '\n    true,\n    nullptr,\n' in header  # EpdFontData::is2Bit
    assert len(codepoints) == expected, (size, len(codepoints))
    if size == 14:
        assert not {ord(char) for char in common - i18n} & codepoints
print('Chinese font coverage OK')
"
```

If `header glyphs` is significantly larger than the expected delta, the
header is stale (most likely committed from an older/larger charset) — re-run
the regeneration steps.
