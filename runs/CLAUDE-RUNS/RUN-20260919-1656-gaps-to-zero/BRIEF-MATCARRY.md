# MATCARRY — I34's last leg. The decision is made; you build it.

**Branch `gz/matcarry`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I34` is ONE OF TWO REMAINING REGISTER ENTRIES**, and after five packets its
open surface is **one carriage problem**. Read first, in full:

1. **`reports/DECISION-20260927-I34-MATERIAL-CARRIER.md`** — the decision. **It
   is made. Do not re-open it.**
2. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-MATERIALPATH.md`
   §7 — the measurement behind it.
3. Entry `I34` (`grep -n '^// I34\.'`, never a line number).

## WHERE THE ENTRY ACTUALLY IS

**MATERIALPATH met the owner's acceptance test**: a Field material write changes
the consumer's served triple — `composepub_acceptance` **154/0**, authored
`{9D,EA,B4}` → served `{2A,7C,B3}`, `field_composed=1024`, height lane unmoved.
The encoding is authored once in `spec/qformats.md` §14 (**literally the
anchors `ops.yml` had been citing into thin air**), `TERRAIN.MATJOIN` is
composed, and capabilities went **112 → 113**.

**What is left is carriage.** The composed triple stops at `tcf_tri_mat_*_w` and
does not reach the mosaic as a **per-triangle** value.

* **The 32 bits already have complete, live carriage.** `base_rgb[23:8]` plus
  `recipe_weight` ride `tri_flat_request_c` into the shell, through the binner's
  metadata bank, are decoded at `zhao_raster_tile_pipe_v2.sv:770-781` and reach
  `zhao_texture_mosaic_v2`. **No file under `fpga/rtl/texture/` or
  `fpga/rtl/raster/` needs to change.**
* **What is missing is a per-triangle SOURCE.** `MAT_BASE_RGB_C` is a named
  constant white and `mw_pub_recipe_weight` is per **span**.
* **The drain price is NOT owed.** `zhao_material_window.sv:486-492`'s `match_c`
  has five terms and **neither `base_rgb` nor `recipe_weight` is one of them**,
  so a per-triangle triple costs **zero drains**.

## THE DECISION YOU ARE BUILDING

**WIDEN THE RIDERS. DO NOT ADD A PARALLEL ALIGNED FIFO.** Two legs:

* **clipdoor → clip**: a per-client field on `u_geom_clipdoor` carried to
  GEOM.CLIP's output and muxed into the 32 bits **by the rider's domain**.
  `GEOM_VID_RIDERW` is a real parameter (`zhao_console_core.sv`, `16 + 32 + 2 =
  50`) and **all fifty are allocated** — `cl_o_rider[49:34]` material, `[33:2]`
  raster, `[1:0]` domain. **Three files**, not eleven.
* **setup → attrpack**: **parameterise the hard-coded 16-bit `tri_src_id` /
  `out_src_id`** (`zhao_geom_setup.sv:150`, `:179`, `:260` — no parameter today)
  and widen it. **Creating that parameter is part of the work.**

**Why not a FIFO, and it is this campaign's own scar tissue:** a side queue that
must stay in lockstep with a pipeline **is the `u_geom_tidq` defect class**. That
queue was permanently one behind, **74 of 75 triangles binned under their
predecessor's index**, and it survived because **the ids stayed in range and
decoded cleanly** — every range guard passed, every gate green. A value riding
**inside** the record cannot drift from it. Widening costs **bits**, which a fit
prices; a FIFO costs an **alignment invariant nothing in the tree checks**.

**You may NOT reuse the 30 bits written as zero on three of the four client
arms.** They are **R28's `raster_state`, allocated and not spare**. Directive
§4: *"never silently overload a field, truncate a handle, or substitute a
convenient zero."* **Bits that happen to be zero today are not free bits.**

## AND A STALE BLOCKER YOU MUST STRIKE, NOT QUIETLY OVERWRITE

Entry `I34` says `material_set`/`material_id` under `fpga/rtl/terrain/` *"return
ZERO hits"*, **with a positive control offered to show the zero was not a broken
grep.** **There are four code hits.** The zero was true when taken and is false
now, and **it has been inherited through FOUR refusals.** **Strike it in place
with the correction stated** — a document cannot go stale loudly.

