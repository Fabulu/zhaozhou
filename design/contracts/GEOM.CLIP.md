# Contract — GEOM.CLIP (Clip and cull)

> Ledger: `design/blocks.yml` · owner ZH-056 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

The front door of the geometry mantle: the near-plane verdict, the double-sided winding law, the (chosen) backface mode, and the `spec/qformats.md` §8 scissored scan box. RTL: `fpga/rtl/geometry/zhao_geom_clip.sv`. NEVER CUT (§26).

Exclusions — none of these are in this block: projection (GEOM.PROJECT / TERRAIN.PROJECT own the matrix, the perspective divide and the guard-band clamp; this block is handed screen vertices plus a per-vertex behind-the-eye bit), edge coefficients (GEOM.SETUP), tile enumeration (GEOM.BINNER), coverage (RASTER.EDGEWALK), attribute handling of any kind, and **vertex generation** — see the near-plane law below.

## Laws FOUND, and laws CHOSEN

**FOUND — the near plane is a WHOLE-PRIMITIVE rejection, not a clip.** `reference/src/zrender/rast.cpp::project_vertex` returns `in = false` when `clip.w.raw <= 0`; `internal.hpp`'s `ProjOut` says so in as many words ("false: w <= 0 (behind the eye) — Phase-3 near-plane rejection culls the whole primitive (documented)"), and every caller — `terrain.cpp` ("a cell (or wall quad) whose corner vertices include one behind the eye is dropped, the rest of the patch still draws"), `sprites.cpp`, `render_frame.cpp` — drops the whole primitive on that bit. `spec/sky_and_beams.md` §1.2's projection corollary names it: "whole-primitive near-plane rejection is the documented Phase-3 clip model", and it is why the under-plane is emitted as an 8×8 cell grid at all.

The consequence is structural and it is why this block is small: **one triangle in, at most one triangle out.** A Sutherland–Hodgman near-plane clip would emit 1 or 2 triangles per input and need a per-vertex reciprocal for the intersection parameter, a vertex FIFO and an attribute lerp. None of that exists, because none of it is this machine's law. There is no output queue, no variable output count and no divider anywhere in the block.

**FOUND — the guard band is an upstream SATURATION, not a clip plane.** §8: "Conversion from fx16: `rescale(x · 256, 16)` … then clamp to the guard band", and `zref_fixp.hpp::to_screen_xy` does exactly that (and bumps the saturation ledger). A screen vertex arriving here is already inside ±2048 px by construction; there is no coordinate this block could be handed that the guard band would reject. What the ±2048 px extent buys is that the 21-bit coordinate and §8's Giesen bound (2⁴³−2 at p = 21) hold for every triangle that reaches the rasterizer, *including the wildly off-screen ones* — which is precisely why they can be thrown away by a cheap rectangle test instead of by a clipper. The "guard-band clipping" of the ledger's purpose line is, in this machine, the scissor test below: a triangle whose scan box misses the viewport is dropped whole, at ±2048 px or at 3 px, by the same compare.

**FOUND — the scissor test is `raster_tri`'s own early return.** rast.cpp computes, in whole pixels scissored to the viewport,

```
min_x = max((min(Ax,Bx,Cx) + 127) >> 8, vp.x0)
max_x = min((max(Ax,Bx,Cx) − 128) >> 8, vp.x0 + vp.w − 1)
```

(and the same on Y), and returns immediately if `min_x > max_x || min_y > max_y`. That is §8's pixel-CENTRE range — the 2026-08-15 defect fix — and it is part of the coverage law, not an optimization: the +127/−128 form is what closes the 1-pixel seam cracks. **The oracle reaches it by CALLING `zref::render::scan_bbox`, the function `raster_tri` itself calls**, extracted from it for this increment (`reference/src/zrender/internal.hpp`, `rast.cpp`) so the bbox law has exactly one site. "RTL == oracle" here is therefore "RTL == the software raster's own early return".

