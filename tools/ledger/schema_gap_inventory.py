#!/usr/bin/env python3
"""For each block the ledger's schema rejects, say WHETHER THE MISSING THING EXISTS.

WHY THIS AND NOT A DIRECT FIX. `npm run -w tools/ledger check` reports 96 schema
errors over 26 blocks, and they are not one defect. They split into three kinds with
completely different costs:

  * STRUCTURAL -- a `maturity_log` written as prose bullets instead of the
    `{state, date, commit, evidence}` objects the schema requires. The content is
    there; the shape is wrong. Fixable from the file plus `git log`.
  * PRESENT BUT UNDECLARED -- the schema wants `tests.random` or `reference_model`
    and the artifact EXISTS in the tree under a name nobody wrote down. Fixable by
    finding it, and the fix is a transcription.
  * GENUINELY ABSENT -- the artifact does not exist. Then the ledger's `maturity` is
    a claim the tree does not support, and the honest move is to say so rather than
    to write a path that resolves to nothing. **A schema gate is easy to silence with
    a plausible string, and that is the failure this file exists to prevent.**

So this reports the split. It writes nothing and decides nothing.

It is deliberately NOT a YAML parser: this repository has no `yaml` module available
to the system Python, and a hand parser that silently drops what it cannot match is
the broken-instrument pattern. So it reads the block records textually, and ASSERTS
at import that it can recover known fields from a known block.
"""
from __future__ import annotations

import io
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKS = os.path.join(REPO, 'design', 'blocks.yml')
TESTS_CML = os.path.join(REPO, 'tests', 'CMakeLists.txt')


def read(p: str) -> str:
    return io.open(p, encoding='utf-8', errors='replace').read().replace('\r\n', '\n')


def split_blocks(text: str) -> list[tuple[int, str, str]]:
    """(index, id, record text) for every block, in file order."""
    body = text[text.index('\nblocks:'):]
    parts = re.split(r'\n  - (?=id:)', body)
    out = []
    for i, rec in enumerate(parts[1:]):
        m = re.match(r'id:\s*(\S+)', rec.strip())
        out.append((i, m.group(1) if m else '?', rec))
    return out


def field(rec: str, key: str) -> str | None:
    """The raw text of a top-level field of a block record, or None."""
    m = re.search(r'^    %s:(.*?)(?=^    \w+:|\Z)' % re.escape(key), rec, re.M | re.S)
    return m.group(1).rstrip() if m else None


def _selftest() -> None:
    text = read(BLOCKS)
    bs = split_blocks(text)
    assert len(bs) > 100, 'selftest: expected >100 blocks, got %d' % len(bs)
    by_id = {b[1]: b for b in bs}
    assert 'INPUT.SNAC' in by_id, 'selftest: a known block id was not recovered'
    rec = by_id['INPUT.SNAC'][2]
    assert (field(rec, 'maturity') or '').strip() == 'UNIT_VERIFIED', \
        'selftest: maturity not read back from a known block'
    assert field(rec, 'reference_model') is not None, \
        'selftest: reference_model not read back from a known block'
    assert field(rec, 'no_such_field_here') is None, \
        'selftest: a missing field must read as None, not as empty text'


_selftest()

STRUCTURED_LOG = re.compile(r'^\s*- state:', re.M)


def ctest_names(text: str) -> set[str]:
    return set(re.findall(r'add_test\(NAME\s+(\S+)', text))


def main(argv: list[str]) -> int:
    text = read(BLOCKS)
    cml = read(TESTS_CML)
    names = ctest_names(cml)
    want = [int(a) for a in argv if a.isdigit()]

    for idx, bid, rec in split_blocks(text):
        if want and idx not in want:
            continue
        issues = []

        log = field(rec, 'maturity_log')
        if log is not None and not STRUCTURED_LOG.search(log):
            issues.append('STRUCTURAL: maturity_log is prose, not {state,date,commit,evidence}')
        if log is None:
            issues.append('ABSENT: no maturity_log at all')
        elif STRUCTURED_LOG.search(log):
            for c in re.findall(r'^\s*commit:\s*"?([^"\s]+)"?', log, re.M):
                if not re.fullmatch(r'[0-9a-f]{7,40}', c):
                    issues.append('STRUCTURAL: commit %r is not a 7-40 hex sha' % c)

        tests = field(rec, 'tests') or ''
        has = {k for k in ('directed', 'random', 'formal', 'unit', 'differential')
               if re.search(r'^\s*%s:' % k, tests, re.M)}
        extra = set(re.findall(r'^\s*(\w+):', tests, re.M)) - has
        if extra:
            issues.append('STRUCTURAL: tests has non-schema key(s) %s' % sorted(extra))
        if 'random' not in has:
            # Does a random arm EXIST for this block, under any plausible name?
            stem = bid.lower().replace('.', '_')
            tail = stem.split('_')[-1]
            cands = sorted(n for n in names
                           if 'random' in n and (tail in n or stem in n))
            if cands:
                issues.append('PRESENT BUT UNDECLARED: tests.random missing; ctest has %s'
                              % cands[:4])
            else:
                issues.append('GENUINELY ABSENT: tests.random missing and no random ctest '
                              'matches this block -- the maturity claim, not the path, is '
                              'what needs deciding')

        if field(rec, 'reference_model') is None:
            contract = field(rec, 'contract')
            cpath = (contract or '').strip()
            found = None
            if cpath:
                fp = os.path.join(REPO, cpath)
                if os.path.exists(fp):
                    m = re.search(r'(zref::[A-Za-z0-9_:]+)', read(fp))
                    found = m.group(1) if m else None
            if found:
                issues.append('PRESENT BUT UNDECLARED: reference_model missing; contract '
                              'names %s' % found)
            else:
                issues.append('GENUINELY ABSENT: reference_model missing and the contract '
                              'names no zref:: symbol')

        if issues:
            print('=== /blocks/%d  %s   maturity=%s'
                  % (idx, bid, (field(rec, 'maturity') or '?').strip().split('\n')[0]))
            for s in issues:
                print('    ' + s)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
