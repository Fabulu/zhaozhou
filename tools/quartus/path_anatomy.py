#!/usr/bin/env python3
"""path_anatomy.py -- walk a critical path hop by hop and say where the time is.

WHY THIS EXISTS
---------------
`split_setup_paths.py` classifies every summarised path by its endpoints and
answers "is this block's clock its own fault or its boundary's". It is the right
first question and it is not enough, because an endpoint census names WHICH
SIGNALS meet on the worst path and never names WHERE THE TIME IS.

On 2026-09-07 that gap cost two edits on one block:

  1. TERRAIN.NORMALS' product was registered because a 33x33 multiply feeding a
     67-bit adder looked expensive. +1.3 MHz. The multiply was not the limit.
  2. TERRAIN.TESS's `cell_solid` was rewritten as a mask because the source had
     256 comparisons and 128 multiply sites. FIVE ALMs, +0.7 MHz -- Quartus had
     already collapsed the loop. That number was derived by COUNTING OPERATORS
     IN THE SOURCE, which CLAUDE.md names as the generation side.

Then the hop-by-hop walk found it in one pass: 28.080 ns from a lattice memory
read through seven chained arithmetic stages of the geomorph blend. The census
had pointed at `solid`, `ea` and `eg`; the walk pointed at `Add65`, `Add66`,
`Add67` and a 113-hop carry tail.

The walk was hand-rolled four times that day before anyone committed it. This
is it, under the rule CLAUDE.md states for the ground-contact probe: a probe
written once and thrown away leaves numbers nobody can reproduce.

WHAT IT PRINTS
--------------
For the worst path (or the Nth, with --index):

  * the clock path and data path totals, separated, because a large arrival
    time is not a large data path;
  * every hop above a threshold, with CUMULATIVE arrival, so the reader can see
    which stage the time accumulates in rather than only which hop is biggest;
  * the hops below the threshold summed and counted, because a long carry tail
    of 113 hops at 0.05 ns each is 5 ns and must not vanish into a filter;
  * the biggest single hops, ranked.

USAGE
    python tools/quartus/path_anatomy.py <module> [--index N] [--min 0.05]
"""

from __future__ import annotations

import argparse
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join(REPO, "reports", "synthesis", "blockpaths")


def hops(text: str, index: int):
    """The Nth 'Data Arrival Path' block, de-duplicated on repeated element."""
    starts = [m.start() for m in re.finditer(r"Data Arrival Path", text)]
    if index >= len(starts):
        return None, 0
    # BOUND THE SEGMENT AT THE NEXT PATH, not at a fixed byte count. The first
    # version took a flat 30,000 characters, which is fine for a long path and
    # silently spans SEVERAL SHORT ONES -- a tree-wide sweep built on it
    # reported zhao_texture_bilerp_lane with 9.792 ns of DSP inside a 3.741 ns
    # data path, which cannot happen and is how the bug was caught. A number
    # larger than its own container is a broken instrument (CLAUDE.md), and the
    # containment check was cheaper than the debugging would have been.
    end = starts[index + 1] if index + 1 < len(starts) else len(text)
    seg = text[starts[index]:end]
    out = []
    prev = None
    for line in seg.splitlines():
        if not line.startswith(";"):
            continue
        f = [c.strip() for c in line.split(";")]
        if len(f) < 8:
            continue
        try:
            tot, incr = float(f[1]), float(f[2])
        except ValueError:
            continue
        el = f[7]
        if el == prev:
            continue
        prev = el
        out.append((tot, incr, f[4], el))
    return out, len(starts)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("--index", type=int, default=0, help="0 = the worst path")
    ap.add_argument("--min", type=float, default=0.05, help="hop threshold in ns")
    a = ap.parse_args(argv[1:])

    p = os.path.join(BLOCKPATHS, a.module + ".setup.rpt")
    if not os.path.exists(p):
        print("no setup report for %s" % a.module)
        print("  (%s)" % p)
        return 1
    text = io.open(p, encoding="utf-8", errors="replace").read()
    rows, total = hops(text, a.index)
    if not rows:
        print("%s: no detailed path at index %d (%d present)" % (a.module, a.index, total))
        return 1

    clock = next((r for r in rows if r[3].strip() == "clock path"), None)
    data = next((r for r in rows if r[3].strip() == "data path"), None)

    print("=" * 92)
    print("%s   path %d of %d   (%s)"
          % (a.module, a.index + 1, total,
             os.path.relpath(p, REPO).replace(os.sep, "/")))
    print("=" * 92)
    # A containment check, printed rather than trusted: every hop this tool
    # attributes to the data path must fit inside the data path it reports.
    body_all = [r for r in rows if r[2] in ("IC", "CELL", "uTco")]
    idx = next((k for k, r in enumerate(rows) if r[3].strip() == "data path"), None)
    if idx is not None and data:
        after = sum(r[1] for r in rows[idx + 1:] if r[2] in ("IC", "CELL", "uTco"))
        if after > data[1] + 0.01:
            print("  *** hops after the data-path marker sum to %.3f ns against a"
                  % after)
            print("  *** reported data path of %.3f ns. The segment is spanning more"
                  % data[1])
            print("  *** than one path; the result below is not trustworthy.")
    if clock:
        print("  clock path %8.3f ns" % clock[1])
    if data:
        print("  DATA PATH  %8.3f ns   <- the number a pipeline cut divides" % data[1])
    print()

    body = [r for r in rows if r[2] in ("IC", "CELL", "uTco")]
    shown = [r for r in body if r[1] >= a.min]
    hidden = [r for r in body if r[1] < a.min]

    print("%9s %8s %-5s %s" % ("cum ns", "incr", "type", "element"))
    print("-" * 92)
    for tot, incr, kind, el in shown:
        print("%9.3f %8.3f %-5s %s" % (tot, incr, kind, el[:64]))

    if hidden:
        s = sum(r[1] for r in hidden)
        print()
        print("  %d hops below %.2f ns, summing %.3f ns -- NOT NOISE. A long carry"
              % (len(hidden), a.min, s))
        print("  tail is where a wide adder actually spends its time, and a filter")
        print("  that hides it makes a chain look like a single expensive gate.")

    print()
    print("biggest single hops:")
    for tot, incr, kind, el in sorted(body, key=lambda r: -r[1])[:8]:
        print("  %8.3f  %-5s %s" % (incr, kind, el[:70]))

    print()
    print("NOTE: the endpoint census (split_setup_paths.py) names WHICH SIGNALS meet")
    print("on this path. This names WHERE THE TIME IS. They are different questions,")
    print("and two edits were made on the wrong answer before that was written down.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
