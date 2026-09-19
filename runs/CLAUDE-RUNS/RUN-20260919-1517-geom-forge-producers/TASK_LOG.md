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

### 2026-09-19 ~16:30 - TERRAIN.HEIGHTTAP landed (261d53c3, pushed)

Built, tested, committed, pushed. 783 directed checks differential against
`zref::terrain::column_query` across all four frozen pitches; mutant control
6/6. Gates: -Wall lint RC=0, check_quartus17_syntax RC=0,
check_console_inventory OK.

Two things the tools found, both recorded in the RTL header:
* explicit `-Wall` (verilate() does not pass it) reported `quot_c[63:32]`
  unused -- a silently truncated height. `interp_overflow_o` closes it.
* the directed test then could NOT fire that counter, which sent me to the
  algebra rather than to tuning the stimulus: the interpolation is a CONVEX
  COMBINATION of three corner heights in both triangle arms, so it cannot
  leave the interval they span. Guard is unreachable => committed mutant.

**WHERE I AM / WHAT IS NEXT** (written before reading the in-flight agent
result, per CLAUDE.md):

The tap is built and NOT COMPOSED. It must not be composed until it has a
live consumer, or it fails the same test GEOM.LOOM fails -- "composing it
would connect nothing and open new tie-off entries at both ends".

Its two candidate consumers:
  1. FORGE.SHADOW -- needs a CASTER as well as the tap. Caster analysis below.
  2. PART.COLLIDE (entry I6) -- needs {height, nx, ny, nz}. The tap already
     holds the four corner heights and the cell width, so a normal is one
     cross product away -- BUT only if the collision normal's law is settled
     and matches TERRAIN.NORMALS. An agent is checking that now. If it is an
     open decision, I stop and say so rather than inventing a second law.

**THE FORGE.SHADOW CASTER, traced to its real blocker.** The console core said
the rung "is unported state inside zhao_geom_meshfetch's LodState". FALSE (see
above). The true chain, each link verified:
  * world x/z/radius and src_id ARE available: `zhao_geom_meshfetch` is a
    STRICT SINGLE-IN-FLIGHT FSM (S_IDLE -> S_REQ -> S_VERD -> S_FILL ->
    S_BOUND -> S_CULL -> S_WAIT -> S_EMIT), so its `cull_c{x,y,z}_o` /
    `cull_radius_o` at S_CULL and its `r_instance_id_o` at S_EMIT are the SAME
    instance, provably, not by an ordering assumption.
  * `strength` has no producer and should not have one: it is an ART value and
    CLAUDE.md rule 6 puts it in a named editable constant.
  * the RUNG is the blocker, and it is not a missing block -- it is
    `zhao_geom_lod`, which is BUILT, and which needs `proj_radius_q8_i`.
    Turning a world radius into a screen size is a RATIFIED law already
    implemented and already called by `zhao_part_project`
    (`zref::render::draw_form_marker`: half = |rescale_s32(fx_mul(r, 1/w), 8)|).
    It needs 1/w for the instance centre, which needs a PROJECTOR CLIENT SLOT --
    and that is OWNER DOCKET item "a third projector port", already open.
  * `zhao_geom_lod` ALSO needs the creature type's micro/splat/glint errors,
    which live in the compiled creature form page (cartridge kind 8) -- the
    same cartridge-page-reader absence that blocks FORGE.PRIM / PRIM_EVAL.

So FORGE.SHADOW is blocked on ONE OWNER DECISION ALREADY ON THE DOCKET plus
the SAME missing block as two of the FORGE cluster. That is a much narrower
statement than "nobody emits a caster" and it is the packet's second finding.

---

## !! FROM ANOTHER PACKET (terrain/I35), 2026-09-19 -- I DESTROYED YOUR UNCOMMITTED `design/prod_manifest.yml` EDIT

Read this before your next commit.

**What happened.** I ran a helper that rewrote `design/prod_manifest.yml` as
`open(p,'wb').write(build(open(p,'rb').read()))`. Python opens the output
handle FIRST, so the file was truncated to zero bytes, and then `build()`
raised because its anchor did not match. The file sat at **0 bytes** until I
restored it with `git checkout HEAD --`.

**What you lost.** Your uncommitted entry. It was at line 990 and it began:

    - zhao_terrain_heighttap: not-yet-adopted  world (x,z) -> {height, no_ground},

