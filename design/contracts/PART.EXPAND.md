# Contract — PART.EXPAND (Polygon-particle expansion)

> Ledger: `design/blocks.yml` · owner ZH-065 · phase 10 · maturity SPECIFIED

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
> **`size << 4` is not a projection of the C2 field.** That much stands: a
> world radius has to be projected and `size << 4` does not project anything.
>
> **But the projection is NOT the missing decision** *(corrected 2026-09-06,
> audit R7)*. `zref::render::draw_form_marker` already implements it, and
> `tests/render/render_directed.cpp` exercises the world-space branch with
> `flags = 0`:
>
> ```
>     half_sub = rescale_s32(fx_mul(size_fx16, c.s.d), 8)   // size * (1/w)
> ```
>
> with the inversion trap recorded beside it -- dividing by `c.s.d` computes
> `size * w` and makes things GROW with distance, which only an ortho matrix
> hides. A particle expansion using this would be CALLING a ratified law.
>
> **What is missing is `base_radius_fx16`**, the per-species base radius that
> `particle_radius()` multiplies. `species` is a u7 and no species table exists
> in `reference/`, `spec/` or `design/`. THAT is the owner's decision: a radius
> per species is content, and guessing one is the actual instance of the
> failure the warning below describes.
>
> `fpga/rtl/particles/zhao_part_expand.sv` already carries this banner. This
> contract did not, so an agent reading the contract met the dead law with an
> authoritative citation to the very section that replaced it. That is the
> failure this banner exists to stop.
>
> **Status: OPEN DECISION, tracked as G5's particle numeric migration.** Do not
> resolve it by editing this table.

## Purpose and exclusions

Expand polygon-particle instances into normal geometry packets for the setup stage.

## Clock and reset semantics

Single clock `clk`, asynchronous active-low `rst_n`, `gpu` domain per the ledger.
Reset clears the output register, its valid, and the counter. The two mode bits
reset to their constant values (depth test on, depth write off) so the block is
never briefly wrong about the pass-7 law.

## Input and output packet layouts

**In** — one PROJECTED particle per beat, ready/valid:

| signal | width | meaning |
| --- | --- | --- |
| `p_in_i` | 1 | GEOM.PROJECT's verdict; 0 = behind the eye |
| `p_x_i`, `p_y_i` | s21 | S 12.8 canvas position |
| `p_d_i` | s32 | Q16.16 1/w |
| `p_size_i` | u8 | U 0.4.4 pixels (qformats §10) |
| `p_r_i`, `p_g_i`, `p_b_i` | u8 | colour |
| `p_src_id_i` | u16 | opaque tag |

**Out** — one geometry packet, ready/valid: three vertices (`t_ax_o` … `t_cy_o`,
s22), the shared depth `t_d_o`, the colour, `t_depth_test_o` / `t_depth_write_o`,
and the source id.

The block never sees a matrix: projection is GEOM.PROJECT's and is done before
the particle arrives here.

## Backpressure rules

Ready/valid both sides, single-entry skid:
`p_ready_o = !t_valid_o || t_ready_i`.

**A behind-the-eye particle is CONSUMED and produces nothing.** It must not
occupy an output beat, so `emits = take && p_in_i` — the input handshake
completes, the output does not.

## Memory ownership

**None.** No memory, no cache, no bus. The block is a fixed expression over one
input beat.

## Q formats and rounding

    side_sub = size << 4
    a = { x,                   y - side_sub   }
    b = { x - (side_sub*3)/4,  y + side_sub/2 }
    c = { x + (side_sub*3)/4,  y + side_sub/2 }

**`size << 4`, not `<< 8`.** Particle size is U 0.4.4 pixels (qformats §10) —
sixteenths of a pixel — so a shift of 4 lands it in S 12.8 subpixels. A shift of
8 would treat the byte as whole pixels and make every particle sixteen times too
large. Caught by mutation at 245 failing checks.

**Both divisions are EXACT for every legal input**, which is not obvious and is
worth recording because it looks like a rounding hazard and is not. `side_sub` is
`size << 4`, hence always a multiple of 16; `side_sub*3` is a multiple of 48. Both
4 and 2 divide both. Nothing is ever discarded.

That was established by mutation, not inspection: a variant that ROUNDED the
half-width instead of truncating passed all 565 directed checks and the random
lane, because no input distinguishes them. It is an **equivalent mutant, not a
gap in the test**, and an earlier draft of this contract and both source headers
claimed the opposite. They were wrong and are corrected.

## Latency (fixed or variable)

