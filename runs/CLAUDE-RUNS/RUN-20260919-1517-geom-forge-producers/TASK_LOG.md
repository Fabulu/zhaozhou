# Task Log: RUN-20260919-1517 - GEOMETRY / FORGE producers

**Created:** 2026-09-19 15:17 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260919-1517-geom-forge-producers/

---

## Objective

Close GEOMETRY and FORGE capabilities that the completion register counts
`built_not_connected`, by BUILDING the producers their refusals name -- not by
re-arguing the refusals. Nine capabilities in scope:

    zhao_geom_loom  zhao_geom_parambuf  zhao_geom_project  zhao_geom_light
    zhao_geom_depthquant  zhao_forge_shadow  zhao_forge_prim
    zhao_forge_prim_eval  zhao_forge_cliff

Register before: connected 70, built_not_connected 25, unbuilt 3,
mandatory_gaps 62, tieoff_gaps 34.

---

## Progress Timeline

### 2026-09-19 15:17 UTC+02:00 - Task Started, refusals re-read and SPOT-CHECKED

Read `fpga/rtl/prod/zhao_console_core.sv` lines 2437-2730 (the refused-blocks
list) and `design/console_inventory.yml` in full.

VERIFIED INHERITED CLAIMS (each by its own search, not by re-reading prose):

* **FORGE.PRIM / PRIM_EVAL cartridge-page claim HOLDS.** `spec/cartridge.md`
  line 106-107: kinds run to 0x000D (CLIP_BANK), "Kinds 13-255 reserved",
  and line 109 "a reader that meets an unknown kind skips the page". There is
  no forge program page kind. Confirmed.
* **GEOM.PARAMBUF "nothing writes the arena" HOLDS.** Grepped every `.sv` under
  `fpga/rtl` for an asserted memory write. Exactly five sites:
  `zhao_debug_frameblit.sv:391`, `zhao_mem_upload.sv:291`,
  `zhao_raster_fbwrite.sv:224`, `zhao_terrain_pageloader.sv:381`,
  `zhao_terrain_writeback.sv:565` (plus `zhao_sdram_ctrl.sv:188`, the
  controller itself). None is geometry. The refusal stands unchanged.
* **`zhao_raster_rcp24_v4` has ONE request port and the texture island owns it.**
  `fpga/rtl/texture/zhao_texture_island_v3_top.sv:922`, `NCTX(12)`,
  `.d_i(frag_invw24_i)`. Confirmed; it is the only instance in the tree.
  FOUND BESIDE IT: `fpga/rtl/raster/zhao_raster_rcp24_svc.sv`, a second
  tokenised reciprocal service, header says "Nothing instantiates this yet".

ONE INHERITED CLAIM IS **FALSE** and it changes the shape of the FORGE.SHADOW fix:

* The core says the creature rung "is unported state inside
  `zhao_geom_meshfetch`'s LodState". **`zhao_geom_meshfetch.sv` contains no
  ladder state at all** -- the strings `rung`, `lod` and `LodState` appear
  exactly once in the whole 505-line file, in a header comment at line 18
  pointing AT `zhao_geom_lod.sv`. The core was quoting `zhao_geom_lod.sv`'s own
  comment ("GEOM.MESHFETCH holds one LodState per live instance"), which is that
  block's statement about its INTENDED consumer, and the same file says two
  hundred lines later that "the consumer (GEOM.MESHFETCH's descriptor fetch) is
  still unbuilt". So the rung is not unported state -- **nobody holds LodState
  at all**, and `zhao_geom_lod` is a stateless evaluator waiting for an owner.

---

## Decisions Made

1. **GEOM.LIGHT -- not touched.** Owner decision, `reports/OWNER-DOCKET-20260919.md`
   item 5. Not composed, not decided here.
2. **GEOM.PROJECT -- not composed.** A second `zhao_project_core` is ~6,199 ALM
   and 33 DSP on a device already over budget. Sharing preserved.
3. Build the producers the refusals name, in tractability order, and leave an
   honest boundary where a seam cannot close.

---

## Next Steps

*Updated as progress is made*
