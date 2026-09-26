# TERRAINTEX — the first TEXTURED terrain pixel, and it is three named things

**Branch `gz/terraintex`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Terrain draws.** TERRAINVISIBLE landed it on 2026-09-26: `raster pixels`
**2,560 → 2,816**, new tile (1,0) that no mesh triangle touches, oracle
regenerated and agreeing exactly (`clip submitted=144 clipped=69 culled=0
setup=75`). **Your job is the texel.**

## WHAT IS MISSING IS MEASURED, AND IT IS NOT A COORDINATE

TERRAINVISIBLE's one-line measurement: **`texture fragments=1216
samples=1190`**. All **26** terrain fragments reached the island and took
**ZERO samples**, with `combine_refused = 0`.

**THE U/V HALF IS ALREADY DONE** — `untex=0`, `uv_sat=0`. Do not rebuild it.

**Three things are missing, all named:**

* **(a) A SAMPLING MATERIAL.** `MATMODE_NONE` publishes
  `NOMAT_SAMPLE_COUNT_C = 0` and **issues no resolve at all.** No resolve, no
  sample, no texel — this is why the count is exactly zero rather than wrong.
* **(b) A BINDING KEY terrain can present.** Zero `material_set` /
  `material_id` hits under `fpga/rtl/terrain/`.
* **(c) A READER FOR THE MOSAIC PICK.** `mosaic_tile_w`, `mosaic_tx_w` and
  `mosaic_ty_w` occur **exactly twice each** in
  `zhao_texture_island_v3_top.sv` — a declaration and a port connection —
  and **nothing reads them.** Re-verified by hand twice, by two packets.

## AND (b) HAS A CANDIDATE THE TREE ALREADY HOLDS

**"Terrain cannot present one material" is FALSE, four ways** — FABRICSINK
measured it, and SHADELADDER flagged the conflation before that:

* **`tileset_id`** exists — `spec/terrain_rules.md:143`, `hdrread.sv:53`;
* the oracle runs **rim and underside** passes (`terrain.cpp:673-675`,
  `773-775`);
* **weight 0/255** is meaningful;
* **ruling R13 explicitly REFUSES** the claim.

**The literal zero-hit grep measures a NAMING BOUNDARY BETWEEN LANES, not an
absence.** Start from `tileset_id` and say what it can and cannot key.

## AND THE CARRIAGE IS FREE — this is the finding that unblocks you

FABRICSINK measured `zhao_material_window.sv:415-420`'s `match_c`: **five terms
— mode, vertex_alpha, frag_state, material_set, material_id — and `base_rgb`
and `recipe_weight`, the exact bits the mosaic slices, are NOT among them.**

**A per-triangle triple on the rider costs ZERO DRAINS.** The drain price that
deterred passes for weeks is real only for **per-cell**
`{material_set, material_id}`, which nobody proposes. It had been charged to the
triple by conflation.

**The route is live and already per-triangle at the boundary**: the authored
layer-E triple walks **eight composed hops** and reaches `proj_out_*`, which is
the granularity *"never interpolate identifiers"* requires.

## THIS CLOSES TWO HALVES AT ONCE, and that is not a bonus — it is the reason

**I13's material half and I34's material half are THE SAME BLOCKER.**
FABRICSINK established it. So a real terrain texel closes work on two register
entries.

**But the bar for I34's material half is the owner's, and it is sharper than
"terrain draws":** *"Do not close that half merely because authored terrain
materials render; **verify that a Field material write changes the intended
consumer.**"* **A Field write, changing a consumer.** Authored materials
rendering is necessary and not sufficient.

## THE FENCE THAT MATTERS MOST

**DO NOT wire the layer-E triple into `base_rgb` to make a number move.** It
would be **actively harmful**, not merely useless: `base_rgb` becomes the
published texel RGB at `sample_count == 0`, and `recipe_weight` is the blend
weight under `R_LERP`. **Both are shut today ONLY BY COINCIDENCE.** CARRIAGE
refused this and was right.

**Build the reader (c) properly, or refuse and say why.**

## The other fences

* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat colour stand-in —
  `terrain_rules` 6.5 makes layer-H tint **per-vertex**, and broadcasting a
  scalar removes Gouraud, which the owner's directive prohibits. If a flat value
  is ever right it must be **NAMED a stand-in in the RTL with 6.5 cited beside
  it**, and it is an escalation.
* **Do not compose `zhao_terrain_normalmap`** for a free register point. That is
  I13's first prohibition and five packets have refused it.
* **`OWNER-DECISIONS` §2's "no port change on a composed block" is a measured
  FALSE PRESENCE** — it cites `zhao_raster_texjoin_v2`, which has **zero
  instantiations** and no `detail_i`/`detail_o` anywhere. Do not plan around it.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map you quote must name its `-Device`**, and **read `rtlCleanAtHead`
  before quoting ANY row** — a dirty row carried a live +33 M10K claim this week.

## Things that will bite you, measured by the packets before you

* **`TRI_CAP = 128` triangles per frame is the real binder** in GEOM.BINNER —
  **not** the 32,768-reference chunk arena. TERRAINVISIBLE hit this wall: an
  over-large fixture walled the binner and `raster pixels` came back **stuck at
  2,560**, a capacity limit wearing the costume of a terrain arm that had
  stopped drawing. The generator now refuses to exceed the cap. **If your pixel
  count goes stuck rather than wrong, suspect a wall before a bug.**
* **The relief field is AFFINE on purpose.** A non-affine field moves
  `lod_deviation` off zero, and with it the LOD level, the triangle count and
  the whole oracle. **Do not perturb it casually.**
* **Three concurrent smoke forms is this box's ceiling.** Five gave
  `cc1plus: out of memory`, **and one form exited `verilator returned 3` with no
  `%Error` line at all** — indistinguishable from a killed g++. Run at most
  three.
* **`check_entry_claims` keys on PROSE** — writing "X is not composed" about a
  composed module in RTL comments turns it red even when you are quoting a claim
  to reject. **Reword rather than baseline.**

## Evidence bar

* **A TEXEL.** `texture samples` moving above 1,190, with terrain fragments
  carrying a real sample, agreeing with the oracle.
* **The per-cell triple arriving at `zhao_texture_mosaic_v2`** with the right
  values for the right cell — checked against the **layer-E source**, not
  against your own carriage bookkeeping.
* **For I34's material half, if you reach it: a FIELD WRITE changing the
  intended consumer.** Not an authored material rendering.
* **Prove every counter you quote.** A guard unreachable with legal stimulus
  needs a **committed mutant** under `tests/mutants/`, renamed so no source list
  elaborates it, polarity inverted so it passes when the counter FIRES.
* **Check that any control you add CAN FAIL.** SEALPLAN registered a `$fatal`
  guard as a build target today and it returned RC=0 — `$fatal` fires at RUN, so
  it was a green that could never go red.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change.
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across FIVE directories**
  (texture, raster, geometry, common, video), so **a signal that goes nowhere
  raises nothing there.** 107+ dead signals are already known. If your work
  turns on "is this consumed?", **count occurrences by hand.**
* **A suite reads the LIVE TREE** — freeze before editing, don't debug phantom
  reds.
* **`git checkout -- <file>` DISCARDS uncommitted work with no reflog** — a
  packet did this today and recovered only by luck.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether
   `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **The texel** — the stimulus, the chain, the oracle it agrees with. **If
   nothing sampled, say that FIRST and say why.**
3. **Which of (a), (b) and (c) you built**, and what each turned out to need.
4. **Whether a FIELD material write changes the intended consumer**, if you
   reach it — the owner's bar for I34's material half.
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
6. **What you refused**, especially any stand-in you declined to ship unnamed.
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/terraintex` only.** Never `--force`.
