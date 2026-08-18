# Contract — TERRAIN.PROJECT (Terrain dual-view projection)

> Ledger: `design/blocks.yml` · owner ZH-051 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

Project the shared terrain cache into both camera views (Duo); select Mosaic
texture candidates.

Implemented as `fpga/rtl/terrain/zhao_terrain_project.sv`. This block is
`reference/src/zrender/rast.cpp::project_vertex` in hardware, applied to the
three vertices of a terrain primitive: the §2 matrix transform, the `clip.w <= 0`
near-plane verdict, the two §3 exact divisions into NDC, the §3 `fx_mad` into
canvas fx16, the §8 `to_screen_xy` conversion and guard-band clamp, and the D7
Q16.16 `1/w` depth.

**It is the door between the Mantle and the rasterizer**: its output packet IS
`zhao_geom_clip`'s input packet, and `tests/terrain/terrain_project_chain.cpp`
wires the two together with no adapter, through `GEOM.SETUP` into the real
rasterizer.

Excluded: no clipping and no culling (GEOM.CLIP owns the near-plane verdict's
*consequence*, the zero-area reject, the winding flip and the scissor), no edge
coefficients (GEOM.SETUP), no lighting or shading of any kind, no texture
sampling, no Mosaic *selection* (see Notes law C), no vertex cache and no index
buffer (see Notes law A), and **no memory port of any kind**.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset clears the configuration registers (both matrices and both viewports), the
vertex sequencer, every pipeline stage, the reassembly slots, the output register
and `terrain_triangles_emitted_o`. No clock-domain crossing lives in this block.

**Configuration is a register file, not a port.** Sixteen matrix words and two
viewport words per view are written one at a time through
`cfg_we_i` / `cfg_view_i` / `cfg_addr_i` / `cfg_data_i`, the way CMD.DECODER
writes a register, rather than appearing as a kilobit-wide boundary. Address map:

| `cfg_addr_i` | meaning |
|---|---|
| 0..15 | matrix element, row-major, `row*4 + col`, fx16 |
| 16 | viewport origin — `y0 = data[27:16]`, `x0 = data[11:0]` |
| 17 | viewport extent — `h = data[27:16]`, `w = data[11:0]` |
| 18..31 | ignored |

Configuration is sampled by a packet as it enters stage 1 (the matrix) and at
stage 6 (the viewport). Rewriting either while packets are in flight is
therefore not frame-coherent; the caller writes both views' registers before the
frame's primitives, which is the ordinary command-stream discipline.

**Matrix row 2 is stored and never read.** `project_vertex` never touches
`clip.z` — the depth lane is `1/w` (D7), and `ProjOut` has no z field — so this
block computes rows 0, 1 and 3 and nine products, not sixteen. Words 8..11
remain writable so the address map stays a plain `row*4 + col`; they are inert.

## Input and output packet layouts

`terrain_primitives` in, ready/valid — **exactly TERRAIN.NORMALS' input packet**,
plus the view select and the layer-E Mosaic candidates:

| field | width | meaning |
|---|---|---|
| `ax_i` `ay_i` `az_i` | signed 32 | vertex A, fx16 world units |
| `bx_i` `by_i` `bz_i` | signed 32 | vertex B |
| `cx_i` `cy_i` `cz_i` | signed 32 | vertex C |
| `src_id_i` | 16 | source id, rides the packet |
| `view_i` | 1 | which camera's registers to use |
| `mat_a_i` `mat_b_i` `weight_i` | 8 each | layer E candidates (§6.2), forwarded |

`terrain_primitives` out, ready/valid — **exactly `zhao_geom_clip`'s input
packet** (`tri_ax_i`…`tri_cy_i`, `tri_behind_i`, `tri_src_id_i`), plus the depth
lane, the view tag and the candidates:

