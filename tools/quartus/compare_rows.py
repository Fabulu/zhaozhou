#!/usr/bin/env python3
"""compare_rows.py -- sum or compare fit rows, and REFUSE when one is stale.

WHY THIS EXISTS
---------------
`reports/synthesis/zhao_block_fit.json` carries one row per block, each stamped
with the commit its sources were at. Comparing a composed row against the sum
of its leaves is a natural and useful thing to do -- G1-D did it for the texture
island -- and it is silently wrong the moment ONE leaf row predates a change to
its block.

On 2026-09-07 that happened four times in one table. Every one of the four
characterisation-wrapper pairs was compared against its leaf sum, and every one
of the four sums contained at least one stale row; two also contained leaves
with no row at all, counted as zero. The conclusion drawn from it -- that the
pairs fit smaller because the wrappers fold logic away, "and a virtual pin has
never consumed a DSP block" -- was argued from a DSP gap of 24 -> 9 on
TESS+NORMALS that does not exist:

    zhao_terrain_normals's row says 18 DSP at commit 96c0394a.
    Two commits later, bfc74710 "one shared multiplier instead of six".

Six 33x33 multipliers became one. The pair's 9 DSP is TESS's 6 plus the one
shared multiplier's 3 -- an exact match with no folding required at all. The
gap was a stale row wearing the shape of a finding.

CLAUDE.md has this law twice over: "never compare a current file to an old
measurement -- declared-versus-measured is a real check; declared-today versus
measured-a-week-ago is a different question wearing the same shape, and it
produces confident nonsense." The staleness check that would have caught it was
added to `check_fit_rules.ps1` the same morning and simply was not run.

So this tool does not offer the comparison and a warning. IT REFUSES. A sum
containing a stale or missing row is not a number with a caveat; it is not a
number.

USAGE
    python tools/quartus/compare_rows.py <composed> <leaf> [<leaf> ...]

Exit 0 if the comparison is sound and printed, 2 if it was refused.
"""

from __future__ import annotations

import io
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
METRICS = ('alms', 'registers', 'ramBlocks', 'dspBlocks', 'blockMemoryBits')


def rows() -> dict:
    p = os.path.join(REPO, 'reports', 'synthesis', 'zhao_block_fit.json')
    d = json.load(io.open(p, encoding='utf-8'))
    rs = d['blocks'] if isinstance(d, dict) and 'blocks' in d else d
    rs = rs if isinstance(rs, list) else list(rs.values())
    return {r['module']: r for r in rs if isinstance(r, dict) and r.get('module')}


def source_file(mod: str) -> str | None:
    for dp, _, fns in os.walk(os.path.join(REPO, 'fpga', 'rtl')):
        if mod + '.sv' in fns:
            return os.path.join(dp, mod + '.sv')
    return None


def commits_since(mod: str, commit: str) -> int:
    """How many commits touched this module's source after its row was taken.
    -1 means the question could not be asked, which is reported as unknown and
    is NOT treated as fresh."""
    f = source_file(mod)
    if not f or not commit:
        return -1
    rel = os.path.relpath(f, REPO).replace(os.sep, '/')
    try:
        out = subprocess.run(['git', '-C', REPO, 'log', '--oneline', commit + '..HEAD', '--', rel],
                             capture_output=True, text=True, timeout=60)
    except Exception:
        return -1
    if out.returncode != 0:
        return -1
    return len([x for x in out.stdout.splitlines() if x.strip()])


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(__doc__.strip().splitlines()[-3])
        return 2
    composed, leaves = argv[1], argv[2:]
    R = rows()

    problems = []
    for mod in [composed] + leaves:
        if mod not in R:
            problems.append('%s: NO ROW -- it has never been fitted, and a missing '
                            'row sums as zero, which flatters the total' % mod)
            continue
        n = commits_since(mod, str(R[mod].get('sourceCommit') or ''))
        if n > 0:
            problems.append('%s: STALE by %d commit(s) -- its row describes an older block' % (mod, n))
        elif n < 0:
            problems.append('%s: provenance unknown -- cannot be shown fresh' % mod)

    if problems:
        print('REFUSED. A sum containing a stale or missing row is not a number with a')
        print('caveat; it is not a number. Refit the named blocks first.')
        print()
        for p in problems:
            print('  ' + p)
        print()
        print('(This refusal is the whole point of the tool. See its docstring for the')
        print(' four comparisons that were published before it existed.)')
        return 2

    c = R[composed]
    print('%-30s %s' % ('metric', 'composed   leaf sum   delta'))
    print('-' * 62)
    for m in METRICS:
        cv = c.get(m)
        lv = [R[l].get(m) for l in leaves]
        if cv is None or any(v is None for v in lv):
            print('%-30s %8s %10s   (a row does not record it)' % (m, cv, '--'))
            continue
        s = sum(lv)
        d = ('%+.1f%%' % (100.0 * (cv - s) / s)) if s else '--'
        print('%-30s %8d %10d   %s' % (m, cv, s, d))
    print()
    print('every row fresh: %s vs %s' % (composed, ', '.join(leaves)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
