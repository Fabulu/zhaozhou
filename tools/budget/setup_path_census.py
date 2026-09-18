#!/usr/bin/env python3
"""Census of a Quartus setup report's worst paths, grouped by DESTINATION.

Why this file exists, in one paragraph, because the reason is the whole value:

`*.setup.rpt` contains THREE tables whose rows have the same column shape --
`Summary of Paths` (the paths), `Data Arrival Path` and `Data Required Path`
(per-node incremental delays). A regex written for the first matches all three.
On 2026-09-18 that produced "1,600 summarised paths, 1,400 of them meeting
timing" from a report holding 200 paths, every one of them negative. The tell
was a histogram with EXACTLY zero entries in each of [-3,-2), [-2,-1) and
[-1,0) across three independent fits -- precision at zero is a tell, not a
result -- and the error read in the flattering direction, as it always does.

This tool had been written from scratch, by hand, at least four times during the
Packet H campaign, and got that wrong at least once. CLAUDE.md's rule about the
ground-contact probe applies exactly: a probe written once and thrown away has
unreproducible numbers. So it is committed, and its parser asserts at import
that it can still find the one table it wants and still REJECT the other two.

Two further things it encodes, both bought expensively:

  * **Group by the DESTINATION SIGNAL, never by the module.** Grouping the
    Packet H paths by module gave a comfortable and wrong answer three times
    running. The endpoint register is what a retiming or pipelining change
    actually moves, so it is the only key whose counts predict anything.

  * **The printed window is not the machine.** Quartus prints the worst N paths
    (200 here). At `@packet-h-mulstage` TNS was -9,514 ns against a worst path
    of -6.372, so the report's 200 rows cannot account for it -- there are
    thousands of negative paths and this file sees the tip. Every count printed
    below is a count of the WINDOW. The tool says so on every run rather than
    leaving the reader to infer it.

Usage:
    python tools/budget/setup_path_census.py <receipt-label> [<receipt-label>]

A label is either a path to a `.setup.rpt` or a row label such as
`zhao_shell_top_v2@packet-h-mulstage`, resolved under reports/synthesis/blockpaths.
Given two, it prints the per-endpoint delta, which is the form that shows
whether a fix removed its target or merely reordered the queue.
"""

from __future__ import annotations

import bisect
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join("reports", "synthesis", "blockpaths")

# A row of any of the three tables. Deliberately identical to the naive regex
# that caused the miscount -- the SECTION is what disambiguates, not the shape.
ROW = re.compile(r"^; (-?\d+\.\d+)\s+; ([^;]+?)\s+; ([^;]+?)\s+;", re.M)
SECTION = re.compile(r"^; (Summary of Paths|Data Arrival Path|Data Required Path)"
                     r"\s*;?\s*$", re.M)
WANTED = "Summary of Paths"


def sections(text):
    """(offset, name) for every table header, in file order."""
    return [(m.start(), m.group(1)) for m in SECTION.finditer(text)]


def paths(text):
    """(slack, from, to) for rows in `Summary of Paths` ONLY.

    Rows before the first header, or under a delay-detail header, are dropped.
    That single filter is the difference between 200 paths and 1,600.
    """
    heads = sections(text)
    starts = [h[0] for h in heads]
    out = []
    for m in ROW.finditer(text):
        i = bisect.bisect_right(starts, m.start()) - 1
        if i < 0 or heads[i][1] != WANTED:
            continue
        out.append((float(m.group(1)), m.group(2).strip(), m.group(3).strip()))
    return out


def leaf(node):
    """The destination REGISTER, with bit indices collapsed.

    `a|b|c|dividend_r[97]` -> `dividend_r[]`. Quartus decorates some endpoints
    (`~DUPLICATE`, `~reg0`) and those are kept: a duplicated register is a
    different physical endpoint and lumping it in hides fitter behaviour.
    """
    return re.sub(r"\[\d+\]", "[]", node.split("|")[-1])


def census(rows):
    """{destination leaf: (worst slack, count)}, worst first."""
    by = collections.defaultdict(list)
    for slack, _src, dst in rows:
        by[leaf(dst)].append(slack)
    return collections.OrderedDict(
        (k, (min(v), len(v)))
        for k, v in sorted(by.items(), key=lambda kv: min(kv[1])))


def resolve(label):
    if os.path.exists(label):
        return label
    cand = os.path.join(ROOT, BLOCKPATHS, label + ".setup.rpt")
    if os.path.exists(cand):
        return cand
    raise SystemExit("no setup report for %r (looked in %s)" % (label, BLOCKPATHS))


def read(label):
    with open(resolve(label), "r", errors="replace") as fh:
        return fh.read()


def histogram(rows):
    v = [s for s, _, _ in rows]
    bins = [(-99, -6), (-6, -5), (-5, -4), (-4, -3), (-3, -2),
            (-2, -1), (-1, 0), (0, 99)]
    return [(lo, hi, sum(1 for x in v if lo <= x < hi)) for lo, hi in bins]


