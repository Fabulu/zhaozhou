#!/usr/bin/env python3
"""Per-entity ALUT/register/memory attribution from a Quartus map report.

WHY THIS EXISTS, AND WHY IT IS COMMITTED
----------------------------------------
The full console fit of 2026-09-25 (`zhao_console_core@edgeclose`) FAILED in
`quartus_fit` with

    Error (170011): Design contains 336023 blocks of type combinational node.
                    However, the device contains only 227120 blocks.

so it produced no placed ALMs and no Fmax.  It was recorded at the time that the
per-entity breakdown "lives in a fitter report that does not exist".

THAT WAS FALSE, AND IT COST NOTHING TO DISPROVE.  `quartus_map` SUCCEEDED -- the
failure was in the fitter, one stage later -- and Analysis & Synthesis writes its
own `Resource Utilization by Entity` table.  The 21 MB `.map.rpt` holding it was
sitting in EDGECLOSE's worktree the whole time.  A fit that cannot place still
tells you exactly what the logic is made of.

So this is the damage-control map for the standing goal's phase 3, and it is
FREE: no new fit, no new measurement, just reading a report we already paid
01:54:49 and 14:19:23 of CPU for.

THE SOURCE REPORT IS GITIGNORED (`.gitignore:158`, `blockpaths/*.map.rpt`) and it
should stay that way -- it is 21 MB of intermediate.  The committed evidence is
the TABLE this tool derives, exactly as the committed evidence for a render is
the contact sheet and not the raw frames.  This tool is what makes that table
reproducible instead of a number somebody once pasted.  Keep the two together:
a table with no probe beside it is unverifiable, and a probe with no output
committed is one nobody runs.

USAGE
    python tools/budget/map_entity_attrib.py <path-to.map.rpt> [--top N]
           [--markdown OUT.md] [--json OUT.json] [--drill ENTITY]...

EXIT CODE
    0  table produced
    2  the report could not be parsed (no entity table found)
"""

from __future__ import annotations

import argparse
import json
import re
import sys

# The shipping part, from design/blocks.yml's closure target. An ALM on
# Cyclone V carries two ALUTs and four registers, so these are the two ceilings
# a synthesis-only measurement can be held against -- neither is a placement
# result and neither should be quoted as one.
SHIP_PART = "5CSEBA6U23I7"
SHIP_ALM = 41910
SHIP_ALUT = SHIP_ALM * 2
SHIP_REGS = SHIP_ALM * 4
SHIP_M10K = 553
SHIP_MEMBITS = SHIP_M10K * 10240


def _num(s: str) -> int:
    m = re.match(r"^(\d+)", s.strip())
    return int(m.group(1)) if m else 0


def _own(s: str) -> int:
    m = re.search(r"\((\d+)\)", s)
    return int(m.group(1)) if m else 0


def parse(path: str) -> list[dict]:
    """Rows of the Resource Utilization by Entity table, in report order.

    `alut`/`reg` are SUBTREE totals; `alut_own`/`reg_own` are what the node
    itself holds. Quartus prints them as "total (own)". Mixing the two is the
    easy way to double count, so both are kept and neither is guessed.
    """
    rows: list[dict] = []
    started = False
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if not started:
                if line.startswith("; Compilation Hierarchy Node"):
                    started = True
                continue
            if not line.startswith(";"):
                if rows:
                    break
                continue
            if line.startswith("+"):
                if rows:
                    break
                continue
            cells = [c.strip() for c in line.split(";")]
            if len(cells) < 9:
                continue
            node_raw = line.split(";")[1]
            node = cells[1]
            if not node.startswith("|"):
                continue
            indent = len(node_raw) - len(node_raw.lstrip(" "))
            rows.append(
                dict(
                    depth=(indent - 1) // 3,
                    node=node.strip("|"),
                    alut=_num(cells[2]),
                    alut_own=_own(cells[2]),
                    reg=_num(cells[3]),
                    reg_own=_own(cells[3]),
                    mem=_num(cells[4]),
                    dsp=_num(cells[5]),
                    full=cells[8],
                )
            )
    return rows


def children(rows: list[dict], parent: dict) -> list[dict]:
    pre = parent["full"] + "|"
    return [r for r in rows
            if r["depth"] == parent["depth"] + 1 and r["full"].startswith(pre)]


def top_disjoint(rows: list[dict], key: str, n: int) -> list[dict]:
    """Biggest subtrees by `key`, skipping anything already inside a listed one.

    Without the skip the same registers appear three times under three
    ancestors and the list reads as though the design were four times its size.
    """
    out: list[dict] = []
    seen: list[str] = []
    for r in sorted(rows[1:], key=lambda x: -x[key]):
        if any(r["full"].startswith(p + "|") for p in seen):
            continue
        seen.append(r["full"])
        out.append(r)
        if len(out) >= n:
            break
    return out


