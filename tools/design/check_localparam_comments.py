#!/usr/bin/env python3
r"""check_localparam_comments.py -- does a DERIVED constant still equal the
number written beside it?

WHY THIS EXISTS (terrain7, 2026-09-20)
--------------------------------------
`zhao_terrain_devstore.sv` declared

    localparam int unsigned RECW = 3 * DEVW + 16;       // 88
    localparam int unsigned HALF = SUBS / 2;            // 8 records per row
    localparam int unsigned ROWW = HALF * RECW;         // 576      <-- 704
    localparam int unsigned ACCW = ROWW - RECW;         // 504      <-- 616

The record gained a 16-bit subpatch centre height after the comments were
written.  `RECW`'s own comment was updated to 88; the two that DEPEND on it
were not, and nothing read them back.

That would be a cosmetic defect if the numbers stayed in the comments.  They
did not.  The block's header spends a whole section discharging **owner ruling
R59** -- "report both sizes" -- and it computes those sizes from 576, so the
store was published at **116 M10K** when its own parameters say **141**.  The
same understated figure was then quoted in `zhao_console_core.sv`'s entry I21
and in a packet report, as the price of composing it.

**It is wrong in the flattering direction**, which is this repository's standing
law about broken instruments: a store that costs 21% of the device sounds
affordable and one that costs 29% does not, so nobody audited it.

WHAT IT CHECKS
--------------
A `localparam`/`parameter` whose value is an INTEGER EXPRESSION over constants
already declared in the same module, and which carries a trailing comment whose
first token is a bare number, is making a CLAIM about its own value.  This tool
evaluates the expression and compares.

It is deliberately narrow:

  * only integer arithmetic (`+ - * / % << >> ( )`) over names it has already
    resolved IN THE SAME MODULE.  An unresolvable name is SKIPPED, never
    guessed -- a guessed width would be a measurement that decides a value,
    which this repository does not do.
  * the comment must LEAD with a number (`// 704`, `// 2,048 rows`,
    `// 8 records per row`).  `// one per lane` and `// see T9` are prose and
    are not claims about the value.
  * thousands separators are read (`2,048`), because that is how this tree
    writes them.

WHAT IT IS NOT
--------------
It is a COMPARISON-SIDE tool, like `check_array_storage.py`.  It never edits
RTL and it decides nothing: it reports that two things someone wrote disagree.

FIRING IT
---------
A detector that has not been seen to fire is a claim, not an instrument.  This
one asserts AT IMPORT that its own parser still finds a planted mismatch in a
known-good example (`_self_test`), because a self-check whose pattern matches
nothing prints reassurance for its whole life.  Run with `--self-test` to see
the positive control alone.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

# A parameter or localparam with an `= <expr>`.  The terminator is optional
# because the LAST entry of a module parameter port list has none:
#     parameter int unsigned MORPHW = 17
#     ) (
DECL_RE = re.compile(
    r"^\s*(?:localparam|parameter)\s+"
    r"(?:type\s+)?"
    r"(?:(?:var|automatic|static)\s+)?"
    r"(?:(?:int|integer|longint|shortint|byte|bit|logic|reg)\s+)?"
    r"(?:(?:unsigned|signed)\s+)?"
    r"(?:\[[^\]]*\]\s*)?"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_$]*)\s*"
    r"=\s*(?P<expr>[^;,]+?)\s*[,;]?\s*$"
)

MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z_][A-Za-z0-9_$]*)")
ENDMODULE_RE = re.compile(r"^\s*endmodule\b")

# THE COMMENT MUST BE NOTHING BUT A NUMBER.
#
# The first version of this tool accepted any comment that LED with a number,
# on the reasoning that `// 8 records per row` is still a claim.  Measured
# against the tree, that rule produced THIRTEEN findings and every one was
# prose describing a different quantity -- `// 64 B / (8 words * 2 B)` beside
# a 4, `// 21x21 signed` beside a 42, `// 64 x 64, charter 12` beside a 4,096 --
# while MISSING the one real stale comment the tool was written for.  A
# detector with thirteen false positives and zero true ones is not a strict
# detector, it is a broken one, and it fails in the direction that gets it
# switched off.  A comment that is a bare number is unambiguously a restatement
# of the value; anything else is a sentence, and a sentence is not a claim this
# tool is able to check.
BARENUM_RE = re.compile(r"^(?P<num>\d[\d,]*)$")

# Only pure integer arithmetic over identifiers is evaluated.
SAFE_EXPR_RE = re.compile(r"^[\w\s+\-*/%()<>]+$")
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_$]*")


def _eval_expr(expr: str, consts: dict[str, int]) -> int | None:
    """Evaluate an integer expression over `consts`, or None if it cannot be."""
    expr = expr.strip()
    # Reject anything with a sized literal, a concat, a ternary, a function
    # call or a string -- all legal SystemVerilog and none of it arithmetic
    # this tool is willing to guess at.
    if "'" in expr or "{" in expr or "?" in expr or '"' in expr:
        return None
    if not SAFE_EXPR_RE.match(expr):
        return None
    # `<` and `>` only ever appear here as part of a shift.
    if re.search(r"(?<!<)<(?!<)", expr) or re.search(r"(?<!>)>(?!>)", expr):
        return None
    for ident in set(IDENT_RE.findall(expr)):
        if ident not in consts:
            return None
    # Substitute longest-first so FOO_BAR is not clobbered by FOO.
    def sub(m: re.Match[str]) -> str:
        return str(consts[m.group(0)])

    pyexpr = IDENT_RE.sub(sub, expr)
    # SystemVerilog `/` on integers truncates; Python `/` does not.
    pyexpr = pyexpr.replace("/", "//")
    try:
        value = eval(pyexpr, {"__builtins__": {}}, {})  # noqa: S307
    except Exception:
        return None
    if not isinstance(value, int):
        return None
    return value


DECL_START_RE = re.compile(r"^\s*(?:localparam|parameter)\b")


def _decl_extent(lines: list[str], i: int) -> int:
    """Index of the LAST physical line of the declaration starting at `i`.

    THE TOOL READ ONE LINE AT A TIME AND SYSTEMVERILOG IS NOT A ONE-LINE
    LANGUAGE.  A constant written across lines --

        localparam int PAYW = (READ_LATE != 0)
            ? 47
            : (47 + 4 * TEXTURE_RESULT_W);   // 239

    -- never matched `DECL_RE` at all, so it was never evaluated, its comment
    was never checked, AND every later constant derived from it silently became
    unevaluable too (`value is None: continue`).  Found 2026-09-23 by the
    ARENAWIRE lane, which planted a deliberately WRONG number in one of four
    new constants and still got "disagreements : 0" -- three of its four were
    multi-line and none of them was being read.

    That is the same single-line assumption `tools/rtl/check_v3_banks.py`
    carried in its own parameter parser, repaired the same day.  Two tools, one
    blind spot, and both failed silently in the flattering direction.

    The terminator is found by paren DEPTH, not by forbidding newlines: a
    declaration ends at `;` or at a `,` at depth zero, and a parameter PORT
    entry with no terminator at all ends at the `)` that closes the list --
    which is why the closer is a stop and is not consumed.
    """
    depth = 0
    for k in range(i, min(i + 40, len(lines))):
        code = lines[k].partition("//")[0]
        for c in code:
            if c in "([{":
                depth += 1
            elif c in ")]}":
                if depth == 0:
                    return k - 1 if k > i else k
                depth -= 1
            elif depth == 0 and c in ";,":
                return k
    return i


def scan_text(text: str, path: str) -> list[dict]:
    """Return every disagreement between a derived constant and its comment."""
    findings: list[dict] = []
    consts: dict[str, int] = {}
    module = "<file scope>"
    lines = text.splitlines()
    i = -1
    while i + 1 < len(lines):
        i += 1
        lineno = i + 1
        line = lines[i]
        mod = MODULE_RE.match(line)
        if mod:
            module = mod.group(1)
            consts = {}
            continue
        if ENDMODULE_RE.match(line):
            consts = {}
            continue
        # Split the code from its trailing comment FIRST: the expression may
        # legitimately contain `/`, so a regex that tries to do both at once
        # either loses division or swallows the comment.
        code, _sep, comment = line.partition("//")
        if DECL_START_RE.match(code) and "=" in code:
            end = _decl_extent(lines, i)
            if end > i:
                # The CLAIM is the comment on the line that terminates the
                # declaration -- that is where an author writes the value --
                # and the code is the joined expression.  The finding is still
                # reported at the line the declaration STARTS on.
                code = " ".join(lines[k].partition("//")[0].strip()
                                for k in range(i, end + 1))
                comment = lines[end].partition("//")[2]
                i = end
        decl = DECL_RE.match(code)
        if not decl:
            continue
        name = decl.group("name")
        value = _eval_expr(decl.group("expr"), consts)
        if value is None:
            continue
        # Record it BEFORE comparing: a wrong comment does not make the
        # constant unusable by the next line, and skipping it here would hide
        # every downstream disagreement behind the first one.
        consts[name] = value
        comment = comment.strip()
        if not comment:
            continue
        bare = BARENUM_RE.match(comment)
        if not bare:
            continue
        claimed = int(bare.group("num").replace(",", ""))
        if claimed != value:
            findings.append(
                {
                    "path": path,
                    "line": lineno,
                    "module": module,
                    "name": name,
                    "expr": decl.group("expr").strip(),
                    "claimed": claimed,
                    "actual": value,
                    "comment": comment,
                }
            )
    return findings


# --------------------------------------------------------------------------
# POSITIVE CONTROL.  Asserted at import, because a self-check whose pattern
# matches nothing prints reassurance for its whole life.
# --------------------------------------------------------------------------
# It reproduces `zhao_terrain_devstore`'s real shape, including the two traps
# that made the first version of this tool useless:
#   * DEVW arrives in the MODULE PARAMETER PORT LIST, terminated by a comma,
#     and MORPHW by nothing at all.  A parser that requires `;` resolves
#     neither, so RECW and ROWW become unevaluable and the tool reports the
#     file CLEAN -- silence in the flattering direction.
#   * `// 64 B / (8 words * 2 B)` beside a 4 is prose, not a claim.  Every one
#     of the thirteen findings the first rule produced was this.
_SELF_TEST_SRC = """
module zhao_selftest_example #(
    parameter int unsigned DEVW   = 24,
    parameter int unsigned MORPHW = 17
) (
    input var logic clk
);
  localparam int unsigned SUBS  = 16;
  localparam int unsigned RECW  = 3 * DEVW + 16;   // 88
  localparam int unsigned HALF  = SUBS / 2;        // 8 records per row
  localparam int unsigned ROWW  = HALF * RECW;     // 576
  localparam int unsigned HISTW = 2 + MORPHW + 8;  // 27
  localparam int unsigned OKAY  = SUBS * 2;        // 32
  localparam int unsigned BEATS = SUBS / 4;        // 64 B / (8 words * 2 B)
  localparam int unsigned PROSE = SUBS + 1;        // one per lane
  localparam int unsigned WIDE  = SUBS
                                + RECW
                                + 4;               // 100
  localparam int unsigned TALL  = SUBS
                                * 2;               // 32
  localparam int unsigned DERIV = TALL + 1;        // 33
