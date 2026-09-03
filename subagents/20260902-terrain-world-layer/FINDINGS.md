# Terrain world layer architecture document - Findings

**Agent ID:** 20260902-terrain-world-layer
**Created:** 2026-09-02
**Parent Task:** Produce reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md
**Status:** Complete

## Summary
Wrote `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md`: block-by-block design of the
world layer around the existing (untested, uninstantiated) zhao_terrain_residency.sv,
with all capacity arithmetic shown against the real device, a dependency-ordered
build sequence (first three immediately buildable), and 10 owner-ruling questions.

## Findings
- Full detail is in `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` (the deliverable).
- Notable inspection finding: same-cycle fin/claim (and dirty/claim) hazard in
  zhao_terrain_residency.sv — an old-generation fin arriving the cycle of a claim
  for the same slot marks the fresh, unfilled page LOADED (source-order NBA wins,
  gen guard reads pre-increment gen). Recorded as interface obligation + test case.
- The residency block also lacks: any test, island_id in its key/tag, an unload
  port, M10K-mappable (registered-read) arrays.

## Recommendations
- Owner should route the document's section 7 (10 open questions) to the reviewer.
- First three buildable: residency test suite; compcache patch front + PATCH->LOD->TESS
  composition; TERRAIN.PAGELOADER.

## Files Examined
reports/Missingterrain, reports/CONSOLE_REMAINING.md, reports/EARTH60_CAPACITY.md,
reports/BINNER_CAPACITY_FOR_8KM_MAPS.md, reports/OWNER-RULINGS-20260831.md,
reports/SUNDER.md, design/contracts/{TERRAIN.PATCH,TERRAIN.TESS,TERRAIN.LOD,
TERRAIN.BAKE,SW.STREAM,MEM.GUARD,MEM.HPS.BRIDGE,CMD.DMA,CMD.SCHEDULER}.md,
fpga/rtl/terrain/zhao_terrain_residency.sv, fpga/rtl/common/zhao_pkg.sv,
spec/terrain_rules.md, spec/memory_rules.md, spec/cartridge.md, spec/commands.zidl,
ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md
