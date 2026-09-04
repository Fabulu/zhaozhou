# Contract — PART.SOFT (Soft particle endpoint)

> Ledger: `design/blocks.yml` · owner ZH-066 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Soft sprite/streak endpoint feeding the fragment pipeline's soft-particle blend path.

## Clock and reset semantics

Single clock `clk`, asynchronous active-low `rst_n`, `gpu` domain per the ledger.
Reset clears the output register, its valid and the counter; the two mode bits
reset to their constant values so the block is never briefly wrong about the
pass-7 law. The viewport registers are inputs, not state.

## Input and output packet layouts

**Viewport** — `vp_x0_i`, `vp_y0_i`, `vp_w_i`, `vp_h_i` (u12 each), canvas-local.

**In** — one PROJECTED particle per beat, ready/valid: `p_in_i` (GEOM.PROJECT's
behind-the-eye verdict), `p_x_i` / `p_y_i` (s21, S 12.8 canvas), `p_d_i` (s32,
Q16.16 1/w), `p_size_i` (u8, U 0.4.4 px), colour, `p_src_id_i`.

**Out** — one scissored whole-pixel span: `s_min_x_o` … `s_max_y_o` (s13,
inclusive), the depth word, the colour, `s_depth_test_o` / `s_depth_write_o`,
and the source id.

PART.EXPAND is the sibling: it emits a TRIANGLE for the setup stage from the
`tris` branch of the same reference function. This block emits a RECTANGLE for
the fragment stage from the `points` branch. Architect ruling 1.D keeps them
apart and they really are different laws.

## Backpressure rules

Ready/valid both sides, single-entry skid:
`p_ready_o = !s_valid_o || s_ready_i`.

A particle that produces no span — behind the eye, zero extent, or scissored to
nothing — is CONSUMED without raising an output beat.

## Memory ownership

**None.** No memory, no cache, no bus. A fixed expression over one input beat
plus four viewport registers supplied from outside.

> ## SUPERSEDED NUMERIC LAW — READ BEFORE THE TABLE BELOW
>
> **The `size` field described in this contract is the PRE-C2 law and is no
> longer what the record carries.** Amendment C2 (`QFMT_VERSION` 3, owner
> ruling R3) replaced `spec/qformats.md` §10 whole:
>
> | | pre-C2 (what this contract still describes) | C2 (binding) |
> |---|---|---|
> | width | u8 | **u6** |
> | format | U 0.4.4 | **U 2.4** |
> | meaning | sixteenths of a **screen pixel** | **relative world-radius multiplier** |
> | use | `size << 4` gives S 12.8 subpixels | `radius = base_radius_fx16 * size / 16`, **world scale, never camera-space pixels** |
>
> **`size << 4` is not a projection of the C2 field.** Turning a world radius
> into a screen half-side requires a projection, and qformats §10 says
> explicitly that inventing one to make the amendment fit is how a plausible
> wrong number gets shipped — **it needs a decision first.**
>
> `fpga/rtl/particles/zhao_part_expand.sv` already carries this banner. This
> contract did not, so an agent reading the contract met the dead law with an
> authoritative citation to the very section that replaced it. That is the
> failure this banner exists to stop.
>
> **Status: OPEN DECISION, tracked as G5's particle numeric migration.** Do not
> resolve it by editing this table.

## Q formats and rounding

    side_sub = size << 4                  (U 0.4.4 px -> S 12.8 subpixels)
    x0_sub   = sx - side_sub/2 ,  y0_sub = sy - side_sub/2
    min_x = max( (x0_sub + 255) >>> 8,      vp_x0 )
    max_x = min( (x0_sub + side_sub) >>> 8, vp_x0 + vp_w - 1 )
    min_y = max( (y0_sub + 255) >>> 8,      vp_y0 )
    max_y = min( (y0_sub + side_sub) >>> 8, vp_y0 + vp_h - 1 )

**THE TWO EDGES ROUND DIFFERENTLY, AND THAT IS THE WHOLE RULE.** The low edge
CEILS, the high edge FLOORS. That is pixel-centre coverage: a pixel is inside iff
its centre is inside the rectangle. Rounding both the same way is the obvious
tidy-up and makes every sprite a pixel too wide or too narrow **on one side
only** — an asymmetry that reads as a sprite drifting as it moves rather than as
an obvious break. Both mutations are caught (17 and 19 checks).

**A note on the shifts, recorded because it is easy to over-claim.** They are
written arithmetic because that is what the reference's `>>` on a signed int
does. At these widths a LOGICAL shift would give the same answer, because
narrowing the 22-bit result to 14 bits keeps bits [21:8] either way — a mutation
to `>>` survives the whole suite. That is an equivalent mutant, not a gap, and
the equivalence is an accident of the current widths rather than a property of
the law.

## Latency (fixed or variable)

Fixed: one cycle from accept to result. Two adds, two shifts and four compares,
all combinational, with the output register the only stage.

## Target throughput

One soft sprite per clock, which is the ledger's rate met literally.