def render(rows: list[dict], top_n: int, drills: list[str]) -> str:
    top = rows[0]
    L: list[str] = []
    a = L.append

    a("# Per-entity attribution — `%s`" % top["node"])
    a("")
    a("Derived by `tools/budget/map_entity_attrib.py` from the Analysis &")
    a("Synthesis entity table. **Synthesis estimates, not a placement result:**")
    a("this design has never placed, so there are no ALM figures and no Fmax,")
    a("and nothing here should be quoted as either.")
    a("")
    a("| | measured | against %s |" % SHIP_PART)
    a("|---|---:|---:|")
    a("| combinational ALUTs | %d | %.0f%% of ~%d |"
      % (top["alut"], 100.0 * top["alut"] / SHIP_ALUT, SHIP_ALUT))
    a("| dedicated logic registers | %d | %.0f%% of %d |"
      % (top["reg"], 100.0 * top["reg"] / SHIP_REGS, SHIP_REGS))
    a("| block memory bits | %d | %.0f%% of %d |"
      % (top["mem"], 100.0 * top["mem"] / SHIP_MEMBITS, SHIP_MEMBITS))
    a("| DSP blocks | %d | |" % top["dsp"])
    a("")
    a("**The registers alone need at least %d ALM, %.0f%% of the part, with the"
      % (top["reg"] // 4, 100.0 * (top["reg"] / 4) / SHIP_ALM))
    a("combinational logic at zero.** Memory, by contrast, fits: %.0f%%. That is"
      % (100.0 * top["mem"] / SHIP_MEMBITS))
    a("the whole shape of the problem in two numbers — storage held in flip-flops")
    a("is what overflows this device, and M10K is where the slack is.")
    a("")

    a("## Biggest subtrees by combinational ALUTs")
    a("")
    a("| entity | ALUTs | % of part | registers | mem bits | DSP |")
    a("|---|---:|---:|---:|---:|---:|")
    for r in top_disjoint(rows, "alut", top_n):
        a("| `%s` | %d | %.0f%% | %d | %d | %d |"
          % (r["node"], r["alut"], 100.0 * r["alut"] / SHIP_ALUT,
             r["reg"], r["mem"], r["dsp"]))
    a("")

    a("## Biggest subtrees by REGISTERS")
    a("")
    a("A module with many registers and **no block memory bits** is holding an")
    a("array in flip-flops. That is the EARTHRAM lever and it is where the ALMs")
    a("are: one such array cost 5,181 registers and ~2,698 estimated ALMs, and")
    a("4,880 M10K bits bought all of it back at zero added cycles.")
    a("")
    a("| entity | registers | % of part | ALUTs | mem bits |")
    a("|---|---:|---:|---:|---:|")
    for r in top_disjoint(rows, "reg", top_n):
        a("| `%s` | %d | %.0f%% | %d | %d |"
          % (r["node"], r["reg"], 100.0 * r["reg"] / SHIP_REGS,
             r["alut"], r["mem"]))
    a("")

    for name in drills:
        p = next((r for r in rows if r["node"] == name), None)
        if p is None:
            a("## `%s` — NOT FOUND in this report" % name)
            a("")
            continue
        a("## Inside `%s`" % p["node"])
        a("")
        a("Subtree: %d ALUTs, %d registers. **Held by the node itself, not by any"
          % (p["alut"], p["reg"]))
        a("child: %d ALUTs, %d registers.**" % (p["alut_own"], p["reg_own"]))
        a("")
        kids = sorted(children(rows, p), key=lambda r: -r["alut"])
        kids = [k for k in kids if k["alut"] or k["reg"]]
        if not kids:
            a("It has no hierarchy under it at all — every one of those registers")
            a("is in this one module.")
            a("")
            continue
        a("| child | ALUTs | registers | mem bits |")
        a("|---|---:|---:|---:|")
        for k in kids:
            a("| `%s` | %d | %d | %d |" % (k["node"], k["alut"], k["reg"], k["mem"]))
        a("")

    return "\n".join(L) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("report", help="a quartus_map .map.rpt")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--drill", action="append", default=[],
                    help="entity to break down; repeatable")
    ap.add_argument("--markdown")
    ap.add_argument("--json")
    args = ap.parse_args()

    rows = parse(args.report)
    if not rows:
        print("no entity table found in %s" % args.report, file=sys.stderr)
        print("(is this a .map.rpt? the .fit.rpt does NOT carry this table)",
              file=sys.stderr)
        return 2

    text = render(rows, args.top, args.drill)
    if args.markdown:
        with open(args.markdown, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        print("wrote %s (%d hierarchy rows read)" % (args.markdown, len(rows)))
    else:
        sys.stdout.write(text)
    if args.json:
        with open(args.json, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(rows, fh, indent=1)
        print("wrote %s" % args.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
