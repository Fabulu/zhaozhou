# The nine remaining Field IR ops: what each one actually costs

Written 2026-08-28, from the reference semantics and the v2 units, before any
v3 RTL for them exists. Numbers here are read out of
`reference/include/zfield/zfield_steps.hpp` and the v2 units' own state
machines, and each is attributed so a wrong one can be traced rather than
argued about.

---

## WHERE THIS PLAN ACTUALLY GOT TO (added 2026-08-28, night)

Everything below was written before any of it existed. This block says what is
now true, so the plan can be read as a record rather than as a to-do list that
has quietly gone stale. **The six-step order below was followed and it held.**

    step 1  mul_ready into the remaining units       DONE
    step 2  NOISE2 / RIDGE                           DONE, swept 23/23
    step 3  ROT2 / ROT3                              DONE, swept 24/24
    step 4  RING                                     DONE, swept 23/23
    step 5  NORMALIZE2 / 3                           DONE, sweep running
    step 6  SPLINE                                   DONE FOUR-WIDE, swept 21/21
                                                     (see the CORRECTION below --
                                                      the lookup was already built
                                                      scalar, and four-wide SPLINE
                                                      may not be wanted at all)

Plus the carriers the plan did not name because they were not ops: a
dispatcher that gathers four points into one request, a writeback arbiter, and
the executor's path for parking a point while its answer is computed.

### CORRECTION, same night, an hour later: I had this wrong

The paragraph that stood here said "SPLINE's lookup half is not built" and
named widening the curve service as the next step. **Both halves of that are
wrong, and I wrote them without opening the file I was making claims about.**

**The lookup IS built, and it is verified.** `fpga/rtl/field/zhao_field_curve.sv`
implements OP_CURVE, OP_DCURVE *and OP_SPLINE* -- the six-step segment search,
the clamp, `t`, all four control points with the ends replicated
(`S_P0/S_P2/S_P3` carry exactly the reference's `i-1`, `i+1`, `i+2` index
clamping), the three coefficients and the Horner. It is live in four test
targets and instantiated by both `zhao_field_v2_core` and
`zhao_field_exec_shared`. What it does NOT do is four points at a time.

**And widening the v3 curve service is not obviously the right move, because
the brief already decided the opposite.** `zhao_probe_curve_svc.sv` says so in
its own header -- "MODES: CURVE (0) and DCURVE (1) only. SPLINE is COLD by the
brief's own service split (section 6 'cold service lane': spline) and is not
barreled." Fieldv3.md section 6 lists spline among the operations that keep
their complete exact SCALAR implementation and are classified cold: exact, but
not certified for the maximum live-field workload.

So the accurate statement of the gap is this:

    SPLINE, one point at a time, from a table    BUILT and verified (v2)
    SPLINE, four points at once, given p0..p3    BUILT and swept 21/21 (v3)
    SPLINE, four points at once, from a table    NOT BUILT -- and possibly
                                                 SHOULD NOT BE

### The real open question, which is architectural and was decided by accident

`zhao_field_v3_spline.sv` computes FOUR points at once. That shape is only
motivated if SPLINE is a hot op. The brief says it is cold. **I built a hot
block for an op the architecture had classified cold, and I did not notice
until I went looking for its lookup.**

Nothing is broken by this -- the block is correct, closed at full marks, and
the four-wide maths is a superset of the one-wide maths. But it is unpaid work
if SPLINE stays cold, and the decision deserves to be made rather than drifted
into:

