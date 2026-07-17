#!/usr/bin/env python3
"""
Build Korean builtin-font charset lists (no OpenCC / no Han conversion).

Pools:
  - chars_ko_hangul_all.txt   — all 11 172 *modern* Hangul syllables (U+AC00–D7A3);
    obsolete / ancient Hangul jamo syllables are intentionally omitted
  - chars_ko_hanja_1800.txt   — 한문 교육용 기초 한자 1800 (MOE educational Hanja)
  - chars_ko_ks1001_2350.txt  — KS X 1001 완성형 common syllables (2 350); the
    standard "commonly used Hangul" set, covers >99.9% of real-world Korean
    text. Regenerate: bytes([lead, tail]).decode("euc_kr") for lead 0xB0–0xC8,
    tail 0xA1–0xFE
  - chars_ko_jamo.txt         — modern choseong/jungseong/jongseong + compatibility jamo

Outputs:
  - ko_common_chars.txt — Hangul-all ∪ Hanja-1800 ∪ jamo ∪ --require-from
                          (drives reader MEDIUM, 14pt)
  - ko_ui_chars.txt     — KS X 1001 2 350 ∪ jamo ∪ --require-from
                          (drives UI sizes 8/10/12pt so file/book lists can
                          render arbitrary Korean titles)
  - ko_i18n_chars.txt   — jamo ∪ --require-from only (drives 16/18pt)

Pass `--require-from korean.yaml` (and any other UI sources) so every
user-facing Hangul/Hanja glyph is force-included in *all* outputs.
Hanja and Hangul from --require-from are kept verbatim (no SC/TC remap).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Hangul syllables, jamo, and CJK ideographs (Hanja).
KO_CHAR_RE = re.compile(r"[가-힣ㄱ-ㅎㅏ-ㅣㄱ-ㆎ\u1100-\u11FF一-鿿]")

SCRIPT_DIR = Path(__file__).resolve().parent
HANGUL_FILE = SCRIPT_DIR / "chars_ko_hangul_all.txt"
HANJA_FILE = SCRIPT_DIR / "chars_ko_hanja_1800.txt"
KS1001_FILE = SCRIPT_DIR / "chars_ko_ks1001_2350.txt"
JAMO_FILE = SCRIPT_DIR / "chars_ko_jamo.txt"
OUTPUT_FILE = SCRIPT_DIR / "ko_common_chars.txt"
UI_OUTPUT_FILE = SCRIPT_DIR / "ko_ui_chars.txt"
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
    parser.add_argument("--ui-output", type=Path, default=UI_OUTPUT_FILE)
    parser.add_argument("--i18n-output", type=Path, default=I18N_OUTPUT_FILE)
    args = parser.parse_args()

    hangul = load_chars(HANGUL_FILE)
    hanja = load_chars(HANJA_FILE)
    ks1001 = load_chars(KS1001_FILE)
    jamo = load_chars(JAMO_FILE)
    required = load_required([Path(p) for p in args.require_from])

    if not ks1001 <= hangul:
        print("error: KS X 1001 pool contains non-modern-Hangul chars", file=sys.stderr)
        sys.exit(1)

    kept = sorted(hangul | hanja | jamo | required)
    ui = sorted(ks1001 | jamo | required)
    i18n = sorted(jamo | required)

    out = args.output if args.output.is_absolute() else SCRIPT_DIR / args.output
    ui_out = args.ui_output if args.ui_output.is_absolute() else SCRIPT_DIR / args.ui_output
    i18n_out = args.i18n_output if args.i18n_output.is_absolute() else SCRIPT_DIR / args.i18n_output
    out.write_text("".join(kept), encoding="utf-8")
    ui_out.write_text("".join(ui), encoding="utf-8")
    i18n_out.write_text("".join(i18n), encoding="utf-8")

    beyond = sorted(required - hangul - hanja - jamo)
    print(
        f"Wrote {len(kept)} chars → {out.name} "
        f"(hangul {len(hangul)} ∪ hanja {len(hanja)} ∪ jamo {len(jamo)} ∪ required {len(required)})",
        file=sys.stderr,
    )
    print(
        f"Wrote {len(ui)} UI chars → {ui_out.name} "
        f"(KS X 1001 {len(ks1001)} ∪ jamo {len(jamo)} ∪ required {len(required)})",
        file=sys.stderr,
    )
    print(f"Wrote {len(i18n)} i18n/jamo chars → {i18n_out.name}", file=sys.stderr)
    if beyond:
        print(f"[required] +{len(beyond)} beyond hangul/hanja/jamo: {''.join(beyond)}", file=sys.stderr)


if __name__ == "__main__":
    main()
