# CN built-in font charset

Files in this directory define the character set baked into CN-build (ENABLE_CHINESE_VERSION) fonts.

## Files

| File | Purpose | Generated? |
|---|---|---|
| `zh_cn_3500.txt` | 3,500 chars from 通用规范汉字表 一级 (PRC State Council, 2013) | Vendored, source: `elephantnose/characters` |
| `zh_cn_common.txt` | Canonical charset consumed by `fontconvert.py --additional-charset` | Yes — run `build_zh_cn_charset.py` to regenerate |
| `build_zh_cn_charset.py` | Builder: merges 3500 base + UI yaml + CJK punctuation + fullwidth | — |

## Regenerating zh_cn_common.txt

```sh
python lib/EpdFont/scripts/charsets/build_zh_cn_charset.py
```

Re-run whenever:
- `lib/I18n/translations/simplified_chinese.yaml` adds new CJK characters
- `zh_cn_3500.txt` is updated

Commit the regenerated `zh_cn_common.txt` so font headers built from it are reproducible.

## Sourcing zh_cn_3500.txt

Original list: 通用规范汉字表 一级常用字 (3,500 简体汉字), 2013 年国务院颁布。

Vendored from <https://github.com/elephantnose/characters/blob/master/3500%E5%B8%B8%E7%94%A8%E6%B1%89%E5%AD%97.txt>
on 2026-05-12. If updating from upstream, re-fetch and compare:

```sh
curl -fsSL "https://raw.githubusercontent.com/elephantnose/characters/master/3500%E5%B8%B8%E7%94%A8%E6%B1%89%E5%AD%97.txt" | diff - zh_cn_3500.txt
```