* **If SPLINE stays COLD** (the brief's position): the v2 unit already
  implements the whole op and nothing further is needed for correctness. The
  v3 four-point block becomes either unused or the seed of a later promotion,
  and should be labelled as such rather than left looking like a gap.
* **If SPLINE goes HOT**: the four-point block is right, and it needs a
  four-point table lookup -- which is a width change on a service that already
  has a proven refusal path and eighteen mutants riding on its current shape.
  Widening means re-deriving the state machine, re-deriving its bank
  arithmetic, and RE-SCORING those eighteen rather than assuming they carry.

The cost is very different, and it is not mine to guess at silently. It goes to
Fabian with both prices attached.

### A related gap that is not recorded as a decision anywhere

`zhao_field_v3_dispatch.sv`'s `dst_width_of` has no entry for SPLINE, so the
dispatcher REFUSES it -- correctly, since 0 means "not a long op this block
knows" and refusing beats guessing a width. That is consistent with SPLINE
being cold. But it is consistent by accident: nothing in that file says "SPLINE
is deliberately absent because it is a cold-lane op". A reader finds a missing
case, not a decision. It should say which it is.

### The composition rule below was right, and it earned its keep four times

"What must not be assumed" says every step ends at a COMPOSITION test, not at
the block's own sweep, because a sweep cannot test a claim a block makes about
somebody else. That has now paid four times over -- the executor's open-loop
multiply, the curve service's hang, and the dispatcher's two missing operands
were all invisible to sweeps that scored full marks, and all four were found
within minutes of wiring two blocks together.

It is now a run of its own rather than a rule to remember:
`tools/sweep_field_v3_svcpath.sh`, twenty-five mutants, almost every one of
them a port map or a slot index. It does not re-check any block's arithmetic.

### Still argued rather than measured

The **two-service starvation question** in "The attach question the bank
raises" below is still open, and still for the reason given there: it needs
the curve service and the noise unit attached to the same bank at once. One
service cannot starve anybody. The composition built so far has one service
and a rival that exists to make refusal reachable -- enough to prove the round
trip and to price a refusal, not enough to answer this.

---

## THE NEXT STEP IS NOT "WIRE THEM TOGETHER" (added 2026-08-28, night)

The engine and the service path look ready to compose. `zhao_probe_v3_engine`
already exposes the whole long-op surface -- valid/ready, ctx, op, dst, s0..s3,
imm, flush, and the release pair -- and `zhao_field_v3_svcpath` consumes exactly
that shape. The ALU writeback matches too: the executor emits
`wb_valid_o/wb_ctx_o/wb_reg_o/wb_data_o` and the service path wants
`alu_wb_valid_i/alu_wb_ctx_i/alu_wb_reg_i/alu_wb_data_i`.

**Connecting them as they stand would silently drop ALU writes.**

`wb_valid_o` is a bare combinational assign:

    assign wb_valid_o = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;

There is **no `wb_ready_i` port on the executor at all**. It cannot be refused,
and it has nowhere to hold a result that is not taken. The service path's write
arbiter, meanwhile, refuses the ALU by design -- that is the entire point of it,
and section 5 of its differential MEASURES the ALU losing exactly eight clocks
to the drain on every four-point group. Every one of those eight clocks would
be a lost register write.

### This is the same defect the project has already paid for three times

`zhao_field_v3_svcpath`'s own header lists them:

    the executor's open-loop DOT      no mul_ready port to refuse it
    the curve service's hang          no mul_ready port at all
    the dispatcher's missing imm      no port to carry it

An open-loop producer meeting a consumer that can refuse. It is the fourth
instance, and the first one caught BEFORE the composition was built rather than
minutes after -- which is what the "compose before you integrate" rule was for.

### What it actually takes

1. **Give `zhao_probe_v3_exec` a `wb_ready_i`, and make it HOLD.** This is the
   structural rule every service claimant in this engine already obeys: an
   instruction may not be stalled between issue and arrival, so the claimant
   holds its request until granted. The executor's writeback must do the same.
2. **That is a pipeline change, not a port change.** The writeback is currently
   a pure function of stage 4's registers; holding means stage 4 cannot advance
   while a write is outstanding, which is backpressure into the barrel.
3. **Re-score the executor's 31 mutants.** They were written against a
   writeback that cannot stall. Assuming they carry to a shape with
   backpressure is exactly the assumption CURVE.SVC's eighteen are already
   flagged for.
4. **Only then compose**, and the composition test must prove ALU writes are
   never lost -- count them at the source and at the register file and require
   the two to agree, the same law the service path just gained for its ready
   lines.

### And one number needs re-reading

"The ALU loses exactly eight clocks and not a clock more" is the measurement
that makes services-first cheap rather than a trade-off. It is still true of the
arbiter. But it describes the ALU being REFUSED, and in the engine as it stands
today the ALU cannot be refused -- so the number characterises a machine that
does not exist yet. It becomes a real statement about the engine on the day
step 1 lands, and not before.

---

## THE FINDING THAT MATTERS MOST

**Six v2 op units drive the shared multiplier with no `mul_ready` input, and
every one of them is correct today for a reason that v3 removes.**

`zhao_field_exec_shared.sv` says so in its own header, and it is right:

> There is no arbiter, and that is the safety argument. Ten controllers can
> drive the lane and none of them can collide, because `zhao_field_seq` has
> exactly one instruction in flight.

One instruction in flight means one requester, which means every request is
granted, which means an issue port with no ready line is sound. The mux is
even written so an unselected unit's request *cannot* reach the lane.

**v3 breaks that premise deliberately.** The four-wide bank serves the
executor's lanes AND the curve service AND whatever else attaches, all at
once. That is the entire point of building it -- the first Field synthesis
measured 79 DSPs of 112 with nine of ten units idle. The moment two claimants
exist, "every request is granted" stops being true.

So attaching ANY of `zhao_field_len`, `zhao_field_normalize`,
`zhao_field_noise`, `zhao_field_ring`, `zhao_field_rot` or `zhao_field_curve`
to the v3 bank requires adding `mul_ready` to it first. This is not six
separate bugs. It is one premise that was true and is being retired, and the
work is bounded and mechanical **provided it is done before the attach rather
than discovered after it** -- which is exactly how the executor's DOT defect
and the curve service's hang were both found: at the seam, by a composition
test, after each block had scored full marks alone.

`zhao_probe_curve_svc` already has the port as of today. The other five do
not.

---

## Per-op demand

Counted from the reference semantics, then checked against the v2 unit's state
list where one exists. "Products" are 32x32 issues on the shared lane.

| op | products | other shared resources | v2 unit |
| --- | ---: | --- | --- |
| CURVE  | 1 | table cache + 6-step search | `zhao_probe_curve_svc` (v3, barreled x4) |
| DCURVE | 0 | table cache + 6-step search | same |
| SPLINE | 4 | table cache, search, **four neighbour reads** | none -- cold lane by the brief |
| NOISE2 | 6 | none | `zhao_field_noise` |
| RIDGE  | 4 | none | `zhao_field_noise` |
| RING   | 9 | **2 reciprocals** | `zhao_field_ring` |
| ROT2   | 4 | **2 sine-table reads** | `zhao_field_rot` |
| ROT3   | 4 | **2 sine-table reads** | `zhao_field_rot` (ROT2 is ROT3's Z case) |
| NORMALIZE2/3 | 3 + 4 + 2or3 | **isqrt, rcp24 seed ROM** | `zhao_field_normalize` |

### Where those numbers come from

* **NOISE2 = 6, RIDGE = 4.** `zref::noise2_hash` is four 32x32 truncating
  multiplies: `x*0x9E3779B1`, `y*0x85EBCA77`, the LCG advance `s*747796405`,
  and the RXS-M-XS `*277803737`. The first two depend only on (x, y), so the
  two NOISE2 lanes SHARE them and differ only by the lane salt: 2 + 2x2 = 6.
  RIDGE takes one lane: 2 + 2 = 4. `zhao_field_noise`'s header states six for
  NOISE2 independently, which is the check.
* **RING = 9 products, 2 reciprocals.** The v2 unit's states are
  `G_SPAN` (reciprocal), then `G_T`, `G_T2`, `G_2T`, `G_CUBE` -- four
  products -- and a `half` flag runs that sequence twice, once for the rising
  edge and once for the falling. `G_FIN` is the ninth: `s0 * (1 - s1)`.
* **ROT = 4 products, 2 sine reads.** `R_CP`, `R_SQ`, `R_SP`, `R_CQ` with a
  wait state each, after `R_COS` and `R_SIN`. ROT2 and ROT3 share the whole
  datapath; ROT3 only permutes which lanes are (p, q).
* **NORMALIZE = three squares, four Newton products, then one output product
  per lane.** `N_GATH` gathers the squares, `N_ROOT`/`N_WAIT` hand the sum to
  the shared root, `N_R0..N_R3` are the two Newton refinement pairs, and
  `N_LANE` issues the per-lane scale. Two lanes for NORMALIZE2, three for
  NORMALIZE3.
* **SPLINE = 4.** The reference does a `fx_mul` for `t`, then Horner:
  `fx_mad(t, C3, C2)`, `fx_mad(t, u, C1)`, `fx_mul(t, u)`. The coefficient
  combination itself (C1, C2, C3) is adds and small constant multiples of
  2, 3, 4 and 5 -- shifts and adds, not lane products.

### A correction to the blockers table

`reports/REMAINING_BLOCKERS.md` groups "CURVE, DCURVE, SPLINE, NOISE2, RIDGE"
as needing **nothing beyond the multiplier**. That is right for DCURVE (which
needs no product at all) and wrong in shape for the others:

* SPLINE needs the **table cache and the search**, and additionally **four
  neighbour reads** (`y[i-1..i+2]`) that CURVE does not make. The curve
  service captures the selected entry on the way down through its search
  precisely because it needs only one -- so SPLINE is not a mode of the
  existing service, it is a second reader of the same cache. The brief already
  calls it a cold lane; this is why.
* NOISE2 and RIDGE need no *extra unit*, but six and four products
  respectively, sequentially dependent. On a shared bank that is six and four
  chances to be refused.

The table is a good map of which SHARED UNITS each op borrows. It is not a
statement that five of the ops are nearly free.

---

## The order the work wants to go in

1. **`mul_ready` into the five remaining v2 units**, one at a time, each with
   a refusal test in its own differential that asserts refusals actually
   happened. Cheap, mechanical, and it is the precondition for everything
   below. The curve service's fix is the worked example: one port, one held
   state, one test section, three mutants.
2. **NOISE2 and RIDGE first among the ops**, because they borrow no unit but
   the bank. They are the smallest honest test of "an op unit on the v3 bank
   under contention", and they are the ones whose products are a straight
   dependent chain -- the shape most likely to expose a hold bug.
3. **ROT2/ROT3 next**, which adds exactly one shared resource (the sine
   table) and reuses a datapath that already exists.
4. **RING**, which adds the reciprocal -- and the reciprocal is itself a lane
   claimant, so RING is the first op that borrows a unit that borrows the
   bank. v2 got away with it because RING issues nothing while waiting on the
   reciprocal; that argument needs re-checking under an arbiter, not
   assuming.
5. **NORMALIZE2/3**, which adds the isqrt and the seed ROM.
6. **SPLINE last**, because it needs a second reader on the table cache and
   is the only one that is not a variation of something already built.

## What must not be assumed

The executor and the curve service both scored full marks alone while carrying
a defect that only exists at the seam. Every step above therefore ends at a
COMPOSITION test with a rival claimant that is proven to have actually refused
-- not at the block's own sweep. A sweep is how a block is checked against its
own claims; it cannot test a claim the block makes about somebody else.

---

## The shape NOISE2 and RIDGE want on a four-wide bank

Worked out before writing the unit, because the mapping turns out to be the
whole design and it is a good fit rather than a compromise.

**The v2 noise unit is scalar.** It walks one point through six product states
with a wait state after each, because the shared lane answers two clocks after
it is asked. The v3 executor is a FOUR-POINT machine and the bank is FOUR
WIDE, so the port is not "run the v2 unit four times":

> **One four-wide bank request = the same hash step for all four points.**

A four-point NOISE2 is then SIX bank requests, not twenty-four products
scheduled somehow. A four-point RIDGE is four. The v2 state machine survives
almost unchanged -- the same six steps in the same order -- and only its
datapath widens.

    step 1   x[l] * 0x9E3779B1      all four points        (shared by lanes)
    step 2   y[l] * 0x85EBCA77      all four points        (shared by lanes)
    step 3   s0[l] * 747796405      lane 0's LCG advance
    step 4   w0[l] * 277803737      lane 0's RXS-M-XS
    step 5   s1[l] * 747796405      lane 1's LCG advance
    step 6   w1[l] * 277803737      lane 1's RXS-M-XS

RIDGE stops after step 4; it uses lane 0 only.

Each step depends on the one before, so the sequence cannot be overlapped
within a group and the latency is six requests x (issue + 2) plus the finish.
That is the same dependent-chain shape as the executor's DOT, which is why
this is the right op to attach FIRST: it is the smallest thing that exercises
"a dependent product chain on a bank that can refuse", and that is precisely
the shape that took six attempts to get right in the executor.

### One correctness argument that has to be written down

`zref::noise2_hash` is defined on `uint32_t` and every product is **modulo
2^32**, never saturating. The bank lane is **33x33 signed**. Those agree on
the only bits that are read: the low 32 bits of a signed product are
bit-identical to the low 32 bits of the unsigned product of the same bit
patterns, because sign extension only affects bits at or above the operand
width. So the unit sign-extends into the lane, reads `mul_p_i[31:0]`, and the
result is exact -- and `zhao_field_noise` already does exactly this, with the
rule written at its operand mux as "law 3: modulo 2^32, never saturating".

That is worth restating rather than inheriting, because it is the one place
where the shared bank's signedness could silently disagree with an op's
semantics, and the disagreement would show up as a hash that is right for
small coordinates and wrong for large ones -- the failure that looks like a
bad seed rather than a bad multiply.

---

## The attach question the bank raises, before anything attaches to it

`zhao_field_v3_mulbank` is already parameterised for three claimants, so
wiring the noise unit alongside the curve service needs no change to it. What
does need deciding first is the PRIORITY, and the bank states its own rule:

> Claimant 0 is the ALU lanes; 1 and above are services. With
> `PRIO_SERVICES_FIRST` the highest index wins, so services outrank lanes.

**Highest index wins is a total order, so with TWO services the lower-indexed
one can be starved by the higher.** Today that is untested in both directions:
the bank's own differential measures a service beating the lanes on every
clock, which is the starvation it was built to demonstrate, but it has never
had two services asking at once.

The reason to raise this before the attach rather than after is the shape of
every defect found in the last two days: each block was correct alone, scored
full marks alone, and the fault lived at the SEAM. A starvation between two
services is exactly that kind of fault -- it cannot appear in either service's
sweep, and it will not appear in a composition test unless the test makes both
services ask at once and MEASURES that both were served.

### What the duty cycles say, which is not the same as a proof

Neither service saturates the bank on its own:

    four-point NOISE2   six requests, each followed by a two-clock wait
    four-point RIDGE    four requests, same shape
    four-point CURVE    ONE request per group, at an II of 13

So on a first reading the two cannot conflict often, and a fixed priority
looks harmless. That reading is a duty-cycle argument, not a starvation
argument: it says collisions are rare, not that a starved claimant recovers.
The measurement that would settle it is a composition test in which both
services run continuously and each one's completion count is asserted, which
is what the attach should carry.

**Do not resolve this by changing the arbiter first.** `PRIO_SERVICES_FIRST`
is a requirement rather than a preference -- `reports/FIELD_V3_SERVICE_ATTACH.md`
records why, and it was written after a round-robin proposal would have
silently broken a service that could not be refused. That reason has since
weakened, because the curve service and the noise unit both handle refusal
now, but "the reason for a rule has weakened" is not the same as "the rule is
wrong", and the order matters: measure the starvation, then decide.

## ROT's sine table probably should NOT be shared, and that is not a shortcut

The blocker table lists ROT2/ROT3 as needing "the shared sine table". Reading
`zhao_field_sin_rom.sv` changes the recommendation.

The ROM is **257 entries of 17 bits -- about 4.4 kbit, one M10K** -- with TWO
read ports, which is exactly the shape a four-point unit wants: each sine
evaluation reads `base` and `base+1`, so one point's lookup is one clock. Four
points needing cos and sin each is sixteen reads, eight clocks on two ports,
and then the four products are FOUR four-wide bank requests rather than
sixteen.

So a four-point ROT2 costs roughly eight lookup clocks plus four bank
requests, which puts it in the same range as NOISE2's twenty.

**The reason not to share it is the owner's own rule.** The sharing rule this
whole architecture came from is about SCARCITY: the first Field synthesis
measured 79 DSPs against a device with 112, and the answer was to share the
multipliers. The device has **553 M10K**. A private sine ROM costs one of
them and removes an arbitration path, a refusal path, and a starvation
question entirely -- all of which have now cost real time on the multiplier
side.

Sharing a plentiful resource buys nothing and adds exactly the class of defect
that has been the expensive one all week. Share what is scarce.

This is a recommendation with a number behind it, not a decision: if SIN and
COS as standalone ops also want a table, that is a second M10K, and two out of
553 is still the right trade. If a later count shows M10K pressure -- the
terrain and texture caches are the plausible source -- this is the first thing
to revisit, and the arbitration it would need is already designed twice over.

---

## The barrel is over POINTS of one program, and that is what makes a
## four-point service request easy -- and one case easy to forget

`test_barrel_occupancy` installs the SAME fplan into all eight contexts and
starts them together. That is the Field engine's shape: one program, many
evaluation points, one context per point. It is worth stating explicitly
because it settles how a four-point service request gets filled.

The services take four POINTS. The executor's four-wide register-file group is
four MEMBERS of one vector. **These are different axes**, and the attach has to
cross them: a four-point CURVE request is built from FOUR CONTEXTS that have
reached the same instruction, not from one context's group.

Because every context runs the same program, four of them arrive at a long op
within a few clocks of each other, so gathering is cheap. But it makes the
dispatcher wait for four, and that raises the case that will otherwise be
found late:

> **A long op must work when fewer than four contexts want it.**

One context alone executing a CURVE is a legal program -- the barrel test runs
exactly that case for ALU ops, and `zhao_probe_ctx_fifo` supports a single
active context. So the dispatcher needs a partial-group path: pad the unused
lanes with a recognisable value, issue anyway, and discard those lanes'
results.

Two things follow that are worth fixing in the design rather than in
debugging:

* **The pad must not be zero.** Zero is a plausible coordinate and a plausible
  result, so a routing bug that lets a padded lane reach a writeback would
  look correct. `zhao_probe_v3_engine` already ties its unused bank lanes to 3
  and 5 for this reason; the same argument applies here.
* **There must be a timeout or a flush, not just a wait-for-four.** If three
  contexts want a CURVE and the fourth has finished its program, waiting for a
  fourth is a deadlock. The condition to issue is "four gathered OR no other
  context can still join", and the second half of that is the part a test has
  to force.

Neither is exotic. Both are the kind of thing that is obvious while writing the
dispatcher and invisible afterwards, which is why they are written down before
it exists.
