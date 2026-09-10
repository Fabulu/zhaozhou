# Task Log: RUN-20260910-0934 - TERRAIN.TESS vertex mode + index-triple mode

**Created:** 2026-09-10 09:34 UTC+02:00
**Status:** Complete (awaiting owner review; nothing committed)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0934-terrain-tess-vertex-mode/

---

## Objective

PROJECTION-ADOPTION §7 items 1 and 2 as a TESS change: `job_mode_i` (0 triangles / 1 vertices / 2 references), a differential over the identity probe's case space, no fit, no commit. Deliverable `reports/TERRAIN-TESS-VERTEX-MODE-20260910.md`.

---

## Progress Timeline

### 2026-09-10 09:34 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260910-0934
- Created working directory
- Initial context: tree clean on zixxtrixx-v8-closeout, no fit running, no other lane.
- Read: PROJECTION-ADOPTION-20260910.md, zhao_terrain_tess.sv (all 919 lines), TERRAIN.TESS.md, zhao_terrain_wcache.sv header+ports, zhao_proj_subsystem.sv ports, TERRAIN_31MHZ_REARCHITECTURE.txt §4/§6/§11/§16/§17, zref_terrain_tess.hpp, tess_harness.hpp, terrain_identity_probe.cpp, terrain_wcache_differential.cpp (producer), zhao_terrain_topo.sv, both tess instantiations (pair wrapper, prod top).

### 2026-09-10 09:50 - Design settled (see SPEC_v1)

