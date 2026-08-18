# Contract — GEOM.SETUP (Triangle setup)

> Ledger: `design/blocks.yml` · owner ZH-057 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

Screen-space triangle setup: the affine decomposition of the frozen §8 edge function, in the exact form RASTER.EDGEWALK already evaluates. RTL: `fpga/rtl/geometry/zhao_geom_setup.sv`. Accepts geometry, Forge and polygon-particle packets alike — the block never looks at where a triangle came from.

Exclusions — none of these are in this block: clipping, the winding decision and the zero-area reject (GEOM.CLIP has done all three; a triangle arriving here has 2A > 0), tile enumeration (GEOM.BINNER), coverage (RASTER.EDGEWALK), and **attribute/depth/UV gradients** — see "The gradients are not built" below, which is a real gap against the ledger's purpose line and is stated as one.

## Laws FOUND, and laws CHOSEN

**FOUND, and it is the whole block: the edge function is affine, and these are its coefficients.** rast.cpp's frozen edge function is

```
orient(a, b, px, py) = (b.x − a.x)(py − a.y) − (b.y − a.y)(px − a.x)
```

Expanding it in the sample position is an IDENTITY, not a model:

```
orient(a, b, px, py) = kx·px + ky·py + kc
    kx = −(b.y − a.y)        ky = +(b.x − a.x)        kc = a.x·b.y − a.y·b.x
```

(the `kc` form falls out because `(b.y−a.y)·a.x − (b.x−a.x)·a.y` telescopes to the plain cross product of a and b). Those three numbers ARE the "edge coefficients" of the ledger's purpose line, and they are exactly what RASTER.EDGEWALK needs: it steps `E′` by `−Δy` per pixel of x and `+Δx` per pixel of y — i.e. by `kx` and `ky` — and its edge value at any pixel centre is `kx·px + ky·py + kc` with `px = 256·p + 128`. Nothing here re-derives the fill rule; it is the same function with the sample position factored out so a consumer can evaluate it anywhere in O(1).

Edges are numbered as rast.cpp numbers them: edge 0 = (B,C) → `w0`, edge 1 = (C,A) → `w1`, edge 2 = (A,B) → `w2`. `tl_i` is `edge_top_left(a_i, b_i)` verbatim — `(a.y == b.y) ? (a.x < b.x) : (a.y < b.y)` — evaluated on the winding-normalised triangle GEOM.CLIP emits, which is the same triangle RASTER.EDGEWALK evaluates it on after its own (now no-op) flip.

**FOUND — the third constant is free, and that is a theorem.** The barycentric identity `w0(p) + w1(p) + w2(p) = 2A` holds for EVERY p — it is the reason rast.cpp divides its interpolants by `area`. Matching coefficients on both sides of an identity in p gives three separate facts:

```
kx0 + kx1 + kx2 = 0      ky0 + ky1 + ky2 = 0      kc0 + kc1 + kc2 = 2A
```

GEOM.CLIP already computed 2A (it needed it for the zero-area reject and the winding) and hands it over, so this block computes `kc0` and `kc1` with four 21×21 signed multipliers and takes `kc2 = 2A − kc0 − kc1` — exactly, in integers, with no rounding and no approximation. That is **two multipliers saved out of six**, a third of the block's arithmetic. The oracle computes all three constants directly, so every iteration of the random lane is a check of the identity rather than an assumption of it.

**No law was CHOSEN in this block.** Every number it emits is a rearrangement of a frozen function, and the one saving it takes is an algebraic identity. The choices in this subsystem live in GEOM.CLIP (the backface mode, the counter split) and GEOM.BINNER (the grid anchor, the enumeration order, the corner test, the overflow wall); this contract has none to record, and saying so is the point.

## The gradients are NOT built, and the reason is not cost

The ledger's purpose line says "edge coefficients, gradients". The edge coefficients are here. The attribute gradients are not, and this is the argument rather than an omission:

1. **The oracle's attribute model is not a plane setup.** rast.cpp does not set a plane up once and step it over the triangle. It computes ONE `div_rhu_s128` gradient per attribute for the x direction, and then re-evaluates the row start with a FULL barycentric division at every scanline (`d = div_rhu_s128(w0·A.d + w1·B.d + w2·C.d, area)` inside the y loop). Each of those is an independent §4 round-half-up. A block emitting `(d0, ∂d/∂x, ∂d/∂y)` would therefore **not be bit-exact with the oracle** — its second row would differ by the rounding rast.cpp re-does. The per-row divide belongs to whatever walks rows, not to triangle setup, and inventing a plane form here would quietly fork the depth law.
2. **There is no consumer.** RASTER.FRAGMENT interpolates nothing: its contract and `zhao_raster_tile_pipe.sv`'s header both state that the fragment colour, alpha, depth, tag and texel are FLAT, taken from the packet. TEXTURE.TMU is handed U/V already through the §8 perspective divide. Every gradient this block could emit would be dropped on the floor.
3. **The cost is a 128÷48 round-half-up divider per attribute** — the most expensive unit anywhere in the geometry mantle — built for nobody.