| field | width | meaning |
|---|---|---|
| `out_ax_o` `out_ay_o` … `out_cy_o` | signed 21 | S 12.8 screen subpixels (§8) |
| `out_behind_o` | 3 | bit *k* = vertex *k* failed `clip.w > 0` |
| `out_src_id_o` | 16 | the id of the primitive |
| `out_ad_o` `out_bd_o` `out_cd_o` | signed 32 | Q16.16 `1/w` per vertex (D7) |
| `out_view_o` | 1 | which camera produced it |
| `out_mat_a_o` `out_mat_b_o` `out_weight_o` | 8 each | `mosaic_candidates` |

Plus `terrain_triangles_emitted_o` (32) and `idle_o`.

`mosaic_candidates` is **not a second stream**: it is three fields on the
primitive packet, valid with `out_valid_o`. See Notes law C.

## Backpressure rules

Ready/valid on both ports. The pipeline is **rigid**: a single `advance` gates
every stage, and `advance` is `!out_valid || out_ready_i`. A stalled consumer
therefore freezes the whole chain — including all 31 divider stages — and
nothing is dropped, reordered or corrupted while it waits. Bubbles are not
squeezed out; that is the price of the fixed latency being a fact rather than a
hope.

`tri_ready_o` is `advance && (no triangle latched || the latched triangle is on
its last vertex)`, so triangles arrive back to back with no bubble between them.

Four stall schedules ride the directed suite (none, alternate, 31-in-32, and a
burst), and both random lanes stall on a pseudo-random schedule per batch.

## Memory ownership

None. No port onto VRAM, no cache, no M10K. The block's entire state is the
configuration registers, the pipeline and the three reassembly slots.

## Q formats and rounding

Every step is a citation, and the order matters more than any single step.

1. **`clip = mat4_vec4(vp, {x, y, z, 1})`** — §2: the four products of a row are
   summed **exactly**, then there is **ONE** `rescale(., 16)` and one saturate.
   Rounding a product would be a second rounding and A3b forbids it.
   `v.w` is the constant `1 << 16`, so column 3 is `m[i][3] << 16` and not a
   multiply.
2. **`in = clip.w > 0`** — the Phase-3 whole-primitive near plane.
3. **`ndc = fx_div_exact(clip, w)`** — §3/§4: `floor((a << 16 + w/2) / w)` with
   floor semantics, saturating to the fx16 word.
4. **`screen = fx_mad(ndc, half_extent, centre)`** — §3: `a·b + (c << 16)` formed
   exactly, then ONE `rescale(., 16)`. `half_extent` is `w · 2^15` and `centre` is
   `(x0 + w/2) << 16`, so the addend inside the MAD is `(x0 + w/2) << 32`.
5. **`px = to_screen_xy(screen)`** — §8: `rescale(., 8)` then **clamp** to
   ±2048 px = ±524,288 subpixels. A clamp, not a clip; see Notes law 5.
6. **`d = fx_div_exact(1, w)`** — the D7 Q16.16 `1/w` depth lane.

**Widths, stated rather than assumed.**

- A row sum is three s32·s32 products (each ≤ 2^62 in magnitude) plus
  `m[i][3] << 16` (≤ 2^47), so |sum| < 3·2^62 + 2^47 < 2^64. The accumulator is
  **68 bits**, four bits clear of that, so it cannot wrap for ANY input word —
  a stricter bar than "for legal inputs".
- The divider's numerator is `N = |a| << 16 ≤ 2^47`, plus `D/2 < 2^30`, so
  `h < 2^48`. The divisor is `clip.w ∈ [1, 2^31−1]`, 31 bits.
- The `fx_mad` product is < 2^58 and its addend < 2^45; the accumulator is 64
  bits.
- `to_screen_xy`'s `rescale(., 8)` on an s32 lands inside 24 bits, so the fx16
  saturating narrow inside it **cannot fire**; only the guard clamp can. That is
  why the RTL implements the clamp and not the narrow, and it is stated here
  rather than left as an inference.

**The quotient needs 31 bits, not 48, and that is the block's one clever step.**
The result saturates to the fx16 word, so the only interesting case is
`q < 2^31`, and

