# Contract — GEOM.BINNER (Tile binner)

> Ledger: `design/blocks.yml` · owner ZH-058 / ZH-026 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

Bin setup triangles into 16×16 tile chunked lists over a formally bounded arena, with safe overflow, and drain them straight into RASTER.EDGEWALK's job port. RTL: `fpga/rtl/geometry/zhao_geom_binner.sv` plus `fpga/rtl/geometry/zhao_geom_arena.sv` (the chunk allocator, a separate module so the formal property proves the shipping bytes and not a copy of them) and `fpga/rtl/raster/zhao_raster_fill.sv` (the §8 fill predicate, instantiated for the trivial-reject).

Exclusions — none of these are in this block: clipping or scissoring (GEOM.CLIP hands over an already-scissored scan box), edge setup (GEOM.SETUP), coverage (this block decides WHICH tiles get walked, never which pixels — RASTER.EDGEWALK owns that), VRAM (the arena is on-chip and fixed), frame scheduling, **re-submission of overflowed work**, and any token POLICY (MEASURE.TOKENS owns that).

## Laws FOUND, and laws CHOSEN

**FOUND — the tile is 16×16 pixels.** Charter §8's "active tile storage", RASTER.TILESTORE and RASTER.EDGEWALK all fix it; `zref::EdgeWalk::kTile` is 16. This block does not get to choose the pitch.

**FOUND — the enumeration rectangle is GEOM.CLIP's scan box**, which is `raster_tri`'s own scissored pixel-centre bounding box (§8, the 2026-08-15 defect fix). A pixel outside that box is never scanned by the software raster, so a tile outside it can hold no coverage and is not a candidate at all.

**FOUND — the trivial-reject predicate is the §8 fill rule itself.** The module instantiated is `zhao_raster_fill`, the one RASTER.EDGEWALK instantiates 48× per row and the one `tests/formal/raster_edgewalk_top_left.sby` proves equal to `E0 + bias ≥ 0`. The binner's reject rule and the rasterizer's accept rule are the same bytes, which is why they cannot disagree.

