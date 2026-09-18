# The missing-organ register — what has a CONTRACT and no RTL

Built 2026-09-18 by cross-referencing every non-software contract in
`design/contracts/` against every module in `fpga/rtl/`, then checking each
candidate by hand, because a name heuristic over-reports and an over-reported
"missing" list sends people to build things that exist.

**106 RTL contracts. Roughly a dozen have no module at all.** That is the real
extent, and it is the answer to "how much of the disaster is not yet even
counted" — because none of these appear in the 96,200-ALM census. They are debits
still to come.

## Genuinely absent — a contract, an envelope, and no RTL

| contract | envelope group | note |
|---|---|---|
| **PART.STATE** | Complete particles, 2,000 ALM | the particle core |
| **PART.UPDATE** | " | " |
| **PART.COLLIDE** | " | " |
| **PART.SPAWN** | " | " |
| **GEOM.LIGHT** | Geometry + lighting, 5,500 | the RGB/multi-light/emission producer R8 names |
| **GEOM.LOOM** | " | |
| **GEOM.WARP** | " | also the missing client-A producer for the shared projector |
| **FORGE.SHADOW** | Terrain/Forge, 4,500 | `forge_cliff`, `forge_jitter_rom`, `forge_prim` exist; shadow does not |
| **POST.COMPOSITE** | Post and 2D, 2,000 | only `zhao_post_gather` exists |
| **POST.ECHO** | " | the golden path charges the selected optional echo here |
| **SYS.PLL** | Backend/platform, 14,000 | the board wrapper |
| **SYS.RESET** | " | " |
| **MEASURE.HISTOGRAM** | Backend/platform | `measure_governor` and `measure_tokens` exist |
| **INPUT.SNAC** | Backend/platform | `input_rumble` and `input_snapshot` exist |
| **MATERIAL.LIQUID** | Backend/platform | `material_combine_v1/v2/v3` exist; liquid is separate |

**What the particle row means concretely:** `zhao_part_expand`,
`zhao_part_ladder`, `zhao_part_record` and `zhao_part_soft` exist. STATE,
UPDATE, COLLIDE and SPAWN do not. That is why the census shows 827 ALUT against
a 2,000 ALM envelope — 0.26× — and why the golden path insists *"its 2,000 is
owed, not banked."* R8 said the same thing from the contract side; the census
said it from the silicon side; they agree.

## Resolved — the heuristic was wrong, these exist

`MEM.SDRAM` → `zhao_sdram_ctrl`. `MEM.ENGINE1.RAWLAST.V2` →
`zhao_engine1_raw_last_v2`. `SYS.CDC` → `zhao_cdc_snapshot`.
`FIELD.SEQ.CORE` → `zhao_field_v2_core`.

## Unresolved — needs a read, not a guess

`MEM.UPLOAD`, `MATERIAL.RESOLVE`, and the `FIELD.SEQ.*` family
(EARTH/FLOW/FORMATION/STAMP/WARP). The FIELD ones are probably PROGRAMS run by
an executor that exists (`zhao_field_alu`, `zhao_field_exec_shared`,
`zhao_field_curve`) rather than modules of their own — but "probably" is not a
disposition, and this register should not pretend otherwise.

## What this does to the number

The census measures **96,200 ALM of what exists**, against a 37,500 portfolio.
Everything in the first table is outside that number. Using the golden path's
own envelopes as the only available estimate for unbuilt work:

```
particles      ~1,500 ALM owed (2,000 envelope, ~500 built)
board wrapper  unpriced -- SYS.PLL and SYS.RESET have no implementation at all
lighting       unpriced -- GEOM.LIGHT/LOOM/WARP
post           unpriced -- POST.COMPOSITE, POST.ECHO
Forge shadow   unpriced
```

**So the honest statement of the disaster is: 2.6× over on the parts that
exist, with at least four groups still to add work to.** The ratio gets worse
before any optimisation makes it better, and that is the fact the plan has to
start from.

## Build order, and the reasoning

Ordered by *how much the absence distorts the total*, not by difficulty:

1. **PART.STATE → UPDATE → COLLIDE → SPAWN.** Largest proportional gap, four
   contracts, and greenfield: nothing to adopt, nothing to supersede, no packet
   registration to re-accept. The cleanest possible place to start.
2. **SYS.PLL and SYS.RESET.** Small, but the board wrapper is explicitly charged
   to Backend/platform and is currently a zero in a group already at 2.2×.
3. **GEOM.WARP.** Doubles as the client-A producer that blocks adopting the
   shared projector — the one unbuilt organ that also unlocks a measured saving.
4. **GEOM.LIGHT, FORGE.SHADOW, POST.COMPOSITE/ECHO, the rest.**

**Not before the baseline lands.** `@whole-console-sizing` is the denominator
for all of it, and adding organs while it runs would mean the first fitted
whole-machine number describes a machine that no longer exists.
