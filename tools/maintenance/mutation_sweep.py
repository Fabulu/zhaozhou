#!/usr/bin/env python3
"""mutation_sweep.py -- break one line on purpose and see which suites notice.

WHY THIS EXISTS
---------------
CLAUDE.md: "A detector that has not been shown to FIRE has not been tested.
Break it on purpose, watch the alarm go off, put it back."

Done by hand three times on 2026-09-07, it found three coverage holes of one
shape, each in the LARGER of the suites that shared a fixture:

    mutation                          suite that caught it      suite that did not
    ------------------------------    ---------------------    -------------------
    tess window loses its row term    tess_directed   6,751     tess_normals  41,731
    v3rq occupancy loses ld_q         (none existed)            --
    rescale16_row loses its rounding  geom_project      900     terrain_project 2,011

Every one of the blind suites was exercising the logic and not testing it,
because its fixture data was too regular to reach the case -- all-solid cells,
exact row products. `tests/terrain/tess_harness.hpp` already names the failure
mode in prose:

    Filling only on the height16 grid makes every parent difference EVEN, so
    the geomorph halving never has a remainder and A TRUNCATION IN PLACE OF
    ROUND-HALF-UP SURVIVES THE WHOLE SUITE -- which is exactly what a mutation
    sweep found.

It kept happening anyway, because the technique lived in whoever remembered it.
This is the technique, committed.

SAFETY, AND IT IS NOT DECORATION
--------------------------------
CLAUDE.md records a fire-test mutation that nearly cost the tree: "the disk
filled DURING a fire-test mutation and left a ZERO-BYTE BACKUP FILE -- a moment
later and the tree would have held deliberately broken RTL with a truncated
backup beside it."

So this tool never writes its own backup. It:

  * REFUSES to start unless the target file is clean in git, so the durable
    copy already exists and is not one this process wrote;
  * restores with `git checkout --` in a `finally`, not from a temp file;
  * VERIFIES the restore afterwards and shouts if it failed;
  * refuses to run at all if `git` cannot be reached.

A backup written by the process that is about to break something is a backup
that shares its failure modes.

USAGE
    python tools/maintenance/mutation_sweep.py \
        --file fpga/rtl/common/zhao_project_core.sv \
        --find "r = (x + 68'sd32768) >>> 16;" \
        --replace "r = x >>> 16;" \
        --targets test_geom_project_directed test_terrain_project_directed

Exit 0 if EVERY named suite detected the mutation, 1 if any did not -- because
a suite that does not notice is the finding, not an inconvenience.
"""

from __future__ import annotations

import argparse
import io
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def git(*args: str) -> tuple[int, str]:
    try:
        # -c core.autocrlf=true. core.autocrlf lives only in Git-for-Windows'
        # system config, so a bare `git` that resolves to
        # c:/devkitPro/msys2/usr/bin/git.exe reports 1,200 line-ending-only
        # diffs on this clean tree. Measured 2026-09-09; see
        # reports/TWO-GITS-DISAGREE-ABOUT-CLEAN-20260909.md.
        # This helper serves both `status --porcelain` (the dirty gate) and
        # `checkout --` (the restore), so guarding it here covers both. An
        # unguarded restore rewrites line endings on the file it repairs.
        r = subprocess.run(['git', '-C', REPO, '-c', 'core.autocrlf=true', *args],
                           capture_output=True,
                           text=True, timeout=120)
    except Exception as e:  # noqa: BLE001
        return 1, str(e)
    return r.returncode, (r.stdout + r.stderr)


