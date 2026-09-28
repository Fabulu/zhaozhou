#!/usr/bin/env python3
"""Size the STATE-IN-FLOPS lever, which is V2's central architectural bet.

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
    return mea.parse(open(src, encoding='utf-8', errors='replace').read())


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
    print("  ^ this BOUNDS the lever from above. Pipeline and control flops and")
    print("    anything needing >2 concurrent ports cannot move to an M10K.")
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
        print(f"  M10K bits consumed      : {bits:,.0f} "
              f"({bits / 10240:,.0f} M10K of 553)")

    if a.out:
        json.dump(d, open(a.out, 'w', encoding='utf-8'), indent=2)
        print(f"\nwrote {a.out}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