def self_test():
    """Both polarities on a synthetic report carrying all three tables."""
    sample = (
        "; Data Arrival Path ;\n"
        "; 1.234 ; node_a ; node_b ; extra ;\n"
        "; Summary of Paths ;\n"
        "; -6.372 ; mod|row_r[2] ; mod|sub|final_sat_r ; x ;\n"
        "; -3.869 ; mod|dndy_r[26] ; mod|sub|dividend_r[97] ; x ;\n"
        "; -3.750 ; mod|dndy_r[26] ; mod|sub|dividend_r[96] ; x ;\n"
        "; Data Required Path ;\n"
        "; 9.999 ; node_c ; node_d ; extra ;\n"
    )
    got = paths(sample)
    assert len(got) == 3, ("FIRE/REJECT case failed: expected 3 Summary rows, "
                           "got %d -- the delay-detail tables leaked in" % len(got))
    assert all(s < 0 for s, _, _ in got), "a positive slack leaked in from a detail table"
    assert got[0][0] == -6.372, "worst path not first"

    c = census(got)
    assert list(c) == ["final_sat_r", "dividend_r[]"], "census order or key wrong: %r" % (list(c),)
    assert c["dividend_r[]"] == (-3.869, 2), "bit indices not collapsed into one endpoint"

    # The naive regex MUST match all three tables -- if it stopped doing so,
    # this tool's whole reason for existing would have quietly evaporated and
    # the section filter would be untested rather than merely unnecessary.
    assert len(ROW.findall(sample)) == 5, "the naive regex no longer over-matches; re-derive the filter"

    assert leaf("a|b|c|dividend_r[97]") == "dividend_r[]"
    assert leaf("a|b|walk_q_r[3]~DUPLICATE") == "walk_q_r[]~DUPLICATE", "decoration must survive"
    return True


if not __debug__:
    raise RuntimeError("setup_path_census refuses Python -O: it removes the "
                       "assertion-based controls that keep the parser honest")
if not self_test():
    raise AssertionError("setup_path_census self-test failed")


def report_one(label):
    rows = paths(read(label))
    print("== %s" % label)
    print("   %d path(s) in `Summary of Paths`, worst %.3f ns"
          % (len(rows), min(s for s, _, _ in rows) if rows else 0.0))
    print("   NOTE: this is the WINDOW Quartus printed, not the machine. Compare")
    print("   the row's TNS against these rows before quoting any count.\n")
    for lo, hi, n in histogram(rows):
        print("   [%3d,%3d)  %5d  %s" % (lo, hi, n, "#" * min(60, n // 3)))
    print("\n   worst slack per DESTINATION signal (the only truthful key):")
    for k, (worst, n) in census(rows).items():
        print("     %8.3f  n=%-5d %s" % (worst, n, k))
    return rows


def report_two(a, b):
    ra, rb = paths(read(a)), paths(read(b))
    ca, cb = census(ra), census(rb)
    # The printed window has a FLOOR -- the best slack Quartus bothered to
    # print -- and it MOVES between fits. When the worst tier shrinks, the
    # floor rises and endpoints that were always there appear for the first
    # time. Without these two numbers, GONE and NEW read as design changes
    # when half of them are window changes, and the reading is flattering in
    # both directions at once: fixes look total, regressions look novel.
    fa = max(s for s, _, _ in ra) if ra else 0.0
    fb = max(s for s, _, _ in rb) if rb else 0.0
    print("== %s  ->  %s\n" % (a, b))
    print("   printed window: %d paths, floor %.3f  ->  %d paths, floor %.3f"
          % (len(ra), fa, len(rb), fb))
    print("   GONE means 'now better than %.3f'. NEW means 'was better than "
          "%.3f'.\n   Neither means the endpoint appeared or disappeared.\n"
          % (fb, fa))
    print("   %-40s %>9s %>9s" .replace(">", "") % ("destination", a[-12:], b[-12:]))
    keys = list(ca) + [k for k in cb if k not in ca]
    for k in keys:
        wa, na = ca.get(k, (None, 0))
        wb, nb = cb.get(k, (None, 0))
        mark = "  GONE>" if nb == 0 else ("  NEW<" if na == 0 else "")
        print("   %-40s %7s/%-4d %7s/%-4d%s"
              % (k[:40],
                 "%.3f" % wa if wa is not None else "--", na,
                 "%.3f" % wb if wb is not None else "--", nb, mark))
    print("\n   A successful retiming shows as GONE>, not as a smaller number on")
    print("   the same row: a fix that only improves its target's slack has")
    print("   usually moved the wall rather than removed it. But GONE> is a")
    print("   LOWER BOUND on the improvement (at least worst - floor), never a")
    print("   measurement of it, and an endpoint marked NEW< may have been")
    print("   present and unprinted all along. Quote the bound, not the mark.")


def main(argv):
    if len(argv) == 2:
        report_one(argv[1])
    elif len(argv) == 3:
        report_two(argv[1], argv[2])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
