/**
 * V23 — THE RESOURCE CENSUS. Read the block-fit report and say the numbers out
 * loud, on every ledger run.
 *
 * The design walked to 2.9x the device's DSP capacity with every gate green,
 * because nothing looked. `resource_budget.dsp_percent` and
 * `resource_actual.dsp` exist in the schema and in types.ts, and NOTHING writes
 * them and no validator reads them. The ALM side has V5, which sums
 * `alm_percent` per budget group against a ceiling — which is exactly why ALM
 * growth never surprised anyone here, and DSP growth did.
 *
 * WHAT THIS DELIBERATELY DOES NOT DO IS FAIL ON A TOTAL. Per-block fits give
 * every block its own multipliers, its own I/O and no sharing, so each sum is an
 * UPPER BOUND, not a prediction — the composed shell currently infers 0 DSPs
 * because none of the measured geometry blocks is integrated yet, and the ALM
 * sum reads 116% of the device while the composed shell fits in 7,442. A hard
 * gate on an upper bound is a FALSE gate, and a false gate is worse than none:
 * it trains people to pass it rather than to fix the design.
 *
 * So it does two honest things instead:
 *
 *   1. PRINTS all three resources against device capacity on every run, so the
 *      numbers are in front of anyone who touches the ledger.
 *   2. FAILS only on a claim that cannot be an artifact of summing — a SINGLE
 *      block exceeding the whole device by itself. That is not an upper-bound
 *      effect, it is a block that cannot be placed.
 *
 * The owner's warning lines (docket, 2026-08-23) are DSP >95 of 112, ALM >80%,
 * M10K >85%. They are not enforced here for the reason above: they apply to the
 * COMPOSED figure, and this report is per-block. When a graphics-composed fit
 * exists, gate on that.
 */
export interface ResourceCensus {
  measured: number;
  /** Rows excluded because they are alternate PARAMETER SETTINGS of another
   *  row (see `variantOf` below). Reported rather than silently dropped. */
  variants: number;
  dsp: Line;
  alm: Line;
  m10k: Line;
}

/** One resource: the summed demand, the device's capacity, the worst offenders,
 *  and any SINGLE block that exceeds the device by itself. */
export interface Line {
  name: string;
  total: number;
  capacity: number;
  worst: Array<{ module: string; n: number }>;
  overCapacityBlocks: Array<{ module: string; n: number }>;
}

export function resourceCensus(report: unknown): ResourceCensus | null {
  const r = report as { blocks?: unknown[]; rows?: unknown[] } | null;
  if (!r) return null;
  const rows = (r.blocks ?? r.rows ?? []) as Array<Record<string, unknown>>;
  if (!Array.isArray(rows) || rows.length === 0) return null;

  let measured = 0;
  let variants = 0;
  const acc: Record<string, { total: number; capacity: number; per: Array<{ module: string; n: number }> }> = {
    dsp: { total: 0, capacity: 0, per: [] },
    alm: { total: 0, capacity: 0, per: [] },
    m10k: { total: 0, capacity: 0, per: [] },
  };
  const fields: Array<[string, string, string]> = [
    ['dsp', 'dspBlocks', 'dspBlocksAvailable'],
    ['alm', 'alms', 'almsAvailable'],
    ['m10k', 'ramBlocks', 'ramBlocksAvailable'],
  ];

  for (const row of rows) {
    if (row.status !== 'ok') continue;
    // A row carrying `variantOf` is the SAME BLOCK at a different parameter
    // setting, measured to expose a resource frontier -- the survey ruling asks
    // for two or three points per block rather than one architecture. Summing
    // them would count that block two or three times, so a three-point frontier
    // on a 72-DSP block would add ~150 phantom DSPs to the census and read as a
    // regression caused by measuring more carefully.
    //
    // They are NOT tested for single-block over-capacity either: a frontier is
    // allowed a deliberately infeasible end, and flagging it as unplaceable
    // would be a false alarm on a point nobody proposed to ship.
    //
    // Counted and reported, never silently dropped -- a census that quietly
    // omits rows reads as "covered everything" when it did not.
    if (typeof row.variantOf === 'string' && row.variantOf.length > 0) {
      variants += 1;
      continue;
    }
    measured += 1;
    for (const [key, nField, capField] of fields) {
      const n = typeof row[nField] === 'number' ? (row[nField] as number) : 0;
      const cap = typeof row[capField] === 'number' ? (row[capField] as number) : 0;
      if (cap > acc[key].capacity) acc[key].capacity = cap;
      acc[key].total += n;
      if (n > 0) acc[key].per.push({ module: String(row.module ?? '?'), n });
    }
  }
  if (measured === 0) return null;

  const line = (name: string, key: string): Line => {
    const a = acc[key];
    a.per.sort((x, y) => y.n - x.n);
    return {
      name,
      total: a.total,
      capacity: a.capacity,
      worst: a.per.slice(0, 3),
      overCapacityBlocks: a.capacity > 0 ? a.per.filter((p) => p.n > a.capacity) : [],
    };
  };

  return {
    measured,
    variants,
    dsp: line('DSP', 'dsp'),
    alm: line('ALM', 'alm'),
    m10k: line('M10K', 'm10k'),
  };
}
