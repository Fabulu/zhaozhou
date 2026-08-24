# Contract — GEOM.PROJECT (Dual-view projection)

> Ledger: `design/blocks.yml` · owner ZH-055 · phase 8 · maturity SPECIFIED

## Purpose and exclusions

Camera 0/1 projection + lighting for cached or warped vertices; screen-space output per A3c widths (S 12.8, s64 edge setup).

## Clock and reset semantics

Single clock `clk`, asynchronous active-low `rst_n` (negedge), `gpu` domain per
the ledger. Reset clears both view register sets, every pipeline stage and the
counter. No clock-domain crossing lives in this block.

## Input and output packet layouts

**Configuration**, `cfg_we_i` / `cfg_view_i` / `cfg_addr_i` / `cfg_data_i`:

| addr | meaning |
| --- | --- |
| 0..15 | the view-projection matrix, row-major `m[0..15]`, fx16 |
| 16 | `{ y0[27:16], x0[11:0] }` |
| 17 | `{ h [27:16], w [11:0] }` |

Two complete sets, selected by `cfg_view_i`. Matrix words 8..11 (row 2) are
writable and inert -- see Q formats.

**Vertices in**, ready/valid, one per beat: `vx_i` / `vy_i` / `vz_i` (s32 fx16
world position), `view_i` (which register set), `src_id_i` (u16, opaque).

**View vertices out**, ready/valid: `out_x_o` / `out_y_o` (s21, canvas S 12.8,
already inside the guard band), `out_d_o` (s32 Q16.16 1/w), `out_behind_o`,
`out_src_id_o`.

The output packet is GEOM.CLIP's per-vertex packet. This block emits vertices,
not triangles: assembling them is the consumer's business, and the ledger's rate
line is per vertex.

## Backpressure rules

Ready/valid on both ports, and the pipeline is **RIGID**: one `advance` enable
drives every stage, so either the whole chain moves or none of it does.
`advance = !out_valid_o || out_ready_i`, and `v_ready_o` is that same signal.

Bubbles are not squeezed out. That is deliberate: a rigid pipeline is what makes
the fixed latency a fact rather than a hope, and it is why a stalled consumer
cannot reorder or drop a vertex. Section 6 of the directed test asserts that
three different stall patterns return byte-identical results.

## Memory ownership

**None.** No VRAM port, no cache, no bus master. The two register sets are the
block's entire state besides its pipeline.

The vertices arrive from GEOM.WCACHE or GEOM.WARP; whichever of them owns the
storage owns it, and this block never addresses memory.

## Q formats and rounding

The law is `zref::render::project_vertex`, step for step:

    clip   = mat4_vec4(vp, {x, y, z, 1.0})    qformats §2
    if (clip.w <= 0) -> behind the eye
    ndc    = fx_div_exact(clip, clip.w)       §3
    screen = fx_mad(ndc, half_extent, centre) §3
    px     = to_screen_xy(screen)             §8
    depth  = fx_div_exact(1.0, clip.w)        Q16.16 1/w (D7)

**ONE ROUNDING PER ROW.** §2 is explicit: four 32x32 products summed EXACTLY,
then a single `rescale(.,16)` with saturation. Rounding each product would be a
second rounding and A3b forbids it. The row accumulator is 68 bits, wider than
the widest sum any input word can produce (3·2^62 + 2^47 < 2^64), so it cannot
wrap for ANY input rather than merely for legal ones.

**`v.w` is the constant 1.0**, so matrix column 3 is a shift, not a multiply.

**`clip.z` is never read.** The depth lane is 1/w, not z, and `ProjOut` has no z
field, so row 2 is never computed: nine multipliers, not sixteen. Words 8..11
stay writable so the register map is a plain sixteen-word block, and they are
inert by construction.

**The division is EXACT and rounds half up**, including for negative numerators
where round-half-up is not round-away-from-zero. No reciprocal multiply
reproduces it bit for bit, which is why this block contains a real divider.

**`to_screen_xy` is `rescale(.,8)` then a CLAMP** to ±2048 px. The clamp is the
law, not an optimisation and not a clip.

Rounding is not a hypothetical here. An RTL variant that truncated the row
rescale instead of rounding half up passed all 382 checks of the first draft and
failed one random vertex in three hundred -- because every directed matrix was
built from whole multiples of 1.0 and had nothing below bit 16 to lose. Section
5b of the directed test exists to make that mutation fail 21 checks.

## Latency (fixed or variable)

