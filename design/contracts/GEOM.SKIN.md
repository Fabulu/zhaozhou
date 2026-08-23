# Contract — GEOM.SKIN (Skinning stage)

> Ledger: `design/blocks.yml` · owner ZH-054 · phase 9 · maturity SPECIFIED

## Purpose and exclusions

Rigid and two-weight skinning of decoded vertices into world space.

## Clock and reset semantics

Single clock `clk`. Asynchronous active-low reset `rst_n`, released synchronously
by the shell. Reset clears the output register, its valid, the
`vertices_transformed` counter, and -- since 2026-08-23 -- the engine's own
state: the latched palette copy, the six row-product accumulators and their
done flags, the multiplier lanes' operand and product registers, the
destination pipeline, and every walk counter. The matrix *inputs* remain
upstream's, but the block now holds a copy of them and that copy is reset.

There is no second clock domain in this block. The bone palette arrives on `clk`
with the vertex, so no CDC applies here -- the palette's own crossing is
GEOM.POSE's to state.

## Input and output packet layouts

Per-vertex, ready/valid, one vertex per beat:

| Signal | Width | Meaning |
| --- | --- | --- |
| `v_x_i`, `v_y_i`, `v_z_i` | s32 | model-space position, fx16 |
| `v_w0_i` | u7 | weight of bone A in 1/64 quanta, 0..64 |
| `v_rigid_i` | 1 | `b1 == b0`, decided upstream |
| `v_src_id_i` | u16 | opaque tag, returned unmodified with the result |
| `a_m_i[12]`, `b_m_i[12]` | s32 x 12 | the two bone matrices, row-major fx16 |

Output carries `o_x_o` / `o_y_o` / `o_z_o` (s32 fx16, world space) and
`o_src_id_o`.

**The block does not index the palette.** It receives two already-selected
matrices. `b0` / `b1` never enter it, which is why `v_rigid_i` is an input rather
than a comparison: the bone indices live upstream, and a palette port's fan-in
would dominate this block's timing for no gain.

## Backpressure rules

`v_ready_o = !busy && (!o_valid_o || o_ready_i)` -- a single-entry skid in front
of a multi-cycle engine. A vertex is accepted only when `v_valid_i &&
v_ready_o`; on any other cycle the held result and the counter are untouched.

**`!busy` is the term the sequenced engine added, and it is load-bearing.**
There is one accumulator bank, so a `v_ready_o` that ignored `busy` would let a
second vertex overwrite the one in flight and emit a plausible skinned vertex
belonging to neither. Pinned by section 6, which asserts `v_ready_o == 0` on
*every* cycle between accept and result.

`o_valid_o` clears when the result is taken and sets when the blend walk writes
its last row; both in the same cycle is a legal back-to-back beat.

**The matrix inputs are LATCHED on accept.** They were read combinationally
when the block was one cycle deep, which was safe because the accept and the
read were the same clock. A ready/valid producer is free to change its data the
cycle after `v_ready_o` goes high, and the engine now reads the palette for up
to eighteen cycles after that, so it takes a copy.

Pinned by `tests/geometry/geom_skin_directed.cpp` section 6, which asserts that a
stalled vertex is neither counted nor allowed to overwrite the held result.

## Memory ownership

**None.** This block owns no memory, holds no cache, and never addresses the bus.
The bone palette belongs to GEOM.POSE and is presented combinationally with the
vertex; the block keeps a 24-word REGISTER copy of the two matrices for the
duration of one vertex, which is a pipeline latch and not a cache -- it is
overwritten by the next accept and is never addressed, indexed by bone, or
reused across vertices.

That is a deliberate split. Giving the skinner its own palette cache would
duplicate storage GEOM.POSE already holds and would put a second reader on the
pose memory for no benefit -- the vertex stream is what is wide here, not the
matrix stream.

## Q formats and rounding

All coordinates and matrix elements are fx16 (Q16.16). Weights are 1/64 quanta,
so a weight contributes 6 fraction bits.