def build_and_run(target: str, timeout: int) -> tuple[bool, str]:
    """Returns (suite_passed, last_line). A build failure counts as DETECTED,
    because RTL that no longer compiles is a mutation the suite noticed."""
    rc, out = 0, ''
    try:
        b = subprocess.run(['cmake', '--build', 'build', '--target', target],
                           cwd=REPO, capture_output=True, text=True, timeout=timeout)
    except Exception as e:  # noqa: BLE001
        return False, 'build error: %s' % e
    if b.returncode != 0:
        return False, 'BUILD FAILED (counts as detected)'
    exe = os.path.join(REPO, 'build', 'tests', target + '.exe')
    if not os.path.exists(exe):
        exe = os.path.join(REPO, 'build', 'tests', target)
    try:
        r = subprocess.run([exe], cwd=REPO, capture_output=True, text=True, timeout=timeout)
    except Exception as e:  # noqa: BLE001
        return False, 'run error: %s' % e
    lines = [l for l in (r.stdout + r.stderr).splitlines() if l.strip()]
    return r.returncode == 0, (lines[-1][:110] if lines else '(no output)')


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', required=True)
    ap.add_argument('--find', required=True)
    ap.add_argument('--replace', required=True)
    ap.add_argument('--targets', nargs='+', required=True)
    ap.add_argument('--timeout', type=int, default=900)
    a = ap.parse_args(argv[1:])

    path = os.path.join(REPO, a.file)
    if not os.path.exists(path):
        print('no such file: %s' % a.file)
        return 2

    rc, _ = git('rev-parse', '--git-dir')
    if rc != 0:
        print('REFUSING: git is not reachable, so there is no durable copy to restore from.')
        return 2

    rc, out = git('status', '--porcelain', '--', a.file)
    if rc != 0 or out.strip():
        print('REFUSING: %s is not clean in git.' % a.file)
        print('  This tool restores with `git checkout --`, deliberately, so it')
        print('  must not be the only thing holding your edits. Commit or stash first.')
        print('  ' + out.strip())
        return 2

    src = io.open(path, encoding='utf-8').read()
    n = src.count(a.find)
    if n != 1:
        print('REFUSING: the --find text appears %d times, not once.' % n)
        print('  An ambiguous mutation site makes the result unattributable.')
        return 2

    print('MUTATION SWEEP')
    print('  file    %s' % a.file)
    print('  find    %s' % a.find.strip()[:88])
    print('  replace %s' % a.replace.strip()[:88])
    print('  suites  %s' % ', '.join(a.targets))
    print()

    results = []
    restored = False
    try:
        io.open(path, 'w', encoding='utf-8', newline='\n').write(
            src.replace(a.find, a.replace, 1))
        for t in a.targets:
            passed, last = build_and_run(t, a.timeout)
            results.append((t, passed, last))
            print('  %-42s %s   %s'
                  % (t, 'DID NOT NOTICE' if passed else 'detected', last))
    finally:
        # NO `return` IN THIS BLOCK. A return inside `finally` DISCARDS any
        # exception propagating through it, and this is the one place in the
        # tool where an exception must never be swallowed: if the mutation loop
        # died, the caller has to see why AND see the restore verdict. Python
        # warns about it ("SyntaxWarning: 'return' in a 'finally' block") and
        # the first version of this file earned that warning -- in a safety
        # path, which is the worst place to be clever.
        rc, out = git('checkout', '--', a.file)
        after = io.open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        restored = (rc == 0 and after == src)
        if restored:
            print()
            print('  restored and verified byte-for-byte against the committed copy')
        else:
            print()
            print('*** RESTORE FAILED. %s IS STILL MUTATED. ***' % a.file)
            print('*** Run: git checkout -- %s ***' % a.file)

    if not restored:
        return 3

    blind = [t for t, passed, _ in results if passed]
    print()
    if blind:
        print('%d of %d suites DID NOT NOTICE:' % (len(blind), len(results)))
        for t in blind:
            print('   %s' % t)
        print()
        print('That is the finding. A suite that exercises the logic without')
        print('testing it is a check count, not coverage -- and every instance')
        print('found so far was the LARGER of the suites sharing its fixture.')
        return 1
    print('every suite detected the mutation.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