Fixed, and long: 36 clocks from accept to result -- one input register, one row
sum, one rescale, one divider setup, 31 restoring steps, one quotient stage and
one viewport stage.

**UNCHANGED by the RUN-20260824-0522 core extraction, and measured rather than
reasoned** (`design/budgets/latency.md` 1 rule 1: a change that moves latency
must say so and by how much -- this one moved it by zero). All 36 stages are now
inside `zhao_project_core`, whose output register is this block's output
register. `caller_regression` compared this block against a verbatim pre-merge
copy of itself on every output port on every cycle: 0 mismatches.

Fixed is the point. The divider is the only expensive thing here, and pipelining
it rigidly buys one vertex per clock at the cost of latency nobody downstream is
waiting on.

## Target throughput

One projected vertex per clock, sustained, with no consumer stall.

That is the ledger's rate met literally rather than on average: the pipeline
accepts a vertex on every cycle `advance` is high, and `advance` is low only when
the consumer is not ready.

The rejected alternative was a single iterative divider -- about 200 flip-flops
and 31 cycles per vertex. It was rejected on the same measurement TERRAIN.PROJECT
records: at 31 cycles a vertex, a frame's worth of geometry does not fit in a
frame. The pipelined divider costs 3 lanes x 31 stages x 63 bits of register and
93 levels of 32-bit subtract, and it meets the rate.

## Overflow and malformed-input behaviour

Saturation everywhere, never wraparound: the row rescale saturates to the fx16
word, the quotients saturate exactly as `fx_div_exact` does, and `to_screen_xy`
clamps to the guard band.

**A behind-the-eye vertex (`clip.w <= 0`) carries ZERO and is not dropped.**
`project_vertex` returns a default-constructed `ProjOut`, whose screen vertex is
{0, 0, 0}; this block emits those zeros and raises `out_behind_o`. Dropping is
GEOM.CLIP's verdict (VERDICT_NEAR), and duplicating it here would make two
counters disagree about one primitive.

`w == 0` exactly is on the REJECT side. A block using `< 0` passes almost
everything and then hands the divider a zero divisor.

The divisor fed to the recurrence is forced to 1 on the behind-the-eye path. That
vertex never uses its quotients, but the `rem < D` invariant then holds on every
cycle rather than only on the cycles that matter.

## Counters and traces

`vertices_transformed_o` (u32) counts vertices **accepted**, not offered, and
saturates rather than wrapping. It is the shared catalog counter GEOM.LOOM and
GEOM.WARP also carry.

`triangles_submitted` is in the ledger's counter list for this block and is **not
implemented**: this block emits vertices and has no notion of a triangle. The
counter belongs to whichever block assembles them, and claiming it here would
produce a number that counts nothing.

No trace hookup yet. DEBUG.TRACE's `kVertexOutput` stage (stage 1) is where this
block's events belong when the two are wired together.

## Scalar reference function

`zref::render::project_vertex` -- declared in
`reference/src/zrender/internal.hpp`, implemented in
`reference/src/zrender/rast.cpp:43`.

**The ledger previously declared `zref::GeomProject`, which does not exist** --
one of the twenty-five phantom reference models audited in
`reports/PHANTOM_REFERENCES.md`. `project_vertex` is the real law: it is what the
software raster projects every vertex with, and it is what TERRAIN.PROJECT is
already verified against. Correcting the citation was the first step of building
this block, before any RTL was written.

## Directed tests

`tests/geometry/geom_project_directed.cpp` -- 621 checks, differential against
`zref::render::project_vertex` on the whole output packet (canvas x, canvas y,
the 1/w depth word, the behind flag and the source id).

Sections: the identity projection; the near plane at exactly `w == 0`; the
divider's sign and rounding behaviour; the guard-band clamp at both rails on both
axes; the dual view; the row rounding wall; backpressure; and the counter.

Two of those sections exist because a mutation survived without them:

- **the dual-view section originally used the Duo viewport pair, which differs
  only in `y0`.** A block that selected the viewport with a hardwired view index
  on the X lane produced identical x for both views and passed everything. The
  section now also uses a pair differing on both axes, with the same matrix, so
  only the viewport register set can make the answers differ.
- **section 5b, the row rounding wall**, is described under Q formats.

## Randomized differential tests

`tests/geometry/geom_project_directed.cpp --random N`. 100 iterations in the fast
lane, 2,000 nightly; 13,185 checks clean at 200.

Each iteration draws a fresh matrix and viewport and a burst of vertices, with
the w row varying with z so the near plane is genuinely reachable rather than
theoretical, and alternates between a free-running and a stalling consumer.

