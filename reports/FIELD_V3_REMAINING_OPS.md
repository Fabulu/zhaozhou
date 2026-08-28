# The nine remaining Field IR ops: what each one actually costs

Written 2026-08-28, from the reference semantics and the v2 units, before any
v3 RTL for them exists. Numbers here are read out of
`reference/include/zfield/zfield_steps.hpp` and the v2 units' own state
machines, and each is attributed so a wrong one can be traced rather than
argued about.

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