```
q = floor(h/D) ≥ 2^31   ⟺   floor(h / 2^31) ≥ D   ⟺   h[47:31] ≥ D
```

is a 31-bit compare taken *before* any division. When it fires the answer is a
rail. When it does not fire, `floor(h/2^31) < D` — exactly the invariant a
restoring recurrence needs in order to start at bit 30 with remainder `h[47:31]`.
One compare, then 31 restoring steps, never 48.

**The negative branch is one division, not two.** With a positive divisor,
`fx_div_exact` is `floor((N + D/2)/D)` for a non-negative numerator and
`−ceil((N − D/2)/D)` for a negative one (and 0 when `N ≤ D/2`), and `ceil` is
`floor + (remainder ≠ 0)`, which the recurrence produces for free.

**A negative result of magnitude exactly 2^31 is `INT32_MIN` and is NOT a
saturation**; a saturating negative is also `INT32_MIN`. Both land on the same
word, which is why the rail test may be taken before the division without losing
the exact case. `terrain_project_directed` §5 pins both, one apart
(`|clip| = 32768` exact, `32769` railing, at `w = 1` raw).

**Verilog signedness.** Every comparison inside the divider is unsigned by
construction (magnitudes), and every comparison outside it is between two signed
operands. A Verilog comparison goes unsigned if EITHER operand is; that trap
cost a real bug in `GEOM.BINNER` and made 29 tiles vanish (see
`design/contracts/GEOM.CLIP.md`).

## Latency (fixed or variable)

**Fixed, 38 cycles at full readiness**, per vertex: 1 sequencer, 1 row sums, 1
rescale, 1 divider setup, 31 restoring steps, 1 quotient assembly, 1 `fx_mad`,
1 `to_screen_xy`, 1 reassembly. The triangle's own latency is that plus the two
clocks its other vertices take.

**MEASURED, not counted off the source.** `terrain_project_directed` §11 pushes
128 triangles back to back and reports **422 cycles**. The steady state is 3
cycles per triangle (384), so the fill is 422 − 384 = **38**, which is the stage
count above. A latency claim that only adds up in prose is not a latency claim.

The ledger says `variable`, which a fixed latency satisfies; this contract
records the stronger fact. Under backpressure the *elapsed* time grows because
the pipeline freezes, but no packet ever overtakes another.

## Target throughput

The ledger asks for **1 projected vertex per clock**, and this block sustains
exactly that: three vertices enter on three consecutive clocks and one triangle
leaves every three clocks when the consumer is ready. `terrain_project_directed`
§11 measures it and prints the number rather than asserting it in prose.

**What that rate costs, stated because it is a real cost.** This block projects
a TRIANGLE, so a lattice vertex shared by six triangles is projected six times.
A 33×33 patch is 1,089 vertices but 2,048 triangles = 6,144 projected vertices,
so a full patch costs 6,144 clocks rather than 1,089. At 100 MHz / 60 Hz
(1.67 M clocks) that is about 270 patches per frame of pure projection, which is
inside the 256-visible-patch budget of `spec/terrain_rules.md` §4.2 but not
comfortably. **The fix is a post-transform vertex cache, and it is not this
block's to build**: the ledger's `GEOM.WCACHE` is exactly that, and it is phase
8. Recorded here so the number is on the table when it is scheduled.

**The rejected alternative for the divider was measured, not preferred.** A
single iterative divider is about 200 flip-flops and 31 cycles per vertex. Even
64 visible patches at 31 cycles per vertex is 2.2 M clocks against a 1.67 M-clock
frame: an iterative divider cannot draw the terrain at all. The pipelined form
costs 3 lanes × 31 stages × 63 bits of register and 93 levels of 32-bit subtract.

## Overflow and malformed-input behaviour

**Input domain: the whole fx16 word on every coordinate and every matrix
element.** This block has no assumed range, because the row accumulator is wide
enough for the whole word (see Widths above) and the divider's pre-rail compare
covers every quotient the word can produce. That is deliberate: the reference's
own `mat4_vec4` uses `__int128` and does not wrap either, so agreeing with it
everywhere is achievable and is therefore required.