Rigid (`v_rigid_i` or `w0 == 64`):

    o = rescale(A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16), 16)

Two-weight, with `w1 = 64 - w0`:

    pa = A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16)
    pb = B.m[r]*x + B.m[r+1]*y + B.m[r+2]*z + (B.m[r+3] << 16)
    o  = rescale(w0*pa + w1*pb, 22)

**Single rounding is the law** (qformats section 3, A3b). `pa` and `pb` are never
rounded before the blend; the whole expression is exact and is rounded once, by
22 = 16 matrix fraction bits + 6 weight fraction bits. `rescale` is round-half-up
then saturate to s32, per qformats sections 3 and 4.

Widths, **proven** rather than assumed (2026-08-23; they were merely *stated*
before, and two of them were two bits wider than the proof supports).
`QUARTUS_GOTCHAS` §5 is why this matters: 72-bit operands bought `zhao_geom_lod`
a 72x72 multiplier where 32x32 was honest, at 28 DSPs instead of 18.

| quantity | bound | signed bits | previously |
| --- | --- | ---: | ---: |
| `m*x` | <= 2^62 (both operands -2^31) | 64 | 64 |
| `pa` = 3 products + `(m3 << 16)` | <= 3*2^62 + 2^47 = 1.3835e19 < 2^64 | **65** | 67 |
| `pa - pb` | <= 2.767e19 < 2^65 | **66** | — |
| `w0*(pa - pb)`, `w0 <= 63` | <= 1.743e21 < 2^71 | **72** | — |
| `(pb << 6) + w0*(pa - pb)` | <= 2.629e21 < 2^72 | **73** | 75 |

The round-half-up addend (2^21) cannot disturb the last of these. `>>> 22`
leaves at most 2^50, which saturates to s32.

These are adders, not multipliers, so narrowing them saves ALMs and not DSPs —
but the rule §5 exists to enforce is *prove the width, then synthesise*, and an
unproven width is exactly what it punished. The bounds come from the s32 input
widths alone, not from the declared types; see the extremes section below,
which is where that argument stopped being a paragraph and became a check.

**The multiplier operands stay a full signed 32x32.** The DSP audit's
suggestion that a bone matrix's 3x3 is a bounded rotation is true of the
*content* and false of the *contract*: the oracle accepts any s32, so narrowing
the multiplier would be a behavioural change wearing an optimisation's clothes.

Double rounding is not a hypothetical risk. An RTL variant that rounds `pa` and
`pb` separately by 16 and blends by 6 passed an earlier draft of the directed
test **in full**, and failed only one random iteration in 300, by a single LSB.
The single-rounding wall section of the test exists to make that mutation fail
696 checks instead of one.

## Latency (fixed or variable)

**Variable, and it depends on the path and on `MUL_LANES`.** Rewritten
2026-08-23 when the block was sequenced; it was fixed:1 before.

| `MUL_LANES` | TL x RL | issue slots blend/rigid | latency blend/rigid |
| ---: | :---: | ---: | ---: |
| 1 | 1 x 1 | 18 / 9 | **24 / 15** |
| 3 | 3 x 1 | 6 / 3 | **12 / 9** |
| 6 | 3 x 2 | 3 / 2 | **10 / 9** |

Two of those clocks are the blend pipeline's fill, added 2026-08-23 when the
combinational blend measured 17.639 ns of data delay and had to be cut into
three stages. The three rows are in flight together -- row 0 is in B2 while
row 2 is in B0 -- so the walk still retires one row per clock and the pipeline
costs two clocks of fill, not six.

Latency is measured accept to `o_valid_o` and equals the sustained issue
interval, because `busy` is still high in the completion cycle and the next
vertex is therefore accepted on the following clock. Both numbers are asserted
by `tests/geometry/geom_skin_directed.cpp` section 8 at all three settings, so
a scheduling change that quietly costs three clocks turns a test red instead of
turning a frame short.

The rigid path is shorter because it forms three row-products, not six.