endmodule
"""


def _self_test() -> list[dict]:
    found = scan_text(_SELF_TEST_SRC, "<self-test>")
    names = {f["name"] for f in found}
    by = {f["name"]: f for f in found}
    # ROWW fires (576 against 704).  HISTW must NOT -- it proves MORPHW, the
    # unterminated last parameter, resolved.  BEATS must NOT -- it proves the
    # prose rule.  OKAY and RECW must not -- they agree.
    #
    # WIDE fires and TALL does not, and BOTH are needed: a joiner that reads
    # multi-line declarations is worth nothing unless it still DISAGREES when
    # the number is wrong, and worth less than nothing if it disagrees when the
    # number is right.  DERIV must not fire either -- it proves TALL, computed
    # across three lines, actually entered `consts` for the constants after it.
    # Before 2026-09-23 all three were invisible and the file read CLEAN.
    assert names == {"ROWW", "WIDE"}, \
        f"self-test expected {{ROWW, WIDE}}, saw {names}"
    assert by["ROWW"]["claimed"] == 576 and by["ROWW"]["actual"] == 704, by["ROWW"]
    assert by["WIDE"]["claimed"] == 100 and by["WIDE"]["actual"] == 108, by["WIDE"]
    return found


_self_test()


def iter_sv(root: str, subdirs: list[str]):
    for sub in subdirs:
        base = os.path.join(root, sub)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirnames, filenames in os.walk(base):
            for fn in sorted(filenames):
                if fn.endswith(".sv") or fn.endswith(".svh"):
                    yield os.path.join(dirpath, fn)


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=repo)
    ap.add_argument(
        "--dirs",
        default="fpga/rtl",
        help="comma-separated subdirectories to scan (default fpga/rtl)",
    )
    ap.add_argument(
        "--self-test",
        action="store_true",
        help="show the positive control and exit 0",
    )
    args = ap.parse_args()

    if args.self_test:
        fired = _self_test()
        print("positive control FIRED, as it must:")
        for f in fired:
            print(
                f"  {f['name']} = {f['expr']}  claims {f['claimed']}, "
                f"is {f['actual']}"
            )
        return 0

    print("localparam comment check -- a derived constant's trailing number")
    print("is a CLAIM about its value; this compares the two.")
    print("self-test PASSED: the parser sees a planted stale comment")
    print()

    findings: list[dict] = []
    scanned = 0
    for path in iter_sv(args.root, [d.strip() for d in args.dirs.split(",")]):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        scanned += 1
        findings.extend(scan_text(text, os.path.relpath(path, args.root)))

    print(f"files scanned : {scanned}")
    print(f"disagreements : {len(findings)}")
    if not findings:
        print()
        print("OK")
        return 0

    print()
    for f in sorted(findings, key=lambda x: (x["path"], x["line"])):
        print(f"{f['path']}:{f['line']}  {f['module']}")
        print(f"    {f['name']} = {f['expr']}")
        print(f"    comment claims {f['claimed']:,}, parameters say {f['actual']:,}")
    print()
    print("A number written beside a derived constant is read as evidence.")
    print("Update the comment, or the expression, so they agree.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
