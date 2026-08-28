# The long-op dispatcher: what the existing RTL forces, decided before writing it

Written 2026-08-28, after the noise unit and the curve service both work and
both handle refusal. This is piece 2 of `reports/FIELD_V3_SERVICE_ATTACH.md`
-- tag allocation and the writeback arbiter -- specified against the RTL that
already exists rather than against the sketch.

Every number below is read out of a shipped file and attributed, so a wrong
one can be traced instead of argued about.

---

## The constraint that shapes everything: ONE write port

`zhao_field_v3_rf` has exactly one:

```
    input var logic                        wr_en_i,
    input var logic [$clog2(CONTEXTS)-1:0] wr_ctx_i,
    input var logic [$clog2(REGS)-1:0]     wr_reg_i,
    input var logic signed [31:0]          wr_data_i,
```

One write, one context, one register, per clock.

A service reply carries **four points, which are four different contexts**. So
a reply cannot land in one clock, and the dispatcher is not free to pretend it
can:

| reply | registers to write | writeback clocks |
| --- | ---: | ---: |
| CURVE / DCURVE | one per point | **4** |
| RIDGE | one per point | **4** |
| NOISE2 | **two** per point (dst0 and dst1) | **8** |
| ROT2 | two per point | 8 |
| ROT3 | three per point | 12 |
| NORMALIZE2 | two per point | 8 |
| NORMALIZE3 | three per point | 12 |

The op shape table already says this -- `dst_width` in
`reference/include/zfield/generated/zfield_optable.hpp` is 1 for CURVE, 2 for
NOISE2 and NORMALIZE2 and ROT2, 3 for NORMALIZE3 and ROT3. The dispatcher
should read the width from there rather than special-casing opcodes, exactly
as the planner does.

### What that costs, next to what the op costs

A four-point NOISE2 is 20 clocks in the unit and **8 more** draining into the
register file. That is a 40% tail, and it is worth stating plainly now rather
than discovering it in a composition measurement: the writeback is not free
and it competes with the ALU lanes' own writes.

This is the first real argument for a second write port, and it should NOT be
taken yet. The measurement that decides it is the composed engine's occupancy
with services attached, and that measurement does not exist. A second port is
also not obviously cheap -- the file is banked four ways for group reads, and
a second writer touches that structure.

---

## The tag is a SLOT, not a context

The services carry an 8-bit tag and reply with it. The obvious reading -- tag
= context id -- does not work, because one tag covers four contexts.

So the tag indexes a small in-flight table:

```
  slot { valid, op, dst_base, ctx[4], lanes_used[4] }
```

and the reply routes back through it. Four fields, and each one is there for a
reason that has already bitten something in this project:

* **`ctx[4]`** because the four points are four contexts and the writeback has
  to know which is which.
* **`lanes_used[4]`** because a group is not always full -- see below.
* **`dst_base`** plus the shape table's `dst_width`, so a two- or three-wide
  destination group is written without the dispatcher knowing the opcode.
* **`op`** so the writeback knows the width without a second lookup.

### How many slots

One per service, to begin with. The curve service already holds four groups
internally and replies in accept order; the noise unit holds one. A single
outstanding group per service means the dispatcher never has to reorder, and
the FIFO property the services already guarantee is enough.

That is a deliberate under-use of the curve service's depth. It is the right
first step because it makes the composition testable, and the depth is
recoverable later by widening the table -- the services do not change.

---

## A group is not always full, and this is the case that gets forgotten

Every context runs the same program at a different point (`test_barrel_occupancy`
installs one fplan into all eight contexts and starts them together), so four
contexts reach a long op within a few clocks of each other and gathering is
usually easy.

**Usually is not always.** One context alone executing a CURVE is a legal
program; `zhao_probe_ctx_fifo` supports a single active context, and the
barrel test runs exactly that case for ALU ops.

Two rules follow, and both are cheap now and expensive later:

1. **Issue on "four gathered OR nobody else can join".** Waiting for a fourth
   context that has already finished its program is a deadlock. The second
   half of that condition is the part a test has to force deliberately,
   because it never happens in a full barrel.

