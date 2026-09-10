# Task Log: RUN-20260910-0846 - projection adoption (terrain shell on zhao_vertex_arena)

**Created:** 2026-09-10 08:46 UTC+02:00
**Status:** Complete (working tree, uncommitted)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0846-projection-adoption/

---

## Objective

Cash the shared-projector cheque as far as one packet honestly reaches: terrain shell + level-0 replay + service client-B wiring + bit-identity differential against `zhao_terrain_project`. No fit, no commit. Report what remains.

---

## Progress Timeline

### 2026-09-10 08:46 - Task Started

- Read, in order: VERTEX-ARENA-DENSE-SEAL, PROJ-ARENA3-ARCHITECTURE s7, ARENA-I-BUILT-A-SECOND-ONE, WCACHE-DROPS-W; service, core, arena, wcache, terrain_project RTL; the tessellator RTL and zref oracle; CLAUDE.md.
- Other live lane: `fpga/rtl/texture/` (4 modified files + new tests in the working tree). Not touched.

### 2026-09-10 09:10 - Findings before writing any RTL

1. `zhao_terrain_project` is instantiated by NO RTL except the generated `zhao_prod_top` harness. There is no terrain pipeline composition (tess -> normals -> project) anywhere in fpga/rtl. "Wire terrain as a client" therefore has no live datapath to enter; the composition is the testbench for now.
2. The tessellator emits WORLD COORDINATES, not lattice indices, and knows the topology (annulus for stitched jobs, run-cells for coarse). A "static subpatch topology table" exists only for the level-0 unstitched case (128 triangles, arithmetic, no ROM). Stitched/coarse topologies are job-dependent and must come from the tessellator.
3. The 81-vertex identity space: every corner the tess emits is a lattice vertex inside the 9x9 window (inner_v/outer_v/proj_t return lattice (vi,vj)), and a vertex's final position is a pure function of (vi, vj, job). So 81 per JOB LIFETIME covers stitch + morph + underside. To be MEASURED by a probe on the zref oracle, not argued.
4. DENSE_SEAL at fixed DEPTH=81 forces 81 fills per subpatch regardless of level. Levels 1/2/3 use 25/9/4 unique vertices. Budget-neutral vs the roadmap's all-level-0 stress; a regression at level >= 2 against the legacy corner path.
5. The core samples the MATRIX at accept but the VIEWPORT at stage 5b (~34 cycles later). A vertex's "config epoch" is therefore not one cycle. Pre-existing; matters for the mid-frame reconfiguration case.

---

## Files Created

(updated below as they land)

---

## Decisions Made

