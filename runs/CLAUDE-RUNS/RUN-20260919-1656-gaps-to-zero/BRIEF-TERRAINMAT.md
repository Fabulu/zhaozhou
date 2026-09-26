# TERRAINMAT — give terrain a material IDENTITY. The lookup already works.

**Branch `gz/terrainmat`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**(c) is BUILT.** TERRAINTEX landed the mosaic pick's reader on 2026-09-26 and
**a texel samples through it**, in the composed island:

```
packet-b mosaic: pick=124 tx=13 ty=50 tileset_line=0047cc80 plain_line=00400c80
```

Chain: `zhao_texture_mosaic_v2` picks → `zhao_texture_mosaic_hold` holds it
under the owner's generation seal → `zhao_texture_binding_resolver_v2`
displaces a TILESET row's base by `tile*4096` → the TMU folds u/v with the
row's own mirror wrap at 64×64 → CLUT8 → palette → retired RGB. Oracle is
`zref::terrain::mosaic_pick` + `mirror_texel`, **sharing no arithmetic with the
RTL's CSD trees.** Negative control and fire test both committed.

**But it is not a TERRAIN texel.** The smoke is unchanged at
`texture fragments=1216 samples=1190`: **terrain still declares `MATMODE_NONE`
and asks the island for nothing.**

## THE FINDING THAT SHRINKS THIS PACKET, AND IT IS BIG

**`zhao_material_resolve.sv`'s header carried TWO false claims and three passes
inherited them:** *"MEM.UPLOAD is composed nowhere"* and *"`dir_*` and `mem_*`
… driven by nobody in this composition."*

**Both false.** `zhao_mem_upload u_mem_upload` is instantiated at
`zhao_console_core.sv:21937` — I verified the line by hand — both ports are
driven, and **the smoke already exercises the path end to end**
(`material responses=1 … not_resident=0`).

**So terrain lacks an IDENTITY, not a LOOKUP.** That is much smaller than this
entry has been carrying, and it is your whole job.

## What (a) and (b) actually are

* **(a) A SAMPLING MATERIAL.** `MATMODE_NONE` publishes
  `NOMAT_SAMPLE_COUNT_C = 0` and **issues no resolve at all** — which is why
  `samples` is exactly 1,190 rather than wrong.
* **(b) A BINDING KEY terrain can present.** Zero `material_set`/`material_id`
  hits under `fpga/rtl/terrain/`.

**(b) has a candidate the tree already holds**, and "terrain cannot present one
material" is **false four ways**: `tileset_id` exists (`terrain_rules.md:143`,
`hdrread.sv:53`); the oracle runs rim and underside passes
(`terrain.cpp:673-675`, `773-775`); weight 0/255 is meaningful; and **ruling R13
explicitly refuses the claim.** The literal zero-hit grep measures **a naming
boundary between lanes, not an absence.**

## THREE THINGS TERRAINTEX LEARNED THE HARD WAY — inherit them

1. **The answer goes on the ADDRESS, not the selector.** `zref::Tileset` is one
   object, `tiles[256][64*64]`.
2. **The declaration must live in the BINDING ROW's `mode[21]`, not per
   fragment.** A per-fragment bit **has no carriage**: the 362-bit flat request
   and the 287-bit descriptor are **both packed solid**. That is the eleven-file
   METAW span, and it is why the obvious design does not fit.
3. **The join needs a GENERATION SEAL**, because the mosaic answers **two cycles
   after** the sample is offered. `zhao_texture_mosaic_hold` exists now — use
   it, do not re-invent it.

## AND ONE THING THAT WAS STRUCK AND IS NOW UNSTRUCK — read this carefully

I struck the *"two incompatible encodings"* paragraph in
`reports/OWNER-ESCALATION-20260926-I34.md`, and FABRICSINK agreed. **PATCHV2
WITHDREW THAT STRIKE on 2026-09-26 and it was right to:** the reference function
`ops.yml` itself names **takes a triple and returns a triple**, so a
**COMMISSIONED** resolver was mistaken for an **existing** one.

**Consequence for you: the layer-E `{a, b, weight}` → opaque-u32 resolution is
WORK THAT HAS NOT BEEN DONE**, not a mapping you can cite. If your identity
needs it, **you are building it**, under the directive's numerical policy:
material is the last covering field that actually writes that lane, in original
command order, and its value **remains an opaque, full-width u32 — never
narrowed to fit an older consumer.**

## AND THE CARRIAGE REGRESSED — do not plan around a port that is gone

**`proj_out_mat_a_o` NO LONGER EXISTS AS A PORT.** CARRIAGE retired the terrain
edge; the layer-E triple now **dangles on an internal wire** under the
directory-wide `UNUSEDSIGNAL` waiver. I counted the occurrences by hand: three,
**all of them comment lines.** PATCHV2's sentence is the one to remember —
**retiring a port is not connecting a lane.**

## The fences

* **Do NOT wire the triple into `base_rgb`.** It is **actively harmful**:
  `base_rgb` becomes the published texel RGB at `sample_count == 0` and
  `recipe_weight` is the blend weight under `R_LERP`, **both shut today only by
  coincidence.** TERRAINTEX refused it and named the comfortable rebuttal it
  rejected — *"a terrain material is PASSTHRU at count 1 so neither reader is
  live"* — as exactly the explanation that absolves the design.
* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat stand-in; broadcasting a scalar
  removes Gouraud, which the directive prohibits.
* **Do not compose `zhao_terrain_normalmap`.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  **every map must name its `-Device`**, and **read `rtlCleanAtHead` first.**
* **Another packet (DSPHUNT) is live in the tree** and may touch multiplier
  sites. Stage your **hunk**, never `git add <file>`.

## Evidence bar

* **A TERRAIN texel** — `texture samples` above 1,190 with terrain fragments
  carrying a real sample, agreeing with the oracle.
* **If you reach it: a FIELD material write changing the intended consumer.**
  That is the owner's bar for I34's material half, and it is sharper than
  authored materials rendering. **Do not claim it on the weaker evidence.**
* **Prove every counter you quote**, and **check that any control you add CAN
  FAIL** — a `$fatal` guard as a build target returns RC=0, and a packet found
  one of its own negative controls had gone **vacuous** because a line number
  moved underneath it.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — 291 sources now;
  run after any port change.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across five directories** —
  **count occurrences by hand**, as I did for `proj_out_mat_a_o`.
* **`git checkout -- <file>` discards uncommitted work with no reflog.**
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE.** If it did not move, say so
   first and why that is correct.
2. **The terrain texel** — stimulus, chain, oracle. **If nothing sampled, say
   that FIRST and say why.**
3. **What identity you gave terrain, and where the declaration lives.**
4. **Whether the `{a,b,weight}` → u32 resolver was needed, and if so what you
   built.**
5. **Whether a FIELD material write changes the intended consumer**, if reached.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/terrainmat` only.** Never `--force`.
