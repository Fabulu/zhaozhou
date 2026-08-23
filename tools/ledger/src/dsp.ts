/**
 * V23 — THE DSP CENSUS. Read the block-fit report and say the number out loud.
 *
 * The design walked to 2.9x the device's DSP capacity with every gate green,
 * and the reason is embarrassingly simple: nothing looked. `resource_budget.
 * dsp_percent` and `resource_actual.dsp` exist in the schema and in types.ts,
 * and **nothing writes them and no validator reads them**. The ALM side has V5,
 * which sums `alm_percent` per budget group against a ceiling, which is why ALM
 * growth has never surprised anyone here. DSP had no equivalent, so 79 DSPs in
 * the Field IR engine and 72 in the skinner sat unnoticed until somebody
 * summed a report by hand.
 *
 * WHAT THIS DELIBERATELY DOES NOT DO IS FAIL ON THE TOTAL. Per-block fits give
 * every block its own multipliers with no sharing, so the sum is an UPPER
 * BOUND, not a prediction — the composed shell currently infers 0 DSPs because
 * none of these blocks is integrated yet. A hard gate on an upper bound would
 * be a false gate, and a false gate is worse than none: it trains people to
 * pass it rather than to fix the design.
 *
 * So it does two honest things instead:
 *
 *   1. PRINTS the measured total against device capacity on every run, so the
 *      number is in front of anyone who touches the ledger.
 *   2. FAILS only on a claim that cannot be an artifact of summing —
 *      a SINGLE block exceeding the whole device by itself. That is not an
 *      upper-bound effect, it is a block that cannot be placed.
 */
export interface DspCensus {
  total: number;
  capacity: number;
  measured: number;
  worst: Array<{ module: string; dsp: number }>;
  overCapacityBlocks: Array<{ module: string; dsp: number }>;
}

export function dspCensus(report: unknown): DspCensus | null {
  const r = report as { blocks?: unknown[]; rows?: unknown[] } | null;
  if (!r) return null;
  const rows = (r.blocks ?? r.rows ?? []) as Array<Record<string, unknown>>;
  if (!Array.isArray(rows) || rows.length === 0) return null;

  let total = 0;
  let measured = 0;
  let capacity = 0;
  const per: Array<{ module: string; dsp: number }> = [];

  for (const row of rows) {
    if (row.status !== 'ok') continue;
    const dsp = typeof row.dspBlocks === 'number' ? row.dspBlocks : 0;
    const cap = typeof row.dspBlocksAvailable === 'number' ? row.dspBlocksAvailable : 0;
    if (cap > capacity) capacity = cap;
    measured += 1;
    total += dsp;
    if (dsp > 0) per.push({ module: String(row.module ?? '?'), dsp });
  }
  if (measured === 0) return null;

  per.sort((a, b) => b.dsp - a.dsp);
  return {
    total,
    capacity,
    measured,
    worst: per.slice(0, 3),
    overCapacityBlocks: capacity > 0 ? per.filter((p) => p.dsp > capacity) : [],
  };
}
