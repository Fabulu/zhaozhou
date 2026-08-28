# Task Log: RUN-20260828-2111 - [Describe objective here]

**Created:** 2026-08-28 21:11 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-2111-spline-hot-and-ring-prep/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-28 21:11 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-2111
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## Rule 1 first: the oracle resolves, and it says what the lookup must fetch

`zfield::steps::exec_op` case `OP_SPLINE`, read before any RTL:

    i    = segment_search(t, src[0])          six compare/select steps, always
    a    = clamp_raw(src[0], x[0], x[n-1])    the table's own ends
    tt   = clamp(rescale((a - x[i]) * dy[i], 16), 0, 1<<16)
    p0   = y[i > 0     ? i - 1 : 0]           ENDS REPLICATED, not wrapped
    p1   = y[i]
    p2   = y[i + 1 < n ? i + 1 : n - 1]
    p3   = y[i + 2 < n ? i + 2 : n - 1]
    C1   = p2 - p0
    C2   = 2*p0 - 5*p1 + 4*p2 - p3            each ONE saturate to s32
    C3   = -p0 + 3*p1 - 3*p2 + p3
    u    = fx_mad(tt, C3, C2); u = fx_mad(tt, u, C1); v = fx_mul(tt, u)
    dst  = fx_add(p1, rescale_s32(v, 1))      the 1/2 of Catmull-Rom

**Everything from `C1` down already exists and is closed at 21/21** in
`zhao_field_v3_spline.sv`, which takes p0..p3 as operands precisely because the
lookup was always going to be a separate half. So this run builds the half
above the line, not the whole op.

## What the curve service actually looks like, before touching it

    tbl_ram[0:255]   96 bits per entry: {x, y, dy}
    two registered read ports, qa <= tbl_ram[ra_addr], qb <= tbl_ram[rb_addr]
    port A serves lanes 0 and 1, port B serves lanes 2 and 3, alternating on
      cyc[0] -- so one lane's registered-read wait is its partner's address
      cycle and both ports issue a useful address every search clock
    6 steps x 2 lanes x 2 cycles = 12 address slots per port; the 13th cycle
      consumes the final read and hands the group on. Designed II = 13.

Per-table meta registers (`meta_n`, `meta_x0`, `meta_xn1`, `meta_y0`,
`meta_dy0`) are latched at table load, which is why the search needs only its
six step reads: the clamp bounds and entry 0 are properties of the TABLE, and
the selected entry is captured ON THE WAY DOWN.

## The widening, costed before it is written

`x[i]`, `y[i]` and `dy[i]` arrive free -- they are the entry captured on the
way down. SPLINE needs **three more y values per lane**: `y[i-1]`, `y[i+1]`,
`y[i+2]`, each with the oracle's end replication.

    3 extra reads x 4 lanes = 12 addresses
    12 addresses / 2 ports  = 6 more cycles
    II 13 -> ~19, FOR SPLINE GROUPS ONLY

CURVE and DCURVE must be untouched by this, which means the extra phase is
entered on mode, not unconditionally. That is also the thing to check rather
than assume: the existing CURVE II is a fitted, measured number and a
regression in it would be a real cost paid for an op that did not need it.

**The clamping is the part to get wrong.** `i-1` clamps to 0 and `i+1`/`i+2`
clamp to `n-1` -- they do NOT wrap, and a table of two entries makes all four
control points collapse onto the same pair. That case belongs in the directed
test from the first commit, not added later when a mutant survives.

## Next, in order

1. Widen the service with the neighbour phase; keep CURVE/DCURVE's path and II.
2. Directed differential against `exec_op` -- including the two-entry table and
   both ends, where replication does all the work.
3. RE-SCORE CURVE.SVC's eighteen. They ride on the current state machine.
4. Join lookup to arithmetic as a spline service on the path.
5. SPLINE into `zhao_field_ops_pkg` -- one line.
6. Then the ring, and the two-service starvation measurement it unlocks.
