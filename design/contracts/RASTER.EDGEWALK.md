# Contract — RASTER.EDGEWALK (Edge walker)

> Ledger: `design/blocks.yml` · owner ZH-022 · phase 4 · maturity SPECIFIED

## Purpose and exclusions

Walk one triangle across one 16×16 tile and produce the EXACT `spec/qformats.md` §8 top-left coverage mask for the fragment pipeline. RTL: `fpga/rtl/raster/zhao_raster_edgewalk.sv` plus `fpga/rtl/raster/zhao_raster_fill.sv` (the per-pixel fill predicate, a separate module so the formal property proves the shipping expression and not a copy of it).

Exclusions — none of these are in this block: binning into tile lists (GEOM.BINNER hands it one triangle × one tile), early-Z (RASTER.EARLYZ), attribute/depth/UV interpolation and shading (RASTER.FRAGMENT), tile RAM (RASTER.TILESTORE), framebuffer writes and resolve (RASTER.RESOLVE), scanout, and shell integration. It emits coverage masks and nothing else. Backface culling is likewise NOT here: the block is double-sided exactly like the software raster (negative area is flipped, not rejected).

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release, in the style of every other block in this tree (`always_ff @(posedge clk or negedge rst_n)`). On reset: FSM idle, `job_ready_o` high, `cov_valid_o` low, all coverage state zero, `cov_count_o` 0, `job_degenerate_o` 0. One job is in flight at a time; there is no pipelining across jobs and therefore no reset ordering hazard.

## Input and output packet layouts

Input job (`job_valid_i` / `job_ready_o`), all sampled on the accepting edge:

| field | width | meaning |
|---|---|---|
| `job_ax_i` … `job_cy_i` | 6 × signed 21 | triangle vertices, S 12.8 screen subpixels (§8), guard band ±2048 px |
| `job_tile_x_i`, `job_tile_y_i` | signed 12 | tile origin — the top-left PIXEL of the 16×16 tile |
| `job_src_id_i` | 16 | source id, carried through untouched (`source_ids: true`) |

Output coverage (`cov_valid_o` / `cov_ready_i`), one beat per NON-EMPTY tile row:

| field | width | meaning |
|---|---|---|
| `cov_row_o` | 4 | tile-local row, 0 = top; rows leave in increasing order |
| `cov_mask_o` | 16 | bit *i* = tile column *i* covered; never zero on a beat |
| `cov_last_o` | 1 | last beat of this job (exact — the whole tile is walked before draining) |
| `cov_src_id_o` | 16 | the job's source id |

Per-job status, valid on the one-cycle `job_done_o` pulse: `job_degenerate_o` (area == 0, culled) and `cov_count_o` (covered pixels, 0…256). A job that covers nothing emits zero beats and still pulses `job_done_o`.

## Backpressure rules

`ready_valid` on both sides. `job_ready_o` is high only in IDLE and never depends on `cov_ready_i`; `cov_valid_o` is high only in DRAIN and never depends on `cov_ready_i` (no combinational valid←ready path in either direction). A stalled beat holds `cov_row_o` / `cov_mask_o` / `cov_last_o` / `cov_src_id_o` stable until accepted — asserted by the test driver on every beat of every job. The 16-row walk itself cannot stall: masks land in registers first and only the drain phase carries backpressure, so a slow consumer costs beats, never coverage.

## Memory ownership

None. No VRAM, no tile RAM, no arena; the block is pure combinational-plus-registers over its own job state (16 × 16-bit row masks). Tile storage belongs to RASTER.TILESTORE.

## Q formats and rounding

Coordinates are S 12.8 screen subpixels, 21-bit, ±2048 px guard band (§8). No rounding happens in this block at all — coverage is an exact integer decision. Three arithmetic domains:

