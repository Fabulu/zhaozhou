#!/usr/bin/env python3
"""Extract the console's declared FRAME-BUDGET claims and the clock base each one
is quoted against.

WHY THIS EXISTS. The external V2 review asks for a joint envelope: the combined
worst-case workload the console is actually promised to serve, so a redesign can be
budgeted against something other than a blank sheet. Assembling one means adding up
what the blocks already claim -- and the first thing that has to be true for a sum
to mean anything is that the addends share a denominator.

They do not. This tool measures that, and nothing else.

WHAT IT IS AND IS NOT. It is a COMPARISON-SIDE instrument: it reads what the
contracts assert and reports the shape of the assertions. It does not decide a
budget, propose a clock, or judge whether a claim is right. A claim it prints is
still a claim -- the authority is the contract, and the number in the contract may
itself be stale. Percentages are quoted here as the contract quotes them; this
tool never recomputes one, because recomputing it against a denominator the author
did not use is exactly the mismatched-comparison error the campaign keeps paying
for.

WHAT A "DENOMINATOR" MEANS HERE. A frame length in clocks. At 60 Hz:

    100 MHz  ->  1,666,666  (the design point FIELD.SEQ.EARTH states)
     80 MHz  ->  1,333,333  (the historical lowest-credible assessment)

A contract that says "2.3 % of a 1,333,333-clock frame" has declared its base. One
that says "2.5 % of a frame" has not, and its percentage cannot be added to the
first without knowing which machine it meant.

SELF-TESTS (the broken-instrument law: a detector that has not been shown to fire
has not been tested). Every pattern below is asserted at import against a line
taken verbatim from a contract, and against a line it must NOT match.
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
from collections import Counter, defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[2]
CONTRACTS = ROOT / 'design' / 'contracts'

# A frame length in clocks, written with or without thousands separators.
DENOM_RE = re.compile(r'\b(1[,.]?666[,.]?66[67]|1[,.]?333[,.]?333)\b')

# A percent-of-frame claim. Group 'pct' is the number; group 'base', when present,
# is the frame length the author attached to it.
PCT_RE = re.compile(
    r'(?P<pct>[0-9]+(?:\.[0-9]+)?)\s*%\s*of\s+(?:a|the)\s+'
    r'(?:reserved\s+)?(?:(?P<base>[0-9][0-9,]*)-clock\s+)?frame',
    re.I)

# Frame lengths a contract states as ITS OWN budget rather than as a percentage
# base -- e.g. FIELD.SEQ.EARTH's acceptance ceiling.
CEILING_RE = re.compile(
    r'(?:ceiling|budget|target|reserved)[^.\n]{0,80}?'
    r'\b(?P<clocks>[0-9]{3}(?:,[0-9]{3})+|[0-9]{6,})\b[^.\n]{0,20}?clocks',
    re.I)

KNOWN_CLOCKS = {
    '1666666': ('100 MHz / 60 Hz', 1_666_666),
    '1666667': ('100 MHz / 60 Hz', 1_666_667),
    '1333333': ('80 MHz / 60 Hz', 1_333_333),
}


def _norm(n: str) -> str:
    return n.replace(',', '').replace('.', '')


def _selftest() -> None:
    # --- DENOM_RE fires on real contract text, in both spellings ---
    assert DENOM_RE.search('~0.7 % of a 1,333,333-clock frame'), 'DENOM_RE: comma form'
    assert DENOM_RE.search('computeClocksPerFrame = 1,666,666'), 'DENOM_RE: assignment form'
    # ... and does NOT fire on an unrelated six-digit number.
    assert not DENOM_RE.search('the store is 23,040 bits'), 'DENOM_RE: false positive'

    # --- PCT_RE separates a declared base from an undeclared one ---
    m = PCT_RE.search('about 2.3 % of a 1,333,333-clock frame.')
    assert m and m.group('pct') == '2.3' and _norm(m.group('base')) == '1333333', 'PCT_RE: declared'
    m = PCT_RE.search('32,768 clocks, ~2.5 % of a frame')
    assert m and m.group('pct') == '2.5' and m.group('base') is None, 'PCT_RE: undeclared'
    m = PCT_RE.search('about 10% of the reserved 1,333,333-clock frame')
    assert m and m.group('pct') == '10' and _norm(m.group('base')) == '1333333', 'PCT_RE: reserved'
    assert not PCT_RE.search('40.2 % at four client-A arms'), 'PCT_RE: false positive'

    # --- CEILING_RE finds a stated ceiling, not a percentage base ---
    assert CEILING_RE.search(
        'Frame acceptance ceiling: <= 850,000 Field/Earth-slice clocks for the'
    ), 'CEILING_RE: EARTH ceiling'
    assert not CEILING_RE.search('2.3 % of a 1,333,333-clock frame'), 'CEILING_RE: false positive'


_selftest()


def scan() -> dict:
    by_contract: dict[str, dict] = {}
    for path in sorted(CONTRACTS.glob('*.md')):
        text = path.read_text(encoding='utf-8', errors='replace')
        denoms: Counter[str] = Counter()
        pcts: list[dict] = []
        ceilings: list[dict] = []
        for lineno, line in enumerate(text.splitlines(), 1):
            for d in DENOM_RE.finditer(line):
                denoms[_norm(d.group(1))] += 1
            for m in PCT_RE.finditer(line):
                pcts.append({
                    'line': lineno,
                    'percent': float(m.group('pct')),
                    'base': _norm(m.group('base')) if m.group('base') else None,
                    'text': line.strip()[:160],
                })
            for m in CEILING_RE.finditer(line):
                ceilings.append({
                    'line': lineno,
                    'clocks': int(_norm(m.group('clocks'))),
                    'text': line.strip()[:160],
                })
        if denoms or pcts or ceilings:
            by_contract[path.stem] = {
                'denominators': dict(denoms),
                'percent_claims': pcts,
                'stated_ceilings': ceilings,
            }
    return by_contract


def report(by_contract: dict) -> dict:
    """Group the contracts by which frame length they price themselves against."""
    groups: dict[str, list[str]] = defaultdict(list)
    for name, rec in sorted(by_contract.items()):
        ds = set(rec['denominators'])
        if not ds:
            key = 'NO FRAME LENGTH STATED'
        elif len(ds) == 1:
            key = KNOWN_CLOCKS.get(next(iter(ds)), ('unrecognised', 0))[0]
        else:
            key = 'BOTH / MIXED IN ONE CONTRACT'
        groups[key].append(name)

    declared = [p for r in by_contract.values() for p in r['percent_claims'] if p['base']]
    undeclared = [p for r in by_contract.values() for p in r['percent_claims'] if not p['base']]

    return {
        'scope': ('What the contracts CLAIM about frame budgets and which clock base each '
                  'claim is quoted against. No percentage is recomputed here; no budget is '
                  'proposed; no claim is checked for staleness.'),
        'contracts_scanned': len(list(CONTRACTS.glob('*.md'))),
        'contracts_with_frame_claims': len(by_contract),
        'grouped_by_frame_length': {k: sorted(v) for k, v in sorted(groups.items())},
        'percent_claims_total': len(declared) + len(undeclared),
        'percent_claims_with_declared_base': len(declared),
        'percent_claims_with_no_declared_base': len(undeclared),
        'declared_bases_seen': sorted({p['base'] for p in declared}),
        'why_this_blocks_a_sum': (
            'Percent-of-frame claims are only addable when they share a denominator. '
            'These do not: two different frame lengths appear across the contracts, and '
            'most percent claims name no frame length at all. Adding them would measure '
            'a machine that does not exist -- the mismatched-comparison error in '
            'CLAUDE.md, applied to a budget instead of to a creature.'),
    }


def main(argv: list[str]) -> int:
    by_contract = scan()
    summary = report(by_contract)
    out = {'summary': summary, 'by_contract': by_contract}
    if '--json' in argv:
        print(json.dumps(out, indent=2))
        return 0

    s = summary
    print('contracts scanned .................. %d' % s['contracts_scanned'])
    print('contracts with frame claims ....... %d' % s['contracts_with_frame_claims'])
    print()
    for key, names in s['grouped_by_frame_length'].items():
        print('%-30s %3d  %s' % (key, len(names), ', '.join(names[:6])
                                 + (' ...' if len(names) > 6 else '')))
    print()
    print('percent-of-frame claims ........... %d' % s['percent_claims_total'])
    print('  with a declared frame length .... %d' % s['percent_claims_with_declared_base'])
    print('  with NO declared frame length ... %d' % s['percent_claims_with_no_declared_base'])
    print('  distinct declared bases ......... %s' % ', '.join(s['declared_bases_seen']))
    print()
    print(s['why_this_blocks_a_sum'])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
