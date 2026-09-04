# How much of the ledger has RTL, measured

2026-09-04. A truthful answer to "what does finishing still require", counted
rather than estimated.

## The count

`design/blocks.yml` declares **107 blocks**: 87 `kind: rtl`, 15 `software`,
5 `profile`.

    rtl blocks with a module file      65   (75%)
    rtl blocks with NO module file     22   (25%)

Of the 65 that exist, **31 appear in the fit census with real numbers** — so
half the built RTL has never been measured for resources at all, and 34 of the
85 census rows that do exist are older than their own source (D19o).

## The 22 that do not exist yet

**Blocked on hardware (4)** — these need the board and device frozen, and are
Wave 9 work by construction:

    phase 0   SYS.CDC     SYS.PLL     SYS.RESET
    phase 2   MEM.SDRAM

**Not blocked (18)**, by phase:

    phase  2   INPUT.SNAC
    phase  3   MEM.UPLOAD
    phase  5   FORGE.SHADOW   GEOM.LIGHT   MATERIAL.RESOLVE   TEXTURE.COMBINE
    phase  6   TERRAIN.SHADE
    phase  7   FIELD.SEQ.CORE
    phase  8   MEASURE.HISTOGRAM
    phase  9   GEOM.LOOM      GEOM.POSE    GEOM.WARP
    phase 10   PART.COLLIDE   PART.SPAWN   PART.STATE   PART.UPDATE
    phase 11   POST.COMPOSITE POST.ECHO

The clustering is informative: **particles (4) and post (2) are wholly unbuilt**,
and the phase-9 geometry trio (`GEOM.POSE`, `GEOM.LOOM`, `GEOM.WARP`) is the
creature-deformation path — which the owner's standing priorities put near the
top.

## How the count was made, and where it can be wrong

A block id maps to a module by `zhao_` + the id lowercased with dots turned to
underscores. **That heuristic is wrong often enough to matter**, and the first
run of this count said 25 rather than 22 because of it:

    MEM.VRAM.ARBITER  ->  zhao_vram_arbiter    (not zhao_mem_vram_arbiter)
    MEM.HPS.ARBITER   ->  zhao_hps_arbiter
    MEM.HPS.BRIDGE    ->  zhao_hps_bridge

Three-part ids drop their middle component. The count above tries every suffix
of the id and the first-plus-tail form before declaring a block absent, so it
now finds those three — but **a block whose module is named on some other
principle entirely would still be counted as missing**. The number is therefore
an upper bound on what is unbuilt.

This is the same failure that made five separate tools report confident wrong
numbers today, and the same fix: test the heuristic against a case you can check
by hand before believing the total.

## What it does not say

Nothing here is about whether the 65 that exist are correct, complete, or meet
their contracts. `TEXTURE.TMU` exists and misses its throughput target by 35%.
`zhao_texture_tmu_pipe` exists and holds 65,536 bits of palette in flip-flops.
**Existing is the weakest thing a block can do.**
