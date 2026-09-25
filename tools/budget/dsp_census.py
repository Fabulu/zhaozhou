#!/usr/bin/env python3
"""DSP census: what MODE each block's multipliers are in, not just how many.

WHY THE MODE AND NOT THE COUNT
------------------------------
`tools/budget/map_entity_attrib.py` already says where the console's 375 DSP
blocks live. It cannot say whether a block's multipliers are EXPENSIVE, and that
is the question an optimization packet actually has.

On Cyclone V a DSP block in `Two Independent 18x18` mode is doing two
multiplies; the same block in `Independent 27x27` mode is doing one. So a 27x27
costs twice what an 18x18 costs, per product. **A block whose DSPs are all 18x18
is doing arithmetic it needs. A block full of 27x27s is either multiplying wide
values or DECLARING wide ones** -- and the second is free to fix, because
operand width is what DSP inference follows.

Quartus prints exactly this in every `.map.rpt`, in `Analysis & Synthesis DSP
Block Usage Summary`. This tool reads it across a labelled set of block maps and
puts the modes side by side. It measures nothing itself: it is the comparison
side, which is the only side a tool belongs on.

HOW TO PRODUCE THE INPUT
    tools/quartus/run_block_fit.ps1 -Module <a>,<b>,<c> -MapOnly -RowLabel '@<label>'
(the label must start with `@` or `-`; the row name is `$mod$RowLabel` glued).
A leaf block maps in 15-120 s, so a census of a dozen is minutes, not a fit.

USAGE
    python tools/budget/dsp_census.py [--label @dsp-census-20260926]
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
    a("# DSP census — `%s`" % args.label)
    a("")
    a("Each row is a block mapped **on its own** by `quartus_map`, so nothing in")
    a("the composition is inflating or sharing it. Modes come from Quartus's own")
    a("`DSP Block Usage Summary`. Derived by `tools/budget/dsp_census.py`.")
    a("")
    a("**A `Two Independent 18x18` block is doing two multiplies; an")
    a("`Independent 27x27` block is doing one.** So the 27x27 column is the")
    a("expensive one, and it is the column that says whether a block is")
    a("multiplying wide VALUES or merely declaring wide ones.")
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