Fixed: one cycle from accept to result. The arithmetic is combinational — three
adds and two shifts — and the output register is the only stage.

## Target throughput

One expanded particle per clock, which is the ledger's rate met literally: the
block accepts a particle on every cycle the consumer is ready.

Behind-the-eye particles cost an input beat and no output beat, so a population
that is mostly off-screen retires FASTER than one that is mostly visible.

## Overflow and malformed-input behaviour

No arithmetic here can overflow its output: screen coordinates arrive inside the
±2048 px guard band GEOM.PROJECT clamps to (21 bits), and `side_sub` is at most
4,080 (twelve bits), so a vertex fits s22 with room. The output is widened to 22
bits rather than silently wrapping a 21-bit port.

**The expanded fan is deliberately NOT re-clamped to the guard band.** A large
particle near the edge legitimately puts a vertex outside ±2048 px, and the
software does exactly the same — it lets the rasteriser's scan box scissor the
triangle. Clamping here would DEFORM the triangle instead of cropping it, which
moves the particle rather than trimming it.

A `size` of zero collapses the fan to a point. That is not an error and is not
special-cased: it is what the arithmetic gives, and it is what the software gives.

## Counters and traces

`polygon_particles_o` (u32) counts particles **expanded**, not offered, and
saturates rather than wrapping. A behind-the-eye particle was never a polygon
particle and is not counted — pinned by a directed case.

`t_depth_test_o` / `t_depth_write_o` are constants carried on the packet rather
than left implicit. The pass-7 law ("test only, no write") is a property of a
particle, and a downstream stage that had to remember it would eventually forget.

## Scalar reference function

`zref::part::expand_polygon` — `reference/include/zref/zref_particle.hpp`.

The ledger declared `zref::ParticleExpand`, which never existed. This is a KIND 1
phantom: the law was already shipped, inline, in `zref::render::draw_population`'s
`tris` branch (`reference/src/zrender/sprites.cpp`).

**The reference had to RESTATE it rather than forward to it**, because
`draw_population` computes the fan inside a loop body and rasterises it
immediately — there is no callable to forward to. A restated law is a second
implementation, so the test checks the reference against the renderer directly;
see Directed tests.

## Directed tests

`tests/particles/part_expand_directed.cpp` — 565 checks, in **two layers**.

**Layer 1: the reference IS the renderer.** The same population is rendered
twice — once through `draw_population` with the triangle flag, once by feeding
`expand_polygon`'s vertices to the same `raster_tri` with the same mode — and the
two surfaces must be identical, pixel for pixel and depth word for depth word.
If `zref_particle.hpp` ever drifts from `sprites.cpp`, this fails, and it fails
whichever of the two moved.

Layer 1 is what makes layer 2 worth anything. Without it, a restated law verified
only against itself is exactly the failure the phantom-reference rules exist to
catch.

**Layer 2: the RTL is the reference.** The size scale (U 0.4.4), the fan's
deliberate non-equilateral shape, a size sweep across the range, behind-the-eye
producing nothing, the guard-band edges being left un-clamped, back-to-back
throughput, and the counter.

## Randomized differential tests

`tests/particles/part_expand_directed.cpp --random N`. 2,000 iterations in the
fast lane, 40,000 nightly; 48,700 checks clean at 5,000.

Positions are drawn across the whole ±2048 px guard band, both rails, so the
22-bit output width is exercised rather than assumed. Sizes span the full byte,
and one particle in eight is behind the eye so the skip path stays live in the
random lane rather than being directed-only.

## Formal properties

None yet.

Two are worth proving and both are small: (1) every accepted particle with
`p_in_i` high produces exactly one output beat and one with it low produces none;
(2) `t_depth_write_o` is low in every reachable state — the pass-7 law as an
invariant rather than an assignment anyone could later edit.

## Synthesis / resource ceiling

Not fitted; no number claimed.

By construction this is one of the cheapest blocks in the tree: no multiplier
(the ×3 is a shift and a subtract), no memory, no divider. Three 22-bit adders,
two shifts, and about 130 flip-flops of output register.

## Integration capture cases

None yet. The natural first capture is a population drawn through the real
GEOM.PROJECT → PART.EXPAND → GEOM.SETUP → RASTER path, compared against
`draw_population` on the same particles — the hardware equivalent of the layer-1
check this block's suite already does in software.

It is blocked on PART.LADDER (which chooses that a particle is a polygon particle
at all) and on a populated particle pool, neither of which exists.

## Notes

Kept separate from PART.SOFT by architect ruling (1.D).
