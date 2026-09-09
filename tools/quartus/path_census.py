#!/usr/bin/env python3
"""Split a Quartus setup-path report into BOUNDARY and INTERNAL paths.

WHY THIS IS A COMMITTED TOOL AND NOT A ONE-OFF SCRIPT
-----------------------------------------------------
On 2026-09-09 an ad-hoc version of this changed a decision. Gate 4 had just
measured the reciprocal tile at 56.24 MHz (svc) against 100.95 (v3), and the
island containing svc fits at 62.83 -- so "the tile is the island's critical
path, the swap buys ~20 MHz" was about to be written into the roadmap.

The census says otherwise: the island's REPORTED figure comes from a pin path
into the palette resolver, the internal-only ceiling is 77.45 MHz, and a
zero-delay reciprocal would raise that to 81.53 before hitting the next path --
which leaves the SAME source register. 42 of 43 internal paths start at one bit,
u_own|live_cnt_q[6].

CLAUDE.md: a probe that produces a number and is then thrown away leaves an
unreproducible number behind. So it lives here.

The leaf-fit boundary artefact is a KNOWN category and a known trap in both
directions -- it is real (399 of 442 rows here touch it) and it has previously
been found "real and almost irrelevant" when the internal paths sat just behind.
Which of those it is, is exactly what this tool answers, per report.

Usage:
    python tools/quartus/path_census.py reports/synthesis/blockpaths/<row>.setup.rpt
    python tools/quartus/path_census.py <rpt> --exclude-instance u_rcp
    python tools/quartus/path_census.py --self-test
"""
import argparse
import io
import os
import re
import sys
import tempfile
from collections import Counter

# Quartus SUMMARY rows look like, with exactly eight fields:
#   ; -5.915 ; from_node ; to_node ; clk ; clk ; 10.000 ; 3.391 ; 19.246 ;
_ROW = re.compile(r'^;\s*(-?\d+\.\d+)\s*;')

# THE FIELD COUNT IS LOad-BEARING, and getting it wrong inflated a number that
# was about to be published. `-detail full_path` makes Quartus follow the summary
# table with a per-path node breakdown, and those detail lines ALSO begin
# `; <number> ;`. In this report that is 8,813 detail rows against 200 real ones.
#
# An ad-hoc first version of this tool matched on the leading number alone and
# reported "442 paths, 399 boundary-touching". Both figures were fiction. The
# internal count survived only by accident: detail rows carry empty node columns,
# so they fail `is_internal` on both ends and fell out of that population anyway.
#
# It is the broken-instrument law again, and note the direction -- over-matching
# detail rows inflated the BOUNDARY share to 90%, making the leaf-fit pin
# artefact look even more dominant than it is. The comfortable reading got
# louder. Requiring the summary table's exact arity is what separates them.
_SUMMARY_FIELDS = 8


def is_internal(node):
    """A node INSIDE the design prints as `module_type:instance|reg`.

    A node on the boundary is a bare pin name with no instance path at all.
    """
    return (':' in node) and ('|' in node)


def instance_of(node):
    m = re.match(r'[A-Za-z0-9_]+:([A-Za-z0-9_]+)\|', node)
    return m.group(1) if m else '(boundary)'


def parse(path):
    rows = []
    with io.open(path, encoding='utf-8', errors='replace') as fh:
        for ln in fh:
            if not _ROW.match(ln):
                continue
            f = [c.strip() for c in ln.strip().strip(';').split(';')]
            if len(f) != _SUMMARY_FIELDS:
                continue
            try:
                slack = float(f[0])
            except ValueError:
                continue
            rows.append((slack, f[1], f[2]))
    return rows


def fmax(slack, period_ns):
    """Fmax implied by a slack against the constrained period."""
    return 1000.0 / (period_ns - slack)


# ---------------------------------------------------------------------------
# SELF-FIRE TEST. A parser that silently matches nothing reports "no problems",
# which is the broken-instrument failure mode this repo has been bitten by
# repeatedly (a `^` without re.MULTILINE; a word boundary a heredoc ate). So the
# scanner must be shown to see a known-good example before it may report.
# ---------------------------------------------------------------------------
_FIRE = (
    '; -5.915 ; pal_ld_gen_i[1] ; zhao_texture_palette_res:u_palette|res_r[3] ;'
    ' clk ; clk ; 10.000 ; 3.391 ; 19.246 ;\n'
    '; -2.912 ; zhao_texture_v3own:u_own|live_cnt_q[6]~DUPLICATE ;'
    ' zhao_raster_rcp24_svc:u_rcp|Add7~21_OTERM4928 ;'
    ' clk ; clk ; 10.000 ; -0.382 ; 12.470 ;\n'
    ';  0.412 ; zhao_a:u_a|q ; zhao_b:u_b|d ; clk ; clk ; 10.000 ; 0.0 ; 1.0 ;\n'
    # NEGATIVE CONTROL: a `-detail full_path` node row. It begins with a number
    # and a semicolon exactly like a summary row, and the first version of this
    # tool counted 8,813 of them as paths. It has seven fields, and it must be
    # rejected. Testing only that the scanner SEES things is half a test.
    '; 0.000    ; 0.000    ;    ;      ;        ;                      ;'
    ' launch edge time                            ;\n'
)


