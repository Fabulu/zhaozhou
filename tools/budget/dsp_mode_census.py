#!/usr/bin/env python3
"""DSP MODE census: what mode each block's multipliers are in.

NOT `tools/budget/dsp_census.py`, which is a different and older
instrument -- the RESOURCE BILL, one selected measurement per instance
across every ledger, with the selection order from the owner's rescue
brief section 2.3. That one answers "what does the machine cost".
This one answers only "what SHAPE are a block's multipliers", which the
bill does not record and cannot infer.

(This file was briefly written OVER that tool on 2026-09-26. Restored
from git and renamed; `uncashed_cheques.py` imports `load_evidence` and
`commit_time` from the real one and is how the collision surfaced.)

WHY THE MODE AND NOT THE COUNT
------------------------------
`tools/budget/map_entity_attrib.py` says where the console's 375 DSP blocks
live and `tools/budget/dsp_census.py` says what the machine is billed for.
Neither says whether a block's multipliers are EXPENSIVE, and that
is the question an optimization packet actually has.

On Cyclone V a DSP block in `Two Independent 18x18` mode is doing two
multiplies; the same block in `Independent 27x27` mode is doing one. So a 27x27
costs twice what an 18x18 costs, per product. **A block whose DSPs are all 18x18
is doing arithmetic it needs. A block full of 27x27s is either multiplying wide
values or DECLARING wide ones** -- and the second is sometimes free to fix.

CORRECTED 2026-09-26 BY THE MEASUREMENT THIS TOOL EXISTS TO PROMPT. These two
lines used to end "because operand width is what DSP inference follows", and
that is FALSE on Quartus 17.0.2 in the shape everybody writes. The ATTRSETUP
packet narrowed `zhao_geom_attrsetup`'s declared widths one group at a time --
96x96 -> 46x32, 46x46 -> 22x21, 72x72 -> 22x32 -- and ALL THREE cost exactly
nothing: 45 DSP before, 45 DSP after, identical mode table, identical ALUTs.
Quartus already strips the sign extension in the plain
`WIDE'(narrow) * WIDE'(narrow)` form and was already multiplying at the true
widths.

What DID cost 9 of that block's 24 Independent 27x27 was one operand written
`(-(72'(cy_by))) * 72'(va_i)` -- the NEGATION TAKEN INSIDE THE CAST. Moving the
minus sign outside the multiply, with every declared width left at 72, recovers
all nine (probe arm 7). `-(sext(x,72))` is a 72-bit subtract from zero, and
after it the top 50 bits are no longer a recognisable replication of the sign
bit, so Quartus must multiply a genuine 72-bit operand.

SO THE ACTIONABLE PATTERN IS NOT "A WIDE LITERAL". It is AN ARITHMETIC
OPERATION APPLIED TO A WIDENED VALUE BEFORE THE MULTIPLY. A wide cast is free;
`-(WIDE'(x)) * ...`, and plausibly `(WIDE'(x) op y) * ...`, is not. The same
file proves the distinction twice over: its edge products negate the PRODUCT,
`-(46'(cx_bx) * 46'(by_i))`, twelve lines away from the partials that negated
the OPERAND, and the first shape costs nothing while the second cost nine
blocks.

A reader who takes the 27x27 column as "declared-width money waiting to be
collected" will spend a packet and collect zero. The column still SORTS blocks
usefully -- it is why attrsetup was looked at first, and that was right -- but
it is a reason to look, not a diagnosis. Evidence:
tests/probes/zhao_attrsetup_mul_probe.sv, eight arms, and the
zhao_attrsetup_mul_probe@probe-m0..m7 rows.

Quartus prints exactly this in every `.map.rpt`, in `Analysis & Synthesis DSP
Block Usage Summary`. This tool reads it across a labelled set of block maps and
puts the modes side by side. It measures nothing itself: it is the comparison
side, which is the only side a tool belongs on.

HOW TO PRODUCE THE INPUT
    tools/quartus/run_block_fit.ps1 -Module <a>,<b>,<c> -MapOnly -RowLabel '@<label>'
(the label must start with `@` or `-`; the row name is `$mod$RowLabel` glued).
A leaf block maps in 15-120 s, so a census of a dozen is minutes, not a fit.

USAGE
    python tools/budget/dsp_mode_census.py [--label @dsp-census-20260926]
                                      [--markdown OUT.md]

EXIT CODE
    0  census produced
    2  no maps found for that label
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import sys

SHIP_DSP = 112  # 5CSEBA6U23I7

BLOCKPATHS = os.path.join("reports", "synthesis", "blockpaths")

# The modes Quartus names, ordered cheap to expensive. "Two Independent 18x18"
# means ONE block doing two products; "Independent 27x27" means one block doing
# one. Anything after the first two is a wide or fused shape.
MODES = [
    "Two Independent 18x18",
    "Independent 18x18",
    "Sum of two 18x18",
    "Independent 18x18 plus 36",
    "Independent 27x27",
    "Sum of two 27x27",
]

SUM_HDR = "Analysis & Synthesis DSP Block Usage Summary"


def read_summary(path: str) -> dict:
    """`Total DSP Blocks` and `Total registers` out of a .map.summary."""
    out: dict[str, int] = {}
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if ":" not in line:
                continue
            k, v = line.split(":", 1)
            k = k.strip()
            v = v.strip().replace(",", "")
            if k in ("Total DSP Blocks", "Total registers",
                     "Total block memory bits"):
                try:
                    out[k] = int(v)
                except ValueError:
                    pass
    return out


def read_modes(path: str) -> dict:
    """The DSP Block Usage Summary table, mode -> count.

    The report contains the header string more than once (a table of contents
    entry and the table itself), so the LAST occurrence is the table.
    """
    modes: dict[str, int] = {}
    if not os.path.exists(path):
        return modes
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    idx = text.rfind(SUM_HDR)
    if idx < 0:
        return modes
    chunk = text[idx: idx + 4000]
    for line in chunk.splitlines():
        if not line.startswith(";"):
            continue
        cells = [c.strip() for c in line.split(";") if c.strip()]
        if len(cells) != 2:
            continue
        name, val = cells
        if not re.fullmatch(r"\d+", val):
            continue
        if name in MODES:
            modes[name] = int(val)
    return modes


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", default="@dsp-census-20260926")
    ap.add_argument("--markdown")
    args = ap.parse_args()

    pat = os.path.join(BLOCKPATHS, "*" + args.label + ".map.summary")
    found = sorted(glob.glob(pat))
    if not found:
        print("no block maps matching %s" % pat, file=sys.stderr)
        print("(produce them with run_block_fit.ps1 -MapOnly -RowLabel '%s')"
              % args.label, file=sys.stderr)
        return 2

    rows = []
    for sm in found:
        module = os.path.basename(sm)[: -len(args.label + ".map.summary")]
        s = read_summary(sm)
        m = read_modes(sm[: -len(".map.summary")] + ".map.rpt")
        rows.append((module, s, m))
    rows.sort(key=lambda r: -r[1].get("Total DSP Blocks", 0))

    seen_modes = [k for k in MODES if any(k in m for _, _, m in rows)]
    total = sum(s.get("Total DSP Blocks", 0) for _, s, _ in rows)
    wide = sum(m.get("Independent 27x27", 0) + m.get("Sum of two 27x27", 0)
               for _, _, m in rows)

    L: list[str] = []
    a = L.append
    a("# DSP MODE census — `%s`" % args.label)
    a("")
    a("Each row is a block mapped **on its own** by `quartus_map`, so nothing in")
    a("the composition is inflating or sharing it. Modes come from Quartus's own")
    a("`DSP Block Usage Summary`. Derived by `tools/budget/dsp_mode_census.py`.")
    a("")
    a("**A `Two Independent 18x18` block is doing two multiplies; an")
    a("`Independent 27x27` block is doing one.** So the 27x27 column is the")
    a("expensive one, and it is the column that sorts blocks worth looking at.")
    a("")
    a("**IT IS NOT A DIAGNOSIS, AND \"DECLARED WIDTH\" IS THE WRONG ONE.** This")
    a("header used to say the 27x27 column separates blocks that multiply wide")
    a("VALUES from blocks that merely DECLARE wide ones, and that the second is")
    a("free to fix because inference follows declared width. Measured on")
    a("2026-09-26, that is false on Quartus 17.0.2: narrowing")
    a("`zhao_geom_attrsetup`'s declared widths one group at a time (96x96 ->")
    a("46x32, 46x46 -> 22x21, 72x72 -> 22x32) moved the row by ZERO blocks each")
    a("time. Quartus already strips the plain `WIDE'(narrow) * WIDE'(narrow)`")
    a("sign extension. What cost 9 of that block's 24 wide blocks was one")
    a("operand written `(-(72'(cy_by))) * 72'(va_i)` -- **the negation taken")
    a("INSIDE the cast** -- and moving the minus sign outside the multiply, at")
    a("unchanged declared width, recovered all nine.")
    a("")
    a("So the pattern to grep for is **an arithmetic operation applied to a")
    a("widened value before the multiply**, not a wide literal. Evidence:")
    a("`tests/probes/zhao_attrsetup_mul_probe.sv`, eight arms, rows")
    a("`zhao_attrsetup_mul_probe@probe-m0..m7`.")
    a("")
    a("**A ROW HERE IS A LABELLED SNAPSHOT, NOT THE CURRENT DESIGN.** The table")
    a("below is whatever `--label` selected. `zhao_geom_attrsetup` was repaired")
    a("to **36 DSP / 15 wide / 836 ALUTs** on 2026-09-26 (`@gz-after`); any row")
    a("above showing it at 45 is the pre-repair measurement and is correct as")
    a("history, not as a budget.")
    a("")
    hdr = "| block | DSP | % of part | registers |"
    sep = "|---|---:|---:|---:|"
    for k in seen_modes:
        hdr += " %s |" % k
        sep += "---:|"
    a(hdr)
    a(sep)
    for module, s, m in rows:
        d = s.get("Total DSP Blocks", 0)
        line = "| `%s` | %d | %.0f%% | %d |" % (
            module, d, 100.0 * d / SHIP_DSP, s.get("Total registers", 0))
        for k in seen_modes:
            line += " %s |" % (m.get(k, "") if m.get(k) else "")
        a(line)
    a("")
    a("**Census total: %d DSP across %d block(s), %.0f%% of the %d-DSP part.**"
      % (total, len(rows), 100.0 * total / SHIP_DSP, SHIP_DSP))
    if wide:
        a("**%d of those %d are in a 27x27 mode**, which is the shape to ask about"
          % (wide, total))
        a("first.")
    a("")
    a("This is a census of what was mapped, not of the console. For the whole")
    a("machine see `reports/synthesis/console_entity_attrib.md`.")
    a("")

    text = "\n".join(L)
    if args.markdown:
        with open(args.markdown, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        print("wrote %s (%d block(s))" % (args.markdown, len(rows)))
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