**FOUND — zero area is rejected and the winding is normalised, exactly as rast.cpp does it.** `area == 0` is dropped; `area < 0` swaps B and C (the double-sided law). Doing the flip HERE rather than in GEOM.SETUP is the ruling this block owns: RASTER.EDGEWALK also flips, and a triangle that arrives already normalised makes its flip a no-op, so the two agree by construction instead of by coincidence — and GEOM.SETUP's top-left bits, which are only meaningful for a positive-area triangle, are derived from the same vertices RASTER.EDGEWALK will use.

**CHOSEN — backface culling is a MODE, and its default is OFF.** The ledger's purpose line names "backface cull". No spec ratifies a winding convention, and the reference is explicit that it has not been decided: `internal.hpp` calls the software raster "Double-sided (Phase-3: terrain quads and the sky drum are emitted with recorded windings but the software raster shades both — **the RTL backface-culling freeze is Phase 4/5**)". So the MECHANISM is built and the POLICY is not taken here: `cull_mode_i` = 0 NONE (double-sided, bit-exact with rast.cpp, and the reset value), 1 = reject 2A < 0, 2 = reject 2A > 0. A default of anything but NONE would silently delete half the geometry of every existing golden capture and would ratify a winding by omission. **Which mode the shipping game binds is an owner decision and is NOT taken by this increment.**

**CHOSEN — the counter split.** `triangles_submitted`, `triangles_clipped` and `triangles_culled` are `spec/counters.md` §1 catalog entries with no stated event. Chosen: `triangles_submitted` counts every triangle ACCEPTED at the input port; `triangles_clipped` counts rejections for WHERE the triangle is (near-plane, or an empty scissored box); `triangles_culled` counts rejections for WHAT it is (zero area, or backface). A degenerate triangle is not off-screen and an off-screen triangle is not degenerate; one counter for both would make neither number readable. The two are disjoint, so `submitted − clipped − culled` is exactly the accepted count — asserted as an invariant by the directed test.

**CHOSEN — the reject order**, which is what makes the counters deterministic: near-plane → zero-area → backface → scissor. It is rast.cpp's own order (the caller's `in` test, then `area == 0`, then the flip — where the backface decision lives — then the bbox/scissor early return). A behind-the-eye triangle has meaningless screen coordinates, so testing its area or its box first would classify it on garbage.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release, in the style of every other block in this tree (`always_ff @(posedge clk or negedge rst_n)`). On reset: all three pipeline stages invalid, `tri_ready_o` high, `out_valid_o` and `ret_valid_o` low, `cull_mode` NONE, all counters zero.

## Input and output packet layouts

Input (`tri_valid_i` / `tri_ready_o`), sampled on the accepting edge:

| field | width | meaning |
|---|---|---|
| `tri_ax_i` … `tri_cy_i` | 6 × signed 21 | projected screen vertices, S 12.8 subpixels (§8), already guard-band clamped |
| `tri_behind_i` | 3 | bit *k* = vertex *k* had `w ≤ 0` at projection (rast.cpp `ProjOut::in`, inverted); bit 0 = A |
| `tri_src_id_i` | 16 | source id, carried through untouched (`source_ids: true`) |

Configuration, sampled with the packet: `vp_x0_i` / `vp_y0_i` / `vp_w_i` / `vp_h_i` (12 bits unsigned each — the scissor rectangle in whole pixels: a canvas in Z60/Storm, one 256×192 view block in Duo, `spec/video_rules.md` §3.1) and `cull_mode_i` (2 bits, see above).

Output (`out_valid_o` / `out_ready_i`), only for an ACCEPTED triangle:

| field | width | meaning |
|---|---|---|
| `out_ax_o` … `out_cy_o` | 6 × signed 21 | the winding-normalised triangle (2A > 0) |
| `out_area2_o` | signed 48 | 2A in subpixel², strictly positive |
| `out_min_x_o`, `out_max_x_o`, `out_min_y_o`, `out_max_y_o` | signed 12 | the scissored scan box, INCLUSIVE whole pixels |
| `out_src_id_o` | 16 | the packet's source id |