**This is a real shortfall against the ledger's stated purpose and is recorded as one.** What lands it is the interpolator increment that also gives RASTER.FRAGMENT a varying input; until that exists there is nothing to be bit-exact against.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release. On reset: all three stages invalid, `tri_ready_o` high, `out_valid_o` low, `triangles_submitted_o` zero.

## Input and output packet layouts

Input (`tri_valid_i` / `tri_ready_o`) — GEOM.CLIP's accepted packet, unchanged: six signed-21 vertices (winding-normalised, S 12.8 subpixels), `tri_area2_i` (signed 48, > 0), the four signed-12 scan-box edges, and `tri_src_id_i`.

Output (`out_valid_o` / `out_ready_i`):

| field | width | meaning |
|---|---|---|
| `out_kx0_o` … `out_kx2_o`, `out_ky0_o` … `out_ky2_o` | 6 × signed 23 | the per-subpixel edge coefficients — RASTER.EDGEWALK's own `sx`/`sy` |
| `out_kc0_o` … `out_kc2_o` | 3 × signed 48 | the edge constants; `kc2` is derived by the barycentric identity |
| `out_tl_o` | 3 | bit *i* = edge *i* is top-left (§8 bias 0 rather than −1) |
| `out_area2_o` | signed 48 | 2A, passed through |
| `out_ax_o` … `out_cy_o`, `out_min_x_o` … `out_max_y_o`, `out_src_id_o` | — | the triangle, its scan box and its source id, carried through to RASTER.EDGEWALK's job port |

## Backpressure rules

`ready_valid` on both sides. Three stages advance on one enable; the pipe stalls only when stage 3 holds a packet the sink will not take. `out_valid_o` never depends on `out_ready_i`. A stalled packet holds all nine coefficients, the three top-left bits and the whole passthrough stable until accepted — asserted by the driver on every beat of the backpressure lane.

## Memory ownership

None. Three stages of registers, no RAM of any kind.

## Q formats and rounding

**The block performs no rounding at all.** Every output is an exact integer function of exact integer inputs; there is no shift, no divide and no saturation anywhere in the datapath. That is worth stating because it is unusual in this tree and because it is what makes the differential a bit-for-bit equality rather than a tolerance.

Widths, with |v| ≤ 2048·256 = 2¹⁹ for a guard-band S 12.8 coordinate:

- `kx`, `ky` are differences of two coordinates, |Δ| ≤ 2²⁰ → **23 bits signed**, RASTER.EDGEWALK's own `DIFF_W`, so the two blocks carry the same numbers in the same width.
- a product is ≤ 2³⁸ → 42 bits signed (`PROD_W`); Verilog's context-determined width makes the 21×21 multiply produce the full product because the destination is 42 bits wide.
- |kc| ≤ 2·2³⁸ = 2³⁹ and |2A| ≤ 2⁴¹ (inside §8's Giesen bound of 2⁴³−2), so |kc2| ≤ 2⁴¹ + 2⁴⁰ < 2⁴². All three are carried in **48 bits signed** — RASTER.EDGEWALK's `CROSS_W`, again deliberately the same domain.

## Latency (fixed or variable)

The ledger says `variable`; it is **fixed at 3 cycles** at full downstream readiness. S1 latches; S2 registers the four products, the six differences and the three top-left bits; S3 forms the three `kc` constants. Under backpressure the pipe stalls together.

## Target throughput

**One setup triangle per clock, met.** All three stages advance on the same enable and the four multipliers are independent; there is no multi-cycle state.

## Overflow and malformed-input behaviour

- The block assumes its input is GEOM.CLIP's ACCEPTED packet: 2A > 0, no coincident vertices, a non-empty scan box. It does not re-check them, because re-checking would duplicate a law that has one site.
- Handed a zero-area triangle anyway, it produces `kc2 = −kc0 − kc1` and three coefficient sets that are still the exact affine decomposition of `orient()` for those vertices — a well-defined, non-scribbling answer that the downstream corner test will reject everywhere. There is no state to corrupt.
- **Arithmetic overflow is structurally impossible** inside the guard band; the three bounds above are proved in the RTL header and exercised at the ±2048 px extremes by the directed test, which also asserts |kc| < 2⁴⁰ and |2A| < 2⁴² on every guard-band fixture.

## Counters and traces

`triangles_submitted_o` (u32, saturating at `0xFFFF_FFFF` per `spec/counters.md` §4). The catalog id and the `frame_tick` shadow-latch are not implemented here, for the same reason RASTER.EDGEWALK's contract records for `covered_fragments`. Trace: the nine coefficients plus the three top-left bits are what the differential lanes compare, per triangle.

## Scalar reference function

`zref::Setup` (`reference/include/zref/zref_geom.hpp`, `reference/src/zrender/geom.cpp`) — the ledger's declared `reference_model`. It computes the affine decomposition from its definition and all three `kc` constants directly; the identity `kx·px + ky·py + kc == orient(a,b,px,py)` is not assumed but ASSERTED, against `zref::EdgeWalk::area2` — which IS rast.cpp's `orient()` — over every pixel centre of a 3×3 tile block in the directed test.

## Directed tests

`tests/geometry/geom_setup_directed.cpp` — **1,202 checks**. Every case diffs all nine coefficients, the three top-left bits, 2A and the whole passthrough against `zref::Setup`, and then:

1. **the plane identity** — `kx·px + ky·py + kc` equals `orient()` at every one of 2,304 probed pixel centres per edge, checked against the frozen helper rather than a re-derivation;
2. **THE JOINT** — coverage rebuilt from `(kx, ky, kc, tl)` ALONE, through the §8 decomposition and the fill rule, diffed against `zref::EdgeWalk` over whole tiles for five shapes (a big triangle, a small one, a canvas half, a tile-aligned one, and one inset by a single subpixel). This is the claim "GEOM.SETUP produces the edge coefficients RASTER.EDGEWALK already consumes", made into an assertion;
3. **the third constant** — `kc0 + kc1 + kc2 == 2A` and both step sums zero, on four shapes including the widest guard-band triangle;
4. **the steps** — `kx`/`ky` per edge equal RASTER.EDGEWALK's own `sx`/`sy` expressions literally, so the two blocks step by the same numbers;
5. **top-left** — six edge orientations (horizontal both ways, vertical both ways, both diagonals), with both bias polarities shown to be exercised;
6. **guard band** — 49 extreme-vertex combinations plus the widest admissible triangle, with |kc| and |2A| bounds asserted;
7. **passthrough and backpressure** — four PCG stall patterns, packets held stable.

## Randomized differential tests

`tests/geometry/geom_setup_random.cpp` — deterministic from fixed seeds.

**Lane A — coefficient differential.** PCG triangles across four populations (canvas-local, guard-band-wide, slivers, tile-aligned), each pushed through `zref::Clip` first so the block sees exactly what GEOM.CLIP emits, a third with `out_ready_i` gated. All nine coefficients, the three top-left bits, 2A and the passthrough must equal `zref::Setup` exactly, and the lane additionally checks `kc0 + kc1 + kc2 == 2A` and both step sums on EVERY iteration — the oracle computes all three constants, the RTL derives the third, so agreeing IS the proof that the two-multiplier saving is exact.

**Lane B — the joint.** For a PCG tile of each triangle's scan box, coverage is rebuilt from the RTL's own coefficients and diffed against `zref::EdgeWalk`. A sign flip on any coefficient, a wrong top-left bit or an off-by-one in `kc` fails immediately.

Default 4,000 / 4,000 iterations (CTest `fast`); `--nightly` 60,000 / 30,000. Failing vectors serialized per charter §29-17.

**Mutation evidence (2026-08-18).** One deliberate RTL defect, injected on its own invocation and proved to have relinked by hashing all seven geometry test binaries (SHA-256) after deleting the generated `Vzhao_geom_*.cpp/.h` — never by touching mtimes:

| mutation | caught by |
|---|---|
| edge coefficient sign flipped (`kx0` loses its negation) | `geom_setup_directed` AND `geom_setup_random` |

It reddened neither the CLIP nor the BINNER lanes, which is correct: those build their packets from the oracles, so a GEOM.SETUP RTL defect cannot reach them.

## Formal properties

**None, and here is why plainly.** Everything this block computes is a total, exact integer function whose only non-linear parts are four 21×21 multiplies — the same bit-blasting wall RASTER.EDGEWALK's contract already records for its own setup, and out of reach for this engine on a whole module. The one statement worth proving — that the affine form equals `orient()` — is an algebraic identity over 2⁸⁴ inputs, and the honest way to check it is what the tests do: evaluate both sides against the frozen function over thousands of positions and, in lane B, over the coverage it produces. That is not a proof and this contract does not call it one.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers. The shape is four 21×21 signed multipliers, six 23-bit subtractors, three 48-bit adder/subtractor trees (one of them the three-input `2A − kc0 − kc1`), three top-left comparators and roughly 600 flops of pipeline state (three stages carrying the vertices, the box and the coefficients). The four multipliers are the cost; the barycentric identity already removed two of them, and a fit that needs more back can serialise the remaining pairs at half the throughput.

## Integration capture cases

None directly. The block is not in `ZHAO_SHELL_RTL`, is not in `fpga/files.qip`, has no capture and has never run on hardware. Its OUTPUT is exercised end to end in simulation: `fpga/rtl/geometry/zhao_geom_bin_pipe.sv` drives GEOM.BINNER → the whole RASTER chain from packets in exactly this layout, and the resolved picture is diffed pixel for pixel against the software raster. Simulated is not synthesized and neither is on-hardware.

## Notes

In-tile stepping s32, edge setup s64 (A3c).

Deliberately not built, so the next wave knows: the attribute/depth/UV gradients (argued above — no bit-exact model and no consumer), any perspective divide (§8 puts it per-pixel in the interpolator, and TEXTURE.TMU receives U/V already through it), any re-check of GEOM.CLIP's verdict, and any counter-catalog wiring.