1. **Setup, exact.** `E0 = (b.x−a.x)(p.y−a.y) − (b.y−a.y)(p.x−a.x)` in subpixel², carried in 48 bits signed. Differences are 23-bit signed (|Δ| ≤ 2²¹), products 46-bit, and the Giesen bound for 21-bit coordinates is 2⁴³−2, so 48 bits has 4 bits of headroom. Area (2A) uses the same unit.
2. **Decomposition, exact.** `E0 = 256·E′ + r` with `E′ = E0 >>> 8` (arithmetic, i.e. floor) and `r = E0[7:0] ∈ [0,255]`. Because one pixel of x steps `E0` by `−Δy·256` and one pixel of y by `+Δx·256`, `r` is CONSTANT over every pixel centre of the tile and is captured once per edge as the single bit `rnz = (r != 0)`. `E′` then steps exactly by `−Δy` / `+Δx`.
3. **Tile-local stepping, 29-bit signed with a sign-preserving clamp.** `E′` is clamped to ±2²⁷ at setup. Inside one tile the value moves by at most `15·|Δx| + 15·|Δy| ≤ 30·2²¹ < 2²⁶`, so a clamped value keeps its sign and its non-zeroness everywhere in the tile (`2²⁷ − 2²⁶ = 2²⁶ > 0`), and an unclamped one is exact. Worst-case magnitude carried is `2²⁷ + 2²⁶ < 2²⁸`, inside the 29-bit signed range.

Fill rule (`zhao_raster_fill`): `accept ⟺ ¬neg(E′) ∧ (top_left ∨ rnz ∨ E′ ≠ 0)`, which is exactly `E0 + bias ≥ 0` with bias 0 for a top-left edge and −1 otherwise — proved, not asserted (see Formal properties). `edge_top_left(p,q)` is `(p.y == q.y) ? (p.x < q.x) : (p.y < q.y)`, evaluated AFTER the winding flip.

No bounding box is computed. The software raster scans only "pixel centres in [v_min, v_max]", but a centre that passes all three edge tests lies in the closed triangle and is therefore inside that bbox on both axes — the bbox can never exclude a pixel the edge functions accept, so walking all 256 centres of the tile gives the identical set.

## Latency (fixed or variable)

