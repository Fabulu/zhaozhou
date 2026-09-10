# SPEC v1: Make the shared projector adoptable -- terrain shell on zhao_vertex_arena

**Run ID:** RUN-20260910-0846
**Created:** 2026-09-10 08:46 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`zhao_project_service` is built, linted, registered and instantiated nowhere.
Its header's blocker ("do not instantiate until the vertex arena is in place")
is cleared at the primitive level (VALID_MODE=DENSE_SEAL, 8/8 sby tasks).
Build what stands between the primitive and a composed top: a terrain shell on
`zhao_vertex_arena`, a level-0 replay walker, the terrain-as-client-B wiring,
and a differential that holds every replayed triangle bit-identical to the
retained `zhao_terrain_project` oracle. Report honestly what remains.

## Scope

**In Scope:**

- `fpga/rtl/terrain/zhao_terrain_wcache.sv` -- the shell (3 read copies, broadcast fill, 106-bit record with `w`, credit-based 2-deep output skid, corner counters)
- `fpga/rtl/terrain/zhao_terrain_topo.sv` -- the level-0 unstitched subpatch walker (128 triangles, sec 4.3 order, underside b/c swap, hold/done)
- `tests/terrain/tb_terrain_wcache.sv` + `tests/terrain/terrain_wcache_differential.cpp` -- service(A: geometry noise, B: terrain) -> shell -> topo vs legacy `zhao_terrain_project`, plus zref `project_vertex` for `w`
- `tests/terrain/terrain_identity_probe.cpp` -- the 81-vertex identity obligation, measured on the zref tess oracle over every level/neighbour/morph/surface/void case
- registrations: tests/CMakeLists.txt, design/prod_manifest.yml (excluded, not-yet-adopted), design/fit_targets.yml
- `reports/PROJECTION-ADOPTION-20260910.md`

**Out of Scope:**

- ANY Quartus fit (the one gate is NAMED, not run)
- committing (owner reviews the working tree)
- `zhao_proj_arena3.sv` (superseded design study; untouched)
- `fpga/rtl/texture/` (another lane is live there)
- the tessellator's vertex-mode rework (named as remaining work)

## Constraints

- Quartus-17 forms: explicit generate/endgenerate, $fatal inside initial begin
- no reset over payload; synchronous read; no bypass; linear addressing is the primitive's
- every counter seen to fire, or a parameter/mutant control named
- standalone verilate: -std=gnu++17, ABSOLUTE include paths, space-free Mdir, rm -rf Mdir between builds
- build tree: lane-local (`build-projadopt/`), never the shared `build/` graph

## Don't Retry

- (none yet)

## Open Questions

- REPLICAS=1 (one copy, one triangle per three clocks = legacy rate, 6 M10K) vs 3 copies (one triangle per clock, 18 M10K): built 3; the downstream rate (GEOM.CLIP) decides and is not measured here
- DENSE_SEAL at DEPTH=81 costs 81 fills per subpatch at EVERY level; at level >= 2 that is MORE projections than the legacy corner path (81 vs 24 / 6). Budget-neutral against the roadmap's all-level-0 stress; a real LOD distribution needs the trace.

- wait_fills(n) counting fills AFTER the last vertex is presented: WRONG with a 36-cycle core (most land during presentation). Count cumulatively: accepted-on-B vs landed-in-shell.
