// cost.ts — static cost report (spec/form/field-ir.md §9). Class assignment
// is frozen (§2); the cycle numbers are provisional until the RTL profile
// engine pins real latencies.

import { CostClass, CostSummary, FieldTable, OP_INFO, PhysOp, CLASS_CYCLES } from './types.js';

export function computeCost(code: PhysOp[], tables: FieldTable[],
                            regHighWater: number): CostSummary {
  const byClass: Record<CostClass, number> = { ALU: 0, MUL: 0, TABLE: 0, NOISE: 0, SPECIAL: 0 };
  for (const ins of code) byClass[OP_INFO[ins.op].cls] += 1;
  let cycles = 0;
  for (const ins of code) cycles += CLASS_CYCLES[OP_INFO[ins.op].cls];
  const dsp = byClass.MUL;   // provisional: SPECIAL-internal muls counted Phase 2+
  const tableBytes = tables.reduce((s, t) => s + 4 + 12 * t.points.length, 0);
  return {
    instrCount: code.length,
    byClass,
    cycles,
    dsp,
    tableBytes,
    regHighWater,
  };
}
