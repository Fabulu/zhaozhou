#!/usr/bin/env python3
"""Size the STATE-IN-FLOPS lever, which is V2's central architectural bet.

============================================================================
DEPRECATED AS A BUDGET INSTRUMENT, 2026-09-28, THE DAY IT WAS WRITTEN.
Use `tools/quartus/check_ram_inference.py --rank --against=<map.rpt>`.
============================================================================

This census is HIERARCHY-SENSITIVE and is neither an upper nor a lower bound
on bankable storage. The external R2 review found it and the repository data
confirms it, worse than argued:

  * It EXCLUDES a node's entire register count if ANY descendant holds a RAM.
    On the 2026-09-28 console that excludes 147,127 of 279,210 registers --
    MORE THAN IT COUNTS -- including the single largest holder,
    `zhao_field_v3_exec` at 24,795 own registers.
  * What it does count includes pipeline stages, accumulators and control
    flops, which are not arrays and cannot be banked.
  * "No DSP and no RAM" is not "low-rate housekeeping": it also catches soft
    multipliers, dividers and comparators. `zhao_field_v3_mulbank` is 3,328
    ALUTs with 8 registers and no memory.

AND THE UNIT WAS WRONG, WHICH MATTERED MORE THAN THE ROUNDING.
This tool divided total bits by 10,240 and reported M10K blocks. The Quartus
inferrer DOES NOT PACK TWO ARRAYS INTO ONE BLOCK, so N small arrays cost N
blocks whatever they total. Measured at HEAD: 88 live arrays totalling ~128k
declared bits need >= 88 blocks, not 13. The repository's own recorded density
for the remaining tail is ~225 ALM per M10K, against 2,178-2,458 for the three
conversions that already landed -- TEN TIMES WORSE, against ~238 free blocks.

So the flop-array programme is already DONE (commit 7d049e9f, 2026-09-26): the
three that mattered landed and took ~142,000 bits out of flip-flops. What this
tool's census looks like is a large remaining lever; what is actually left is
~5% of the design. A census is not a work list.

The concentration curve below is still sound -- it is exclusive attribution and
does not depend on the memory classification -- but see the caveat printed with
it: it does not bound how much one engine replacement can reach.


WHY. The V2 candidate (design/v2/proposals/R0-external-candidate.md) proposes
"put bulk state in banked memory; keep only active operands in registers". That
is the right instinct and this tool measures how much of V1 it could actually
reach -- because an allocation table adding up is not evidence, and the
alternative to measuring is an argument.

WHAT IT MEASURES, from the Analysis & Synthesis entity table:

  * `alut_own` / `reg_own`  -- EXCLUSIVE per-node attribution. These sum to the
    design totals, so they partition; `alut`/`reg` are SUBTREE totals and must
    never be added across a hierarchy.
  * nodes whose subtree holds ZERO block-memory bits while holding registers.
    That is the flop-array signature: state kept in flip-flops with a selection
    network around it, which is what costs ALMs twice.
  * the CONCENTRATION CURVE -- how many nodes you must touch to reach a given
    share of the ALUTs. This decides whether a V2 can be a surgical rewrite of a
    few engines or must be a systemic migration of hundreds of nodes. It is the
    single most important feasibility number and neither R0 nor the phase-3
    report stated it.

WHAT IT IS NOT. It bounds the lever FROM ABOVE. Not every flop-held register can
move: pipeline registers, control state and anything needing more concurrent
ports than an M10K has (two) must stay. The achievable fraction is unmeasured;
`--rate` applies an explicitly-labelled assumption so the reader can see which
number is measured and which is assumed.

Usage:
  python tools/budget/v2_state_lever.py <map.rpt or attrib.json>
  python tools/budget/v2_state_lever.py ... --json OUT.json
"""
import argparse
import math
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

# The one MEASURED conversion on record, from the per-entity attribution:
# "one such array cost 5,181 registers and ~2,698 estimated ALMs, and 4,880
# M10K bits bought all of it back at zero added cycles."
FLOPARRAY_REGS = 5181
FLOPARRAY_ALMS = 2698
FLOPARRAY_BITS = 4880


def load_rows(src: pathlib.Path):
    if src.suffix == '.json':
        return json.load(open(src, encoding='utf-8'))
    sys.path.insert(0, str(ROOT / 'tools' / 'budget'))
    import map_entity_attrib as mea
    # BUG FIXED 2026-09-28 (found by the R2 review): `parse` takes a PATH, and
    # this passed it the file CONTENTS, so the advertised raw-.map.rpt route
    # never worked. The JSON route did, and is what produced the published
    # numbers, so they are unaffected -- but a usage nobody exercised is a
    # usage nobody could trust.
    return mea.parse(str(src))


