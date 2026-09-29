#!/usr/bin/env python3
"""Rank arrays that will not infer as memory and whose CLOCKED READ sits under more
than one condition -- the structural shape that kept the Field executor's 384 x 60
uop store in flip-flops.

READ THIS BEFORE USING THE OUTPUT
=================================
**Nesting depth is a SHAPE, not a CAUSE.** What was measured on
`zhao_field_v3_exec`'s `store` is narrower and the difference matters:

    the fetch was nested inside `if (!hold_c && !mul_denied_c)`, and that gate was
    REDUNDANT -- `issue_c`, the inner condition, already contained both of its
    terms, so removing the outer one changed nothing at all.

Five mapped reductions established it (`zhao_probe_execstore@R1/@R3a/@R3c/@R3d` in
`reports/synthesis/zhao_block_map.json`). `@R3c` is the decisive row: it moved the
write below the read, changed nothing else, and came back byte-identical to the
baseline, so statement order is free and the nesting is what Quartus chokes on.

**Redundancy cannot be decided textually, and this tool does not try.** A row here
means only "same shape, worth the same analysis". If the outer gate is NOT
redundant, hoisting the read CHANGES BEHAVIOUR and the repair is not available at
that site -- a different remedy is, but it is not free. Treating a row here as a
saving is precisely the error CLAUDE.md warns about: the comfortable explanation
arrives first and explains almost all of the evidence.

Rows also carry any OTHER cause `check_ram_inference.py` already names for that
array. Where one is present it is probably the binding one: `zhao_terrain_patch_acc`
is multidimensional AND has two distinct write addresses, and no amount of
un-nesting fixes either.

WHAT A ROW IS EVIDENCE OF
=========================
That an array is (a) in the committed uninferred ranking, and (b) read inside a
clocked process under two or more conditions. Nothing else. It is not a measurement,
not a saving, and not a claim that the array can be a memory at all -- a large array
genuinely read on three ports every cycle belongs in flip-flops.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RTL = ROOT / 'fpga' / 'rtl'
RANK = ROOT / 'design' / 'v2' / 'evidence' / 'ram_inference_rank_widened.txt'

_END_NOT_BLOCK = re.compile(r'\bend\b(?!module|case|function|task|generate|package|interface)')


def clocked_read_depth(text: str, name: str) -> tuple[int, int, str] | None:
    """How many CONDITIONS enclose a read of `name` inside an `always_ff`.

    Returns (conditions, line, source) for the deepest such read, or None.

    Counting CONDITIONS rather than `begin`/`end` depth is the whole point, and the
    first version of this got it wrong: absolute block depth includes the process's
    own `begin` and the reset `else begin`, so a read sitting in the plain body
    scored 2 and the repaired store looked exactly like the broken one. Its own
    self-test caught that, which is the only reason it is not still wrong.

    The reset condition is EXCLUDED by name (`rst_n`): every clocked process here is
    `if (!rst_n) ... else ...`, so counting it would add one to every row and
    distinguish nothing.

    A dangling `if (c)` with the statement on the following line is carried forward,
    because that is how the repaired store is written and a measure that missed it
    would report the repair as unchanged.
    """
    rd = re.compile(r'<=[^;]*\b%s\s*\[' % re.escape(name))
    in_ff = False
    stack: list[str | None] = []
    pending: str | None = None
    best: tuple[int, int, str] | None = None

    for lineno, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if s.startswith('//') or not s:
            continue
        if re.match(r'always_ff\b', s):
            in_ff = True
            stack = [None] if re.search(r'\bbegin\b', s) else []
            pending = None
            continue
        if not in_ff:
            continue

        # `end else begin` / `end else if (...) begin` replace the top frame.
        m = re.match(r'end\s+else\s+if\s*\((.*)\)\s*begin\b', s)
        if m:
            if stack:
                stack.pop()
            stack.append(m.group(1))
            pending = None
            continue
        if re.match(r'end\s+else\s+begin\b', s):
            prev = stack.pop() if stack else None
            stack.append(prev)  # the else branch is guarded by the same condition
            pending = None
            continue

        if rd.search(s):
            n = sum(1 for c in stack if c and 'rst_n' not in c)
            if pending and 'rst_n' not in pending:
                n += 1
            m_inline = re.match(r'if\s*\((.*?)\)\s*\S', s)
            if m_inline and 'rst_n' not in m_inline.group(1):
                n += 1
            if best is None or n > best[0]:
                best = (n, lineno, s[:140])

        # A bare `if (c)` with its statement on the next line.
        m_dangle = re.match(r'if\s*\((.*)\)\s*$', s)
        pending = m_dangle.group(1) if m_dangle else None

        opens = len(re.findall(r'\bbegin\b', s))
        closes = len(_END_NOT_BLOCK.findall(s))
        if opens:
            m_if = re.search(r'\bif\s*\((.*)\)\s*begin\b', s)
            cond: str | None = m_if.group(1) if m_if else None
            for _ in range(opens):
                stack.append(cond)
                cond = None
        for _ in range(closes):
            if stack:
                stack.pop()
            else:
                in_ff = False
        if closes and not stack:
            in_ff = False
    return best


def parse_ranking(path: pathlib.Path) -> list[dict]:
    """Rows of the committed uninferred ranking, with the causes it already names."""
    rows: list[dict] = []
    cur: dict | None = None
    head = re.compile(r'^\s*(\d+) bits\s+(\S+)\s+(\w+)\s+\[(\w+)\]')
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        m = head.match(line)
        if m:
            cur = {
                'bits': int(m.group(1)),
                'path': m.group(2),
                'array': m.group(3),
                'klass': m.group(4),
                'existing_causes': [],
            }
            rows.append(cur)
        elif cur is not None and line.strip().startswith('- '):
            cur['existing_causes'].append(line.strip()[2:])
    return rows


def _selftest() -> None:
    """A detector that has not been shown to fire has not been tested.

    The fixture is the uop store's shape BEFORE the 2026-09-29 repair. It cannot be
    taken from the tree any more -- the repair removed it -- which is exactly why it
    is inlined here: a positive control whose subject no longer exists is the kind
    that silently stops testing anything.
    """
    before = """
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          s1_v_r <= 1'b0;
        end else begin
          if (up_we_i)
            store[wa] <= payload;
          if (!hold_c && !mul_denied_c) begin
            s1_v_r <= issue_c;
            if (issue_c) begin
              s1_uop_r <= store[ra];
            end
          end
        end
      end
    """
    got = clocked_read_depth(before, 'store')
    assert got is not None, 'selftest: the pre-repair read was not found at all'
    assert got[0] == 2, 'selftest: pre-repair read is under TWO conditions, got %r' % (got,)

    after = """
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          s1_v_r <= 1'b0;
        end else begin
          if (up_we_i)
            store[wa] <= payload;
          if (!hold_c && !mul_denied_c) begin
            s1_v_r <= issue_c;
          end
          if (issue_c)
            s1_uop_r <= store[ra];   // dangling if: ONE condition, not two
        end
      end
    """
    got2 = clocked_read_depth(after, 'store')
    assert got2 is not None, 'selftest: the repaired read was not found'
    assert got2[0] == 1, 'selftest: repaired read is under ONE condition, got %r' % (got2,)

    # And it must not report a WRITE as a read.
    assert clocked_read_depth(
        'always_ff @(posedge clk) begin if (a) begin if (b) begin m[x] <= y; end end end',
        'm') is None, 'selftest: a write was mistaken for a read'


_selftest()


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--min-conditions', type=int, default=2)
    ap.add_argument('--min-bits', type=int, default=4096)
    ap.add_argument('--json', action='store_true')
    args = ap.parse_args(argv)

    if not RANK.exists():
        print('missing ranking: %s' % RANK, file=sys.stderr)
        print('regenerate it with tools/quartus/check_ram_inference.py', file=sys.stderr)
        return 2

    out = []
    for row in parse_ranking(RANK):
        if row['bits'] < args.min_bits:
            continue
        stem = row['path'].split('/')[-1]
        found = list(RTL.rglob(stem))
        if not found:
            continue
        hit = clocked_read_depth(found[0].read_text(encoding='utf-8', errors='replace'),
                                 row['array'])
        if hit is None or hit[0] < args.min_conditions:
            continue
        out.append({**row, 'conditions': hit[0], 'line': hit[1], 'source': hit[2],
                    'file': str(found[0].relative_to(ROOT)).replace('\\', '/')})

    out.sort(key=lambda r: -r['bits'])

    if args.json:
        print(json.dumps({
            'scope': ('Arrays in the committed uninferred ranking whose clocked read sits '
                      'under two or more conditions. A SHAPE, not a cause: the measured '
                      'cause is a REDUNDANT outer gate, and redundancy is not decidable '
                      'from the text.'),
            'candidates': out,
        }, indent=2))
        return 0

    print('SAME SHAPE AS THE REPAIRED uop STORE: clocked read under >= %d conditions'
          % args.min_conditions)
    print('(a shape, NOT a cause -- see this file\'s header before quoting a row)')
    print()
    for r in out:
        flag = ' <-- another cause already named' if r['existing_causes'] else ''
        print('%8d bits  conds %d  %s  %s:%d%s'
              % (r['bits'], r['conditions'], r['array'], r['file'], r['line'], flag))
        print('              %s' % r['source'])
        for c in r['existing_causes']:
            print('              ALSO: %s' % c[:120])
    print()
    print('%d candidates at >= %d bits. Each needs the same per-site analysis the store'
          % (len(out), args.min_bits))
    print('got: is the OUTER gate redundant with the inner one? If not, hoisting the read')
    print('changes behaviour and this repair is not available there.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
