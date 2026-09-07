#!/usr/bin/env python3
"""entity_census.py -- where a fitted block's area and registers actually sit.

WHY THIS EXISTS
---------------
Quartus's Analysis & Synthesis report contains a "Resource Utilization by
Entity" table that attributes ALUTs, registers, memory bits and DSPs to every
node in the hierarchy, with a SELF figure in parentheses beside each TOTAL. It
answers "which part of this design is the expensive part" directly, and it is
written by every map run.

`reports/G1D-COMPOSED-ISLAND-20260905.md` §4.3a records that this census "could
not be attempted" because the RAM Summary "was deleted with the workspace, for
the second time in this project", and notes that PERSPUV's register census had
to be done by static analysis for exactly that reason, said so in its own
header, and WAS WRONG.

The runner was then fixed to harvest the map report. It worked. The file sat in
`reports/synthesis/blockpaths/` for a day and nobody opened it. When it finally
was, it said 48% of the composed island's registers are in the top-level glue
file itself -- four times more than the largest real block.

So this is committed rather than being a shell one-liner retyped each time,
under the same rule CLAUDE.md states for the ground-contact probe: a probe
written once and thrown away leaves numbers nobody can reproduce.

USAGE
    python tools/quartus/entity_census.py <module> [<module> ...]
    python tools/quartus/entity_census.py --list

Reads `reports/synthesis/blockpaths/<module>.map.rpt`. Prints the top entity's
self figures, then every direct child ranked by self registers, then the
memories declared in the top itself, then the uninferred-RAM verdicts -- which
are the reason an array that could be an M10K is not one, in Quartus's own
words rather than in a guess.
"""

from __future__ import annotations

import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join(REPO, "reports", "synthesis", "blockpaths")

SELF = re.compile(r"^(\d+)\s*\((\d+)\)$")


def census(path: str):
    t = io.open(path, encoding="utf-8", errors="replace").read()
    hits = [m.start() for m in
            re.finditer(re.escape("Analysis & Synthesis Resource Utilization by Entity"), t)]
    if not hits:
        return None, [], []
    sec = t[hits[-1]:hits[-1] + 400000]

    top = None
    kids = []
    for line in sec.splitlines():
        if not line.startswith(";"):
            continue
        f = [c.strip() for c in line.split(";")]
        if len(f) < 9:
            continue
        ma, mr = SELF.match(f[2]), SELF.match(f[3])
        if not ma or not mr:
            continue
        depth = f[8].count("|") - 1
        row = {
            "name": f[1].strip("| "),
            "alut": int(ma.group(1)), "alut_self": int(ma.group(2)),
            "reg": int(mr.group(1)), "reg_self": int(mr.group(2)),
            "mem": int(f[4]) if f[4].isdigit() else 0,
            "dsp": int(f[5]) if f[5].isdigit() else 0,
            "vpins": int(f[7]) if len(f) > 7 and f[7].isdigit() else 0,
        }
        if depth == 0 and top is None:
            top = row
        elif depth == 1:
            kids.append(row)

    # Quartus states WHY an array did not become memory. Its words, not a guess.
    # `[A-Za-z ]` AND NOT `[a-z ]`. The first version stopped at the capital in
    # "inappropriate RAM size", captured "inappropriate " and matched neither
    # bucket -- so the tool printed "UNINFERRED RAM (2)" and then "0 ... 0",
    # which is CLAUDE.md's "a number that is exactly zero is a broken
    # instrument until proven otherwise" catching its own author within a
    # minute of the tool being written.
    un = re.findall(r'RAM logic "([^"]+)" is uninferred due to ([A-Za-z ]+)', t)
    return top, kids, un


def report(mod: str) -> int:
    p = os.path.join(BLOCKPATHS, mod + ".map.rpt")
    if not os.path.exists(p):
        print("no map report for %s at %s" % (mod, p))
        print("  (map reports on disk: run with --list)")
        return 1
    top, kids, un = census(p)
    if not top:
        print("%s: map report has no entity table" % mod)
        return 1

    print("=" * 96)
    print("%s   (from %s)" % (mod, os.path.relpath(p, REPO).replace(os.sep, "/")))
    print("=" * 96)
    print("%-38s %8s %8s %9s %9s %10s %5s" %
          ("entity", "ALUT", "(self)", "REG", "(self)", "mem bits", "DSP"))
    print("-" * 96)
    print("%-38s %8d %8d %9d %9d %10d %5d" %
          ("TOP: " + top["name"][:33], top["alut"], top["alut_self"],
           top["reg"], top["reg_self"], top["mem"], top["dsp"]))
    print("-" * 96)

    blocks = [k for k in kids if not k["name"].startswith("altsyncram")]
    mems = [k for k in kids if k["name"].startswith("altsyncram")]
    for k in sorted(blocks, key=lambda x: -x["reg_self"]):
        print("%-38s %8d %8d %9d %9d %10d %5d" %
              ("  " + k["name"][:36], k["alut"], k["alut_self"],
               k["reg"], k["reg_self"], k["mem"], k["dsp"]))
    if mems:
        print("  -- memories declared in the TOP itself --")
        for k in sorted(mems, key=lambda x: -x["mem"]):
            print("%-38s %8s %8s %9s %9s %10d %5s" %
                  ("  " + k["name"][:36], "", "", "", "", k["mem"], ""))

    kid_reg = sum(k["reg_self"] for k in blocks)
    kid_alut = sum(k["alut_self"] for k in blocks)
    print()
    if top["reg"]:
        print("registers %6d total | %6d IN THE TOP ITSELF (%.0f%%) | %6d in named blocks"
              % (top["reg"], top["reg_self"], 100.0 * top["reg_self"] / top["reg"], kid_reg))
    if top["alut"]:
        print("ALUTs     %6d total | %6d IN THE TOP ITSELF (%.0f%%) | %6d in named blocks"
              % (top["alut"], top["alut_self"], 100.0 * top["alut_self"] / top["alut"], kid_alut))
    if top.get("vpins"):
        print("virtual pins %d -- a leaf fit's boundary; see split_setup_paths.py" % top["vpins"])

    if un:
        async_ = [(n, w) for n, w in un if "asynchronous" in w]
        size = [(n, w) for n, w in un if "size" in w]
        print()
        print("UNINFERRED RAM (%d) -- arrays that did NOT become memory, in Quartus's words:"
              % len(un))
        print("  %d x asynchronous read logic  <- these COULD be memory; the read is why they are not"
              % len(async_))
        for n, _ in async_:
            print("       %s" % n)
        print("  %d x inappropriate RAM size   <- too shallow to be worth an M10K; correctly flops"
              % len(size))
        # ANY reason that is neither is printed rather than dropped. A parser
        # that silently discards what it cannot classify reports fewer problems
        # than exist, which is the one direction a broken instrument always
        # lies in.
        other = [(n, w) for n, w in un if "asynchronous" not in w and "size" not in w]
        if other:
            print("  %d x OTHER REASON -- not classified by this tool, shown verbatim:" % len(other))
            for n, w in other:
                print("       %-40s %s" % (n, w.strip()))
    return 0


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--list":
        for f in sorted(glob.glob(os.path.join(BLOCKPATHS, "*.map.rpt"))):
            print("  " + os.path.basename(f).replace(".map.rpt", ""))
        return 0
    if len(argv) < 2:
        print(__doc__.strip())
        return 2
    rc = 0
    for m in argv[1:]:
        rc |= report(m)
        print()
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
