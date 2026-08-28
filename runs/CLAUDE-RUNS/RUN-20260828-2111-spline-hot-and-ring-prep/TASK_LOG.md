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

---

## 2026-08-29 — the SPLINE lookup closes, 6930/6930

The neighbour phase is correct and the differential against the shipped
interpreter is green across every table, every knot, both neighbours of every
knot, every midpoint, and past both ends.

### The defect, and why it took three passes to see

`lookup_complete` fired at `ncyc == 6`. Addresses go out on cycles 0..5 and the
table's read is REGISTERED, so the datum addressed on cycle 5 does not land
until cycle 6 — as a non-blocking assignment, i.e. after the handoff on that
same edge has already sampled `s_p3`. The spline unit was therefore handed a
p3 that had not been written yet.

The phase is seven cycles, not six. This is the same "the thirteenth cycle
consumes the final read" that the search itself already pays, and I wrote that
sentence in the comment for the neighbour phase while getting the constant
wrong two lines below it.

### The symptom was actively misleading

* **96 of 6930**, so it read as an edge case rather than a structural error.
* Only on **midpoint** probes — the only ones where `t != 0`, so the cubic
  actually evaluates and a wrong p3 can move the answer. At a knot the answer
  is p1 regardless, so 3/4 of the probes passed while the bug was fully present.
* Only on the **64-knot** table, because on the small tables the clamps drove
  p3 to an index the previous group had already loaded.
* **The same probe passed in isolation.** The stale register held the correct
  value when the group before it happened to leave the right number there.

That last one is the finding. A value that is right when you test it alone and
wrong in sequence is not a flaky test — it is uninitialised state being read,
and the ordering dependence is the evidence, not the noise. I had already
recorded "one context is a DIFFERENT test, not a weaker one"; here the two
contexts disagreed and the disagreement was the whole diagnosis.

### What actually found it

Not reasoning — five temporary debug ports (`dbg_t3_o`, `dbg_p0..p3_o`) hauling
the lane-3 operands out to the C++ side, printed next to the oracle's. One line
of output settled it:

    DUT t3=1408 p0=7 p1=7 p2=1007 p3=-10345

`t` correct, `p0/p1/p2` correct, `p3` not a number in the table at all. That
localises the fault to one of six captures without a single guess about the
arithmetic. Hand-checking `t` first mattered — had `t` been wrong the whole
rescale path would have been suspect and the search much wider.

The ports and the C++ diagnostics are removed; the count drops 6948 -> 6930
because the DIAG group went with them.

### Standing

* SPLINE lookup + arithmetic: **green, 6930 checks**, lint clean at `-Wall`.
* NOT yet done, and none of it is assumed: CURVE.SVC's 18 mutants must be
  **re-scored** against the new shape (the phase is new logic and the old score
  does not carry), SPLINE into `field_long_width`, the service on the path,
  `UOP_RING_PREP`, and the two-service starvation measurement.
