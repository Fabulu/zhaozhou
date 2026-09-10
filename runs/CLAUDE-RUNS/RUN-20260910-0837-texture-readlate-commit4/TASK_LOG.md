# Task Log: RUN-20260910-0837 - Commit4: the owner/COMBINE resident-read seam

**Created:** 2026-09-10 08:37 UTC+02:00
**Status:** Done (uncommitted)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0837-texture-readlate-commit4/

---

## Objective

Build the ALM-Liberation Roadmap's Commit4 (section 4.2 / 14): the texture
island's ready tickets carry only the owner handle and the combiner reads the
sample planes itself, per phase. Every recipe and every output preserved; the
island's composed and fault suites pass unchanged; no fit run, no commit.
Deliverable: `reports/TEXTURE-READLATE-COMBINE-20260910.md`.

---

## Progress Timeline

### 2026-09-10 08:37 - Task started (late: the run was created after the RTL)

- Read, in the brief's order: the owner-residency architecture report, roadmap
  sections 3/4/14/16, v3own, combine_v2, the island top, CLAUDE.md.
- VERIFIED rather than inherited, from `@g2-prod.fit.rpt`'s own entity table:
  perspuv_svc 3,240 / v3own 3,018 / cache_pipe 2,945 of 16,285 registers.
  The pairpipe swap (8f61e083, 09-09 14:32) is AFTER the g2-prod pin
  (82a4f317, 09-09 05:13): no composed receipt with the pairpipe exists.
- FOUND in the same fit: `cq_s0..cq_ax_q` are FOUR altsyncrams of TWO M10Ks
  EACH (8 of 49), not MLABs. The copy chain's real cost is M10K.
- FOUND: the unlabelled island fit row (20,561 regs) is a 09-08
  MIGRATION_SHADOWS=1 build; `@g2-prod` (16,285) remains the receipt.
- DECIDED: READ_LATE parameter on v3own and combine_v2 (default 0 keeps both
  leaf suites byte-identical; the island sets 1). No epoch planes in this
  commit -- the architect's 4.2 lost-ticket hazard is a property of `cmt` in a
  RAM mirror and is NOT cured by retaining the ready flag. Named as the next
  increment with its prerequisites.

### 2026-09-10 ~08:00-08:30 - RTL

- v3own: READ_LATE, plane port `src_rd_*`/`src_*_o`, job queue carries the
  handle only, g_legacy/g_readlate generate, `ev_src_unpub_o` counter,
  three boundary assertions. Lint -Wall clean in both modes.
- combine_v2: READ_LATE, `f_slot_i`/`f_has_aux_i`/`src_*`, payload 110 -> 47,
  slot file 8x6 (fabric, declared), canonicalisation at D. Lint clean both.
- island_v3_top: READ_LATE=1 on both; aux/s2 mux deleted; 7th error class.
- island_top (ORACLE): eight new pins tied off, READ_LATE=0, wiring only.
  Found by configure-time PINMISSING, not by reasoning.
- Gates: check_quartus17_syntax OK; check_prod_manifest OK; check_v3_banks --
  identical findings on HEAD and working tree (pre-existing, none new).

### 2026-09-10 08:30 - Tests written

- tests/texture/tb_combine_readlate.sv (four real v3bank planes),
  material_combine_readlate_diff.cpp (V2's workload + seam laws + poison),
  texture_v3own_readlate_directed.cpp (data path, counter fired on four
  classes by stimulus, blind spot pinned),
  tests/mutants/zhao_texture_material_combine_v2_slotswap_mutant.sv +
  control (inverse polarity). CMake wired, incl. READ_LATE=1 lint tests.

### 2026-09-10 08:37 - WHERE I AM while the build runs

- `cmake --preset windows-native` + build of nine targets running in the
  background (scratchpad/build2.log). NEXT: run the five existing suites
  unchanged + gate3_paired.py + the three new tests; then write the report;
  then re-read this log against the report.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| -- | -- | none spawned | -- | -- |

### 2026-09-10 08:50 - Results

- Build: configure RC 0, build RC 0, nine binaries stamped 08:42-08:44
  (checked before running -- the stale-binary law).
- UNCHANGED suites against the new RTL: island_v3_composed 133/133,
  island_v3_prod 127/127, island_v3_fault 31/31, material_combine_v2_diff
  27/27, texture_v3own_adversarial 541/541. Gate 3: 392 records
  byte-identical incl. order.
- NEW: material_combine_readlate_diff 33/33, 0 seam violations over 4,062
  plane reads. Mutant control PASSES: 1,598/1,600 oracle mismatches,
  2,600/2,800 unowned-slot reads, all counters balanced -- the checker is
  seen to fail. texture_v3own_readlate_directed 30/30: counter 0 -> 4 on the
  four classes, both boundary assertions fired, blind spot pinned at zero.
- ctest: new tests registered (#94, #95, #234, #236, #237); both READ_LATE=1
  lint tests pass.
- Report written: reports/TEXTURE-READLATE-COMBINE-20260910.md.

### Status: DONE (uncommitted, by instruction). Owner reviews and commits.

Working tree: M island_top.sv (oracle tie-off), island_v3_top.sv,
material_combine_v2.sv, v3own.sv, tests/CMakeLists.txt; new: the report,
tb_combine_readlate.sv, material_combine_readlate_diff.cpp,
texture_v3own_readlate_directed.cpp, the slotswap mutant, this run.
The one fit gate: zhao_texture_island_v3_top@readlate, AFTER commit, from a
clean tree -- named in the report with what it can and cannot settle.