def self_test():
    fd, p = tempfile.mkstemp(suffix='.rpt')
    os.close(fd)
    io.open(p, 'w', encoding='utf-8').write(_FIRE)
    try:
        rows = parse(p)
    finally:
        os.unlink(p)
    assert len(rows) == 3, \
        ('scanner saw %d rows, expected exactly 3 -- if it saw 4 the arity '
         'filter is not rejecting `-detail full_path` node rows' % len(rows))
    assert not is_internal(rows[0][1]), 'a bare pin was called internal'
    assert is_internal(rows[1][1]) and is_internal(rows[1][2]), \
        'an instance path was called boundary'
    assert instance_of(rows[1][2]) == 'u_rcp', \
        'instance extraction gave %s' % instance_of(rows[1][2])
    assert rows[2][0] > 0, 'a positive-slack row was dropped'
    # and it must SEPARATE the two populations, not merely parse them
    ii = [r for r in rows if is_internal(r[1]) and is_internal(r[2])]
    assert len(ii) == 2, 'internal split found %d, expected 2' % len(ii)
    return True


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('report', nargs='?',
                    help='a *.setup.rpt from reports/synthesis/blockpaths/')
    ap.add_argument('--period', type=float, default=10.0,
                    help='constrained period in ns (default 10.0 = 100 MHz)')
    ap.add_argument('--exclude-instance', default=None,
                    help='report the internal ceiling if every path ENDING in this '
                         'instance had zero delay -- the most a perfect replacement '
                         'of that block could possibly buy')
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()

    self_test()   # unconditional: it may not report before proving it can see
    if a.self_test:
        print('path_census self-test: scanner sees boundary, internal and '
              'positive-slack rows, and separates them.')
        return 0
    if not a.report:
        ap.error('a report path is required')

    rows = parse(a.report)
    if not rows:
        print('NO PATH ROWS PARSED. The self-test passed, so the scanner works -- '
              'this file is not a Quartus setup-path report, or it is empty.',
              file=sys.stderr)
        return 2

    ii = [r for r in rows if is_internal(r[1]) and is_internal(r[2])]
    n_boundary = len(rows) - len(ii)
    worst = min(rows, key=lambda r: r[0])

    print('%d path rows: %d boundary-touching, %d internal-to-internal'
          % (len(rows), n_boundary, len(ii)))
    print()
    print('worst OVERALL   %8.3f  %s -> %s' % worst)
    print('                          reported Fmax  %.2f MHz'
          % fmax(worst[0], a.period))
    if ii:
        wi = min(ii, key=lambda r: r[0])
        print('worst INTERNAL  %8.3f  %s -> %s' % wi)
        print('                          internal-only  %.2f MHz'
              % fmax(wi[0], a.period))
        print()
        print('internal paths by SOURCE node:')
        for n, c in Counter(r[1] for r in ii).most_common(6):
            print('   %-58s %d' % (n, c))
        print('internal paths by DESTINATION instance:')
        for n, c in Counter(instance_of(r[2]) for r in ii).most_common(6):
            print('   %-58s %d' % (n, c))

    if a.exclude_instance and ii:
        rest = [r for r in ii if a.exclude_instance not in r[2]]
        gone = len(ii) - len(rest)
        wi = min(ii, key=lambda r: r[0])
        print()
        print('IF EVERY PATH ENDING IN %r HAD ZERO DELAY (%d paths removed):'
              % (a.exclude_instance, gone))
        if not rest:
            print('   no internal paths remain -- the boundary becomes the only limit')
        else:
            w2 = min(rest, key=lambda r: r[0])
            print('   next internal worst %.3f -> %.2f MHz  (was %.2f)'
                  % (w2[0], fmax(w2[0], a.period), fmax(wi[0], a.period)))
            print('   the ceiling moves by %.2f MHz, then stops at:'
                  % (fmax(w2[0], a.period) - fmax(wi[0], a.period)))
            print('      %s -> %s' % (w2[1], w2[2]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
