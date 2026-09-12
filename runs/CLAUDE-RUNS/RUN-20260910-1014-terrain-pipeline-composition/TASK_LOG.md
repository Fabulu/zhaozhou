# Task Log: RUN-20260910-1014 - Compose terrain projection pipeline

**Created:** 2026-09-10 10:14 UTC+02:00
**Status:** Complete (independently verified by RUN-20260912-1841)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-1014-terrain-pipeline-composition/

---

## Objective

Compose real TESS ModeVtx/ModeRef terrain jobs through one shared projection service, selected-view arena groups, and the projected terrain shell; independently verify lifecycle and retained behavior without adopting or claiming fitted savings.

---

## Progress Timeline

### 2026-09-10 10:14 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260910-1014
- Created working directory
- Initial context: projection service and arena shell existed, but real view/group sequencing and downstream-rate evidence were missing.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 10:14 — read phase, first findings (before any RTL)
* Brief item 4 asks "can GEOM.CLIP take 1 tri/clk". Contract + RTL: pipe_en = !(s3_v && s3_accept && !out_ready_i) — YES, 1/clk, 3 stages one enable. GEOM.SETUP same. **GEOM.BINNER does NOT**: contract says 2.83 clk per tile reference, tri_ready only in IDLE, TRI_CAP=128 triangles/frame. The downstream wall is the binner, not CLIP. To MEASURE, not cite.
* Brief says "the arena's reference counter exists". zhao_terrain_wcache header: "It does not reference-count groups"; zhao_terrain_topo: "HOLD, NOT A REFERENCE COUNT". The counter was the zhao_proj_arena3 design study's and was deliberately not carried. Candidate 17th claim.
* Tess mode 1 is view-independent (prev report §5) -> fan one vertex out to V fills, never re-tessellate per view.
* Normals: legacy chain is tess(mode0)->normals->project. Arena path has no world-triangle stream. FORK — check TERRAIN.NORMALMAP before deciding.
Next: read tess end-of-job/reject timing, LOD job port, harness, then design zhao_terrain_group_seq.

## 10:40 — ITEM 4 MEASURED (before building anything)
* tb_terrain_downstream (GEOM.CLIP ATTRS=1 -> GEOM.SETUP, no adapter), terrain_downstream_rate.cpp:
  4096 accepted triangles in 4102 clocks, consumer always ready = **1.0015 clk/tri, fixed fill 6**.
  1536 of 4096 rejected (near/offscreen/degenerate): still 4102 clocks -> rejects cost nothing.
  30% consumer stall: 4096/4096 out, in order. 17 checks.
* terrain_downstream_binner.cpp (BinnerDev, 128 ONE-TILE terrain triangles): **5.00 clk per triangle**,
  TRI_CAP = 128 triangles/frame. 133 checks.