Variable, bounded. 5 setup cycles (a shared 23×23 signed cross-product unit issues area, then the three edge values at the tile's first pixel centre; the winding decision lands between area and the edges), then exactly 16 walk cycles, then 0…16 drain beats. **21…37 cycles per triangle × tile job** at full downstream readiness; a degenerate (zero-area) job completes in 3 cycles. One job in flight — `job_ready_o` stays low until `job_done_o`.

## Target throughput

One 16-pixel coverage row per clock during the walk and during the drain — the ledger's "1 coverage set per clock". Per tile job the sustained figure is one 16×16 tile per 21…37 cycles; the walk is the fixed 16-cycle part and empty rows cost nothing downstream because they are never emitted.

## Overflow and malformed-input behaviour

- **Zero area** (coincident, repeated or collinear vertices): rejected exactly as the software raster rejects it — no coverage, `job_degenerate_o` set, `cov_count_o` 0. Not an error, not a stall.
- **Negative area**: the double-sided winding flip (B ↔ C) is applied BEFORE the edge values, per-pixel steps and top-left biases are derived, so every vertex permutation of a triangle covers identically.
- **Triangle wholly outside the tile**: zero beats, `job_degenerate_o` clear — culled by the edge functions, not by an error path.
- **Arithmetic overflow**: structurally impossible inside the guard band; the three width bounds above are proved in the RTL header and exercised at the ±2048 px extremes by the directed and random tests.
- There is no malformed input that can make the block scribble, hang, or emit an empty mask: the output is derived from registers written by a fixed 16-step walk.

## Counters and traces

Per-job `cov_count_o` (covered pixels, 0…256) is the block's contribution to the `covered_fragments` counter. The catalog id and the `frame_tick` shadow-latch (spec/counters.md §3/§5) are NOT implemented here: no `covered_fragments` id exists in `zhao_pkg` yet, and minting one is a counters.md amendment that belongs with the RASTER.EARLYZ / DEBUG.COUNTERS integration wave, not with this block. Trace: the full per-row coverage mask stream is what the differential tests compare.

## Scalar reference function

`zref::EdgeWalk` (`reference/include/zref/zref_edgewalk.hpp`, `reference/src/zrender/edgewalk.cpp`) — exact §8 coverage of one 16×16 tile for one triangle, plus `area2()` so a caller can predict the zero-area verdict.

It is deliberately NOT a second implementation of the fill law: the body calls `zref::render::raster_tri` (`reference/src/zrender/rast.cpp`) — the frozen `orient()`, `edge_top_left()`, pixel-centre bbox, winding flip and zero-area reject — and reads the coverage back out of the work surface. There is one fill rule in this repository and `zref::EdgeWalk` is a view onto it, so "RTL == `zref::EdgeWalk`" is literally "RTL == the §8 law".

`raster_tri`'s `Viewport` is unsigned, so a tile at an arbitrary (including negative) origin is handled by translating the triangle by (−256·tile_x, −256·tile_y) into a 16×16 surface: `orient()`, the top-left predicate and the bbox law are all translation invariant, and `256·tile` is a whole multiple of the pixel pitch so `(v + 127) >> 8` shifts by exactly `tile`. The RTL is still handed the untranslated triangle plus the tile origin, so a tile-origin bug still shows as a mask mismatch.

## Directed tests

`tests/raster/raster_edgewalk_directed.cpp` (driver in `tests/raster/raster_dev.hpp`) — 146 checks: zero-area cases (coincident / repeated / collinear, including a subpixel-collinear one); all six vertex permutations of a triangle covering identically; triangles wholly left / right / above / below / diagonally outside and one a guard band away; edges exactly on the tile boundary lines and exactly through pixel centres, plus one-pixel slivers on each border; the tile-diagonal shared-edge split (256 pixels covered exactly once, all 16 diagonal centres claimed once); **a vertical and a horizontal seam swept through all 256 subpixel fractions**, four triangles each, requiring zero holes and zero doubles at every fraction; guard-band extremes (vertices at ±2048 px, tiles at both ends of the screen); a full-tile triangle (16 × 0xFFFF, count 256); subpixel triangles on and off a pixel centre and a one-subpixel needle; backpressure (8 PCG stall patterns, identical coverage, beats held stable); and translation invariance across five tile origins including negative ones.

## Randomized differential tests

`tests/raster/raster_edgewalk_random.cpp` — deterministic from two fixed seeds. Lane A: PCG triangles over five populations (tile-local, guard-band-wide, slivers, pixel-centre-aligned, exactly degenerate) at PCG tile origins, half with PCG-gated `cov_ready_i`; row masks, count and degenerate verdict must equal `rast.cpp` exactly. Lane B: a PCG rectangle with independent subpixel corners split on BOTH diagonals — every triangle diffed against the oracle, then neither split may double-cover a pixel and the two splits must cover the identical pixel set (the seam may not move when the diagonal does). Default 4,000 iterations per lane (fast: 4,000 tiles + 16,000 tiles), `--nightly` 60,000. Failing vectors are serialized per charter §29-17.

**Mutation evidence** (2026-08-18): six deliberate RTL defects — strict `>`, bias on the floored `E′`, no winding flip, pixel corner instead of centre, no zero-area reject, and an unsafe saturation threshold — were each injected and each caught by BOTH lanes. The floored-`E′` mutation reproduces the documented 2026-08-15 defect precisely: 16 holes at seam fraction 128 (`E0 = ±128`), caught by the directed seam sweep and by lane B's split-independence check.

## Formal properties

`tests/formal/raster_edgewalk_top_left.sby` + `tests/formal/raster_edgewalk_top_left_fv.sv` — **PASS (bmc + cover)**, both tasks, `smtbmc boolector`, depth 2. The DUT is `zhao_raster_fill`, the exact module the tile walker instantiates 48× per row.

- `a_exact` — the narrow `(E′, rnz, tl)` form the RTL evaluates EQUALS `E0 + bias ≥ 0` with bias 0 / −1 on the exactly reconstructed `E0`.
- `a_exactly_once` — **the adjacent-triangle law** (charter §20.4): two triangles sharing an edge see `E0` and `−E0` with complementary top-left flags, and accept EXACTLY once for every edge value. No hole, no double fill, at any subpixel position.

The free inputs are `E′` and `r ∈ [0,255]`, which is a bijection onto the integers via `E0 = 256·E′ + r` — the proof covers every edge value, with no reachability gap. Six cover statements (all reached) pin the on-the-edge corners on both bias polarities and the sub-unit `E′ = 0, r ≠ 0` tiebreak. The proof was itself mutation-checked: removing `rnz` or forcing a strict `>` both make `a_exactly_once` fail.

**Not proved formally, and why:** the edge SETUP — the s64 cross products, the winding flip and the tile-local saturation — carries 46-bit multipliers that put a whole-module BMC out of reach for this engine. That half of the block is covered by the differential lanes against `rast.cpp` (20,000 tiles per fast run) and by the mutation evidence above, not by a proof.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers. The shape is 48 × 29-bit adders plus 48 fill comparators for the 16-wide row evaluator, two 23×23 signed multipliers (shared across the four setup cross products), one 48-bit subtractor, and ~400 flops (job state, three 29-bit accumulators, 16 × 16-bit row masks). The 16-wide row evaluator is the whole cost; narrowing it to 8-wide halves the adders and doubles the walk to 32 cycles if the fit demands it.

## Integration capture cases

None yet — the block is standalone. It is not in `ZHAO_SHELL_RTL`, has no capture, and has never run on hardware. Phase-4 gate ("single triangle is bit-exact in ZRef, Verilator and physical FPGA") is satisfied for ZRef and Verilator only; the FPGA leg belongs to ZH-024 (resolve one exact tile to a hardware framebuffer) together with RASTER.TILESTORE and RASTER.RESOLVE.

**Composed, in simulation only (2026-08-18).** `fpga/rtl/raster/zhao_raster_tile_pipe.sv` instantiates the WHOLE RASTER chain — RASTER.EDGEWALK → RASTER.EARLYZ → RASTER.FRAGMENT → RASTER.TILESTORE → RASTER.RESOLVE — and `tests/raster/raster_tile_pipe_directed.cpp` / `..._random.cpp` diff it against `zref::EdgeWalk` -> `zref::EarlyZ` -> `zref::FragmentPipeline` -> `zref::TileStore` -> `zref::TileResolve` driven through the identical clear/fragment/swap sequence. (Until the phase-4/5 raster completion the composition wrote one flat 64-bit word at every covered pixel as a stand-in for RASTER.FRAGMENT; that write path has since been replaced by the real blocks, and the `state == 0` fragment recipe reproduces the earlier behaviour bit for bit, which is why the pre-existing suite still passes unchanged.) That composition is **not a ledger block**: `design/blocks.yml`'s RASTER group has five entries and none of them is the composition, and registering one is a validator-gated ledger edit; the rationale and the three laws the composition owns live in that file's header. It is Verilator-only — not in `fpga/files.qip`, no Quartus fit, no capture, never on hardware. Simulated is not synthesized and neither is on-hardware.

## Notes

Top-left rule exactness is a formal candidate (ZH-023 covers fill conventions) — now discharged for the fill rule itself, see Formal properties.

Deliberately not built in this block, so the next wave knows: no empty-row skipping ahead of the walk (all 16 rows are evaluated, only non-empty ones are emitted); no bounding-box early-out (proved unnecessary for correctness, would only save walk cycles); no multi-job pipelining; no `covered_fragments` counter-catalog entry; no backface culling.