- **`clip.w <= 0` is a rejection, not a clamp.** The vertex's bit of
  `out_behind_o` rises and it carries `{0, 0, 0}` — the same zeros
  `project_vertex` returns from a default-constructed `ProjOut`. The triangle is
  **not** dropped here: dropping is GEOM.CLIP's `VERDICT_NEAR`, and duplicating
  it would make two counters disagree about one triangle.
- **A behind-the-eye vertex still runs the divider**, with the divisor forced to
  1 so the recurrence's `rem < D` invariant holds on every cycle rather than only
  on the cycles whose results are used. Its quotients are discarded at stage 6.
- **Row-sum saturation clamps and never wraps** (§2). Reached by the randomized
  lane B and by `terrain_project_directed` §8.
- **Divider saturation clamps to the fx16 rails.** Reached by lane B and pinned
  by directed §5 at both rails.
- **The guard band clamps.** A screen coordinate beyond ±2048 px becomes exactly
  ±524,288, which is what makes GEOM.CLIP's 21-bit port safe — its header states
  the assumption and names this block as the enforcer.
- **Viewport `w` or `h` of 0** produces `hw = 0`, so every vertex lands on the
  viewport origin. That is `project_vertex`'s own behaviour and is reproduced,
  not special-cased. Lane B samples it.

## Counters and traces

`terrain_triangles_emitted_o` counts triangles that left the output port, not
vertices, not offered packets and not cycles. Pinned by the directed suite's
counter case under all four stall schedules. No counter-catalog id is bound:
`terrain_triangles_emitted` has several claimants (TERRAIN.TESS also raises it)
and minting an id is a `spec/counters.md` amendment, not an RTL decision.

`idle_o` is true only when nothing is latched, no stage holds a valid packet —
including all 31 divider stages — and the output register is empty.

## Scalar reference function

`zref::render::project_vertex`, declared in `reference/src/zrender/internal.hpp`
and defined in `reference/src/zrender/rast.cpp`.

**No oracle was written for this block.** That is the strongest available
position and it is deliberate: the function this block reproduces already exists,
already ships, and is already pinned by every golden capture in the tree, so
"RTL matches oracle" means "RTL matches the pictures the project draws". The
tests include `zrender/internal.hpp` white-box (as `terrain_patch_directed`
already does) and call it.

The ledger's `reference_model` was **AMENDED** from `zref::TerrainProject` to
that symbol — a deviation from "honour the ledger entry", recorded as one, and
the same move `TERRAIN.TESS` and `TERRAIN.NORMALS` made. Inventing a
`zref::TerrainProject` would have meant writing a second implementation of
`project_vertex`, which charter §29-6 exists to forbid.

## Directed tests

`tests/terrain/terrain_project_directed.cpp` — eleven sections:

1. **The orthographic fixture, with values computed by hand.** World origin lands
   on the canvas centre column (32,768 = 128.0 px) and `ndc = +1` lands on the
   viewport's right edge (65,536 = 256.0 px), so a wrong half-extent or a wrong
   centre moves both. Plus an offset viewport.
2. **Perspective at six depths**, including the assertion that `1/w` *decreases*
   with depth — the D7 lane's whole point, checked as an order and not only as a
   value.
3. **The near plane**, eight cases: `w == 0` rejecting each vertex in turn (the
   test is `<= 0`, not `< 0`), `w < 0`, two behind, all three behind, and
   `w == 1` raw accepted. Every rejected vertex is checked to carry zeros.
4. **The exact division at the round-half-up boundary**, both directions:
   `+0.5 → 1` and `−0.5 → 0`, `+1.5 → 2` and `−1.5 → −1`. Round-half-up is not
   round-away-from-zero, and a divider that negates-then-rounds is off by one on
   the negative side only. Each hand-derived quotient is first confirmed against
   `zref::fx_div_exact` so the case is anchored to §3.
5. **The divider's rails and the negative value that is not a rail**, eight
   values of `clip` at `w = 1` raw, straddling ±32,768.