Particles that cover nothing cost an input beat and no output beat, so a
population that is mostly off-screen retires faster than one that is mostly
visible.

## Overflow and malformed-input behaviour

**A ZERO EXTENT DRAWS NOTHING.** `blit_pattern_block` returns on `w_sub <= 0`
BEFORE any clamping, so `size == 0` produces no pixels rather than a one-pixel
dot. Checked, and the counter does not move.

**A FULLY SCISSORED SPRITE IS NOT AN EMPTY SPAN.** When `min > max` on either
axis the reference's loops simply do not execute. This block raises no beat: a
fragment span covering zero pixels is work for RASTER.FRAGMENT and a zero-pixel
primitive in every capture.

**THE VIEWPORT ORIGIN IS NOT ASSUMED TO BE ZERO.** `sprites.cpp` carries a fixed
defect note about exactly this — bounds that were viewport-RELATIVE while the
coordinates were canvas, so the Duo second view's markers were silently
invisible, latent until a viewport had a non-zero origin. Both high bounds are
`origin + extent - 1` and both are pinned by directed cases on an offset
viewport's right and bottom edges.

Widths cannot overflow: `sx` is inside GEOM.PROJECT's ±2048 px guard band and
`side_sub` is at most 4,080, so the subpixel sums fit s22 and the pixel results
fit s13 (±4096 px, wider than the guard band).

## Counters and traces

`soft_particles_o` (u32) counts sprites that COVER something, and saturates
rather than wrapping. A sprite behind the eye, of zero size, or scissored away
entirely was never a soft particle on this screen, and three directed cases pin
that the counter does not move for any of them.

`s_depth_test_o` / `s_depth_write_o` are constants carried on the packet. Charter
§8 pass 7: particles never occlude. The law belongs to the particle, and a
downstream stage that had to remember it would eventually forget.

## Scalar reference function

`zref::part::soft_rect` — `reference/include/zref/zref_particle_soft.hpp`.

The ledger declared `zref::SoftParticles`, which never existed. KIND 1 phantom:
the law was already shipped inline in `zref::render::draw_population`'s `points`
branch and the `blit_pattern_block` it calls.

**The reference RESTATES the geometry rather than forwarding**, because
`blit_pattern_block` computes the rectangle and immediately paints it. A restated
law is a second implementation, so the test checks it against the renderer
directly; see Directed tests.

## Directed tests

`tests/particles/part_soft_directed.cpp` — 214 checks, in **two layers**.

**Layer 1: the reference IS the renderer.** The same population is drawn twice —
once through `draw_population` with the points flag, once by painting
`soft_rect`'s rectangle with the same depth test — and the surfaces must be
identical, pixel for pixel and depth word for depth word.

**Layer 2: the RTL is the reference.** A subpixel sweep across a pixel boundary
(which is what exposes the ceil/floor asymmetry), zero extent, fully scissored on
all four sides, straddling all four edges, the Duo second view, an odd-origin
viewport, and behind the eye.

The odd-origin section grew after a mutation survived it: a version computing the
right edge as `(w - 1)` instead of `(x0 + w - 1)` passed every directed case,
because every sprite there sat left of the point where the two bounds differ. The
section now includes cases on the right and bottom edges of an offset viewport.

## Randomized differential tests

`tests/particles/part_soft_directed.cpp --random N`. 2,000 iterations in the fast
lane, 40,000 nightly; 8,442 checks clean at 3,000.

The generator samples positions RELATIVE to a random viewport and biases sizes
small. The first version spread by a fixed ±512 px regardless of viewport size
and biased nothing, and only about 7% of iterations covered anything at all — the
lane was mostly re-testing "empty". Sampling relative to the viewport is what
makes the random checks land on the law rather than on the early return.

## Formal properties

None yet. Two worth proving, both small: (1) `s_depth_write_o` is low in every
reachable state — the pass-7 law as an invariant rather than an assignment
anyone could later edit; (2) whenever a beat is raised, `min_x <= max_x` and
`min_y <= max_y`, so a consumer never receives an inverted span.

## Synthesis / resource ceiling

Not fitted; no number claimed.

By construction one of the cheapest blocks in the tree: no multiplier, no memory,
no divider. Two 22-bit adders, two constant shifts, four 14-bit compares and
about 110 flip-flops of output register.

## Integration capture cases

None yet. The natural first capture is a population drawn through
GEOM.PROJECT → PART.SOFT → RASTER.FRAGMENT and compared against
`draw_population` on the same particles — the hardware equivalent of the layer-1
check this suite already does in software.

Blocked on PART.LADDER (which decides a particle is a soft sprite at all) and on
RASTER.FRAGMENT consuming the span. The ledger also assigns this block the beam
ladder rung for projected widths of 2–6 px and the star/glint rung
(`spec/stars_and_flares.md` §6/§7, cap 2,744 glints per view); neither is
implemented here yet, and both are additional intake paths onto the same span
output rather than new geometry.

## Notes

Depth-fade math per spec/qformats.md.
