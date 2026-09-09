# Task Log: RUN-20260909-2311 - [Describe objective here]

**Created:** 2026-09-09 23:11 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2311-shade-light-consolidation/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 23:11 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-2311
- Created working directory
- Initial context: [brief description]

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

## 23:11 — Field trip begins
Brief: D-1 one level down — terrain lighting and vertex lighting become ONE engine.
Read: SHADE-AND-LIGHT-ARE-ONE-ENGINE-20260909.md, GEOM.LIGHT.md, TERRAIN.SHADE.md,
internal.hpp:300-430, terrain.cpp:55-125, zhao_terrain_shade.sv (all 491 lines),
terrain_shade_rtl_directed.cpp (all 449 lines), creature_core.cpp:540-620.

FINDING (brief premise, task 2): zhao_terrain_shade.sv HAS NO face_normal stage.
Its input ports n_x/y/z_i ARE a world normal (Q16.16, un-normalised); face_normal
lives in zhao_terrain_normals (its own block) and, in the reference, inside
shade_flat_tri_dir_unclamped. The RTL is already the hardware of the proposed
core. Plan: no datapath change; re-point tier-2 oracle at the NEW compiled core.

BEFORE-evidence build launched (zhao-reel, render_golden, render_heightfield,
terrain_shade_oracle, terrain_shade_rtl_directed) — background bt8a7bjdl.

## 23:2x — while the build runs (outside its closure)
- Contracts cross-referenced BOTH ways: TERRAIN.SHADE.md (purpose para + A7),
  GEOM.LIGHT.md (header, entry-point resolution + OWNERSHIP section, scalar-ref
  section). blocks.yml: GEOM.LIGHT note un-BLOCKED (D-1 resolved) + ownership;
  TERRAIN.SHADE note + ownership. Maturity fields untouched (V6 gate honoured).
- Baseline checkers: check_quartus17_syntax PASS (220 files), check_prod_manifest
  PASS (215 modules). Both from the un-edited RTL tree.
- Reference-core + test edits DRAFTED in scratchpad; not applied — the build is
  reading those files (live-tree rule).

## 23:25-23:50 — the split, proved
- BEFORE (clean tree at HEAD): reel_sequence_crc 29 seqs all match; render_golden,
  render_heightfield green; terrain_shade_oracle 12; RTL differential 4,142;
  break-oracle fails exactly 1. Extracts: BEFORE-goldens.txt.
- Applied the split: core shade_from_world_normal_unclamped (terrain.cpp, verbatim
  code motion), wrapper keeps face normal; declared in internal.hpp + PUBLICLY in
  zref_terrain_shade.hpp. AFTER: all 6 suites green, diff BEFORE/AFTER extracts ->
  GOLDENS_IDENTICAL.
- Test tier-2 oracle re-pointed at the COMPILED core + thin-view drift guard:
  6,656 checks passed, break-oracle still fails exactly 1.
- RTL: comment-only ownership header; suite + lint re-run green. NO datapath
  change — the brief's "bypass the face_normal stage" premise is wrong (no such
  stage exists in the RTL); reported loudly.
- Gates: check_quartus17_syntax PASS, check_prod_manifest PASS (before AND after).
  tools/ledger check: 3 schema errors IDENTICAL at HEAD (stash comparison) —
  inherited, blocks/38 + blocks/92, not repaired here.
- CONCURRENT LANE detected: texture island_v3 files modified 23:42 by another
  session; never touched here; disclosed in report (stash window noted).
- Deliverable: reports/SHADE-LIGHT-CONSOLIDATION-20260909.md. Nothing committed.