2. **Pad unused lanes with a recognisable value, never zero.** Zero is a
   plausible coordinate and a plausible result, so a routing bug that let a
   padded lane reach a writeback would look correct.
   `zhao_probe_v3_engine` already ties its unused bank lanes to 3 and 5 for
   exactly this reason; the same argument applies here, and the same constants
   should be used so the two read as one decision.

`lanes_used[4]` in the slot is what makes rule 2 enforceable at the writeback:
a padded lane's result is discarded, and a test can assert that the padded
context's registers did not move.

---

## The context lifecycle already has a model, and my executor does not follow it

`zhao_probe_ctx_fifo`'s header describes it:

> S2 dispatch: short op completes and re-enqueues; long op enters the service;
> the LAST op releases the context.

and

> service completion RE-ENQUEUES it.

**`zhao_probe_v3_exec` does not do this.** Its `inflight_r` bit holds a context
for a fixed pipeline depth and releases it on a schedule. That is correct for
short ops and wrong for long ones: a context waiting on a service must leave
the ready set entirely, for a duration nobody can predict, and come back when
its reply lands.

The FIFO probe's model is the correct one and it is already built and measured
(257 ALM, 97.8 MHz). The dispatcher should use it rather than extending the
executor's fixed-depth bit, and that is a change to the executor, not only an
addition beside it.

---

## What the composition test has to prove, and what it must not assume

Three claims, and none of them can be tested in any block alone:

1. **Both services are served.** The bank's rule is "highest index wins",
   which is a total order, so with two services the lower-indexed one can
   starve. The bank's own differential has never had two services asking at
   once -- it measures a service beating the LANES, which is a different
   claim. The test must run both continuously and assert BOTH completion
   counts rise.

2. **A partial group completes and does not corrupt anybody.** Fewer than four
   contexts, padded lanes, and an assertion that the padded contexts' registers
   are untouched.

3. **A long op's context really leaves and really comes back.** Occupancy, not
   just values: the barrel's clock counts are pinned for ALU programs, and the
   equivalent pin for a program containing a long op is what would catch a
   context released early or re-enqueued twice.

The duty-cycle argument -- NOISE2 asks six times with two-clock gaps, CURVE
once per group at an II of 13, so collisions are rare -- is **not** an answer
to claim 1. It says collisions are rare, not that a starved claimant recovers.

---

## Order of work

1. The in-flight slot table and the writeback arbiter, with the ALU's own
   writes as the second claimant. Measured alone first.
2. Attach the curve service. Its probe is built, swept 18/18 and handles
   refusal.
3. Attach the noise unit. Now two services exist and claim 1 becomes testable.
4. Only then decide the second write port, on the occupancy the composed
   engine actually measures.

Step 3 is where the interesting failure lives, which is the argument for not
stopping after step 2 and calling the attach done.

---

## MEASURED, 2026-08-28: the writeback policy is DRAIN FIRST

The composition exists now (`zhao_field_v3_svcpath`), so the question this
document left open has an answer instead of an argument. Same traffic, one
model, an ALU asking every clock against a single four-point NOISE2 drain:

| policy | drain finished | drain served | ALU stalled |
| --- | ---: | ---: | ---: |
| ALU first | **never** | 0 | 0 |
| drain first | 31 clocks | 8 | 8 |
| round robin | 38 clocks | 8 | 8 |

**ALU-first starves the drain outright.** The argument for why it might be
tolerable -- a stalled drain holds contexts out of the ready set, which reduces
the ALU's own supply of work, so it is self-limiting -- is a claim about a
feedback loop. Both this document and the arbiter's header said in advance that
feedback-loop claims are the ones measurement overturns. It took one run.

**Drain-first costs the ALU exactly the drain's length**: eight stalls for
eight writes, once per group. Not a trade-off, a fixed and small price. Round
robin works too and is strictly worse -- the same eight ALU stalls plus seven
more clocks for the drain.

The starvation is now PINNED as a check rather than treated as a failure. It is
the evidence for the choice, and if a later change makes ALU-first live, that
is worth noticing rather than absorbing silently.

### What this does NOT settle

The second write port. A four-point NOISE2 still costs 20 clocks in the unit
and eight draining, and drain-first only decides who waits for the one port --
it does not make the tail shorter. That number wants the executor attached and
a real program mix before anyone spends the area.
