# NORMALMAP — the last entry on the DISCONNECTED list, and it has a free carrier

**Branch `gz/normalmap`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**THREE register entries remain and this is one of them.** `I34` and `I55` are
the other two. `I13` closed today. **`zhao_terrain_normalmap` is the last entry
on the `BUILT BUT NOT CONNECTED` list**, and a disconnected implementation is
counted as a gap on purpose.

## The specification, handed over by I13CLOSE and measured

`f_detail_i` **has zero producers** and `zhao_terrain_normalmap` is
**instantiated zero times.** But I13CLOSE closed the surrounding entry and left
you three facts:

1. **`prod_manifest.yml`'s stated gap — "the fragment stream" — NOW EXISTS.**
   That was the blocker and it is gone.
2. **The declaration has a FREE CARRIER.** The binding row's `mode[21]` TILESET
   bit **already reaches the resolver as `read_tileset_c`.** You do not need to
   find carriage for a declaration bit — I13CLOSE built that road.
3. **The natural site is inside `zhao_texture_island_v3_top` beside `u_mosaic`,
   and the consumer is `zhao_texture_sheetmod`'s per-fragment modulation.**

**So what is owed is the PORT.**

## THE FALSE PRESENCE THAT WILL MISLEAD YOU

**`OWNER-DECISIONS` §2 says the normal map needs *"no port change on a composed
block"*, citing `zhao_raster_texjoin_v2`.** That module has **ZERO
instantiations**, is marked not shell-connected, and has **no `detail_i` or
`detail_o` anywhere** under `fpga/rtl/texture` or `fpga/rtl/raster`.

**The normal map DOES need a detail port built into a composed block.** CELLCARRY
measured it. **A false presence is worse than a false absence, because nobody
re-asks a thing already said to be there** — and this one has survived six
packets.

## The fences

* **Do not compose it as a PREFIX.** This entry's first prohibition is a chain
  whose last link does not exist, and six packets have refused exactly that.
  **A detail port with no consumer reading it is that shape.** The consumer is
  named above — wire it, or refuse.
* **`GEOM_CLIP_ATTRS` STAYS 7.** No unnamed flat stand-in — broadcasting a
  scalar removes Gouraud, which the owner's directive prohibits outright, and
  **the directive also names "remove detail normals" in the same breath.** You
  are building one, not removing it; do not trade it for a constant.
* **Do NOT wire the layer-E triple into `base_rgb`.** `base_rgb` becomes the
  published texel RGB at `sample_count == 0` and `recipe_weight` is the
  `R_LERP` blend weight — **both shut today only by coincidence.** Three packets
  refused it.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP.** Measured today:
  that is a **fourth Quartus 17.0.2 inference killer** beside
  `QUARTUS_GOTCHAS` §10's three, and it cost `zhao_geom_arenabin` **146,414
  registers against 1,010** for the identical circuit. **Put the array at a
  module's scope — its own module, instantiated inside the loop.**
  `check_ram_inference.py` **rule 6** now catches it; it did not before, and the
  checker was **100% false alarms and 100% miss** on this file.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting any row.**

## Evidence bar

* **The register moving 3 → 2**, measured BARE, with
  `zhao_terrain_normalmap` **off the `BUILT BUT NOT CONNECTED` list.**
* **A pixel or a fragment that CHANGES because the detail normal reached its
  consumer** — not merely a port that elaborates. The whole point of this
  entry's six refusals is that composition without consumption is the shape the
  register exists to catch.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Four packets today found their own controls vacuous: a guard never true, a
  line number that moved underneath it, one that **asserted the bug**, and one
  whose second obvious guard was **also** wrong.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **GATE 31 HAS A BLIND SPOT I13CLOSE FOUND:**
  `check_console_closure_lint.py` **deduplicates its source list** while
  `run_console_board_lint.ps1` does not, so a duplicated line in
  `fit_targets.yml` makes gate 31 say **OK twice**. Only a `%Warning-MODDUP`
  inside the inherited board red saw it.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants** — one was missed today and `wrapper_port_parity` went red, a
  failure that **reads like a broken core**.
* **Regenerating a generated file is part of the change** — four have gone
  stale this week, two on merges.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across five directories**
  — **count occurrences by hand** when a claim turns on whether a value is read.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED** — a packet produced a row
  whose device was a file path.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether
   `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **The detail port** — where you put it and which composed block gained it.
3. **What CHANGED because the detail normal arrived**, demonstrated.
4. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
5. **What you refused.**
6. **Anything you got wrong and caught yourself.**
7. Branch and commit hash. **Push `gz/normalmap` only.** Never `--force`.