6. **The guard band**, both rails on both axes, checked to be exactly ±524,288.
7. **An odd viewport (3 × 5)**, which is the only way the `fx_mad` rounding is
   observable: with an even width the product's low 16 bits are always zero. The
   sweep asserts it actually reached the exact half.
8. **A saturating row sum**, with `INT32_MAX` matrix elements and `INT32_MIN`
   coordinates.
9. **The dual view**: two matrices and two viewports (the `video_rules` §3.1
   stacked Duo pair), alternating packets, each checked against its own view —
   plus the assertion that the two views really do differ, without which the
   check proves nothing. Riders (`src_id`, view tag, Mosaic candidates) checked
   here.
10. **Backpressure** under four schedules including 31-in-32, with order,
    counter and `idle_o`.
11. **The measured rate**, printed.

## Randomized differential tests

`tests/terrain/terrain_project_random.cpp` — two lanes, 40 batches × 24
triangles each (400 nightly):

- **Lane A, lattice-shaped:** a 1/32-unit cell inside a plausible envelope seen
  through a pinhole camera whose Z offset sweeps the patch through the near
  plane. This is the regime the console runs in, and it is where a wrong
  viewport centre is a crack in a picture rather than a number.
- **Lane B, domain limit:** world coordinates over the ±2048 world-unit envelope
  *and* over the whole coordinate word, matrix elements over the whole fx16 word,
  viewports up to 512 × 512.

Both lanes assert they reached their states, because a green lane that sampled
nothing is how a flooring defect elsewhere in this tree survived 20,000 random
triangles:

| assertion | lane |
|---|---|
| reached the near plane | A and B |
| reached the guard band | A and B |
| drew inside the viewport | A |
| **never** saturates a row sum | A (it is the real regime) |
| saturated a row sum | B |
| railed the divider | B |

The classification uses `zref::mat4_vec4` and `zref::fx_div_exact` — the ratified
primitives — as **instrumentation only**. Every expectation comes from
`project_vertex`.

## Formal properties

**None, deliberately.** The candidate was the divider: "the 31-step recurrence
computes `floor(h/D)`". Stating it needs the same restoring recurrence in the
property, so the proof would restate the design. The invariant that *is* worth
having — `rem < D` at every stage, which is what makes the 32-bit compare
sufficient and the pre-rail start legal — is bounded, but it is also
*exhaustively* established by lane B: the rail test and the recurrence disagree
by construction if it fails, and lane B rails, near-misses and ordinary-divides
the same divisor set across 28,800 vertices per run.

A proof that restates the implementation is worse than no proof. Recorded here
so the next increment does not have to re-derive the decision. **If a solver
lane is ever added to this tree, this divider is the first thing to point it
at**, with the property `rem < D` and the target `q = floor(h/D)` against a
symbolic 48-bit `h`.

## Synthesis / resource ceiling

**Not synthesized.** `fpga/files.qip` is untouched, this block has never been
through Quartus, and nothing here has run on hardware. What can be stated is
structural, from the RTL rather than from a fitter: nine 32×32 signed
multipliers for the row sums, two more for the `fx_mad`, three lanes × 31 stages
× 63 bits of pipeline register (5,859 flip-flops) plus about 2,600 flip-flops of
rider, and 93 levels of 32-bit subtract-and-compare. Whether that fits a
5CSEBA6's ALM and DSP budget alongside the rest of the geometry mantle is a
question for a fit, not for this file.

## Integration capture cases

None yet — no golden capture routes through this block, because the terrain draw
path in `render_frame` is still the software raster.

What exists instead is `tests/terrain/terrain_project_chain.cpp`: real terrain
geometry projected by this block, straight into the real `zhao_geom_clip`, the
real `zhao_geom_setup` and `zhao_geom_bin_pipe` — GEOM.BINNER wired to
RASTER.EDGEWALK → EARLYZ → FRAGMENT → TILESTORE → RESOLVE. See that file's header
for what it asserts and for the restriction it inherits from
`zhao_raster_tile_pipe` (one triangle per tile per job).