* VERDICT: CLIP and SETUP consume 1 tri/clk (the brief's named block CAN take it). GEOM.BINNER cannot:
  5 clk/tri and 128 tri/frame. The binner is the wall, not CLIP. Replica decision: keep 3 copies —
  frame demand (2x256x16x128 = 1,048,576 tri >= 0.63 tri/clk) exceeds one copy's 0.33; CLIP/SETUP
  consume the rate; the 12 M10K delta is ~2% of the device. State plainly that nothing past SETUP
  consumes it today.
Next: subsystem refactor (walker out), zhao_terrain_group_seq, zhao_terrain_pipe.

## 2026-09-12 — continuation closeout handoff

### Boundary completed

- `zhao_proj_subsystem` is now service + arena shell with generic fill/reference ports. The embedded `zhao_terrain_topo` job/walker interface is gone; the retained walker exists only in `tb_terrain_wcache.sv` as a legacy differential source.
- `zhao_terrain_group_seq` now owns per-view arena allocation, ModeVtx fan-out, accepted-fill/landed-fill accounting, dense or bitmap sealing, ModeRef selected-view fan-out, and release.
- `zhao_terrain_pipe` composes the real `zhao_terrain_tess` ModeVtx/ModeRef producer, group sequencer, shared projector service, and three-copy terrain arena shell. Client A remains the geometry pass-through.
- A committed, renamed `zhao_terrain_group_seq_mutant` releases on reference presentation rather than completion. It is reachable only through the explicit test closure and gives `release_unsafe_o` an inverted-polarity positive control.
- The new composed differential compares both views against retained `zhao_terrain_project`, and checks `w` against `zref::render::project_vertex`. It covers masks 01/10/11, LOD 0..3, morph/clamp, stitching, void rules, top/underside winding, no-view/empty jobs, dense and bitmap sparse fill, service contention, geometry client A, stalls, exact riders/material/order, and new-job/open while an older copied output is stalled.

### Defects found and repaired

1. Landed-fill accounting truncated `fill_arena_i` to legal index width. Invalid high/refusal encodings could alias a real arena. The sequencer now compares the full `ARENA_W` encoding against every legal arena before incrementing.
2. The inherited release expression released successful groups at ModeVtx completion and again after ModeRef replay. The first dense differential expected 12 arena releases and observed 21, exactly one extra release for each successful replaying arena. Successful fills now stay held through seal and replay; fill-stage release is only for rejected/empty groups.
3. The first release detector depended only on pending reference presentation and could not see successful-fill early release. It now independently flags either a successful ModeVtx pass that still requires replay or a ModeRef triple that has not completed all selected-view handshakes.
4. The recovered binner rate probe omitted `ZHAO_GEOM_DEV_BINNER`, so its harness types were compiled out. The define is now present immediately before `geom_dev.hpp`.

### Newly verified receipts on the resumed working tree

- Fresh lane-local configure: `CONFIGURE_RC=0` in `build-terrain-pipe-20260912`.
- Production composition lint: `LINT_RC=0`; mutant closure lint: `MUTANT_LINT_RC=0`.
- First dense run deliberately caught the premature release: build `DENSE_BUILD_RC=0`, test `DENSE_TEST_RC=1`, expected 12 releases / observed 21.
- After repair: `DENSE_REBUILD_RC=0`, `DENSE_RETEST_RC=0`; 478 terrain packets + 200 geometry vertices, 2,343 cycles, 36 checks.
- Bitmap composition: `ALT_BUILD_RC=0`, `BITMAP_TEST_RC=0`; 478 packets + 200 geometry vertices, 2,228 cycles, 32 checks.
- Release mutant: `MUTANT_TEST_RC=0`; 3 inverted-polarity checks, with the detector observed nonzero.
- Retained shell differentials: dense `WCACHE_DENSE_RC=0` (18,131 checks), RPP1 `WCACHE_RPP1_RC=0` (9,871), bitmap `WCACHE_BITMAP_RC=0` (18,134); 8,192 triangles each.
- Recovered downstream probes rebuilt with `RELATED_REBUILD_RC=0`.
- CLIP->SETUP: `DOWNSTREAM_RATE_RC=0`, 17 checks; 4,096 triangles in 4,102 clocks (1.0015 clocks/triangle), the same offered duration with 1,536 rejects, and all 4,096 outputs preserved in order under 30% stalls (5,860 clocks).
- BINNER: `DOWNSTREAM_BINNER_RC=0`, 133 checks; 128 one-tile triangles in 640 bin clocks = exactly 5.00 clocks/triangle; drain 1,666 clocks; `TRI_CAP=128`.
- Four selected registered lints only (no broad CTest): `RELEVANT_LINT_CTEST_RC=0`, 4/4 passed.
- Static Quartus-17 source scan only (no Quartus executable): `QUARTUS17_STATIC_SCAN_RC=0`; self-test 3 fire / 6 no-fire, 228 RTL files scanned, no rejected forms.
- Manifest checker: the first invocation from the wrong cwd failed before reading content (`FileNotFoundError`, both RC=1); rerun from the repo root passed: `CHECK_PROD_MANIFEST_RC=0`, 223 modules = 66 tops + 78 inside + 79 excluded.
- Generated-top check: `GEN_PROD_TOP_CHECK_RC=0`; `zhao_prod_top.sv` is fresh with 66 instances, so it was not regenerated.

### Evidence inherited, not newly claimed

- The arena primitive's SymbiYosys tasks and earlier core MATW/row-mux proofs were not rerun because those source files were not modified.
- No Quartus map/fit, entity census, RAM-inference measurement, ALM/M10K/Fmax result, or production closure measurement was produced.

### Claims corrected or refused

- Refused the inherited claim that an arena reference counter already existed. It does not; the release detector is independent instrumentation, and the mutant proves it can fire.
- Corrected `ARENAS=4` versus physical replicas: four is lifetime/scheduling capacity; three copies provide simultaneous corner reads.
- Refused RPP1 as a dense two-view throughput/resource result. It is correctness evidence only; at dense two-view load it cannot meet the producer schedule.
- Refused the campaign's `99 DSP / gap 5` headline because it depends on that illegal RPP1 point. The legal RPP3/MATW18 one-core figure is structural until fit and is not a savings claim.
- Refused any ALM, DSP, M10K, ceiling, or net-savings claim without a clean `zhao_terrain_pipe` composed fit.
- Corrected the old subsystem ledger text: the walker is not in the leaf closure, and `zhao_terrain_pipe` is the actual subsystem fit boundary.
- Downstream conclusion is bounded: CLIP and SETUP can accept the arena's one-triangle-per-clock output; no current block beyond SETUP does. BINNER measures 5 clocks/one-tile triangle and caps a frame at 128 triangles.

### Registration and adoption state

- `tests/CMakeLists.txt` registers dense, bitmap and release-mutant composition tests/lints plus the recovered downstream probes.
- `design/fit_targets.yml` retains the generic subsystem as a diagnostic leaf, removes `zhao_terrain_topo` from it, and registers `zhao_terrain_pipe` as the future composed gate with its exact single-core/RAM/ALM-DSP-M10K/100-MHz questions.
- `design/prod_manifest.yml` lists subsystem, sequencer, pipe and arena shell as excluded/not-yet-adopted; the old walker is superseded legacy/test support.
- Production remains unchanged. No production adoption was flipped, no generated top changed, no Quartus process ran, and no commit or push was made.

### Exact remaining work

1. Design and verify the NORMALS seam. ModeVtx/ModeRef composition has no world-coordinate triangle stream for the existing face-normal producer; do not pretend the current pipe supplies normals.
2. Route all three emitted `w` values through the downstream cache/setup record and into `zhao_geom_depthquant`; this DEPTHQUANT seam is not composed here.
3. Run one clean subsystem-boundary `zhao_terrain_pipe` fit at legal `ROWS_PER_PASS=3` (including the accepted MATW18 profile as applicable), inspect that exactly one core exists, all three arena replicas infer RAM, record total ALM/DSP/M10K, and inspect arena-read/projector timing at 100 MHz. Do not use RPP1 as the fit headline.
4. Only after those seams and fit pass, atomically adopt the pipe and retire the duplicated production projector accounting, regenerate `zhao_prod_top.sv`, and rerun manifest/census gates.
5. Independent reviewer should inspect every packet diff/untracked source and rerun the bounded direct tests before any commit.

### Independent disposition (RUN-20260912-1841)

- Review completed. It found that the release detector was still blind to release after a non-final triple's last selected-view handshake; the obligation now covers the complete ModeRef job until TESS reports done.
- The single-view committed mutant proves that widened detector: its current triple can be fully accepted in the release cycle, but later triples remain and `release_unsafe_o` must fire.
- The differential now verifies complete held-output stability on every stalled cycle and requires arena reopening while that older copied packet is stalled.
- Final receipts: dense 37 checks, bitmap/sparse 33 checks, mutant 3 checks; registered composition/lint 6/6; retained shell/downstream 5/5; static syntax, manifest, generated-top freshness, and whitespace gates pass.
- `reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md` records the result and explicit non-adoption. No Quartus executable ran.