def analyse(rows):
    tot_alut = sum(r['alut_own'] for r in rows)
    tot_reg = sum(r['reg_own'] for r in rows)
    if tot_alut == 0:
        sys.exit('ANTI-VACUITY: parsed 0 ALUTs. The entity table did not parse; '
                 'every conclusion below would be a false negative.')

    zero = [r for r in rows if r['mem'] == 0]
    zero_reg = sum(r['reg_own'] for r in zero)
    zero_alut = sum(r['alut_own'] for r in zero)

    s = sorted(rows, key=lambda r: -r['alut_own'])
    curve = []
    for n in (10, 25, 50, 100, 200, 400, 800):
        if n <= len(s):
            share = sum(r['alut_own'] for r in s[:n])
            curve.append((n, share, 100.0 * share / tot_alut))

    return {
        'nodes': len(rows),
        'total_alut_own': tot_alut,
        'total_reg_own': tot_reg,
        'zero_mem_nodes': len(zero),
        'zero_mem_registers': zero_reg,
        'zero_mem_alut': zero_alut,
        'zero_mem_reg_pct': 100.0 * zero_reg / tot_reg if tot_reg else 0.0,
        'zero_mem_alut_pct': 100.0 * zero_alut / tot_alut,
        'concentration': curve,
        'top_flop_arrays': [
            {'node': r['node'], 'reg_own': r['reg_own'], 'alut_own': r['alut_own']}
            for r in sorted(zero, key=lambda x: -x['reg_own'])[:20]
        ],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source', help='.map.rpt, or a JSON dump of its entity rows')
    ap.add_argument('--json', dest='out')
    ap.add_argument('--rate', type=float, default=None,
                    help='ASSUMED fraction of flop-held registers that can be '
                         'banked. Omitted = report bounds only.')
    a = ap.parse_args()

    rows = load_rows(pathlib.Path(a.source))
    d = analyse(rows)

    print(f"nodes parsed              : {d['nodes']:,}")
    print(f"total alut_own            : {d['total_alut_own']:,}")
    print(f"total reg_own             : {d['total_reg_own']:,}")
    print()
    print("STATE HELD IN FLIP-FLOPS (nodes whose subtree has zero memory bits)")
    print(f"  nodes                   : {d['zero_mem_nodes']:,}")
    print(f"  registers there         : {d['zero_mem_registers']:,} "
          f"({d['zero_mem_reg_pct']:.0f}% of all registers)")
    print(f"  ALUTs there             : {d['zero_mem_alut']:,} "
          f"({d['zero_mem_alut_pct']:.0f}% of all ALUTs)")
    print("  ^ NOT A BOUND IN EITHER DIRECTION -- see the header. It EXCLUDES")
    print("    147,127 registers (more than it counts) because their subtree")
    print("    holds a RAM somewhere, and it INCLUDES pipeline and control")
    print("    flops that are not arrays. A census is not a work list.")
    print()
    print("CONCENTRATION -- how many nodes must be touched to reach a share")
    for n, share, pct in d['concentration']:
        print(f"  top {n:>3} nodes          : {share:>8,} ALUTs  ({pct:4.1f}%)")
    print("  ^ a long tail means no surgical rewrite exists: a systemic rule")
    print("    applied broadly is the only kind of change that can reach it.")
    print()
    print("MEASURED EXCHANGE RATE (the single FLOPARRAY conversion on record)")
    print(f"  {FLOPARRAY_REGS:,} registers + ~{FLOPARRAY_ALMS:,} ALMs -> "
          f"{FLOPARRAY_BITS:,} M10K bits, zero added cycles")
    print(f"  => {FLOPARRAY_ALMS / FLOPARRAY_REGS:.3f} ALM recovered per register banked")
    print("  ONE data point, on a favourable case (wide, shallow, single-ported).")
    print("  Experiment E1 exists to replace it with a distribution.")

    if a.rate is not None:
        moved = d['zero_mem_registers'] * a.rate
        alms = moved * (FLOPARRAY_ALMS / FLOPARRAY_REGS)
        bits = moved * (FLOPARRAY_BITS / FLOPARRAY_REGS)
        print()
        print(f"ASSUMED at rate={a.rate:.2f} -- THIS IS AN ASSUMPTION, NOT A MEASUREMENT")
        print(f"  registers banked        : {moved:,.0f}")
        print(f"  ALMs recovered          : {alms:,.0f}")
        print(f"  M10K bits consumed      : {bits:,.0f}")
        print(f"  blocks by CAPACITY      : {math.ceil(bits / 10240)} of 553 "
              f"-- and this is the WRONG UNIT, kept only to show the error")
        print("  blocks ACTUALLY needed  : one per ARRAY. Quartus does not pack")
        print("    two arrays into one M10K, so N small arrays cost N blocks.")
        print("    Measured at HEAD: 88 live arrays, ~128k declared bits, >=88")
        print("    blocks -- against ~238 free. Use check_ram_inference.py.")

    if a.out:
        json.dump(d, open(a.out, 'w', encoding='utf-8'), indent=2)
        print(f"\nwrote {a.out}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