## The fences

* **DO NOT REGRESS VELOCITY**, and note its test was **repaired this week**:
  `terrain_veljoin_directed` printed a **hardcoded `0 failures`** and could not
  go red. It passes honestly now. **Re-run it and believe it.**
* **Do not touch nav.** Owner-ruled to SW.CPUCOLL; leave `efa_nav_cost` produced
  and classified.
* **Do not build a `zhao_terrain_patch_v2`** and **do not raise `FAB_LANES`,
  `FAB_GROUP_PTS` or `FRONT_PTS`** — LANESCOST priced those at **+11,979 ALUTs
  and +9 DSP** and refused.
* **Do not touch `zhao_geom_bin_pipe_v2`, `zhao_raster_tile_pipe_v2` or
  `zhao_geom_binner_v2`.** A concurrent packet (`DOORCOST`) owns those. **You
  own `clipdoor`, `clip` and `setup`.**
* **`fpga/rtl/prod/zhao_console_core.sv` is SHARED with that packet.** **Stage
  the HUNK, not the file** — `git add <file>` stages whatever is in the tree,
  including work you did not write, and `git checkout --` DISCARDS theirs with
  no reflog.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — it cost
  `zhao_geom_arenabin` **146,414 registers against 1,010**. Generate-**IF**
  infers; module scope infers; **the LOOP is the killer.** Correct remedy: one
  flat array **at a module's own scope**, its own module instantiated inside the
  loop — `zhao_dc_sdp_ram` is that module. **Rule 6 is NOT complete**; a live
  counterexample exists in the binner and the cause is unestablished.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## Evidence bar

* **The triple arriving WITH ITS OWN TRIANGLE under interleaving** — not merely
  arriving. **The `tidq` lesson is that arrival proves nothing; the
  discriminator is an equality against the producer, per triangle**, never a
  variety check on the consumer. Use material values that differ per triangle
  and are **not** symmetric under an off-by-one.
* **A leaf `-MapOnly` row on the shipping part**, `rtlCleanAtHead` true, device
  named. The decision is **declared unpriced** and **you may refuse on the
  number** — the console needs **293,352 ALUTs against 227,120 present**, and
  two packets were refused on area this week.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Twelve packets this week found their own controls vacuous — one measured
  **refusals** and called it a 62× speed-up; one asserted three counters fired
  when its stimulus could not reach one; one printed *"expected 0x1, got 0x1"*
  **on failure**; and one positive control had **silently become a no-op**
  because arithmetic moved one file away.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **RUN GATE 31.** `check_console_closure_lint.py` caught **31 PINMISSING** on
  the last merge because the packet's gate list omitted it. **An intentionally
  unconnected output is an EXPLICIT EMPTY CONNECTION, `.port_o ()`, never an
  omission.**
* **A new core port costs four things**: the port, **BOTH** `.*` wrapper mutants
  (fix the wrapper, never the module — R220), the smoke bench's wire, and a
  **reader** for it. A counter the bench declares and never prints is the
  `geom_tidq_*` failure again.
* **`UNUSEDSIGNAL` is waived across whole directories** by
  `tests/shell/v3_closure_inherited.vlt` — **a dead wire raises nothing.** Count
  readers by hand.
* **`zhao_console_core.sv` is LF** — normalise before believing merge conflicts.
* **Backticks in a `git commit -m` string get COMMAND-SUBSTITUTED** and silently
  eat your words. Use `git commit -F <file>`.
* **Three concurrent smoke forms is this box's ceiling**, and another lane is
  live. **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **Both legs**: the widened rider and the parameterised `tri_src_id`.
3. **The per-triangle equality test**, with values an off-by-one cannot pass.
4. **The `-MapOnly` row**, or the refusal it produced.
5. **The stale "zero hits" claim struck in place.**
6. **Velocity still reaching its consumer**, on the repaired test.
7. **Every claim in this brief or the records you found FALSE.** Every packet
   this week found at least one; most were mine.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/matcarry` only.** Never `--force`.
