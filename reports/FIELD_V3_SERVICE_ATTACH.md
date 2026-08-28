# Attaching the long ops: what the probes already decided, and what my
# executor got wrong

> 2026-08-28. NORMALIZE2/3, CURVE, DCURVE, SPLINE, NOISE2, RING, RIDGE, ROT2
> and ROT3 are all refused by the v3 executor today — they fall through
> `zhao_field_alu`'s `default` arm and raise `unsupported_o`. This is the
> design for attaching them, written before RTL because reading the existing
> service probes changed two things I had assumed.

## The oracle resolves

Unchanged and already proven: `zfield::execute_point` implements every
canonical op, and the Phase 2 planner was swept 16/16 against it. So the
differential for every op below is the same one the executor already uses —
run the same FPLAN through both sides. No new oracle is needed, and none may
be invented.

## What the probes already decided, which I had not accounted for

### 1. The multiplier bank is FOUR WIDE and is engine property

`zhao_probe_curve_svc` does not contain a multiplier. It drives one:

```
    output logic               mul_issue_o;
    output logic signed [32:0] mul_a_0_o .. mul_a_3_o;
    output logic signed [32:0] mul_b_0_o .. mul_b_3_o;
    input  logic signed [65:0] mul_p_0_i .. mul_p_3_i;
```

and its own comment names the ownership: *"the vector multiplier bank (engine
property, not probe silicon)"*.

**My executor has one lane and its own private multiplier.** That was a
deliberate, stated simplification — the header says "ONE LANE, AND THAT IS THE
POINT… the four lanes are INDEPENDENT REPLICAS". It is wrong in one specific
way: the lanes are *not* fully independent, because they share the multiplier
bank with each other **and with the services**.

So the real engine has three claimants on one four-wide bank:

| claimant | when it needs the bank |
| --- | --- |
| the four ALU lanes | every MUL/MAD, one product per lane |
| the curve service | while stepping a lookup |
| the distance service | while squaring and summing |

That is an arbiter, and it is the single most load-bearing piece of the
composition. It does not exist yet in any form.

### 2. Services are four-lane and tagged, and reply IN ACCEPT ORDER

A request carries four points and an 8-bit tag; the reply carries four
results and the tag back. `zhao_probe_dist_svc` has the same shape. Two
consequences:

* the tag is how a reply finds its context, so the executor must allocate
  tags and hold the destination register with them;
* replies come back **in accept order**, so a service is a FIFO and no
  reordering buffer is needed — but two *different* services can complete out
  of order relative to each other, so the writeback port needs arbitration
  too.

### 3. The context FIFO already has a service state

`zhao_probe_ctx_fifo`'s header describes exactly the lifecycle this needs:

> S2 dispatch: short op completes and re-enqueues; long op enters the service;
> the LAST op releases the context.

and

> service completion RE-ENQUEUES it.

So a context issuing a long op leaves the ready set entirely and comes back
when its reply lands. That is *not* what my executor's `inflight` bit does
today — it holds the context for a fixed pipeline depth. The FIFO probe's
model is the correct one and it is already measured.

## The design

```
  ready-context FIFO ──issue──▶ fetch ──▶ RF read ──▶ classify
                                                        │
                          short op ──────────────────────┤
                                                        │
                          long op ──▶ tag alloc ──▶ service queue
                                                        │
        4-wide multiplier bank ◀── arbiter ──▶ ALU lanes │
                                                        │
                          writeback ◀── arbiter ◀────────┘
                                    └──▶ re-enqueue context
```

Four pieces, in the order they should be built and measured:

1. **The multiplier-bank arbiter.** Fixed priority, services above lanes,
   because a service already holds a context hostage while a lane can simply
   wait one clock. Its cost is a measurement, not a guess.
2. **Tag allocation and the writeback arbiter.** One tag per in-flight long
   op; the tag carries context and destination register.
3. **The service attach itself**, one service at a time: CURVE/DCURVE first
   because its probe is built and measured, then the distance service.
4. **The remaining ops**, which split into two groups that need different
   work — see below.

## The nine ops are not nine problems

| op | what it needs |
| --- | --- |
| ROT2, ROT3 | vector multiplies only — the ALU lanes plus the bank arbiter. No service. |
| NORMALIZE2/3 | a reciprocal-square-root, which v2 has as `zhao_field_normalize` |
| CURVE, DCURVE | the curve service, already probed and measured |
| SPLINE | the curve service's table path with a different step |
| NOISE2, RIDGE | `zhao_field_noise`, which v2 has |
| RING | `UOP_RING_PREP` — the planner already lowers it to 9 prepared multiply slots, so it is a BANK problem, not a service one |

So the bank arbiter unblocks ROT2/ROT3/RING immediately, and the service
attach unblocks the rest. That is the order.

## What this means for the DOT sequencer just built

Its "freeze issue globally while a DOT is in flight" rule was chosen because
it removed all multiplier contention **when there was one multiplier and one
lane**. With a four-wide bank and an arbiter, that freeze becomes the wrong
answer: it stalls three lanes that could have proceeded.

It is not wrong *today* — it is measured, correct, and costs +32% on eight
contexts. But it should be revisited **when the arbiter exists**, not before,
and the re-pinned barrel numbers (66 and 166 clocks) are what will show
whether the arbiter paid for itself.

## What is NOT decided here

The lane count of the shipped engine. Four is the brief's figure and the
services are built for four, but nothing in this note measures whether four
lanes of ALU plus a four-wide bank fits alongside the register file at the
target clock. The composition test measures that, and until it does, "four
lanes" is an inherited assumption rather than a result.