## Target throughput

**~120,000 skinned vertex instances per 60 Hz frame** (owner ruling,
2026-08-23) — the point at which the Measure degrades. That is the number this
block's multiplier count is derived from, and until it existed the earlier
version of this section correctly refused to invent one.

    gpu_clk                 100 MHz (10.000 ns, fpga/quartus/shell_fit/zhao_shell_fit.sdc)
    clocks per 60 Hz frame  1,666,666
    demand                  120,000 vertices
    clocks per vertex        13.88
    products per vertex      18 two-weight, 9 rigid
    honest multiplier count  18 / 13.88 = 1.30

| `MUL_LANES` | sustained vertices/frame @100 MHz | vs. demand |
| ---: | ---: | --- |
| 1 | 69,444 | **fails, 58%** |
| 3 | 138,888 | 1.16x |
| 6 | 166,666 | 1.39x |

**The acceptance test is rate, not clock**, which is what makes those two extra
pipeline clocks affordable:

    required = 120,000 vertices x 60 Hz = 7,200,000 vertices/s
    II = 12 needs 86.4 MHz ; II = 13 needs 93.6 MHz ; II = 14 fails at 100 MHz

So II = 12 is comfortable and 13 is the absolute ceiling. Report
vertices/frame from measured Fmax over measured II -- **and report raw Fmax
too**, because `gpu_clk` is shared and a block that stops at 86 MHz caps every
other block on the same clock.

### The verdicts do not depend on which clock number you believe

`gpu_clk` is constrained at 100 MHz by the SDC, the composed machine currently
measures about **95.5 MHz**, and the owner's recorded target is **120 MHz**
(`docs/OWNER_DOCKET.md`). All three are in play, so the table above would be a
weak argument if it only held at one of them:

| sustained vertices/frame | 95.5 MHz | 100 MHz | 120 MHz |
| --- | ---: | ---: | ---: |
| `MUL_LANES = 1` (24 clk) | 66,319 | 69,444 | 83,333 |
| `MUL_LANES = 3` (12 clk) | **132,638** | **138,888** | **166,666** |
| `MUL_LANES = 6` (10 clk) | 159,166 | 166,666 | 200,000 |

**`MUL_LANES = 3` clears 120,000 at every one of them and `MUL_LANES = 1`
fails at every one of them** -- still true after the pipeline cost two clocks,
and the margin at 95.5 MHz is 1.11x rather than 1.33x. The frontier's verdicts are a property of the
architecture, not of an assumption about the clock. The directed test asserts
against the 100 MHz figure because that is what the SDC constrains and what
every fit is measured against; at 95.5 MHz the margin is 1.33x rather than
1.39x, which changes nothing.

The owner's own estimate for this block was "rigid ~4 clocks, weighted ~10-11".
The weighted number landed at **10**. Rigid landed at **7**, not 4: three of
those clocks are the registered multiplier lane's latency plus the blend walk,
and they are the price of putting the DSP's own pipeline registers to work
rather than a combinational multiplier in front of an output register.

`MUL_LANES = 3` is the intended setting. `MUL_LANES = 1` is kept, built and
differentiated **because it fails**: a frontier with no failing end does not
show where the wall is, and section 8 of the directed test asserts that this
configuration is below the demand rather than letting it pass quietly.

The old target — "one skinned vertex per clock" — was 13.9x the demand and cost
72 DSP blocks on a 112-DSP device to deliver.

## Overflow and malformed-input behaviour

Saturation, never wraparound: a result beyond s32 clamps to `0x7FFFFFFF` or
`0x80000000`. Pinned by the saturation-rail cases in the directed test.

