# SPEC v1: Terrain pipeline composition — view/arena sequencing + downstream rate (adoption §7 items 3 and 4)

**Run ID:** RUN-20260910-1014
**Created:** 2026-09-10 10:14 UTC+02:00
**Status:** Complete (independently verified by RUN-20260912-1841)

## Objective
1. MEASURE the downstream triangle rate (GEOM.CLIP -> GEOM.SETUP -> GEOM.BINNER) before building; decide 1 vs 3 arena replicas from it.
2. Build the sequencing: per-view job issue, arena assignment, group release (provable), as `zhao_terrain_group_seq`; compose tess + seq + service + shell as `zhao_terrain_pipe`.
3. A composed differential: the pipe against the retained `zhao_terrain_project`, bit-identical, both views.
4. Report `reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md`.

## Scope
In: the RTL above, `zhao_proj_subsystem` refactor (walker OUT, generic ref port IN), benches, CMake registration, fit_targets/prod_manifest bookkeeping (not-yet-adopted), a committed mutant for the release detector.
Out: Quartus fit; commits; manifest flip; deleting zhao_terrain_topo; the normals leg (named, priced, not composed); DEPTHQUANT wiring.

## Constraints
- No fit. No commit. No manifest adoption. Do not change stitch topology.
- `gen_prod_top.py --check` only (never --help).
- Standalone verilator: -std=gnu++17, ABSOLUTE include paths, space-free Mdir, rm -rf Mdir first. RC=1 with no output = exit deadlock: rerun exe directly.
- Every counter SEEN TO FIRE or covered by a committed mutant.

## Don't Retry
- (none yet)

## Open Questions
- Normals leg in the arena architecture (mode-0 pass vs world arena vs normal map).
