# SPEC v1: SPLINE goes HOT, and the prepared ring gets wired

**Run ID:** RUN-20260828-2111
**Created:** 2026-08-28 21:11
**Status:** Active
**Previous:** RUN-20260827-1747-field-v3-rearchitecture (ten blocks closed, engine
and service path composed at 84/84)

---

## The decision this run implements, and whose it is

**Fabian, 2026-08-28:** *"so we spend work but get a better thing. Non-issue.
Spend the work."*

He was answering two questions I had deliberately left open because they were
his to answer, not an agent's. Both are now decided the same way: take the
expensive option.

    SPLINE          HOT   -- not the brief's cold lane
    UOP_RING_PREP   WIRED -- the brief already costs it as the hot path

This SPEC records that the decision was ASKED FOR AND GIVEN, because a run that
quietly widens an architecture is indistinguishable later from one that
overstepped.

### What the alternative was

Cold cost nothing at all: `zhao_field_curve.sv` already computes SPLINE
exactly, lookup included, one point at a time, and Fieldv3.md section 6 puts
spline on the cold service lane. The engine would report it through
`unsupported_o` and programs needing it would run on the v2 path.

He chose the other one on purpose. This run is not allowed to quietly retreat
to cold because the work turns out to be awkward -- if it becomes the right
call, it goes back to him.

---

## Objective

SPLINE and the prepared ring execute on the v3 engine, through the service
path, bit-identical to `zfield::steps::exec_op`, with the same evidence every
other block in this family carries: a differential against the shipped
interpreter and a mutation sweep with no unexplained survivor.

---

## Scope

**In scope**

1. **The curve service fetches FOUR neighbours.** `zhao_probe_curve_svc.sv`
   barrels four lanes over a two-port table cache for CURVE and DCURVE. SPLINE
   needs p0..p3 -- `y[i-1]`, `y[i]`, `y[i+1]`, `y[i+2]` with the ends
   REPLICATED -- plus `x[i]` and `dy[i]` for the segment parameter. That is a
   width change on a service with a proven refusal path.
2. **Re-score CURVE.SVC's eighteen mutants** against the new shape. They ride
   on the current state machine and MUST NOT be assumed to carry.
3. **A spline service on the service path**, joining the lookup to
   `zhao_field_v3_spline.sv` (already closed 21/21, and takes p0..p3 as
   operands precisely because the lookup was going to be separate).
4. **SPLINE (0x1B) into `zhao_field_ops_pkg::field_long_width`** -- width 1.
   One line, and it is one line only because the executor and dispatcher now
   derive from a single table.
5. **`UOP_RING_PREP` (0xF1) into the same table**, and a ring service attached
   to the bank so it has something to answer it.
6. **The two-service starvation measurement**, which becomes possible for the
   first time: one service cannot starve anybody, and this run adds the second.

**Out of scope**

* `OP_RING` (0x21), the varying-radius form. The brief keeps it cold and
  nothing in this run changes that.
* Generating the opcode/width table from `design/ops.yml`. Worth doing --
  ops.yml has `field_ir_opcode` but no widths, so it needs a field, a generator
  change and a ledger update -- and it is not worth blocking this on.

---

## Constraints

* **Check the oracle resolves BEFORE writing RTL.** For SPLINE that is
  `zfield::steps::exec_op` case `OP_SPLINE`: `segment_search`, the clamp, `t`,
  p0..p3 with ends replicated, three coefficients, Horner, and the final
  `fx_add(p1, rescale_s32(v, 1))`.
* **Every sweep is re-scored, not assumed.** Anchors that stop resolving are
  repaired AND the sweep re-run -- repairing alone makes a table runnable and
  scores nothing.
* **One build tree, one writer.** No RTL edit while a sweep is reconfiguring.
* **The composed test is the acceptance gate**, not the block tests. Three of
  this engine's defects were only visible with four contexts and one needed
  eight.

---

## Don't Retry

Carried from the previous run because they cost real time there:

* **Do not stall the pipe for a slow consumer.** The multiplier is
  fixed-latency; freezing an instruction while its product arrives on schedule
  desynchronises the two. Measured wrong twice: 2 of 12 programs for multiplier
  denials, 1 of 12 for the write port. Retire into a skid instead.
* **Do not hold operands for only one freeze.** The register file's read is a
  pipeline stage that NO upstream freeze stops. Covering `mul_denied_c` and not
  `hold_c` left the penultimate context taking its successor's operands.
* **Do not let two blocks keep private copies of the same table.** That is what
  this run's own decision is unblocking, and it deadlocked the machine.
* **Do not test two contentions one at a time.** The write port alone passed 37
  checks over broken silicon.