## Notes

**LAWS FOUND, not invented** (each is also argued in the RTL header): the exact
row sum with ONE rescale (§2); `v.w = 1.0` making column 3 a shift; `clip.z`
never being read, so row 2 is never computed; the near plane as a per-vertex
rejection carrying zeros; and the guard band as a CLAMP.

**LAWS CHOSEN, not found.** Each is also argued in the RTL header.

1. **This block takes a triangle and emits a triangle.** `project_vertex` is a
   per-vertex function and the ledger's rate line is per vertex, but the only
   consumer that exists (GEOM.CLIP) takes triangles and the only producer that
   exists (TERRAIN.TESS through TERRAIN.NORMALS) emits them. A per-vertex port
   would need a vertex cache and an index stream between two finished blocks —
   a block the ledger does not have at phase 6. The rejected alternative was to
   emit vertices and let the composition assemble triangles in C++, which would
   have hidden the missing block instead of naming it. See Target throughput for
   what the choice costs.
2. **The dual view is two register sets and a packet bit, not two datapaths.**
   Duo runs two 256×192 views that share the frame's clock budget rather than
   each needing all of it. Duplicating the datapath would double the expensive
   part to buy a rate the mode does not need.
3. **`mosaic_candidates` is three fields on the primitive packet, and this block
   selects nothing.** `spec/terrain_rules.md` §6.2 gives the Mosaic pick to
   TEXTURE.MOSAIC and the pick is per TEXEL, which is not a quantity this block
   has. Layer E's `{matA, matB, weight}` triple is per CELL and arrives with the
   primitive; it rides through unaltered. Inventing a selection rule here would
   ratify a §6.2 amendment by omission.
4. **The face normal does not ride through.** The ledger lists `terrain_normals`
   as an input; it is a per-triangle quantity, this block does no shading, and
   carrying 96 bits through 37 rigid stages would buy nothing a `src_id`
   re-association does not already buy. Recorded as a deliberate divergence from
   the ledger's input list rather than hidden.
5. **The pipeline is rigid.** An elastic pipeline (skid buffers between stages)
   would keep the divider busy through short stalls, at the cost of 31 more
   valid/ready pairs and a latency that is no longer a number. Rejected for a
   block whose consumer is a rasterizer that stalls in long tile-sized bursts,
   not in single cycles.

**MUTATION-CHECKED.** Six defects were injected one at a time, each proved to
have relinked by hashing the three test binaries before running them, and each
reverted afterwards:

| mutation | directed | random | chain |
|---|---|---|---|
| §2 row rescale by 15 instead of 16 | 451/2007 red | 1169/2089 red | **green** |
| the divider rounds with D instead of D/2 | 520/2007 red | 881/2089 red | 4/26 red |
| the guard band clamps one subpixel short | 24/2007 red | 155/2089 red | **green** |
| the near plane is `< 0` instead of `<= 0` | 18/2007 red | **green** | **green** |
| the negative quotient floors instead of ceils | 87/2007 red | 847/2089 red | 2/26 red |
| the reassembly swaps vertices B and C | 593/2007 red | 1330/2089 red | **green** |

**Three of those greens are findings, not gaps, and each one is a fact about the
machine.**

- *The chain is blind to a uniform scaling of the row sums,* because projection
  is homogeneous: rescaling by 15 doubles `clip.x`, `clip.y` AND `clip.w`, and
  `ndc = clip.x / clip.w` is unchanged. Only `1/w` moves, and the composition
  runs the tile pipe with the depth test off. A composition can only see what
  reaches the framebuffer.
- *The chain is blind to a B/C swap,* because GEOM.CLIP NORMALISES THE WINDING —
  `area < 0` swaps B and C, which is its documented double-sided law. Vertex
  order is absorbed one block downstream by design.
- *The random lanes are blind to `w == 0`,* which is a measure-zero event under
  random input. That case exists in the directed suite for exactly this reason,
  and it is why "the boundary value, by hand" is not a formality.
