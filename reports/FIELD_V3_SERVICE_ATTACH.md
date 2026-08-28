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

## CORRECTION 2: THE SERVICES CANNOT BE REFUSED, and two of them can ask at once

Found while specifying the curve-service attach, before writing it.

**Neither `zhao_probe_curve_svc` nor `zhao_probe_dist_svc` has a `mul_ready`
input.** Grepped both: zero matches. The curve service simply asserts

```
  assign mul_issue_o = (f_state == F_ISSUE);
```

and advances its state machine on the next clock regardless. A refused
service does not retry -- it proceeds as though the multiply had been issued
and later consumes a product that was never computed.

### What this changes

**`PRIO_SERVICES_FIRST` is not a preference. It is a requirement.** The
`zhao_field_v3_mulbank` header calls it "a CHOICE, not a law" and offers round
robin as "the obvious alternative". That is wrong as written: round robin would
refuse a service roughly half the time, and a refused service is silently
incorrect rather than slow.

**And services-first is not sufficient either.** The bank has three claimants,
and CURVE and DIST are both services. If both assert in the same clock one of
them loses -- and the loser has no way to know. Fixed priority merely decides
WHICH service is corrupted.

### The three ways out, none of them chosen yet

1. **Give the services back-pressure.** Add `mul_ready_i` to both and hold
   `F_ISSUE` until granted. Correct, and it changes two blocks that are
   already probed and measured -- their fits would need redoing.
2. **Guarantee they never ask together.** A single service queue that admits
   one long op at a time. Cheap, and it serialises CURVE against DIST even
   when the bank could have served both.
3. **Widen the bank so both fit.** Eight lanes. Doubles the DSP cost the
   79-DSP measurement was fought to avoid, so it needs a sustained-rate
   argument before anyone reaches for it.

**Option 1 is the honest one** -- a claimant that cannot be told "no" is not a
claimant, it is an assumption. But it touches measured blocks, so it is a
decision to take deliberately rather than a fix to slip in.

### Why this was not caught by the bank's own sweep

Because the bank is correct. It arbitrates exactly as specified and its 14
mutants all score. The defect is at the SEAM: a port that does not exist on
the other side cannot be mutated, and no sweep of either block can see a
missing wire between them. That is what composition tests are for, and it is
the argument for building the composition before the services rather than
after.

## CORRECTION, 2026-08-28: there are FIVE shared resources, not one, and
## v2 already arbitrates them

Everything above about the multiplier bank stands. The claim below it --
that ROT2, ROT3 and RING need "the ALU lanes plus the bank arbiter, no
service" -- is **wrong**, and reading the RTL before building them is what
caught it.

`zhao_field_rot` owns neither a multiplier nor a sine table. It borrows
BOTH, through ports, exactly like the curve service. It is a service.

And `fpga/rtl/field/zhao_field_exec_shared.sv` ALREADY EXISTS. It is v2's
answer to this entire problem, and it was built from a measured disaster --
its own header records the first synthesis of the Field engine at **10,623
ALMs and 79 DSPs against a device with 112**, because ten op units each owned
a private multiplier while nine of them sat idle at any instant.

It holds one of everything:

| resource | who takes turns on it |
| --- | --- |
| `zhao_field_mul` | every MUL/MAD/DOT, and CURVE, LEN, NOISE, NORMALIZE, RCP, RING, ROT |
| `zhao_field_isqrt` | LEN and NORMALIZE |
| `zhao_field_sin` | OP_SIN, OP_COS, and ROT's two reads |
| `zhao_field_rcp` | OP_RCP and RING's two smoothstep spans |
| `zhao_field_rcp24_rom` | NORMALIZE's other seed table |

So the nine remaining ops do not reduce to "bank clients" and "service
clients". They reduce to WHICH SHARED RESOURCES each one borrows:

| op | multiplier | sine | isqrt | rcp |
| --- | :-: | :-: | :-: | :-: |
| ROT2, ROT3 | yes | **yes** | | |
| NORMALIZE2/3 | yes | | **yes** | rcp24 rom |
| RING | yes | | | **yes** |
| CURVE, DCURVE, SPLINE | yes | | | |
| NOISE2, RIDGE | yes | | | |

### The owner's ruling is the law here, and it is quoted in that file

> Give each major subsystem the smallest local multiplier farm its SUSTAINED
> RATE actually needs, and share only operations that are MUTUALLY EXCLUSIVE
> inside that subsystem. DSP allocation is justified by sustained frame
> demand, not by preserving one-clock placeholder throughputs.

That decides the width question, and it decides it AGAINST uniformity.
`zhao_field_v3_mulbank` is four wide because **every instruction** may need a
product -- the sustained rate is one per instruction per lane. The sine,
isqrt and reciprocal units are needed only by specific ops, so making them
four wide as well would multiply the exact cost that produced the 79-DSP
measurement in the first place.

**So the working assumption for v3 is: the multiplier bank is four wide; the
other four resources stay ONE wide with a queue in front, until a measured
sustained rate says otherwise.** That is an assumption, not a result, and the
composition test is what turns it into one.

### What this changes about the build order

The order stated above -- "the arbiter unblocks ROT2/ROT3/RING immediately"
-- was wrong. The corrected order:

1. **CURVE, DCURVE, SPLINE, NOISE2, RIDGE** need only the multiplier, which
   is now arbitrated. These are the ops the arbiter actually unblocks.
2. **ROT2/ROT3** additionally need a shared sine table with its own arbiter.
3. **RING** additionally needs the shared reciprocal.
4. **NORMALIZE2/3** additionally need isqrt and the rcp24 seed ROM.

And the honest reading of `zhao_field_exec_shared` is that v3 does not need a
new design for any of this -- it needs that block widened where the sustained
rate demands it and left alone where it does not.

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
