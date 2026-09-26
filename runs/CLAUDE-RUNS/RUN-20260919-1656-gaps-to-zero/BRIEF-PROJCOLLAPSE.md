# PROJCOLLAPSE — 256 terrain triangles reach GEOM.CLIP and every one culls ZERO AREA

**Branch `gz/projcollapse`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**The carriage is LAID and not one pixel moved.** CARRIAGE composed
`zhao_terrain_clipfeed` as the fourth clipdoor client, built the fourth
depthquant + rcp24 pair, instantiated both of SHADELADDER's laws and retired the
terrain edge from core and board. **The value traverses the composed machine
under real backpressure.** And the raster still draws 2,560 pixels, none of them
terrain's.

**This packet is one question: why is every terrain triangle zero-area on
screen?** It is a bug hunt, not a build. **Do not compose anything new to make a
number move.**

## The measurement you are starting from

From the composed console smoke, CARRIAGE's run:

```
SMOKE: terruv    refs_taken=256 emitted=256
SMOKE: terrlight refs_taken=256 lights=256 degenerate=0
SMOKE: terrcf    triangles=256 emitted=256 src_mismatch=0 uv_sat=0
                 shade_clamped=0 dq_refused=0 dq_stray=0
SMOKE: clip      submitted=272 clipped=2 culled=256 setup=14
SMOKE: PASS      raster pixels=2560 (UNCHANGED)
```

`submitted` goes **16 → 272** — the mesh's plus every one of terrain's 256. Then
**all 256 are culled, verdict ZERO AREA.** Read rigorously: `zhao_geom_clip`
tests `near` **first** (that counts as *clipped*, and clipped is 2 — the mesh's),
then `zero`, then `back` — which is **false by construction** under the
`CULL_NONE` terrain declares. So the verdict is `zero`, by elimination, not by
assumption. **Re-confirm that elimination before you trust it.**

## THE CLAIM THAT COST THIS, AND THREE PACKETS INHERITED IT

Entry `I13` has carried since 2026-09-25: *"the arm's triangles have AREA now"*,
on the evidence `terrlight degenerate=0`.

**Those are two different quantities.** `terr_light_degenerate_o` is the **3D
face normal's** degeneracy, computed from the compose cache's **world
positions**. **Screen area is a property of the PROJECTED corners.** TERRAINAUX's
repair was real; the sentence written after it was about the other quantity, and
**three packets inherited it as evidence that projection was fine.**

**And the inference from it is the useful part**: the world normals are
non-degenerate — the world positions are distinct — while screen area is
**exactly zero**. **So the collapse is in the PROJECTION, not in the lattice.**
That is your search space, and it is small.

## Where to look, and what not to assume

* **`zhao_terrain_project` / `zhao_project_core`** and whatever
  `zhao_terrain_clipfeed` hands the door. The projector is **shared** with GEOM
  (`zref::render::project_vertex` is declared the `reference_model` of **both**
  `GEOM.PROJECT` and `TERRAIN.PROJECT`), so a fault in the shared core would
  break the mesh too — **and the mesh draws.** That asymmetry is your strongest
  clue: **look first at what terrain feeds it that the mesh does not.**
* **Candidates worth pricing before chasing**: a view/proj matrix or viewport
  that terrain's lane never receives; a Q-format or scale mismatch between the
  compose cache's world units and the projector's expectation; `w` handed where
  `1/w` is expected or the reverse (CARRIAGE fed the new pair from **raw `w`**,
  deliberately); a per-triangle vs per-vertex indexing error that hands three
  copies of one corner. **Three identical corners is exactly zero area**, and it
  is the failure this packet should rule in or out FIRST because it is cheap.
* **Do NOT assume the lattice.** It is measured non-degenerate in world space.
  If you end up back at the lattice, you must overturn that measurement with a
  better one, not with a suspicion.

## The instrument that is blind here, and it is a big one

**`tests/shell/v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across FIVE whole
directories** — `texture`, `raster`, `geometry`, `common`, `video`. CARRIAGE
found dead RTL inside that blanket: the mosaic's `mosaic_tile_w`/`tx_w`/`ty_w`
occur **exactly twice each** (declaration and port connection) and **nothing
reads them**. I verified that by hand.

**So in those directories, a signal that goes nowhere raises nothing.** If your
hunt turns on "is this value actually consumed?", the lint will not tell you —
**count occurrences by hand**, as CARRIAGE did. And if you find more dead RTL,
report it; that waiver is hiding a class of defect this campaign keeps paying
for.

## The fences

* **This is a diagnosis packet. Do not build a workaround.** If the projection
  is wrong, repair it; if the input is wrong, repair that. **Do not add a clamp,
  a bias or an epsilon to make triangles pass a zero-area test** — that ships a
  wrong pixel past a gate, which is the exact failure I13's laws exist to
  prevent.
* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat stand-in.
* **Do not compose `zhao_terrain_normalmap`.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map you quote must name its `-Device`**, and **read `rtlCleanAtHead`
  before quoting ANY ledger row** — a dirty row carried a live +33 M10K claim
  this week.

## Evidence bar

* **A pixel that changes** is the win condition. `raster pixels` moving off
  2,560 with terrain's triangles in it, agreeing with the oracle.
* **If you cannot get the pixel, the deliverable is the NAMED CAUSE** with the
  value measured at the seam where it goes wrong — file, line, the number that
  is wrong and the number it should be. That is a full packet.
* **Show the three projected corners** of a triangle that culls, as actual
  numbers. If they are identical, say which stage made them identical.
* **Prove every counter you quote.** `culled` counting 256 is a count of a
  *population*, not a verdict — CARRIAGE noted the cull counter **cannot
  discriminate zero-area from backface**. If you need that discrimination,
  build it rather than infer it.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change; 289 sources now.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`zhao_console_core.sv` is a SHARED HOT FILE** and another packet is in it.
  Stage your **hunk**; never `git add <file>`; never `git checkout --`.
* **A `$fatal` elaboration guard WEDGES ctest at 0.00 CPU** — make it a build
  target, not a test.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **Editing inside a running build's closure gives "ninja: no work to do"** and
  the stale binary reports the old numbers. CARRIAGE hit this once.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **One `ctest` at a time per build tree.**

## One thing that is OWED and may be yours

**`zhao_terrain_clipfeed` has no directed test of its own.** Its two arithmetic
blocks carry SHADELADDER's benches, but the **glue — the join, the clamp, the
packing — has only counter evidence, not value-against-oracle.** If your hunt
lands inside that glue, the test comes with the fix.

## Deliverable

1. **The register before and after, measured BARE.**
2. **The named cause**, with the wrong number and the right one, at a file and
   line. **Or the pixel.**
3. **The three projected corners** of a culled triangle, as numbers.
4. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
5. **What you refused** — especially any epsilon or clamp you declined to add.
6. **Anything you got wrong and caught yourself.** CARRIAGE caught its own
   nine-port tint walk that was a flat broadcast wearing a per-vertex port list.
7. Branch and commit hash. **Push `gz/projcollapse` only.** Never `--force`.