**FOUND — submission order is preserved within a tile.** The reference renderer is a painter (plan W3.5/D7, restated at `internal.hpp`'s `raster_tri`: "terrain cells rasterize with depth_test = OFF and depth_write = ON — the painter sort IS the ordering between terrain cells"), so the order in which triangles hit a tile is part of the picture, not an implementation detail. The tile list is therefore **FIFO**: appended at a TAIL pointer and drained head first. A push-front singly-linked list — the cheap one — would reverse every tile's draw order and quietly break the painter's algorithm on exactly the geometry (flat terrain, constant 1/w) that has no depth test to save it. That is why a tile entry carries a tail as well as a head, and it is asserted directly by the directed test.

**FOUND — the drain port's UNITS.** RASTER.EDGEWALK's contract says `job_tile_x_i` is "the tile origin — the top-left PIXEL of the 16×16 tile", not a tile index, so the index is scaled once, here, and the pixel is what leaves the block. *(This was got wrong first: the port emitted the index, a silent 16× error that no differential against a tile-indexed oracle could ever see. It took `zhao_geom_bin_pipe` rendering a real picture in the wrong place to catch it, which is exactly why that composition was built. `geom_binner_directed.cpp:test_pixel_origin` now pins it directly.)*

---

**CHOSEN — the tile grid is anchored at surface pixel (0,0)**, pitch 16, so tile (tx,ty) owns pixels [16tx, 16tx+16) × [16ty, 16ty+16). Nothing states an anchor. This one is chosen because it is the only one under which a tile never straddles a viewport edge in ANY shipping mode: `spec/video_rules.md` §1 gives 384×240 (Z60), 320×240 (Storm) and Duo's two 256×192 view blocks STACKED at rows 0 and 192 (§3.1), and 384, 320, 256, 240 and 192 are all multiples of 16. A grid anchored on the viewport origin would be identical here and would differ the moment a canvas stops being 16-aligned; anchoring on the SURFACE keeps one grid for both Duo views.

**CHOSEN — enumeration order is row-major**, ty ascending outer, tx ascending inner. Nothing states an order and it is observable — it is the order tiles reach RASTER.EDGEWALK. Chosen to match the framebuffer's own row-major top-left-origin layout (video_rules §3) and RASTER.RESOLVE's tile order, so a tile's work and its resolve run in the same direction and a trace reads the same way in both places. Boustrophedon (serpentine) order would halve the worst-case tile-to-tile distance for a future tile cache and is the obvious alternative; it is NOT taken, because no tile cache exists to benefit and the asymmetry would have to be undone later. The DRAIN order is likewise row-major over the whole grid.

**CHOSEN — the trivial-reject is the affine corner test.** Each edge value `E′` is affine in the pixel position — it steps by `kx` per pixel of x and `ky` per pixel of y (GEOM.SETUP) — so its MAXIMUM over the 256 pixel centres of a tile is at a corner, the one selected by the signs of `kx` and `ky`:

```
E′_max = E′(tile top-left centre) + (kx > 0 ? 15·kx : 0) + (ky > 0 ? 15·ky : 0)
```

If `E′_max` fails the §8 fill test for ANY edge, no centre in the tile can pass it and the tile is certainly empty. The test is **SOUND** (it never rejects a tile that has coverage) and **CONSERVATIVE** (it may keep an empty one); the directed and random lanes assert the soundness half against `zref::EdgeWalk` over every tile of the grid, which is the property that matters — a lost tile is a hole in the picture, a kept empty tile is only a wasted 21-cycle edge walk.

A plain bbox-only binner is the alternative and it is what a naive implementation does; on a thin diagonal spanning the screen it hands RASTER.EDGEWALK the entire bounding rectangle of tiles — for a 24×15 grid that is 360 tile jobs where roughly 24 have coverage, i.e. 15× the edge-walk work for the same picture. Three 36-bit adders and three fill comparators buy that back.

**CHOSEN — safe overflow is a WALL, not a scribble, and its edge is named.** See "Overflow" below.

**CHOSEN — the token interface is the minimum surface that honours the ledger.** MEASURE.TOKENS is phase 8, its contract is still a stub, and no packet layout for `token_grant` exists anywhere. The ledger nevertheless lists it upstream with `backpressure: credit`. So: one combinational request/grant pair. `tok_req_o` pulses on the cycle a triangle is accepted and `tok_grant_i` is sampled on that same edge; a denied triangle is dropped whole and counted into `triangles_culled`. Tie `tok_grant_i` high and the guard is absent, which is what every test that is not about tokens does and what the reset state assumes. Deliberately NOT invented here: the Duo 45/45/10 fairness split, any token WIDTH or cost model, and the return path — all of those are MEASURE.TOKENS' law to write.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release. Reset enters the tile-head CLEAR phase, which writes all `TILES` entries to zero over `TILES` cycles with `tri_ready_o` low; `frame_begin_i` re-enters it. Nothing survives a frame boundary: the same pulse releases the whole chunk arena in one cycle and restarts the triangle store, the wall and the overflow flag.

## Input and output packet layouts

**Setup triangle in** (`tri_valid_i` / `tri_ready_o`) — GEOM.SETUP's packet verbatim: nine edge coefficients (`kx`/`ky` signed 23, `kc` signed 48, per edge), `tri_tl_i[2:0]`, the six signed-21 vertices, the four signed-12 scan-box edges, and `tri_src_id_i`.

**Frame control:** `frame_begin_i` (release the arena, clear the tile heads), `frame_end_i` (close the bin phase; the drain starts as soon as the block is idle), and `grid_w_i` / `grid_h_i` — the active grid in TILES. The caller MUST cover the viewport, because a tile index is `ty·GRID_W + tx`; the enumeration is clamped to `[0, grid−1]` so a caller that gets this wrong LOSES tiles rather than aliasing two tiles onto one list.

**Token credit:** `tok_req_o` out, `tok_grant_i` in, both combinational on the accepting cycle.

**Drain out** (`job_valid_o` / `job_ready_i`) — RASTER.EDGEWALK's job port, field for field including units:

| field | width | meaning |
|---|---|---|
| `job_ax_o` … `job_cy_o` | 6 × signed 21 | the triangle, from the frame's triangle store |
| `job_tile_x_o`, `job_tile_y_o` | signed 12 | the tile's **top-left PIXEL** (index × 16) |
| `job_src_id_o` | 16 | the triangle's source id |

`drain_busy_o` is high for the whole drain; `drain_done_o` pulses for one cycle when the last tile of the grid has been visited.

**Status:** `tile_references_o` (u32), `max_tile_list_depth_o` (u16 high-water), `triangles_culled_o` (u32), `overflow_o` (sticky), `arena_full_o` and `arena_used_o` (observability for the tests, and the same two signals the formal harness proves on the allocator itself).

## Backpressure rules

`ready_valid` on both sides. `tri_ready_o` is high only in IDLE and only while no drain is pending; it never depends on `job_ready_i`. `job_valid_o` is a registered flag and never depends on `job_ready_i` (no combinational valid←ready path in either direction). A stalled job holds all nine of its fields stable until accepted — asserted by the driver on every beat of the backpressure lanes. The token pair is combinational by design: it is a credit decision, not a data path.

## Memory ownership

Four on-chip RAMs, all private to this block, all fixed at elaboration:

| RAM | entries | width | contents |
|---|---|---|---|
| `tri_ram` | `TRI_CAP` = 128 | 142 | `{src_id[15:0], cy, cx, by, bx, ay, ax}` — the frame's triangle store |
| `ref_ram` | `CHUNKS·CHUNK_REFS` = 1024 | 7 | one triangle index per entry, so a push is a single write and never a read-modify-write |
| `next_ram` | `CHUNKS` = 256 | 8 | the chunk chain: ONE pointer per CHUNK_REFS references |
| `tile_ram` | `TILES` = 576 | 27 | `{count[10:0], tail[7:0], head[7:0]}` per tile |

That is what "chunked" buys: the pointer overhead is 8/(4·7) ≈ 29 % instead of 8/7 ≈ 114 %. Total = 128·142 + 1024·7 + 256·8 + 576·27 = 42,944 bits ≈ 43 kbit, roughly five M10K.

**No read and write of the same RAM address ever occur in the same cycle.** Binning writes `tile_ram` one cycle after reading it and consecutive tiles of one triangle are always distinct; across triangles at least three cycles separate the last write from the next read. The clear phase only writes; the drain only reads.

## Q formats and rounding

Coordinates are S 12.8 screen subpixels (§8). The block performs **one** non-exact operation, and it is a FLOOR, not a rounding: `E′ = E0 >>> 8`, the §8 decomposition (`zhao_geom_binner.sv::ep_of`). `>>>` on a signed value is the arithmetic shift; the fill rule is stated on `E0 = 256·E′ + r` with `r ∈ [0,255]`, which only holds for the flooring quotient. Truncation toward zero — what a C++ `/` or a logical shift would give — moves every NEGATIVE, non-multiple-of-256 value by exactly one unit. That difference is one E′-unit wide (256 subpixel²) and is pinned by a constructed directed witness and a constructed random population; see "Randomized differential tests".

`E0` at the centre of pixel (0,0) is `kx·128 + ky·128 + kc`, computed once per edge per triangle in the 48-bit setup domain. Because both edge steps are multiples of 256, that value's LOW BYTE is the same `r` at every pixel centre of the screen — one constant bit per edge (`rnz`), exactly RASTER.EDGEWALK's own.

**Why `ACC_W` is 36.** With |v| ≤ 2¹⁹, GEOM.SETUP's constants obey |kx|,|ky| ≤ 2²⁰ and |kc| ≤ 2³⁹. Then `E0` at pixel (0,0) is < 2⁴⁰ so `E′_base` ≤ 2³²; the walk adds `kx·px + ky·py` over the grid with |px|,|py| < 2⁹, each < 2²⁹; and the corner offset is `15·(|kx| + |ky|)` < 2²⁵. The largest value the accumulator ever carries is below 2³² + 2²⁹ + 2²⁹ + 2²⁵ < 2³³, and 36 bits signed (±2³⁵) holds it with two bits to spare. **No saturation is used**: unlike RASTER.EDGEWALK, which narrows to a tile-local domain, this block evaluates one exact value per tile and needs the sign of the true number.

## Latency (fixed or variable)

Variable, bounded. Per triangle: 1 accept cycle + 2 setup cycles, then one cycle per REJECTED candidate tile and two per KEPT one (evaluate + push). Per frame: `TILES` = 576 clear cycles at `frame_begin_i`, and on the drain two cycles for every tile of the active grid plus four per emitted job.

## Target throughput

**The ledger asks "1 bin reference per clock". THIS BLOCK DOES NOT MEET IT.**

MEASURED by `tests/geometry/geom_binner_directed.cpp:test_throughput`, on a full-canvas triangle over a 24×15 grid:

```
bin:   561 cycles for 198 references = 2.83 cycles per emitted reference   (target: 1)
drain: 1,946 cycles for 198 jobs over 360 tiles = 9.83 cycles per job
```

A 2.83× shortfall on the binning side. Closing it would mean pipelining the `tile_ram` read-modify-write behind a same-address forwarding path — consecutive tiles of one triangle are always distinct, so only the triangle boundary needs the forward. **That is not built**, and the number is stated here rather than left to be discovered.

The drain figure is not a per-job cost at all: it is structural, two cycles for EVERY tile of the grid (the head read, whether or not the list is empty) plus four per emitted job. It is not a bottleneck by construction — RASTER.EDGEWALK spends 21…37 cycles on the job it is handed, so behind it the drain is idle roughly 80 % of the time, and the whole-grid scan is 720 cycles out of a 251,520-cycle Z60 frame (0.3 %). The directed test holds the block to the structural bound `2·tiles + 6·jobs + 64` so a regression cannot quietly widen it.

## Overflow and malformed-input behaviour

**Safe overflow is a WALL, not a scribble.** Two arenas can run out: the triangle store (`TRI_CAP` = 128 triangles per frame) and the chunk arena (`CHUNKS` = 256 chunks, 1024 references). On either:

- **nothing outside the arena is ever written** — `zhao_geom_arena` never presents a grant when full, and that is the formal property below;
- the current triangle is abandoned and `overflow_o` LATCHES;
- every subsequent triangle of the frame is dropped whole and counted into `triangles_culled` — the WALL.

Because submission order is painter order, walling off the TAIL of the frame is exactly the "degrade to next-frame" the ledger asks for: what is lost is the work the next frame would carry anyway, not a random half of the scene.

**ONE triangle per frame can be PARTIALLY binned** — the one that hits the wall mid-enumeration, which appears in a prefix of its tiles. That is stated rather than hidden: making it atomic would need either a two-pass count (doubling the enumeration cost of every triangle, for a case that should never fire) or a rollback journal.

**NOT BUILT, and named so the next wave knows:** the re-submission half of "degrade to next-frame". Nothing here remembers a dropped triangle or hands it to the following frame; that is a frame-scheduler behaviour (CMD.SCHEDULER / MEASURE.TOKENS) and this block only reports, through `overflow_o` and `triangles_culled_o`, that it happened.

Other malformed inputs: a denied token drops the triangle whole and counts it (not an overflow); an undersized `grid_w_i`/`grid_h_i` loses tiles at the clamp rather than aliasing lists; a triangle whose scan box is empty enumerates nothing; a degenerate triangle produces coefficients the corner test rejects everywhere. There is no input that can make the block hang, scribble or emit a job for a tile outside the grid.

## Counters and traces

`tile_references_o` (u32), `triangles_culled_o` (u32) and `max_tile_list_depth_o` (u16), all saturating, never wrapping (`spec/counters.md` §4). They are **cumulative**, not per frame: counters.md §3 samples them through the `frame_tick` shadow protocol, and this block does not reset them at a frame boundary. `max_tile_list_depth` is a high-water mark per counters.md §4, latching the maximum list length ever observed; the read-clear re-arm described there belongs to DEBUG.COUNTERS, which this block is not wired to (no snapshot channel here — the same stance RASTER.EDGEWALK's contract records for `covered_fragments`). The test driver reports deltas across a frame and treats the high-water as global, which is what the hardware actually does.

Trace: the drained job stream itself — every tile, in order, with its triangle and source id — is what the differential lanes compare.

## Scalar reference function

`zref::Binner` (`reference/include/zref/zref_geom.hpp`, `reference/src/zrender/geom.cpp`) — the ledger's declared `reference_model`. It enumerates the tiles of a scan box row-major and applies the same affine corner test, reaching the fill predicate through `zref::fill_accept`, the one-line C++ transcription of `zhao_raster_fill.sv` that the formal lane proves.

The binning law is the block's own CHOICE, so oracle and RTL are two implementations of it — the same situation the TEXTURE.TMU bilinear increment recorded and for the same reason. What keeps that honest is that the property which MATTERS is checked against a different oracle entirely: soundness against `zref::EdgeWalk` (the §8 coverage law) over every tile of the grid, in both the directed and the random lane. A shared mistake in `zref::Binner` and the RTL would still be caught there.

## Directed tests

`tests/geometry/geom_binner_directed.cpp` (driver `tests/geometry/geom_dev.hpp`) — **818 checks**. Every case drives a whole frame (`frame_begin` → triangles → `frame_end` → drain) and diffs the drained job stream against the expectation built from `zref::Binner`; on top of that:

- **many tiles** — one triangle spanning 40+ tiles: tile set and ORDER equal the oracle's; no job outside the scan box; and the corner test demonstrably rejects part of the bbox rectangle;
- **SOUNDNESS** — every tile of the grid with non-zero `zref::EdgeWalk` coverage IS in the list, checked over the whole grid rather than over the oracle's answer;
- **pixel origin** — the drain port carries the tile's top-left PIXEL (the joint with RASTER.EDGEWALK's units), and `EdgeWalk` covers the tile the port names;
- **the flooring boundary** — a CONSTRUCTED witness. For the right triangle (0,0),(W,0),(0,W) the hypotenuse has `kx` and `ky` both negative, so the corner maximising it over tile (0,0) is pixel (0,0) itself and the value there is exactly the number `ep_of` rounds: `e0_base = W·(W − 256)`. W = 255 gives −255, where FLOOR rejects (correctly — the centre 128+128 = 256 > 255 is outside) and truncation toward zero would accept. W = 254 and W = 256 are the controls either side. This is the case a sampled lane cannot reach; see the mutation evidence;
- **one tile** — a triangle inside a single tile: exactly one job, at the right tile, with `max_tile_list_depth` = 1;
- **order** — the drain is row-major over the grid and FIFO within a tile (the painter order);
- **chunk boundary** — a tile receiving 1…9 triangles, crossing the 4-reference chunk boundary twice;
- **tokens** — a denial drops the triangle whole, counts it, is not an overflow, and no job carries it;
- **triangle-store overflow** — the 129th triangle of a frame walls the frame off, and the wall cuts the TAIL (the highest surviving source id is exactly 127);
- **arena overflow** — 16 canvas-half triangles exhaust the 1024-reference arena: `overflow_o` latches, `arena_used ≤ CHUNKS` (never scribbles), every counted reference is still drained, and the wall cut the tail;
- **frames** — a repeated frame drains identically and an empty frame after a full one drains nothing;
- **backpressure** — four PCG stall patterns, identical jobs, held stable;
- **throughput** — measured and printed, and held to the structural bound.

## Randomized differential tests

`tests/geometry/geom_binner_random.cpp` — deterministic from fixed seeds.

**Lane A — tile-list differential.** PCG scenes of 1…12 triangles across four populations (canvas-local, tile-sized, thin diagonals across the canvas, tile-straddling), each pushed through the `zref::Clip` / `zref::Setup` oracles first so the block sees exactly what the real chain emits, with occasional token denials and a third of the scenes `job_ready_i`-gated. The whole drained job stream — every tile, in order, with the right triangle and source id — must equal the expectation from `zref::Binner`, and `tile_references` must equal the drained job count.

**Lane B — soundness against the coverage oracle.** For a single PCG triangle, EVERY tile of the grid is walked with `zref::EdgeWalk` and every tile with non-zero coverage must appear in the drained list. This is checked against the FILL LAW, not against the binner's own oracle, so a shared mistake in both would still be caught.

**Lane C — the rounding boundary, reached by construction because it cannot be reached by sampling.** Divisor pairs of `16384 − k` place `e0_base` on any chosen value in the window where floor and truncation disagree; `k` outside [1,255] gives the controls either side. The lane asserts it actually reached the window, so the day the population stops covering it the lane says so rather than going quietly green.

Default 400 scenes / 600 triangles / 500 boundary cases (CTest `fast`); `--nightly` 6,000 / 8,000 / 4,000. Failing vectors serialized per charter §29-17.

**Mutation evidence (2026-08-18).** Four deliberate RTL defects, each injected on its own invocation and each proved to have relinked by hashing all seven geometry test binaries (SHA-256) after deleting the generated `Vzhao_geom_*.cpp/.h` — never by touching mtimes forward, which a previous increment used to poison three sweeps:

| mutation | caught by |
|---|---|
| enumeration misses the last COLUMN | directed AND random AND `geom_bin_pipe_directed` |
| enumeration misses the last ROW | directed AND random AND `geom_bin_pipe_directed` |
| a triangle assigned to a tile it does not touch (corner test disabled) | directed AND random AND `geom_bin_pipe_directed` |
| the §8 decomposition truncates toward zero instead of flooring | directed AND random |

**The fourth found a real hole and it was fixed rather than argued away.** On its first run NEITHER binner lane went red: floor and truncation differ by one E′-unit and only for a negative, non-multiple-of-256 value, so a tile's verdict changes only when its extreme corner sits within ONE unit of an edge — 256 subpixel². The random populations step past that window `16·|k|` at a time; a 20,000-triangle scan measured **zero** hits. The directed witness and random lane C above were added to reach it by construction, and both lanes then went red. That is recorded here because "the mutation was not caught" is the finding, and the fix is the test, not the RTL.

## Formal properties

`tests/formal/geom_binner_arena_bounds.sby` + `tests/formal/geom_binner_arena_bounds_fv.sv` — **PASS (prove + cover)**, both tasks, `smtbmc boolector`. The DUT is `zhao_geom_arena`, the exact module `zhao_geom_binner` instantiates.

The ledger's note is "Safe overflow: excess triangles degrade to next-frame, NEVER SCRIBBLE", and "never scribble" reduces to one arithmetic fact: the allocator must never present a grant for an index at or past the end of the arena, however many requests arrive, in any order, with any interleaving of frame releases.

- `a_bound` — `used ≤ CHUNKS`, always.
- `a_in_range` — a GRANT always names a chunk strictly inside the arena. This is the "never scribble" statement itself.
- `a_wall` — full ⇒ no grant, however hard the allocator is asked. This is what makes the frame wall total.
- `a_monotone` + `a_step_one` — between releases `used` never decreases and a grant advances it by EXACTLY one, so two grants never name the same chunk.
- `a_release` — a release empties the arena in one cycle; no residue survives a frame boundary.

The task is `prove` (temporal induction), not `bmc`: every assertion is one-step inductive, so induction closes it for ALL time — which matters here in a way it did not for the fill rule, because the interesting state (a FULL arena) is 256 grants from reset and no bounded run of a sane depth reaches it. The cover task carries a deliberately tiny second instance (CHUNKS = 4) in the same harness and shows a grant (step 2), the arena filling (step 6), a full arena refusing a request, and a release re-arming it — all at real step counts. Without it, `a_bound` and `a_wall` would also hold for an allocator that never granted anything.

Two harness defects were found and fixed while writing it, and are recorded in the file: the history registers needed a reset (a free initial `used_q` gave a false base-case counterexample at step 1), and the initial state needed pinning to reset (an async reset only bites while `rst_n` is low, and the cover task "reached" a full four-chunk arena in one step — arithmetically impossible).

**The proof was itself mutation-checked:** removing the `!full_o` term from `alloc_ok_o` makes `a_in_range` and `a_wall` both fail.

**Not proved formally, and why:** the binner's overflow POLICY (which triangle is dropped, when the wall goes up, that at most one triangle per frame is partially binned) is behaviour over a 12-state FSM with four RAMs, out of reach for this engine; it is covered by the directed lane's two overflow cases and by the differentials. Nor is the corner test, the enumeration order or the tile indexing proved: those are 36-bit affine arithmetic over a 576-entry tile RAM, covered by the soundness sweeps against `zref::EdgeWalk`.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers. The shape is four block RAMs (42,944 bits ≈ 43 kbit total, roughly five M10K), six small signed multipliers (23 × 10, for `E′` at the first tile of a range), three 36-bit adders and three `zhao_raster_fill` comparators for the corner test, three 48-bit adders for the `E0` base, one `zhao_geom_arena` (a 9-bit counter and a compare), and roughly 400 flops of FSM and cursor state. The RAMs are the cost. `TRI_CAP`, `CHUNKS`, `CHUNK_REFS` and the grid are elaboration parameters, so a fit that needs the memory back trades frame capacity for it — and the overflow wall is exactly what makes that trade SAFE rather than a corruption.

## Integration capture cases

None on hardware. The block is not in `ZHAO_SHELL_RTL`, not in `fpga/files.qip`, has no capture and has never run on a device.

**Composed, in simulation only (2026-08-18).** `fpga/rtl/geometry/zhao_geom_bin_pipe.sv` wires this block's drain port straight into `zhao_raster_tile_pipe` — RASTER.EDGEWALK → RASTER.EARLYZ → RASTER.FRAGMENT → RASTER.TILESTORE → RASTER.RESOLVE — so a setup triangle goes in at one end, the chunked tile list decides which tiles are walked, and RGB565 framebuffer words come out at the other. `tests/geometry/geom_bin_pipe_directed.cpp` asserts that the rasterized tile set IS the binner's list in order, that every tile's coverage count equals `zref::EdgeWalk`'s, that every covered tile of the grid reached the rasterizer, and that the resolved picture matches `zref::TileResolve` **pixel for pixel including the known, escalated `resolve.cpp` defect** (pure black resolving to 0x0020 in 8 of 16 Bayer cells, RASTER.RESOLVE.md "the BLACK rail is not clean"). The oracle is the law; the defect rides through and the test pins the actual behaviour, not the desired one.

That composition is **not a ledger block** — the GEOMETRY group has no entry for it, exactly as the RASTER group has none for `zhao_raster_tile_pipe`, and registering one is a validator-gated ledger edit. Its rationale and the two laws it owns live in its own header.

**RESTRICTION, recorded rather than hidden:** `zhao_raster_tile_pipe` is one clear + one triangle + one resolve per job (its own header says so), so a tile appearing TWICE in the drain is cleared and resolved twice and the second resolve overwrites the first. The composition is therefore exercised on scenes where every tile receives at most one triangle — one triangle across many tiles is the natural such scene and the interesting one. Lifting it would mean giving `zhao_raster_tile_pipe` a "continue this tile" job flavour, i.e. changing a finished block's interface, which this increment does not do.

Simulated is not synthesized and neither is on-hardware.

## Notes

Safe overflow: excess triangles degrade to next-frame, never scribble.

Deliberately not built, so the next wave knows: the re-submission half of "degrade to next-frame"; any per-tile depth or ordering hint (the coarse depth bins are RASTER.EARLYZ's); any multi-frame arena or free list (a tile list lives for exactly one frame); boustrophedon enumeration; the pipelined `tile_ram` forwarding path that would close the 2.83× throughput shortfall; the MEASURE.TOKENS packet layout, cost model or return path; and any counter-catalog wiring.
