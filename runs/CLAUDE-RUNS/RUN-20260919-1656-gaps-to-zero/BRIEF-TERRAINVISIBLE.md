# TERRAINVISIBLE — draw terrain in the composed console, for the first time

**Branch `gz/terrainvisible`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**The decision is already taken. Read
`reports/DECISION-20260926-TERRAIN-FIXTURE.md` first** — it is a decision record
under the owner's vacation directive, and it authorises the thing every previous
packet correctly refused to do on its own: **change the fixture so terrain is
drawn, and let `raster pixels=2560` move.**

## What is already settled, measured, and not yours to re-derive

PROJCOLLAPSE measured the whole cause on 2026-09-26. **The projection is
correct. The carriage is innocent. The cull is lawful.**

* The played terrain pages have an **all-zero body**, so layer A (*"Top base
  height, 33×33, height16"*, `terrain_rules.md` §7) is a **constant** — all 81
  lattice vertices at **world y = 0**.
* `tests/prod/smoke_geom_fixture_gen.cpp:116-121` — `kMat` has rows 0/1 the
  identity and row 3 `{0,0,1,0}`, so `ndc_y = world_y / world_z` with the eye at
  **world y = 0**.
* **A ground plane through the eye projects to a line.** All three corners carry
  `y = 8192` — exactly `y0 + h/2 = 0 + 64/2` = 32.0 px in S 12.8. Cross product
  exactly zero. Corners are **not** identical: `A=(5851,8192) B=(5862,8192)
  C=(5870,8192)`, x differing by 11 and 19.
* **Proven by reversal**, with the committed `-TerrainRelief` control: relief in
  layer A and nothing else gives `clip submitted=272 clipped=258 **culled=0**`,
  `tri[0] A=(5851,8192) B=(5862,8265) C=(5870,8229)`.
* **The scale is known**: at this patch's measured `w = 14680064` (224.0 m), one
  metre of relief is `(1/224)·(64/2)·256 = 36.6` S 12.8 units. Predicted before
  the run; **measured 37 and 36.**

**AND RELIEF ALONE IS NOT ENOUGH.** With `-TerrainRelief` the 256 stop being
zero-area and become **OFFSCREEN — they are sub-pixel.** One metre of lattice is
18.5 subpixels, a 1 m cell is **0.072 px**, and the whole 32 m patch is **~2.3 px
wide** (patch 0 at x0 = 96 m, z = 224 m; `ndc_x = 96/224` → 22.86 px in a 32 px
view). `box_lo = (vmin+127)>>8`, `box_hi = (vmax−128)>>8` is the pixel-*centre*
range, so a 2.3 px span lands nowhere. **GEOM.CLIP is right again.**

**So this packet must move the patch as well as give it relief.** Both, or the
pixel does not come.

## The job

1. **Author relief into the fixture's layer A.** Simplest field that makes the
   geometry non-degenerate and legible — this is a fixture, not art. The
   existing `-TerrainRelief` control is your starting point and its behaviour is
   already measured.
2. **Place the patch so it covers real pixels.** Camera, patch origin or view
   extent — your choice, but **state which knob you turned and why**, and keep
   the mesh's own triangles drawing so the existing coverage is not traded away.
3. **REGENERATE THE REFERENCE in the same change.** The oracle and the RTL must
   still agree bit-for-bit. A new pixel count from a fixture whose reference was
   not regenerated is not a measurement, it is drift. **This is part of the
   change, not a follow-up.**
4. **DECLARE the new number.** Record the old 2,560, the new value, and the
   reason, beside each other. **Update every gate and document that quotes 2,560
   in the same commit** — `CLAUDE.md`'s own law about fixing the rules file when
   you fix the thing it describes.
5. **Keep a negative control.** The plain form's flat-lattice behaviour is
   evidence and should survive as a named control, not be deleted.

## The fences

* **NO epsilon, clamp or bias to make flat triangles pass a zero-area test.**
  The area is arithmetically zero from a *correct* projection. PROJCOLLAPSE
  refused this and so do you. It would ship a wrong pixel past a gate, which is
  the exact failure I13's two arithmetic laws exist to prevent.
* **Do not "fix" `kMat`/`kVp` silently.** If you touch the camera, that is a
  declared change to what the gate measures, recorded in the decision record's
  terms.
* **THIS DOES NOT CLOSE I13, and do not claim it does.** I13 additionally needs
  a real mosaic consumer built and `zhao_terrain_normalmap` given a detail port
  on a composed block. **If the register does not move, that is CORRECT** — say
  so plainly rather than reaching for a number.
* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat stand-in. Do not compose
  `zhao_terrain_normalmap`.
* **Do NOT start a console or full-device fit.**

## The dead consumer, which becomes testable the moment you succeed

**The mosaic's answer is dead RTL.** `mosaic_tile_w`, `mosaic_tx_w` and
`mosaic_ty_w` occur **exactly twice each** in `zhao_texture_island_v3_top.sv` —
a declaration and a port connection — and **nothing reads them.** CARRIAGE found
it, I verified the counts by hand, and it was invisible because
`tests/shell/v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across **five whole
directories**. PROJCOLLAPSE then found **107 more dead signals** under the same
waiver.

**Do not wire the triple into `base_rgb` to "finish" anything.** It would be
actively harmful: `base_rgb` becomes the published texel RGB at
`sample_count == 0` and `recipe_weight` is the blend weight under `R_LERP`, and
both are shut today **only by coincidence**.

**What IS wanted from you:** once terrain draws, say what the first *textured*
terrain pixel would need, measured against the live path rather than argued.
That is the next packet's specification and you are the first who can write it
from evidence.

## Evidence bar

* **Terrain pixels in the composed console smoke**, with the count declared and
  the reference regenerated so the oracle agrees.
* **The mesh still drawing** — show you did not trade one coverage for another.
* **The flat-lattice control still demonstrating the zero-area cull**, so the
  measured cause survives as evidence.
* **Prove every counter you quote.** Note PROJCOLLAPSE's warning: eight
  projection counters were bound into the bench by its `.*` and **printed by
  nobody**, and they are the only thing separating *"the arena refused every
  corner"* from *"the geometry is flat"* — identical symptoms. They print now;
  keep them printed.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change. It caught 7 PINMISSING for BINARENA today.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **A suite reads the LIVE TREE.** Two packets today edited the core while
  builds or controls were reading it; one froze the tree and re-ran all four
  controls. Do that, don't debug the phantom reds.
* **`Copy-Item` carries the source timestamp**, so a restored file can be older
  than objects built from the mutant — verify restores on CONTENT.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE.** If it did not move, say so
   first and say why that is correct.
2. **Terrain pixels**, the number, and the regenerated reference agreeing.
3. **Which knob you turned** — relief, patch placement, camera — and why.
4. **The old and new `raster pixels`, declared**, with every quoting site
   updated.
5. **What the first TEXTURED terrain pixel needs**, measured against the live
   path.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.** PROJCOLLAPSE was convinced
   for half its hunt that the arena was refusing corners — complete,
   self-consistent and wrong — and found out only by *printing* the counter
   instead of reasoning from the header. That is the standard.
9. Branch and commit hash. **Push `gz/terrainvisible` only.** Never `--force`.
