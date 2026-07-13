#!/usr/bin/env python3
"""
Build Korean builtin-font charset lists (no OpenCC / no Han conversion).

Pools:
  - chars_ko_2350_common.txt — KS X 1001 완성형 Hangul (2350 syllables via EUC-KR)
  - chars_ko_jamo.txt        — compatibility + choseong jamo

Outputs:
  - ko_common_chars.txt — KS X 1001 ∪ jamo ∪ --require-from (8/10/12pt)
  - ko_i18n_chars.txt   — jamo ∪ --require-from (16/18pt)

Any Hangul / Hanja scraped from --require-from is kept verbatim (no SC/TC remap).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Hangul syllables, jamo, and CJK ideographs (Hanja in UI strings only).
KO_CHAR_RE = re.compile(r"[가-힣ㄱ-ㅎㅏ-ㅣㄱ-ㆎ\u1100-\u11FF一-鿿]")

SCRIPT_DIR = Path(__file__).resolve().parent
KSX_FILE = SCRIPT_DIR / "chars_ko_2350_common.txt"
JAMO_FILE = SCRIPT_DIR / "chars_ko_jamo.txt"
OUTPUT_FILE = SCRIPT_DIR / "ko_common_chars.txt"
I18N_OUTPUT_FILE = SCRIPT_DIR / "ko_i18n_chars.txt"


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
        required.update(KO_CHAR_RE.findall(path.read_text(encoding="utf-8")))
    return required


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--require-from",
        action="append",
        default=[],
        metavar="FILE",
        help="Force-include every Hangul/jamo/Hanja found in FILE. Repeatable.",
    )
    parser.add_argument("--output", type=Path, default=OUTPUT_FILE)
    parser.add_argument("--i18n-output", type=Path, default=I18N_OUTPUT_FILE)
    args = parser.parse_args()

    ksx = load_chars(KSX_FILE)
    jamo = load_chars(JAMO_FILE)
    required = load_required([Path(p) for p in args.require_from])

    kept = sorted(ksx | jamo | required)
    i18n = sorted(jamo | required)

    out = args.output if args.output.is_absolute() else SCRIPT_DIR / args.output
    i18n_out = args.i18n_output if args.i18n_output.is_absolute() else SCRIPT_DIR / args.i18n_output
    out.write_text("".join(kept), encoding="utf-8")
    i18n_out.write_text("".join(i18n), encoding="utf-8")

    beyond = sorted(required - ksx - jamo)
    print(
        f"Wrote {len(kept)} chars → {out.name} "
        f"(ksx {len(ksx)} ∪ jamo {len(jamo)} ∪ required {len(required)})",
        file=sys.stderr,
    )
    print(f"Wrote {len(i18n)} i18n/jamo chars → {i18n_out.name}", file=sys.stderr)
    if beyond:
        print(f"[required] +{len(beyond)} beyond ksx/jamo: {''.join(beyond)}", file=sys.stderr)


if __name__ == "__main__":
    main()
