# Contract — GEOM.SKIN (Skinning stage)

> Ledger: `design/blocks.yml` · owner ZH-054 · phase 9 · maturity SPECIFIED

## Purpose and exclusions

Rigid and two-weight skinning of decoded vertices into world space.

## Clock and reset semantics

Single clock `clk`. Asynchronous active-low reset `rst_n`, released synchronously
by the shell. Reset clears the output register, its valid, and the
`vertices_transformed` counter; it does not touch the matrix inputs, which are
combinational and owned upstream.

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

`v_ready_o = !o_valid_o || o_ready_i` -- a single-entry skid. A vertex is
accepted only when `v_valid_i && v_ready_o`; on any other cycle the held result
and the counter are untouched.

`o_valid_o` clears when the result is taken and sets when a vertex is accepted;
both in the same cycle is a legal back-to-back beat, so a never-stalling consumer
sees one vertex per clock.

Pinned by `tests/geometry/geom_skin_directed.cpp` section 6, which asserts that a
stalled vertex is neither counted nor allowed to overwrite the held result.

## Memory ownership

**None.** This block owns no memory, holds no cache, and never addresses the bus.
The bone palette belongs to GEOM.POSE and is presented combinationally with the
vertex.

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

Widths, stated rather than assumed: a product is s64; three of them plus the s48
translation needs s67; times a 7-bit weight gives s74; the sum of two is s75; the
round-half-up add cannot overflow that; `>>> 22` leaves s53, which saturates to
s32.

Double rounding is not a hypothetical risk. An RTL variant that rounds `pa` and
`pb` separately by 16 and blends by 6 passed an earlier draft of the directed
test **in full**, and failed only one random iteration in 300, by a single LSB.
The single-rounding wall section of the test exists to make that mutation fail
696 checks instead of one.

## Latency (fixed or variable)

Fixed: one cycle from accept to result. The arithmetic is combinational and the
output register is the only stage.

This is a deliberately unpipelined first implementation. The 32x32 products are
the long path and will need retiming before this block meets the shell clock; see
the resource section, where that is an open item and not a solved one.

## Target throughput

One vertex per clock when the consumer never stalls.

The frame-rate arithmetic that would turn this into a creature-count budget
depends on the meshlet vertex counts and the LOD schedule, neither of which is
settled. Stating a vertices-per-frame target before those are fixed would be
inventing a number, so the target here is the per-clock rate the handshake
actually delivers.

## Overflow and malformed-input behaviour

Saturation, never wraparound: a result beyond s32 clamps to `0x7FFFFFFF` or
`0x80000000`. Pinned by the saturation-rail cases in the directed test.

`w0 > 64` is outside the contract. The RTL's `w1 = 64 - w0` would underflow its
7-bit width, so the caller must not present it; `v_w0_i` is 7 bits because 64
needs seven, not because 65..127 are meaningful. This is an upstream obligation
and is **not** currently checked in hardware -- recorded here as a known hole
rather than as a defended edge.

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

## Scalar reference function

`zref::creature::skin_vertex` -- declared in
`reference/include/zref/zref_creature.hpp`, implemented in
`reference/src/zcreature/creature_core.cpp`.

Not a model written beside the RTL: it is the function the reference renderer
skins every creature with, so "RTL matches the oracle" means "the hardware moves
vertices exactly where the shipped pictures put them".

## Directed tests

`tests/geometry/geom_skin_directed.cpp` -- 2,125 checks, differential against
`zref::creature::skin_vertex` on every arithmetic case.

Sections: rigid identity and translation; the branch boundaries (`w0` = 0, 1, 32,
63, 64, and `w0 == 64` with `b1 != b0`); the single-rounding wall; the rounding
boundary at exact halves, positive and negative; the saturation rails; and the
interface laws.

The **single-rounding wall** is the section this file exists for, and it was not
in the first draft. It sweeps every `w0` in 1..63 against eleven vertex residues
using 1-ULP and 3-ULP matrices, so `pa` carries a fraction below bit 16 and the
two rounding orders can disagree. Without it, the double-rounding mutation passed
all 39 original checks.

The **interface laws** section was added for the same reason: a mutation zeroing
the `src_id` passthrough survived all 2,118 arithmetic checks, because every one
of them compares coordinates and nothing else.

## Randomized differential tests

`tests/geometry/geom_skin_directed.cpp --random N`. 500 iterations in the fast
lane, 8,000 nightly; 24,000 checks clean at 8,000.

Matrix elements are drawn in a plausible pose range rather than across the full
s32 word. A bone matrix is a rotation and a translation, and sampling the whole
word would spend nearly every iteration pinned to the saturation rails instead of
exercising the arithmetic. The rails are covered by directed cases precisely
because the random lane is aimed away from them -- confirmed by mutation, where
the no-saturation mutation fails 6 directed checks and passes 900 random ones.

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

**Not yet characterised, and expected to be a problem.**

The blend path issues eighteen 32x32 products -- two matrices x three rows x three
terms -- plus two 75-bit weight multiplies. That is the largest multiplier count
of any block written so far, against a device with 112 DSPs of which the project
already accounts for 171.

This block must therefore be read as evidence for the DSP argument in
`design/budgets/dsp.md`, not as a finished implementation. The obvious levers,
none of them yet taken: share one row engine across three rows and three cycles
(3x fewer products, 3x the latency); share one matrix engine across A and B (2x
fewer, 2x the latency); or exploit that a bone matrix's 3x3 is a rotation, whose
elements are bounded well inside s32 and may not need full-width multipliers.

No fit has been run for this block. Any resource number stated here would be
invented.

## Integration capture cases

None yet. Integration capture requires GEOM.POSE to supply a real palette, and
GEOM.POSE has no RTL -- it is SPECIFIED with a reference only.

The first meaningful capture is a posed creature skinned through this block and
compared against the reference renderer's frame for the same clip and frame id.
That is a Phase 9 integration item, and it is blocked on GEOM.POSE rather than on
this block.

## Notes

Q formats per spec/qformats.md (A3c screen widths downstream).