- Three read copies of the primitive with broadcast fill (the roadmap 8.4 layout); single-copy 3-clock replay named as the unbuilt alternative.
- Output backpressure via a credit-gated 2-deep skid (the primitive's reply register has no enable and its counters count every presented lookup; re-issuing under stall would double-count).
- Refused/missed corners are ZEROED in the shell and flagged per triangle (`out_refused_o`, `out_missed_o`); never another group's vertex.
- The fill producer in the differential is the zref tess oracle's `vertex_at` (proven == TESS RTL by terrain_tess_directed); the hardware vertex producer is remaining work.

---

## Next Steps

- write RTL, lint, quartus17 gate, build tb standalone, run differential at RPP=3, RPP=1, VALID_MODE=0

### 2026-09-10 09:40 - RTL written, linted, differential green in three elaborations

- `fpga/rtl/terrain/zhao_terrain_wcache.sv` (shell: 3 arena copies, broadcast fill, 106-bit record, credit-gated 2-deep skid, corner/triangle counters), `fpga/rtl/terrain/zhao_terrain_topo.sv` (level-0 walker, three adders), `fpga/rtl/common/zhao_proj_subsystem.sv` (service + shell + walker composed in RTL -- the fit gate's top).
- `tests/terrain/tb_terrain_wcache.sv` = subsystem + retained `zhao_terrain_project`; `tests/terrain/terrain_wcache_differential.cpp`; `tests/terrain/terrain_identity_probe.cpp`.
- Lint -Wall: 0 diagnostics on shell, walker, subsystem, bench. Quartus-17 syntax gate clean (226 files). no_control_bytes clean.
- identity probe: 110,592 jobs / 7,529,760 corners on the zref tessellator: 0 off-lattice, 0 outside the 9x9 window, 0 position conflicts; distinct max 81/25/9/4 per level. 10/10.
- differential RPP=3: 18,131 checks passed; RPP=1: 9,871; VALID_MODE=0: 18,134. 8,192 triangles per run bit-identical to the legacy projector AND project_vertex (incl. w). 1,152 behind corners, 36 rail corners reached. contended_o fired (3,515). Positive control fired 38/128. 128 triangles in 131 clocks.
- HARNESS BUG found and fixed on the first run: `wait_fills(n)` counted fills landing AFTER the last vertex was presented; with a 36-cycle core ~60 of 81 land during presentation. Made cumulative (sent vs landed). The 71 "failures" were all this line; every substantive check had passed. Recorded in SPEC Don't Retry.
- Registered: tests/CMakeLists.txt (3 differential elaborations, probe, lint), prod_manifest.yml (3 x not-yet-adopted), fit_targets.yml (zhao_proj_subsystem gate with max_dsp 33; zhao_terrain_wcache leaf). check_prod_manifest OK (221 modules); gen_prod_top --check fresh.

## Findings against the brief (candidate fourteenth claims)

1. "81 unique vertices per subpatch" is true at level 0 only; levels 1/2/3 use 25/9/4 (MEASURED). The IDENTITY space is covered (probe); the dense FILL COST is 81 at every level -- at level >= 2 MORE projections than the legacy corner path (81 vs 24 / 6).
2. "A static subpatch topology table" exists only for level-0 unstitched (and is three adders, not a table). Stitched/coarse topologies are job-dependent and belong to the tessellator.
3. "~6,100 ALM" is the GROSS cost of the retired core, not a net saving: the shells that make retirement legal (3 arena copies + skid + walker + the 2x1089 geom wcache with its 2,178-flop bitmap) cost ALM nobody has measured. Net is UNKNOWN until the gate.
4. ROWS_PER_PASS=1 is bit-identical (measured) but its budget argument (71.8%) is single-view whole-patch dedup; two-view subpatch demand at II=3 is ~2.7M of 1.67M clocks -- does not fit.
5. The 12,267 ALM baseline rests on fit rows uncashed_cheques flags as stale (files moved 17-21 d after the fit; terrain_project's row from a DIRTY TREE).

### 2026-09-10 10:05 - Report written; CMake registration proven to configure

- `reports/PROJECTION-ADOPTION-20260910.md` written: re-derived parameters (§1), the 81-vertex identity measurement (§2), the per-level dense-fill cost the brief did not ask about (§3), M10K arithmetic incl. the core's own 23-29 M10K (§4), differential evidence (§5), counters (§6), the remaining list (§7), the ONE fit gate and its question (§8), not-verified ledger (§9), brief errors (§10).
- Walker elaboration guard SEEN TO FIRE: `--binary -GINDEX_W=7` -> $fatal at time 0, rc 1.
- `cmake --preset windows-native -B build-projadopt/cmake` (lane-local, shared build/ untouched): CONFIGURE_RC=0 in 258 s. Build of the four new targets running in the background at the time of writing; result goes into the report's addendum.
- uncashed_cheques after registration: the service is no longer rootless (the subsystem instantiates it); `zhao_proj_subsystem` is PENDING. The cheque moved up one level. Said so in the report and the manifest.

## Files Created

- fpga/rtl/terrain/zhao_terrain_wcache.sv, fpga/rtl/terrain/zhao_terrain_topo.sv, fpga/rtl/common/zhao_proj_subsystem.sv
- tests/terrain/tb_terrain_wcache.sv, tests/terrain/terrain_wcache_differential.cpp, tests/terrain/terrain_identity_probe.cpp
- reports/PROJECTION-ADOPTION-20260910.md
- modified: tests/CMakeLists.txt, design/prod_manifest.yml, design/fit_targets.yml

## Status

Working tree left for owner review. NOT committed. NO fit run. Other lane (fpga/rtl/texture/) untouched.

### 2026-09-10 10:20 - CMake registration verified end to end

- lane-local tree: configure RC 0 (258 s), build of the four new targets RC 0, `ctest -R` 4/4 passed with the standalone check counts (18,131 / 9,871 / 18,134 / 10). Report §9 row moved to verified. Shared build/ untouched. Done.