Per-triangle verdict: `ret_valid_o` pulses for one cycle as EVERY triangle retires (accepted or not), with `ret_verdict_o` ∈ {0 accept, 1 near-plane, 2 zero-area, 3 backface, 4 offscreen} — the same encoding as `zref::Clip::Verdict`. `out_valid_o` is high on exactly the cycles where `ret_verdict_o == 0`, which the test driver asserts.

## Backpressure rules

`ready_valid` on both sides. Three pipeline stages advance together on one enable: the pipe stalls only when stage 3 holds an ACCEPTED packet the sink will not take, so a scene of culled triangles never stalls on a busy rasterizer. `tri_ready_o` is that enable; `out_valid_o` never depends on `out_ready_i` (no combinational valid←ready path). A stalled packet holds every output field stable until accepted — asserted by the driver on every beat of the backpressure lanes.

## Memory ownership

None. No VRAM, no RAM, no arena; the block is three stages of registers over its own packet.

## Q formats and rounding

Coordinates are S 12.8 screen subpixels, 21-bit, ±2048 px guard band (§8). **The block performs no rounding of a VALUE**; the only non-exact operation is the scan box's ceiling, which is `(v_min + 127) >> 8` — an add followed by an ARITHMETIC shift (floor), because `>>` on a signed C++ `int32_t` is what rast.cpp uses and the `+127` form is only a ceiling under flooring semantics. Three arithmetic domains:

