# Japanese & Korean Builds

> Deep reference for [AGENTS.md](../../AGENTS.md). How the `gh_release_ja` /
> `gh_release_ko` firmwares mirror the Chinese SKU pattern: flags, fonts,
> charsets, and regenerating bitmaps. **No OpenCC / no Han conversion** —
> Japanese and Korean orthography are stored and looked up as-is.

| Env | Locale | UI | Font face | OTA asset | Version suffix |
|---|---|---|---|---|---|
| `gh_release_ja` | `ja-JP` | `japanese.yaml` | GenSen Rounded 2 **JP** Regular | `firmware-ja.bin` | `-ja` |
| `gh_release_ko` | `ko-KR` | `korean.yaml` | Resource Han Rounded **KR** Regular | `firmware-ko.bin` | `-ko` |

Shared compile flags: `-DENABLE_CJK_VERSION` (CJK tokenization / inter-character
gaps) plus either `-DENABLE_JAPANESE_VERSION` or `-DENABLE_KOREAN_VERSION`.
Chinese-only features (WeRead, 农历, SC↔TC remap) stay behind
`ENABLE_CHINESE_VERSION` and are **not** compiled into JA/KO SKUs.

## Character coverage

### Japanese (official lists)

| Tier | Sizes | Pool | Source |
|---|---|---|---|
| Minimal (Small) | 8/10/12 | 常用漢字 2136 ∪ kana ∪ i18n | 内閣告示 常用漢字表 (2010) |
| Base (MEDIUM) | 14 | JIS X 0208 Lv1+Lv2 6355 ∪ joyo ∪ kana ∪ i18n + symbols | JIS X 0208 (1990) rows 16–84 |
| I18n | 16/18 | kana ∪ `japanese.yaml` | UI force-include |

Hiragana + katakana always accompany every tier (via `chars_ja_kana.txt` and
pyftsubset kana Unicode ranges). Ideographs are **not** remapped SC↔TC.
The 14pt pool unions Joyo so MEDIUM stays a strict superset of the minimal set
(JIS X 0208 omits 塡/頰 from the 2010 Joyo table).

### Korean

| Tier | Sizes | Pool | Source |
|---|---|---|---|
| UI / SMALL / LARGE | 8/10/12/16/18 | modern jamo ∪ every glyph in `korean.yaml` | UI force-include via `--require-from` |
| Reading MEDIUM | 14 | **All modern** Hangul (11 172) ∪ 기초 한자 1800 ∪ modern jamo ∪ i18n + EPUB symbols | Hangul Syllables block + MOE 한문 교육용 기초 한자 |

Full Hangul is embedded only at 14pt (the default `FONT_SIZE::MEDIUM`) so the
dual-OTA `0x640000` app slot fits; 8/10/12/16/18 stay i18n-only but still
cover every `korean.yaml` UI glyph. Obsolete / ancient Hangul jamo are **not**
embedded. Hangul syllables are listed in `chars_ko_hangul_all.txt` and fed via
`--text-file`. Hanja uses the official educational 1800-character list — a
deliberately **small** ideograph pool (no SC/TC conversion).
`build-ko-builtin-fonts.sh` always passes `korean.yaml` to `--require-from` so
every UI string glyph is present at **every** point size (8–18).

## Regenerating fonts

```bash
python3 -m venv /tmp/cjk_font_venv
/tmp/cjk_font_venv/bin/pip install -r lib/EpdFont/scripts/requirements.txt

# Japanese — GenSenRounded2JP-R.otf from
# https://github.com/ButTaiwan/gensen-font (GenSenRounded2JP-otf.zip)
cp /path/to/GenSenRounded2JP-R.otf \
  lib/EpdFont/builtinFonts/source/GenSenRounded2JP/

PYTHON=/tmp/cjk_font_venv/bin/python \
  bash lib/EpdFont/scripts/build-ja-builtin-fonts.sh

# Korean — ResourceHanRoundedKR-Regular.ttf from
# https://github.com/CyanoHao/Resource-Han-Rounded (RHR-KR-*.7z)
cp /path/to/ResourceHanRoundedKR-Regular.ttf \
  lib/EpdFont/builtinFonts/source/ResourceHanRoundedKR/

PYTHON=/tmp/cjk_font_venv/bin/python \
  bash lib/EpdFont/scripts/build-ko-builtin-fonts.sh

pio run -e gh_release_ja
pio run -e gh_release_ko
```

Source OTFs/TTFs live under `lib/EpdFont/builtinFonts/source/` (gitignored).
Generated `notosans_ja_*.h` / `notosans_ko_*.h` headers are committed.

## Simulator

```bash
# Japanese
cmake -S simulator -B simulator/build_ja -DSIMULATOR_JAPANESE_VERSION=ON
cmake --build simulator/build_ja -j

# Korean
cmake -S simulator -B simulator/build_ko -DSIMULATOR_KOREAN_VERSION=ON
cmake --build simulator/build_ko -j
```

Use **separate build directories** per SKU — `gen_i18n.py` writes shared
`lib/I18n/I18nStrings.*`.

## Files

| Path | Role |
|---|---|
| `lib/EpdFont/scripts/chars_joyo_2136.txt` | 常用漢字 pool (minimal / 8–12pt) |
| `lib/EpdFont/scripts/chars_jis_l1l2_6355.txt` | JIS X 0208 Level 1+2 kanji pool (14pt) |
| `lib/EpdFont/scripts/chars_ja_kana.txt` | Hiragana + katakana |
| `lib/EpdFont/scripts/build_ja_charset.py` | Emits `ja_common_chars.txt` / `ja_i18n_chars.txt` |
| `lib/EpdFont/scripts/build-ja-builtin-fonts.sh` | GenSen JP → `notosans_ja_*.h` |
| `lib/EpdFont/scripts/chars_ko_hangul_all.txt` | All 11 172 modern Hangul syllables |
| `lib/EpdFont/scripts/chars_ko_hanja_1800.txt` | 한문 교육용 기초 한자 1800 |
| `lib/EpdFont/scripts/chars_ko_jamo.txt` | Modern combining + compatibility jamo (no obsolete) |
| `lib/EpdFont/scripts/build_ko_charset.py` | Emits `ko_common_chars.txt` / `ko_i18n_chars.txt` |
| `lib/EpdFont/scripts/build-ko-builtin-fonts.sh` | RHR KR → `notosans_ko_*.h` |
| `lib/I18n/translations/japanese.yaml` | JA UI (`_locale: ja-JP`) |
| `lib/I18n/translations/korean.yaml` | KO UI (`_locale: ko-KR`) |
| `lib/EpdFont/CjkVersion.h` | Shared `ENABLE_CJK_VERSION` helper |

## Known limitations

- No bold/italic CJK bitmaps (single Regular weight), same as Chinese.
- JA 16/18pt and KO 8/10/12/16/18pt are i18n-only by design — use MEDIUM (14pt)
  for full JIS Lv1+Lv2 (JA) or full modern Hangul + Hanja 1800 (KO) EPUB
  coverage. JA 8/10/12pt still cover Joyo for UI and lighter reading.
- JA/KO do **not** convert Chinese characters (no `ScToTcRemap` /
  `TcToScRemap`); mixed SC/TC EPUB text may show □ for the unmapped form if
  that codepoint was not in the Japanese/Korean subset.