Honest about its reach: the random lane drives **view 0 only**, so every
dual-view law is pinned by directed cases. The mutation sweep confirms it -- both
view mutations pass the random lane and fail the directed one.

## Formal properties

None yet.

The properties worth proving, stated so the gap is a plan rather than a silence:
(1) the rigid pipeline never drops or reorders a vertex -- every accepted vertex
produces exactly one output beat, in order; (2) `out_x_o` and `out_y_o` are
always within ±2048 px, which is the assumption GEOM.CLIP's header states and
this block is the enforcer of; (3) the restoring recurrence's `rem < D` invariant
holds at every step for every input word. (2) and (3) are the tractable ones, and
(3) is the one that would make the divider trustworthy without exhaustive
simulation.

## Synthesis / resource ceiling

**Not yet characterised.** No fit has been run on this block alone; any number
here would be invented.

What is known from the design: nine 32x32 multipliers for the row sums (not
sixteen -- row 2 is never computed) plus two for the viewport MAD, and the
divider's 3 lanes x 31 stages x 63 bits of register with 93 levels of 32-bit
subtract. The registers, not the multipliers, are the bulk of this block.

TERRAIN.PROJECT carries the same divider at three times the width. If DSP or
register pressure forces a cut, the shared-core extraction under Notes is the
lever: one projector serving both paths costs one divider instead of two.

## Integration capture cases

None yet. The first meaningful capture is a posed, skinned creature projected
through this block and compared against the reference renderer's frame for the
same camera -- which needs GEOM.SKIN's palette (GEOM.POSE, still unfinished) and
GEOM.CLIP downstream.

`tests/terrain/terrain_project_chain.cpp` is the pattern to copy: it wires
TERRAIN.PROJECT to GEOM.CLIP with no adapter and runs the result through
GEOM.SETUP and the real rasterizer, so the packet compatibility is a compile-time
fact rather than a claim.


## Notes

**The duplication is gone from the source. It is NOT gone from the silicon, and
the difference is the whole content of this section.**

This block used to contain a complete copy of `project_vertex` -- the same
localparams, the same eight helper functions, the same configuration register
file, the same row sums, the same 31-stage divider and the same viewport
`fx_mad` that TERRAIN.PROJECT contained. Its header called that "a cost, not a
feature". The follow-up this section used to record was carried out in
RUN-20260824-0522: the law now lives once, in
`fpga/rtl/common/zhao_project_core.sv`, and this block is the vertex-level
interface around it -- a ready/valid handshake, the accepted-vertex counter, and
nothing else.

**It was merged on a differential, not on a resemblance.** The budget audit had
reported the two blocks' arithmetic SIGNATURES byte-identical, which is a claim
about shape. `pair_equivalence` drove both pre-merge blocks and the shipped
oracle from one stimulus stream and compared 16,416 projected vertices three
ways with zero mismatches, against ten positive controls it had to catch and
did.

**Latency did not move, and the seam is why.** The core's output register IS
this block's output register, so this block adds no stage and the 36 clocks
below are unchanged. TERRAIN.PROJECT needs the core to end one stage before ITS
output; this block needs it to end exactly AT its output; those two requirements
pick the boundary between them. Evidence: `caller_regression` runs each shell
beside a verbatim copy of its own pre-merge self and compares every output port
on every cycle -- 1,080,000 port-cycles, 0 mismatches, 7 of 7 timing controls
caught.

The pipeline enable is the CALLER'S (`en_i`) rather than derived inside the
core, because the two callers back-pressure from different places: this block
from the core's own output register, TERRAIN from its triangle register one stage
further on. A core that decided for itself would have had to pick one and
silently change the other.

**WHAT THIS DID NOT BUY: any DSPs.** This block and TERRAIN.PROJECT each
instantiate their OWN core, so the pair still holds two sets of multipliers --
33 + 33, map-measured before and after, identical to the unit. The sentence this
section used to carry, "have both instantiate it ... that halves the divider
cost", asserted both halves of a contradiction: **a module two blocks
instantiate is not a module they share.** Halving requires ONE arbitrated
instance serving both callers, which is an architecture change and not a
refactor -- each caller would then be stallable by the other, and the aggregate
rate would halve. It is costed on the 2026-08-24 docket entry, together with the
reason it should wait for the projected-vertex cache.

**Follow-up, in order:** the projected-vertex cache (`GEOM.WCACHE`), then one
shared core instance, then the 27-bit width narrowing. They compose, only the
first two are order-dependent, and all three now land in one file instead of
two.