1. **2A, exact.** `orient(A,B,C)` in subpixel², carried in 48 bits signed, from 23-bit differences and 46-bit products — RASTER.EDGEWALK's own `DIFF_W` / `PROD_W` / `CROSS_W`, deliberately, because it is the same quantity that block computes. |Δ| ≤ 2²⁰ for a pair of guard-band coordinates (|v| ≤ 2048·256 = 2¹⁹), so |2A| ≤ 2⁴¹, inside §8's Giesen bound of 2⁴³−2 with four bits over.
2. **The scan box, exact.** A 21-bit coordinate plus 127 fits in 22 bits signed; taking bits [21:8] is exactly `>>> 8` and never truncates, since the sum is bounded by 2¹⁹ + 127 < 2²⁰. The emptiness test is taken at the full 14-bit width BEFORE the box is narrowed to the 12-bit output, because an unclamped `max` can be as low as −2049; when the triangle is accepted the clamp guarantees `0 ≤ min ≤ max ≤ vp0 + vpw − 1`, so 12 bits is exact for every value a consumer ever sees.
3. **Comparisons, signed throughout.** Every min/max and every box compare is between two signed operands. (Verilog makes a comparison unsigned if EITHER operand is; that trap cost a real bug in GEOM.BINNER's corner offset during this increment and is called out here so the next reader does not repeat it.)

## Latency (fixed or variable)

The ledger says `variable`; it is **fixed at 3 cycles** at full downstream readiness, which is a strictly stronger statement. S1 latches the packet and takes the per-axis min/max; S2 issues the two 23×23 signed products and the box's shift/clamp; S3 forms 2A, decides the verdict, applies the winding flip and moves the counters. Under backpressure the whole pipe stalls together, so latency grows only by the stall.

## Target throughput

**One triangle per clock, met.** All three stages advance on the same enable and the input is accepted every cycle the pipe is not stalled; there is no multi-cycle state anywhere. A rejected triangle costs nothing extra — it retires in the same cycle it reaches S3 regardless of `out_ready_i`.

## Overflow and malformed-input behaviour

- **Behind the eye** (any `tri_behind_i` bit): the WHOLE triangle is rejected, `ret_verdict_o = 1`, and the near-plane wins over every other verdict — a behind-the-eye triangle's screen coordinates are meaningless, so classifying it on its area or its box would be classifying it on garbage.
- **Zero area** (coincident, repeated or collinear vertices, including subpixel-collinear): rejected exactly as rast.cpp rejects it, `ret_verdict_o = 2`. Not an error, not a stall.
- **Empty scissored box**: `ret_verdict_o = 4`. This is the only sense in which anything is "clipped": nothing is trimmed, the triangle is dropped whole.
- **A degenerate viewport** (`vp_w_i == 0`) makes `max_x = vp_x0 − 1 < min_x`, so every triangle is `kOffscreen`. Deterministic, not an error path.
- **Arithmetic overflow**: structurally impossible inside the guard band; the three width bounds above are proved in the RTL header and exercised at the ±2048 px extremes by the directed and random tests.
- There is no malformed input that can make the block scribble, hang or emit a packet it did not derive: the output is a function of three registered stages.

## Counters and traces

`triangles_submitted_o`, `triangles_clipped_o`, `triangles_culled_o`, all u32 and **saturating at `0xFFFF_FFFF`** per `spec/counters.md` §4 (never wrap). The event definitions are the CHOSEN split above. The catalog ids and the `frame_tick` shadow-latch (counters.md §3/§5) are NOT implemented here — this block has no snapshot channel, exactly as RASTER.EDGEWALK's contract records for `covered_fragments`; wiring the three into DEBUG.COUNTERS belongs with that integration wave. Trace: `ret_valid_o` + `ret_verdict_o` is a one-cycle-per-triangle verdict stream, which is what the differential lanes compare.

## THE NEAR-PLANE OBLIGATION — 2026-09-03

The near-plane law **drops an entire triangle when any vertex lies behind the
eye**. That is a deliberate simplification, not an accidental omission — and
the owner has attached an obligation to it.

**The close-camera stress reel is MANDATORY before the v1 geometry path
freezes.** It must include:

* the camera entering or grazing Zixx's spring pose;
* giant limbs and body geometry crossing the camera;
* long beams passing through the near plane;
* large terrain/cliff triangles;
* water surfaces;
* **fast camera motion through each case.**

**The owner judges the moving footage at native 240p.** If whole-triangle
rejection visibly removes unacceptable chunks, the response is either a
**bounded proper near-plane clipper**, or a **proven content/tessellation
restriction** that makes the pop acceptably small.

> **"Documented simplification" is not itself an acceptable visual result.**

## Scalar reference function

`zref::Clip` (`reference/include/zref/zref_geom.hpp`, `reference/src/zrender/geom.cpp`) — the ledger's declared `reference_model`.

It is deliberately NOT a second implementation. The 2A cross product is `zref::EdgeWalk::area2`, which IS rast.cpp's `orient()` (`orient(a,b,px,py) == EdgeWalk::area2({a, b, (px,py)})`), and the scan box is `zref::render::scan_bbox` — the function `raster_tri` itself calls. The near-plane law is likewise read out of the reference rather than restated. What the oracle adds is the ORDER of the four tests and the two counter buckets, which are the block's own chosen laws and are stated once, here and in the RTL header.

`scan_bbox` was extracted from `raster_tri` for this increment. It is a pure extract-function refactor — the arithmetic is byte-identical and `raster_tri` calls it — and the whole golden capture suite passing unchanged is the evidence.

## Directed tests

`tests/geometry/geom_clip_directed.cpp` (driver `tests/geometry/geom_dev.hpp`) — **3,438 checks**. Every case runs the Verilated block and `zref::Clip` over the same triangle, viewport and cull mode and requires the verdict and the whole accepted packet to be identical; on top of that each asserts its own law:

triangles wholly inside (box pinned by hand as well as diffed) and one filling the guard band; wholly left / right / above / below and one a guard band away; **all seven non-zero `behind` masks**, plus the proof that the near plane beats a zero area on the same triangle; five degenerate shapes (coincident, repeated vertex, collinear, subpixel-collinear, exactly horizontal) each cross-checked against `EdgeWalk::area2 == 0`; **all six vertex permutations** giving the same |2A| and the same box; **the box law swept through all 256 subpixel fractions** on both axes, plus a one-subpixel needle at every fraction — this is the 2026-08-15 seam-crack defect class, and an off-by-one in either the +127 or the −128 constant fails somewhere in the sweep; the scissor straddling each edge of all four shipping viewports and the exact ±2-pixel boundary at each; guard-band extremes in every corner combination and the widest triangle the band admits (2A = 4·2³⁸, checked exactly); all three cull modes including the reject ORDER (zero area beats backface, backface beats offscreen); the counter invariant; and four PCG backpressure patterns.

## Randomized differential tests

`tests/geometry/geom_clip_random.cpp` — deterministic from fixed seeds.

**Lane A — packet differential.** PCG triangles across six populations (canvas-local, guard-band-wide, slivers, exactly degenerate, wholly off-screen, behind-the-eye) against all four shipping viewports and all three cull modes, a third with `out_ready_i` PCG-gated. Verdict, normalised vertices, 2A and all four box edges must equal `zref::Clip` exactly.

**Lane B — the property that makes a reject SAFE.** A `kOffscreen` verdict is a promise that the triangle covers no pixel of the viewport, and the lane checks it against the COVERAGE oracle rather than against the box: every tile of the viewport is walked with `zref::EdgeWalk` and the total must be zero. Populations are biased onto the viewport edges, where an off-by-one lives.

Default 4,000 / 1,000 iterations (CTest `fast`); `--nightly` 60,000 / 20,000. Failing vectors are serialized per charter §29-17.

**Mutation evidence (2026-08-18).** Two deliberate RTL defects were injected one per invocation, each proved to have relinked by hashing all seven geometry test binaries (SHA-256) after deleting the generated `Vzhao_geom_*.cpp/.h` — never by touching mtimes, which a previous increment used to poison three sweeps:

| mutation | caught by |
|---|---|
| the near-plane test inverted (`behind != 0` → `behind == 7`) | `geom_clip_directed` AND `geom_clip_random` |
| the scan box's pixel-centre constant off by one (`+127` → `+128`) | `geom_clip_directed` AND `geom_clip_random` |

Neither reddened the GEOM.SETUP or GEOM.BINNER lanes, which is correct: those lanes build their input packets from the `zref::Clip` ORACLE, so a GEOM.CLIP RTL defect cannot reach them. The isolation is deliberate.

## Formal properties

**None, and here is why plainly.** The block's arithmetic is one 23×23 signed cross product and a pair of shift-and-clamp comparators. The cross product puts a whole-module BMC out of reach for this engine for exactly the reason RASTER.EDGEWALK's contract already records about its own setup; and the parts that WOULD be provable — the box clamp, the verdict priority — are total functions of at most 24 free bits and are already covered exhaustively by the 256-fraction directed sweep, which is a proof by enumeration over the same space a solver would search. Writing an .sby for them would add a lane and no information. The one property worth proving in this subsystem is GEOM.BINNER's arena bound, and it is proved (`tests/formal/geom_binner_arena_bounds.sby`).

## Synthesis / resource ceiling

Budget group `geometry_mantle`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers. The shape is two 23×23 signed multipliers, one 48-bit subtractor, four 3-way min/max comparator trees on 21-bit values, four 14-bit shift-and-clamp units, three 32-bit saturating counters and roughly 350 flops of pipeline state. The multipliers are the whole cost; they are the same pair RASTER.EDGEWALK spends, and a fit that needs them back can serialise the two products over two cycles at half the throughput.

## Integration capture cases

None. The block is not in `ZHAO_SHELL_RTL`, is not in `fpga/files.qip`, has no capture and has never run on hardware. It IS composed in simulation only, indirectly: `fpga/rtl/geometry/zhao_geom_bin_pipe.sv` drives the whole raster chain from GEOM.BINNER, and the packets that composition consumes are the ones this block's oracle produces. Simulated is not synthesized and neither is on-hardware.

## Notes

Guard band ±2048 px ratified (A3c); fixed-point clipping exactly per reference.

Deliberately not built, so the next wave knows: no near-plane vertex GENERATION (the law is whole-primitive rejection, see above), no far-plane test (no spec states one and `invw24` has no far rail), no small-triangle or sub-pixel cull (rast.cpp keeps them and so must this), no per-triangle scissor rectangle (the viewport is a configuration input, latched per packet), no counter-catalog wiring, and no ratification of a winding convention.