That is all I captured before the truncation -- a grep of mine had cut the line
at 80 characters. Unstaged changes have no reflog, so there is nothing to
recover from; please re-apply it from your own context while you still have it.
If you had other uncommitted manifest edits, they are gone too and I cannot
tell you what they were.

**What is in the file now.** Exactly `HEAD:design/prod_manifest.yml`, nothing
else. I have NOT added my own entry to the working tree -- `zhao_terrain_hdrread`
goes in through a private-index blob built from HEAD, so it will not touch your
copy in either direction.

**Also worth knowing**, because it will confuse you otherwise:
`check_prod_manifest.py` was reporting a flood of UNACCOUNTED modules during
the window the file was empty. That was this, not your work.

-- the packet that closed core entry I35 (`zhao_terrain_hdrread` /
`zhao_mem_share2`)

### 2026-09-19 ~17:30 - The three refusals, run to ground

**PART.COLLIDE's normal (entry I6) is an OWNER DECISION, and nobody had written
that down.** I nearly added a normal to the height tap -- it already holds the
four corner heights and the cell width, so it is one cross product and NO new
arithmetic. Checked first, and stopped:
  * `spec/terrain_rules.md` 4.4: normals come "from the composed lattice by
    FINITE DIFFERENCES at tessellation time";
  * `design/contracts/TERRAIN.NORMALS.md` 194: "This block emits FACE normals,
    not vertex normals ... Averaging adjacent face normals into a vertex normal
    ... is NOT RATIFIED ANYWHERE ... left open for whoever ratifies it."
  * and the formats disagree too: `zref::terrain::face_normal` is DELIBERATELY
    unnormalised Q16.16, `zhao_part_collide` declares NRM_W=12/NRM_Q=10, a UNIT
    normal. `normalize3_approx` (qformats 7.4) would bridge it, but no contract
    says PART.COLLIDE's normal IS normalize3_approx(face_normal(...)).
  * `zref::terrain::column_query` returns no normal at all; `ColumnResult` is
    {cls, top, bottom}.
So the arithmetic is free and the LAW is contested. Recorded in the core beside
I6 and belongs on the owner docket. This is the "read the SIBLING contract"
rule paying for itself before the second implementation got written.

**GEOM.LOD's five inputs, all searched.** `thresh_q8_i`: `zhao_measure_governor`
exists, is UNIT_VERIFIED, is NOT composed, and emits per-camera SCALES
(`cam0_scale_o`, default 256) -- not a pixel-error threshold. The ledger edge
GEOM.MESHFETCH `upstream: [..., MEASURE.GOVERNOR]` is an INTENTION, not a port.
`bound_radius/micro/splat/glint`: no module in `fpga/rtl` emits any of them;
`zhao_geom_meshfetch.cull_radius_o` is the only bound-radius-shaped output and
it is the WRONG QUANTITY (a world-space instance bound scaled by the instance
matrix, not the bind-pose type radius `zref::lod_raw` divides by).

**And the reason they cannot simply be built is a RULING.** `spec/cartridge.md`
202-208 on kinds 8/9: "Byte-exact layouts freeze with SW.TOOLS.ASSET at Phase-12
entry; until then the packer refuses to emit them (deterministic refusal, never
a guessed layout)." There is no layout because the project has ruled there must
not be one yet.

**The cartridge claim holds but points at the wrong layer.** Confirmed: zero
hits on `page_id`, `byte_length`, `RESOURCE_PAGES` or a `kind`-byte parse across
all of `fpga/rtl`. But `design/contracts/SW.STREAM.md` 35-56 ASSIGNS the
container parse to software by ruling T12 -- "SW.STREAM stages; the loader
fetches" -- and hardware already reads staged page BODIES
(`zhao_terrain_hdrread`, `zhao_terrain_pageloader`, `zhao_terrain_writeback`
all parse a page-body header per terrain_rules 2.1). So FORGE.PRIM does not
need "a hardware cartridge page reader"; it needs a forge page KIND with a
frozen layout and a staging path, on the terrain pattern. Corrected in the core.

**Gate note (not mine to fix):** `mutant_copy_drift.py` reports
`zhao_terrain_bake_v2_mutant` DRIFTED, and `check_prod_manifest.py` reports
`zhao_terrain_hdrread` UNACCOUNTED. Both belong to the terrain packet that is
live in the tree; my own additions are accounted and drift-clean.
