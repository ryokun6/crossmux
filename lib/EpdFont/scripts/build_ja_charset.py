#!/usr/bin/env python3
"""
Build Japanese builtin-font charset lists (no OpenCC / no Han conversion).

Pools (official):
  - chars_joyo_2136.txt      — 常用漢字表 (内閣告示, 2136 chars; minimal / 8–12pt)
  - chars_jis_l1l2_6355.txt  — JIS X 0208 Level 1+2 kanji (6355; used by 14pt script)
  - chars_ja_kana.txt        — hiragana + katakana

Outputs:
  - ja_common_chars.txt  — 常用漢字 ∪ kana ∪ --require-from (drives 8/10/12pt)
  - ja_i18n_chars.txt    — kana ∪ --require-from only (drives 16/18pt)

Kanji from --require-from are force-included as-is (Japanese orthography;
never remapped to Simplified/Traditional Chinese variants).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# CJK Unified Ideographs + kana. Punctuation is covered by pyftsubset --unicodes.
JA_CHAR_RE = re.compile(r"[一-鿿ぁ-ゖァ-ヺー]")

SCRIPT_DIR = Path(__file__).resolve().parent
JOYO_FILE = SCRIPT_DIR / "chars_joyo_2136.txt"
KANA_FILE = SCRIPT_DIR / "chars_ja_kana.txt"
OUTPUT_FILE = SCRIPT_DIR / "ja_common_chars.txt"
I18N_OUTPUT_FILE = SCRIPT_DIR / "ja_i18n_chars.txt"


def load_chars(path: Path) -> set[str]:
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        sys.exit(1)
    return {c for c in path.read_text(encoding="utf-8") if not c.isspace()}


def load_required(paths: list[Path]) -> set[str]:
    required: set[str] = set()
    for path in paths:
        if not path.is_file():
            print(f"error: --require-from path not found: {path}", file=sys.stderr)
            sys.exit(1)
        required.update(JA_CHAR_RE.findall(path.read_text(encoding="utf-8")))
    return required


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--require-from",
        action="append",
        default=[],
        metavar="FILE",
        help="Force-include every kanji/kana found in FILE. Repeatable.",
    )
    parser.add_argument("--output", type=Path, default=OUTPUT_FILE)
    parser.add_argument("--i18n-output", type=Path, default=I18N_OUTPUT_FILE)
    args = parser.parse_args()

    joyo = load_chars(JOYO_FILE)
    kana = load_chars(KANA_FILE)
    required = load_required([Path(p) for p in args.require_from])

    kept = sorted(joyo | kana | required)
    i18n = sorted(kana | required)

    out = args.output if args.output.is_absolute() else SCRIPT_DIR / args.output
    i18n_out = args.i18n_output if args.i18n_output.is_absolute() else SCRIPT_DIR / args.i18n_output
    out.write_text("".join(kept), encoding="utf-8")
    i18n_out.write_text("".join(i18n), encoding="utf-8")

    beyond = sorted(required - joyo - kana)
    print(
        f"Wrote {len(kept)} chars → {out.name} "
        f"(joyo {len(joyo)} ∪ kana {len(kana)} ∪ required {len(required)})",
        file=sys.stderr,
    )
    print(f"Wrote {len(i18n)} i18n/kana chars → {i18n_out.name}", file=sys.stderr)
    if beyond:
        print(f"[required] +{len(beyond)} beyond joyo/kana: {''.join(beyond)}", file=sys.stderr)


if __name__ == "__main__":
    main()