- Per-job `job_mode_i`, not an elaboration parameter: the composed flow presents the SAME job twice (fill, then references) and a tied-constant instance is the parameter case for free.
- Vertex output is a 2-deep credit-gated skid (the shell's own rule); a 1-read vertex can land the cycle after the previous one, which the ModeTri output register survives only because a triangle has >= 3 reads.
- Off-grid vertices at level >= 1 = plain lattice vertex, flagged (law 6). REASON, and where the brief is wrong: `vertex_at` over all 81 at level >= 1 reads parents OUTSIDE the lattice at the patch edge (ox=0, vi=1, s=2 -> parent -1). The criterion "bit-identical to vertex_at over the identity probe's case space" is restated: identical on every stride vertex; fillers equal `vertex_at` with morph 0.
- Geomorph cone untouched: ModeVtx lands `m_y` through the same `last_y` wire into a register. Not lengthened, not shortened.
- TOOL HAZARD: `gen_prod_top.py --help` regenerates the top (ignores unknown flags). Verified no-op via `git diff` (empty). Use `--check`.

### 2026-09-10 10:05 - RTL edited

- zhao_terrain_tess.sv: header (modes, laws 6/7), `IDX_W` parameter + `initial` guard, `job_mode_i`, `vtx_*` (9 ports), `ref_*` (7 ports), 3 saturating counters, `j_vtx/j_ref/j_vshift/j_plain_hi`, `pend_idx/pend_stride`, skid, triple register, scan-skip for unstitched ModeVtx.
- tess_harness.hpp: Driver gains set_mode(), `verts`, `refs`; existing test SOURCES untouched.

### 2026-09-10 10:40 - Evidence, standalone lane (build-tessvtx/, script build_standalone.sh)

- `verilator_bin --lint-only -Wall` zhao_terrain_tess: 0 diagnostics (after one UNUSEDPARAM on ModeTri, fixed by naming all three encodings in the invalid test). Pair wrapper closure: 0.
- terrain_tess_modes_directed: **33/33**, 29.8 s. 110,592 jobs; VTX 6,162,480 vertices, REF 2,509,920 triples (== the probe's triangle count), TRI 227,884; 0 mismatches in all three comparators; positive controls fired (1-LSB vertex, flipped index, rebuild); counters == tallies. Hand check of the vertex count: (110,592 - 16,080 rejected - 18,432 legacy undersides) x 81 = 6,162,480. The probe's "24,576 legacy-page undersides empty" label is loose: 6,144 of those are level-3 jobs on the void lattice whose single run-cell covers a void cell (origins (0,0) and (8,16), 2 x 256 x 6 x 2).
- terrain_tess_directed UNCHANGED SOURCE: 6751 passed; 456 / 936 cycles unchanged. terrain_tess_random: 2277 passed.
- Throughput: VTX L0 no morph 87 cyc/81 verts; morph 0.5: 167 (161 reads); L1: 87; stitched VTX 153 (scan ran); REF L0 199 = 65 + 128 + 6.
- IDX_W guard: -GIDX_W=6 -> %Fatal at time 0 with the named message; the exe then HUNG at exit (the zhao_sim.hpp VlThreadPool deadlock) and was killed; rc unobtainable, message is the evidence.
- check_quartus17_syntax: clean, 226 files. gen_prod_top.py: regenerated (54+/17-), --check fresh. check_prod_manifest: OK. no_control_bytes: clean on 5 files.
- uncashed_cheques: zhao_terrain_tess fit rows are DIRTY (96c0394a) / BEHIND (7395d793, 14.4 d) -- the tess has no clean leaf row to compare an area delta against.
- Lane-local cmake tree build-tessvtx/cmake started in background (configure + 4 targets + ctest).

### 2026-09-10 11:05 - Lane-local configure FAILED, my claim refused

- `cmake --preset windows-native -B build-tessvtx/cmake`: CONFIGURE_RC=1 -- `tests/terrain/tb_terrain_compose.sv:540` instantiates the tess; 20 PINMISSING (3 inputs + 14 outputs + 3 counters). My "both instantiations" grep skipped tests/. Fixed with the same tie-off as the pair wrapper (mode 0, readies 1, unread wires in the bench's own lint_off block).
- The PowerShell wrapper reported exit 0; the planted `CONFIGURE_RC=` line in the log is what caught it. CTEST_RC=0 there is "no tests ran", not a pass.
- Contract TERRAIN.TESS.md updated (ports, laws 6/7, measurements, the new test). Report written: reports/TERRAIN-TESS-VERTEX-MODE-20260910.md (lane result placeholder pending).
- Lane re-run started (5 targets incl. test_compose_rtl_directed; ctest -R "terrain_tess|compose_rtl").

### 2026-09-10 11:20 - Lane re-run GREEN; packet complete, awaiting owner review

- build-tessvtx/cmake: CONFIGURE_RC=0, BUILD_RC=0 (733 steps), ctest 6/6: terrain_tess_directed, terrain_tess_random, lint_terrain_tess, terrain_tess_modes_directed (40.9 s), terrain_tess_normals, compose_rtl_directed.
- Report finalised: reports/TERRAIN-TESS-VERTEX-MODE-20260910.md. Contract updated. Nothing committed, no fit run, per brief.
- Background tasks: none running (guard exe killed; lane finished).

## Files Created / Modified

- fpga/rtl/terrain/zhao_terrain_tess.sv (M), fpga/rtl/synth/zhao_pair_tess_normals.sv (M), fpga/rtl/prod/zhao_prod_top.sv (regenerated), tests/terrain/tess_harness.hpp (M), tests/terrain/tb_terrain_compose.sv (M), tests/CMakeLists.txt (M), design/contracts/TERRAIN.TESS.md (M), tests/terrain/terrain_tess_modes_directed.cpp (NEW), reports/TERRAIN-TESS-VERTEX-MODE-20260910.md (NEW), build-tessvtx/ (lane-local, gitignored).

## Decisions Made

- Per-job mode input over an elaboration parameter; 2-deep skid on the vertex port; off-grid vertices = plain lattice vertex, flagged (law 6); reject in every mode, scan skipped for unstitched mode-1 jobs (law 7); geomorph cone untouched; zhao_terrain_topo left in place with a retire recommendation.

## Next Steps

- Owner review + commit. Item 3 (TERRAIN.SEQ composition: reference tagger replacing topo, per-view presentation of the view-independent fill). The one fit: zhao_pair_tess_normals (+ leaf zhao_terrain_tess) in the owner's Step-1 session.

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
