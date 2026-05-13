#!/usr/bin/env python3
"""Generate zh_cn_common.txt — the canonical charset for CN built-in fonts.

Merges:
  * 3,500 chars from 通用规范汉字表 一级 (vendored as zh_cn_3500.txt)
  * Every CJK char actually used in lib/I18n/translations/simplified_chinese.yaml
    (so the UI is guaranteed to render even if a word uses a non-一级 char)
  * CJK punctuation block      (U+3000–U+303F, 64 codepoints)
  * Fullwidth ASCII variants   (U+FF01–U+FF5E)
  * A small set of common math/typography glyphs that appear in Chinese
    reading content (e.g. "·" U+00B7, "—" U+2014, "…" U+2026, "★" U+2605)

Run after editing simplified_chinese.yaml or zh_cn_3500.txt; commit the
regenerated zh_cn_common.txt so fontconvert.py builds are reproducible.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[3]
OUT = HERE / "zh_cn_common.txt"
BASE_3500 = HERE / "zh_cn_3500.txt"
YAML = REPO_ROOT / "lib" / "I18n" / "translations" / "simplified_chinese.yaml"

EXTRA_BLOCKS = [
    (0x3000, 0x303F),   # CJK Symbols and Punctuation
    (0xFF01, 0xFF5E),   # Fullwidth ASCII
]
EXTRA_SCATTERED = [
    0x00B7,  # · MIDDLE DOT
    0x2014,  # — EM DASH
    0x2026,  # … HORIZONTAL ELLIPSIS
    0x2605, 0x2606,  # ★ ☆
    0x300C, 0x300D,  # 「 」 (already in 3000-303F but keep explicit)
    0x300E, 0x300F,  # 『 』
]


def main() -> int:
    cps: set[int] = set()

    if BASE_3500.exists():
        cps.update(ord(c) for c in BASE_3500.read_text(encoding="utf-8") if not c.isspace())
    else:
        print(f"WARN: {BASE_3500} not present — base 3500 chars will not be included.")

    if YAML.exists():
        ui_text = YAML.read_text(encoding="utf-8")
        for value in re.findall(r'^STR_[A-Z0-9_]+:\s*"(.*)"$', ui_text, re.MULTILINE):
            for c in value:
                cp = ord(c)
                if 0x4E00 <= cp <= 0x9FFF or 0x3400 <= cp <= 0x4DBF:
                    cps.add(cp)
    else:
        print(f"WARN: {YAML} not present — UI-only CJK chars will not be added.")

    for lo, hi in EXTRA_BLOCKS:
        cps.update(range(lo, hi + 1))
    cps.update(EXTRA_SCATTERED)

    if not cps:
        print("ERROR: empty charset")
        return 1

    lines = []
    line: list[str] = []
    for cp in sorted(cps):
        line.append(chr(cp))
        if len(line) == 32:
            lines.append("".join(line))
            line = []
    if line:
        lines.append("".join(line))

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT.name}: {len(cps)} codepoints across {len(lines)} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
