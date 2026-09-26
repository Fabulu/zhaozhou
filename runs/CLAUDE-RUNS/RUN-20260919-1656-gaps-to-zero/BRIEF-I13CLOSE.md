# I13CLOSE — the last two items, and the blocker is a PALETTE IDENTITY

**Branch `gz/i13close`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I13` and `zhao_terrain_normalmap` are two of the last FOUR register entries.**
Seven packets have worked this entry; six refused it, and each refusal was right
at the time. **The ground is now almost entirely cleared.** Read the entry
(`grep -n '^// I13\.'`, never a line number) — the SHADELADDER, CARRIAGE,
PROJCOLLAPSE, TERRAINVISIBLE, TERRAINTEX and TERRAINMAT sections are
measurements.

## What is DONE, so you build only what is left

* **Both arithmetic laws are BUILT and COMPOSED** — the five-rung palette ladder
  and the S8.24 saturate.
* **Terrain DRAWS.** `raster pixels` 2,560 → **2,816**, oracle regenerated,
  new tile (1,0) that no mesh triangle touches.
* **The mosaic pick has a READER** — `zhao_texture_mosaic_hold` holds it under a
  generation seal, and the binding resolver displaces a TILESET row's base by
  `tile*4096`, TMU mirror-wraps at 64×64, CLUT8 → palette → RGB.
* **Terrain has an IDENTITY and a terrain fragment CARRIES A TEXEL.**
  `texture fragments=1216 samples=1216` — the 26 that never sampled were
  terrain's. `SetEnvironment 0x0311`'s `pad[12]` now carries
  `terrain_material_set` @36 and `terrain_material_id` @40, record still 48 B,
  ABI version unmoved.
* **MEM.UPLOAD is composed and exercised end to end** — the header claiming
  otherwise was false and three passes inherited it.

## THE REMAINING BLOCKER, MEASURED BY TERRAINMAT AND NOT SUSPECTED

**A TILESET row is CLUT8 by law, so it needs a PALETTE IDENTITY.** Today the
composer publishes that identity as **constants `{slot 0, gen 0}`**, and the
witness forces the row to match.

**And generation ZERO is the one generation the palette resolver cannot be
handed in a single pass.**

**So whoever composes the mosaic owes a palette identity with a REAL PRODUCER.**
That is your first job and it is the whole reason the mosaic is still
uncomposed. `mat_win_clut_unowned_o` counts exactly this today rather than
hiding it — **it is a live instrument, so read it rather than reasoning about
it.**

## The job

1. **Give the palette identity a real producer**, so a CLUT material's slot and
   generation come from somewhere rather than being constants a witness is bent
   to match. **Prove generation zero is no longer a special case.**
2. **Compose the mosaic** on top of that.
3. **`zhao_terrain_normalmap`** — the last entry on the `BUILT BUT NOT
   CONNECTED` list. It needs a **detail port built into a composed block**.

**If (1) and (2) land and (3) does not, say so plainly** — that is still the
entry moving further than seven packets have taken it.

## A FALSE PRESENCE THAT WILL MISLEAD YOU IF YOU TRUST IT

**`OWNER-DECISIONS` §2 says the normal map needs *"no port change on a composed
block"*, citing `zhao_raster_texjoin_v2`.** That module has **ZERO
instantiations**, is marked not shell-connected, and has **no `detail_i` or
`detail_o` anywhere** under `fpga/rtl/texture` or `fpga/rtl/raster`.

**The normal map DOES need a detail port built into a composed block.** Do not
plan around that sentence. CELLCARRY measured it; a false presence is worse than
a false absence because **nobody re-asks a thing already said to be there.**

## The fences

* **Do NOT wire the layer-E triple into `base_rgb`.** It is **actively
  harmful** — `base_rgb` becomes the published texel RGB at `sample_count == 0`
  and `recipe_weight` is the `R_LERP` blend weight, **both shut today only by
  coincidence.** Two packets refused it; TERRAINTEX named the comfortable
  rebuttal it rejected (*"a terrain material is PASSTHRU at count 1 so neither
  reader is live"*) as exactly the explanation that absolves the design.
* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat stand-in — broadcasting a
  scalar removes Gouraud, which the owner's directive prohibits outright.
* **`proj_out_mat_a_o` NO LONGER EXISTS as a port.** CARRIAGE retired the
  terrain edge and the triple dangles on an internal wire under the
  directory-wide `UNUSEDSIGNAL` waiver. I counted by hand: three occurrences,
  all comment lines. **Retiring a port is not connecting a lane.**
* **The `{a,b,weight}` → opaque-u32 resolver is COMMISSIONED, NOT EXISTING.** I
  struck the "two incompatible encodings" paragraph and FABRICSINK agreed;
  **PATCHV2 withdrew that strike and was right** — the reference function
  `ops.yml` names takes a triple and returns a triple. If you need that
  resolution, **you are building it**, and material stays an **opaque
  full-width u32, never narrowed to fit an older consumer.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`** and **read `rtlCleanAtHead` first.**
* **ARENACOMPOSE is live in the tree** on the geometry/binner side. Stage your
  **hunk**, never `git add <file>`, never `git checkout --`.

## Evidence bar

* **The palette identity produced, not constant** — and generation zero
  demonstrated working rather than special-cased.
* **A pixel through the composed mosaic**, agreeing with the oracle.
* **If you reach the Field bar:** the owner's test for I34's material half is
  that **a FIELD material write changes the intended consumer** — sharper than
  authored materials rendering, and TERRAINMAT explicitly did NOT claim it.
  Do not claim it on weaker evidence.
* **Prove every counter you quote**, and **check any control you add CAN FAIL**.
  TERRAINTEX found one of its own negative controls had gone **vacuous** because
  a line number moved underneath it; TERRAINMAT **fire-tested** its latch check
  (reading the identity live fails exactly 4 of 38, in the flattering
  direction). That is the standard.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — 291 sources; run
  after any port change.
* **IF YOU ADD A PORT TO `zhao_console_core`, UPDATE BOTH `.*` WRAPPER
  MUTANTS.** `wrapper_port_parity` went red on the merge today because a packet
  updated one and not the other, and that failure **reads like a broken core**.
* **Regenerating a generated file is part of the change** — `zhao_prod_top.sv`,
  the console wrapper port blocks and the texture-v3 interface manifest have all
  gone stale this week; the manifest went red on a merge today.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across five directories** —
  **count occurrences by hand** when a claim turns on whether a value is read.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I13` closed
   and whether `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **The palette identity's producer**, and generation zero demonstrated.
3. **The pixel** through the composed mosaic, with its oracle.
4. **How far the normal map got**, and what its detail port needed.
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/i13close` only.** Never `--force`.
