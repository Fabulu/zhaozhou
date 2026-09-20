"""Refuse a directed test whose case labels do not identify their own block.

---------------------------------------------------------------------------
WHY THIS EXISTS
---------------------------------------------------------------------------
2026-09-20, owner ruling R62. `tests/command/cmd_exec_directed.cpp` had THREE
blocks numbered 17 -- the token ceiling, SetEnvironment and SetPost, merged in
from three lanes -- and the string `"case17:"` labelled 35 checks across all
three. A red check printed a number that named no block.

The numbers are comments and string literals. Nothing in the compiler, the
linker or ctest can see a collision, and the blocks passed for days while the
label was ambiguous. The failure is silent and it is in the flattering
direction: the report looks precise.

Two things are refused here, and the SECOND is the one worth having:

  1. two blocks carrying the same number;
  2. a `"caseN:"` label sitting in a block that is not N.

(2) is the likelier defect, because a label travels with the code it labels --
copy a check into a neighbouring block and it keeps pointing at where it came
from. A renumber fixes (1) once; only (2) stops it recurring.

A sub-case is allowed: `case6b` inside block 6 needs no header of its own, and
moves with its parent.

Run:  python tools/budget/check_case_labels.py
Exit: 0 clean, 1 labels wrong, 2 self-test failure.
"""

from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# The files that use the `// ---- N. title` / `"caseN: what"` convention. Add a
# file here when it adopts the convention; a file that does not use headers at
# all is reported as such rather than passing silently, because "0 blocks, 0
# problems" is the broken-instrument reading.
FILES = [
    "tests/command/cmd_exec_directed.cpp",
]

_HDR = re.compile(r"^\s*// ---- (\d+b?)\. ")
_TOK = re.compile(r'"case(\d+b?)[ :]')


def belongs(tok: str, block: str) -> bool:
    """Is label `tok` legitimately inside block `block`?"""
    return tok == block or (not block.endswith("b") and tok == block + "b")


def scan_text(text: str) -> list[str]:
    lines = text.split("\n")
    blocks = [(i, m.group(1)) for i, l in enumerate(lines)
              for m in [_HDR.match(l)] if m]
    problems = []
    if not blocks:
        return ["no `// ---- N. title` block headers found at all"]

    seen: dict[str, int] = {}
    for i, num in blocks:
        if num in seen:
            problems.append("line %d: block number %s is already used at line %d"
                            % (i + 1, num, seen[num] + 1))
        seen[num] = i

    bounds = [b[0] for b in blocks] + [len(lines)]
    for k, (i, num) in enumerate(blocks):
        for j in range(bounds[k], bounds[k + 1]):
            for m in _TOK.finditer(lines[j]):
                if not belongs(m.group(1), num):
                    problems.append(
                        'line %d: label "case%s" sits in block %s -- %s'
                        % (j + 1, m.group(1), num, lines[j].strip()[:70]))
    return problems


# ---------------------------------------------------------------------------
# SELF-TEST AT IMPORT. A detector that has not been shown to FIRE has not been
# tested -- and this one would read "0 problems" on an empty match, which is
# exactly the shape CLAUDE.md warns about.
# ---------------------------------------------------------------------------
_GOOD = (
    '  // ---- 1. a thing --------\n'
    '  check(a, "case1: it works", 1, 1);\n'
    '  check(b, "case1b: and the sub-case", 1, 1);\n'
    '  // ---- 2. another --------\n'
    '  check(c, "case2: also", 1, 1);\n'
)
_DUP = _GOOD + '  // ---- 1. a third, wrongly numbered --------\n'
_STRAY = (
    '  // ---- 1. a thing --------\n'
    '  check(a, "case1: it works", 1, 1);\n'
    '  // ---- 2. another --------\n'
    '  check(c, "case1: copied from block 1", 1, 1);\n'
)

_fail = []
if scan_text(_GOOD):
    _fail.append("FALSE POSITIVE on the clean sample: %s" % scan_text(_GOOD))
if not scan_text(_DUP):
    _fail.append("FAILED TO FIRE on a duplicate block number")
if not scan_text(_STRAY):
    _fail.append("FAILED TO FIRE on a label from another block")
if not scan_text(""):
    _fail.append("FAILED TO FIRE on a file with no headers")
if _fail:
    sys.stderr.write("check_case_labels SELF-TEST FAILED:\n")
    for f in _fail:
        sys.stderr.write("  %s\n" % f)
    raise SystemExit(2)


def main() -> int:
    print("check_case_labels: self-test 3 fire / 1 no-fire cases PASSED")
    rc = 0
    for rel in FILES:
        full = os.path.join(REPO, rel.replace("/", os.sep))
        if not os.path.exists(full):
            print("MISSING: %s" % rel)
            rc = 1
            continue
        text = io.open(full, encoding="utf-8", errors="replace", newline="").read()
        problems = scan_text(text)
        if problems:
            rc = 1
            print("\n%s -- %d problem(s):" % (rel, len(problems)))
            for p in problems:
                print("  %s" % p)
        else:
            nblocks = len([l for l in text.split("\n") if _HDR.match(l)])
            print("%s: %d blocks, every label names its own" % (rel, nblocks))
    return rc


if __name__ == "__main__":
    sys.exit(main())