`w0 > 64` is outside the contract, and the rewrite made that matter more rather
than less. The old RTL's `w1 = 7'd64 - v_w0_i` wrapped to `192 - w0` there; the
weight identity now in use reads `64 - w0` as negative. **Neither is right,
because there is no right answer for an input the contract excludes** -- but
the two are different wrong answers, so the exclusion had to stop being an
assumption.

ENFORCED-BY: `tests/geometry/geom_skin_directed.cpp`, `require_legal_w0()`. It
runs on every vertex the differential drives and fails the suite if any driver
presents `w0 > 64`, so every comparison this project makes against the oracle
is knowingly a statement about the legal domain. The RTL header names this
check, which is why it is a check and not a comment asserting one.

**Owner docket / upstream obligation.** There is still no HARDWARE enforcer.
`v_w0_i` is 7 bits because 64 needs seven, not because 65..127 are meaningful,
and the upstream that must guarantee it -- GEOM.VDECODE -- is `SPECIFIED` with
no RTL. When GEOM.VDECODE is built it must either clamp or reject `w0 > 64`,
and this line is the record that the obligation was passed to it deliberately
rather than forgotten. GEOM.SKIN deliberately does **not** clamp: adding a
defensive clamp here would be inventing behaviour in the out-of-contract region
and would hide the missing upstream check behind a plausible answer.

A saturating result is not flagged. The reference threads a `SatLedger` for
exactly this telemetry and the RTL has no equivalent; see Counters.

## Counters and traces

`vertices_transformed_o` (u32) counts vertices **accepted**, not offered, and
saturates at `0xFFFFFFFF` rather than wrapping. A vertex held off by backpressure
is not work done, and the counter says so.

Not implemented: a saturation counter mirroring the reference's `SatLedger::mul`.
The reference records every clamped multiply; the RTL clamps silently. That gap
matters because a silently saturating vertex is a creature limb snapped to the
world edge -- visible on screen, and otherwise untraceable.

## The weight identity, and the test gap that gated it

**DONE 2026-08-23.** This section recorded an exact reduction, refused to take
it, and named two things that had to be handled first. Both were handled; the
reduction is in the shipped RTL. What follows is what was actually done, kept
in the same order as the objections so each one can be checked off.

    blend = w0*pa + (64 - w0)*pb   ==   (pb << 6) + w0*(pa - pb)

Six weight multiplies per vertex become three. The identity holds exactly over
the legal domain and the code permits it: `blend` is exact and unrounded and
`rescale_sat` is applied ONCE to the finished sum, which is the single-rounding
law and not a relaxation of it.

**Correcting this section's own arithmetic while it is being closed:** the
saving is **ALMs, not DSPs**. The block measured 72 DSP blocks and 72 = 18 x 4,
the eighteen signed 32x32 matrix products at four blocks each. `w0` and `w1` are
seven bits, and Quartus had already put both weight multiplies in logic. Any
report crediting the DSP reduction to this identity would be crediting the
wrong change; the DSP win is entirely in the multiplier farm.

**1. The identity is FALSE outside the legal domain. HANDLED.** See *Overflow
and malformed-input behaviour* above: `require_legal_w0()` in the differential
is the `ENFORCED-BY:` this section demanded, the RTL header names it, and the
missing upstream hardware check is recorded as an obligation on GEOM.VDECODE
rather than papered over.

**2. The differential did not reach the operand extremes. HANDLED, and it
mattered more than expected.** The rewrite narrowed the accumulator from 67
bits to 65 and the blend lane from 75 to 73 -- so the rewrite made the untested
argument *load-bearing* instead of merely true. Closed by:

- **section 7, the row-product rails**: 9 rail values for the matrix elements
  crossed with 9 for the vertex, with `B` set to the negative of `A` so
  `pa - pb` reaches twice `|pa|` -- the bound the 66-bit difference lane is
  sized for and the one nothing previously drove;
- **the near-cancellation family**, which is the sensitive part. With `B = -A`
  and `w0 == 32` the blend is `32*(pa + pb)`, so the ANSWER is small while
  every intermediate sits at its bound. A lane one bit too narrow wraps and
  gives a large wrong answer here, where anywhere else it would give the same
  saturated rail as the correct one;
- **section 7b, a full-range random lane** of 600 iterations drawing matrix and
  vertex elements across the whole s32 word.

7b is a **separate** lane rather than a widening of `--random`. The pose-range
lane is deliberately aimed away from the rails and a recorded mutation property
depends on that (the no-saturation mutation fails 6 directed checks and passes
900 random ones). Widening it would have destroyed a known property of an
existing lane to gain one the directed sections can carry.

## Scalar reference function

`zref::creature::skin_vertex` -- declared in
`reference/include/zref/zref_creature.hpp`, implemented in
`reference/src/zcreature/creature_core.cpp`.

Not a model written beside the RTL: it is the function the reference renderer
skins every creature with, so "RTL matches the oracle" means "the hardware moves
vertices exactly where the shipped pictures put them".

## Directed tests

`tests/geometry/geom_skin_directed.cpp` -- differential against
`zref::creature::skin_vertex` on every arithmetic case. **It is built THREE
times**, against `MUL_LANES` = 1, 3 and 6, as `test_geom_skin_directed`,
`test_geom_skin_lanes1` and `test_geom_skin_lanes6`.

Building the frontier's other points is not decoration. `MUL_LANES = 3`
collapses the term walk (TSTEPS == 1), so the default build **never executes
the multi-cycle accumulate path at all**; only the `MUL_LANES = 1` build does.
The mutation sweep found this the hard way in the useful direction: two mutants
are alive in one configuration and dead in another, and a sweep scored against
the default build alone would have reported them as survivors and invented a
test gap that does not exist.

Sections: rigid identity and translation; the branch boundaries (`w0` = 0, 1, 32,
63, 64, and `w0 == 64` with `b1 != b0`); the single-rounding wall; the rounding
boundary at exact halves, positive and negative; the saturation rails; the
interface laws; **the row-product rails (section 7)**; **a full-range random
lane (7b)**; and **the rate (section 8)**.

The **single-rounding wall** is the section this file exists for, and it was not
in the first draft. It sweeps every `w0` in 1..63 against eleven vertex residues
using 1-ULP and 3-ULP matrices, so `pa` carries a fraction below bit 16 and the
two rounding orders can disagree. Without it, the double-rounding mutation passed
all 39 original checks.

The **interface laws** section was added for the same reason: a mutation zeroing
the `src_id` passthrough survived all 2,118 arithmetic checks, because every one
of them compares coordinates and nothing else. Sequencing added one law to it --
a busy engine must refuse a vertex -- which is checked on every cycle between
accept and result rather than once.

**Section 7, the row-product rails**, closes the gap this contract recorded as
gating the weight identity. See that section above for what it drives and why
the near-cancellation family is the sensitive part.

**Section 8, the rate, is the section that keeps the DSP argument honest.** This
block spends roughly a sixth of the multipliers it used to because the owner's
demand is ~120,000 vertices per frame and the engine serves 166,666. That entire
argument lives in the issue interval, so the issue interval is a LAW here:
section 8 measures the accept-to-valid latency, measures the sustained interval
of a never-stalling stream, requires the two to agree, and then requires the
resulting vertices-per-frame to be on the correct side of the demand -- ABOVE
for `MUL_LANES` 3 and 6, and **BELOW for 1**, which is the point of keeping 1.
A scheduling change that quietly cost three clocks would otherwise leave every
arithmetic check green and every frame short, and nothing in this repository
would notice.

## Randomized differential tests

`tests/geometry/geom_skin_directed.cpp --random N`. 500 iterations in the fast
lane for each of the three builds, 8,000 nightly.

Matrix elements are drawn in a plausible pose range rather than across the full
s32 word. A bone matrix is a rotation and a translation, and sampling the whole
word would spend nearly every iteration pinned to the saturation rails instead of
exercising the arithmetic. The rails are covered by directed cases precisely
because the random lane is aimed away from them -- confirmed by mutation, where
the no-saturation mutation fails 6 directed checks and passes 900 random ones.

That property is why the full-range lane added in 2026-08-23 is section **7b**
of the directed test and **not** a widening of this one. Widening this lane
would have destroyed a known property of it to gain one the directed sections
can carry.

`w0` is drawn from 0..64 inclusive so both rails of the weight are reachable, but
the exact-equality boundaries are not left to chance; they are directed.

## Formal properties

None yet. This block has no formal harness.

The properties worth proving are stated here so the gap is a plan rather than a
silence: (1) the result equals the reference for all inputs with `w0 <= 64` --
likely out of reach for a 32x32 product without abstraction; (2) `o_valid_o`
never drops a vertex, i.e. every accepted vertex produces exactly one output
beat; (3) `vertices_transformed_o` equals the number of completed handshakes until it
saturates. Properties 2 and 3 are ordinary liveness and counting properties, and
are the tractable ones.

## Synthesis / resource ceiling

**Measured, then rearchitected, then measured again.** Quartus Prime Lite
17.0.2, device 5CSEBA6U23I7, constrained per-block fit (`clk` at 10.000 ns),
virtual pins, via `tools/quartus/run_block_fit.ps1`.

### Before (commit 16df9ee, `rtlCleanAtHead: true`)

| | measured |
| --- | ---: |
| ALMs | 1,801 |
| **DSP blocks** | **72** |
| registers | 145 |

**64% of a 112-DSP device for one stage**, second only to the Field IR engine's
79. The block header had predicted the problem and the block was fitted to
confirm it; `reports/REMAINING_BLOCKERS.md` then left it un-queued because the
rate question could not be answered without a vertex budget. It was the right
call on the evidence available and the wrong conclusion once the budget existed:
the demand needs 1.30 multipliers and the block had eighteen.

72 = 18 x 4. A signed 32x32 in that combinational cone cost four DSP blocks, and
the six 7-bit weight multiplies cost approximately none.

### After (measured 2026-08-23, both constrained)

| | before | `MUL_LANES=1` | `MUL_LANES=3` |
| --- | ---: | ---: | ---: |
| **DSP blocks** | **72** | **3** | **9** |
| ALMs | 1,801 | 1,530 | 2,187 |
| registers | 145 | 1,449 | 1,448 |
| Fmax | *never measured* | 56.11 MHz | 58.45 MHz |
| sourceCommit | 16df9ee | e7591e8 | 2e013e2 |

**9 DSP blocks against a 12-18 target: 8% of the device for the stage that was
64%.** Three registered signed 32x32 lanes at three blocks each.

**ALMs ROSE, and the campaign's standing claim does not extend here.**
`TASK_LOG.md` records that ALMs fell for `field_seq`, `terrain_lod` and
`geom_lod`. Those were parallel datapaths collapsing into one; GEOM.SKIN was
already minimal in logic and its area WAS the multipliers, so sequencing added
state instead (145 registers -> 1,448: a 24-word palette latch, six 65-bit
accumulators, the lane and destination pipelines). The trade is still
overwhelming -- 63 DSPs is 56% of the device's DSP budget against 386 ALMs at
0.9% of its ALM budget -- but it is a trade here and was not there.

Caveat on that ALM delta: the *before* row is one of the 47 measured before the
per-block SDC was fixed, so it was taken under no timing pressure. The
comparison is a constrained fit against an unconstrained one and overstates the
ALM cost by an unknown amount.

### The Fmax problem, DIAGNOSED and FIXED IN RTL (re-fit pending)

At 58.45 MHz a 10-cycle engine serves **97,417 vertices/frame against the
120,000 required**. Diagnosed rather than guessed at:

    from   br[1]              the blend-walk row counter
    to     o_y_o[14]~reg0     the output register for row 1
           10 logic levels
           Data Delay  17.639 ns  against a 10.000 ns period
           slack       -7.823 VIOLATED
           Cell 9.452 ns (54%) / interconnect 8.187 ns (46%)

All 200 worst setup paths end at `o_y_o[*]~reg0` -- one endpoint family. Neither
end is a virtual-pin node, and 54% cell delay is genuine logic depth, so this is
structural and not a placement or wrapper artifact.

`MUL_LANES = 1` was run as the discriminating experiment: it leaves this path
bit-identical and collapses the accumulator reduction from three 65-bit adds to
one. Fmax did not move (56.11), so the accumulator reduction is exonerated.

The chain is `br -> acc[] 6:1 mux -> 66-bit (pa - pb) -> six-term 73-bit
shift-add tree -> + (pb << 6) -> + 2^21 -> two 73-bit saturation compares ->
output register`, all in one clock.

**The acceptance test is Fmax / II, not Fmax.** 120,000 x 60 Hz = 7.2 M
vertices/s; II = 13 still clears it at 93.6 MHz, so a fix that adds pipeline
stages can still win. The combinational form was 58.45/10 = 5.845 M/s.

**The fix is implemented.** The blend is three stages:

- **B0** registers the accumulator pair, their difference, `w0`, `rigid` and
  the row tag. This is the stage that matters most: `br` selected the pair, so
  the 6:1 mux sat at the HEAD of the chain, and registering here takes the
  counter and the mux out of the long path entirely.
- **B1** registers the exact rounded numerator as ONE balanced 8-term tree --
  six shifted difference terms, the base term, and the rounding constant as the
  eighth. **Folding 2^21 into the tree is what keeps this a single rounding**:
  the sum is exact and the shift in B2 is the only rounding, so A3b holds
  exactly as stated above. Three adds deep instead of a seven-deep sequence.
- **B2** replaces the two 73-bit saturating magnitude comparisons -- two full
  carry chains at the very end of the longest path -- with a sign-extension
  test. The shift is constant per path, so "does it fit signed 32" is exactly
  "are the bits above the result all copies of its sign":
  `(&num[72:53]) | (~|num[72:53])` selects `num[53:22]`; rigid uses `[72:47]`
  and `[47:16]`. Identical answers by definition, not by approximation.

Differentials are green and **bit-identical** at all three `MUL_LANES` settings
after the change, with the same 3,976 oracle-checked / 1,778 beyond-the-
narrowing split as before.

**The re-fit has not been run at the time of writing**, so no Fmax is claimed
for the pipelined form. II is 12, which needs 86.4 MHz.

The architecture is a `MUL_LANES`-wide farm of signed 32x32 lanes, LOCAL to
this block, input- and output-registered so the DSP's own pipeline registers
are the ones inferred. Lanes are bound to TERMS (x, y, z), not to rows, which
removes the coordinate mux entirely and lets a whole row-product issue in one
cycle; the blend is one shared shift-add unit walked across the three rows, and
the walk overlaps the tail of the issue walk rather than following it.

Sharing is **within this subsystem only**. A console-global multiplier farm was
explicitly rejected; nothing outside GEOM.SKIN can reach these lanes.

### The levers that were NOT taken, and why

- **Sharing one bank across pose decode, skinning and projection.** This is the
  DSP audit's "wider opportunity" and it is still open, but it is a
  cross-block change and the ruling is smallest local farm per subsystem,
  sharing only what is mutually exclusive inside it. Skinning and projection
  are not mutually exclusive: they are consecutive stages of one pipeline.
- **Narrowing the 32x32 multiplier to the range a rotation actually occupies.**
  Rejected: see the Q formats section. The oracle accepts any s32.
- **`(* multstyle = "logic" *)` on the weight multiply.** It is silently ignored
  by this Quartus (`QUARTUS_GOTCHAS` §3). The multiply is written as an explicit
  shift-add instead.

## Integration capture cases

None yet. Integration capture requires GEOM.POSE to supply a real palette, and
GEOM.POSE has no RTL -- it is SPECIFIED with a reference only.

The first meaningful capture is a posed creature skinned through this block and
compared against the reference renderer's frame for the same clip and frame id.
That is a Phase 9 integration item, and it is blocked on GEOM.POSE rather than on
this block.

## Notes

Q formats per spec/qformats.md (A3c screen widths downstream).
