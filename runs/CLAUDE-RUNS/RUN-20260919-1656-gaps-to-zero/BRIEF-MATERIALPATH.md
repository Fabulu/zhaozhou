# MATERIALPATH — I34's LAST OPEN LANE. Three channels are owned; material is the one.

**Branch `gz/materialpath`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I34` is ONE OF TWO REMAINING REGISTER ENTRIES**, and after four packets its
open surface is **one channel**. You are not being asked to settle an
architecture question. **Every architecture question on this entry is settled.**

Read entry `I34` (`grep -n '^// I34\.'`, never a line number), paragraphs **(P1)**
and **(P3)**, and `reports/DECISION-20260926-I34-PATCH-V2-CHANNELS.md`.

## THE ACCOUNTING, MEASURED AND SETTLED — DO NOT RE-DERIVE IT

| ordinal | channel | owner | status |
|---:|---|---|---|
| 0 | `height_o` | `u_terrain_patch` | **real consumer**, live in `composepub_acceptance` case 2 |
| 1 | `velocity_o` | veljoin → TERRAIN.VELOCITY → compcache §4.2 → spdesc → heighttap → `zhao_part_collide` | **real consumer**, composed by TERRVEL |
| 2 | **`material_o`** | **NOBODY** | **YOURS. The one open channel.** |
| 3 | `nav_cost_o` | SW.CPUCOLL / `zref::nav::Service` | **owner-ruled elsewhere**, classified and KEPT |

**Do not touch nav.** The owner ruled navigation truth belongs to the CPU
runtime, `zref::nav::Service` is built, and `FIELD.WRITE.NAV` is preserved with
`efa_nav_cost` still produced and classified. **"Do not make the register reach
zero through a dated stopgap, a renamed gap, or a computed-but-unread lane."**

**Do not build a `zhao_terrain_patch_v2`.** PATCHV2 retired the four-channel
prescription on measurement. Its surviving commission is the *stream order*, a
**performance** item, and **LANESCOST has now priced and refused that** at
**+11,979 ALUTs (14.3% of the shipping part) and +9 DSP** for an arrangement
still at 3.21× the contract. **That is separate from your work** — owner
standing authorization 4: *"Separate functional completion from performance
qualification."*

## THE GAP, MEASURED BY OPENING PORT LISTS RATHER THAN GREPPING

**Material's blocker is an ABSENT ENCODING, not a missing port** — and it is
broken at **BOTH** ends.

**UPSTREAM — a field result has no port to arrive on:**

* `zhao_field_sinks.sv:31-49` carries material as **`{u8 mat_a, u8 mat_b, u8
  weight}` on BOTH faces.** There is **no 32-bit material port on that module in
  either direction.**
* `zref::fieldir::compose_material` (`zref_fieldir.hpp:103-109`) — **the very
  reference function `design/ops.yml:525` names for `FIELD.WRITE.MATERIAL`** —
  **takes a triple and returns a triple.** The word `u32` appears nowhere in it.
* `zhao_field_earth_adapter.sv:522` and `zhao_terrain_patch_acc.sv:156-159` are
  **`[31:0]` at every hop.**
* **Nothing in this tree packs three u8s into that u32 and nothing unpacks it.**
  No encode, no decode, no table, no bit layout.
* `zhao_terrain_compcache_front.sv:169-174` has **ONE material write face**, u8
  triple, **one driver** — the page stream, i.e. authored layer E — **no
  override input, no second writer, no arbitration port.**

**DOWNSTREAM — and this half is WORSE than the entry used to record**, which is
the unflattering direction and is why it is stated carefully:

* `tcf_tri_mat_a_w`, `_mat_b_w`, `_weight_w` have **exactly two occurrences each**
  in `zhao_console_core.sv` — the declaration at `:19568` and the projector's
  write at `:19705-19707`. **NO READER.**
* **`zhao_terrain_clipfeed` HAS NO MATERIAL INPUT PORT AT ALL.** Its material
  outputs are the GEOM `{set, id, mode}` trio, driven **from constants** at that
  file's `:695-697`.
* So **the authored triple dies too.** It has the same status as
  `tcf_tri_ad/bd/cd_w`, which the same comment block openly declares
  *"DELIBERATELY NOT USED"* — **the difference is that those say so and the
  triple does not.**

**You must not take the measurement above on trust.** It was made on
2026-09-26; re-open those port lists. **`tests/shell/v3_closure_inherited.vlt`
waives `UNUSEDSIGNAL` across whole directories, so a dead wire raises nothing
and the count has to be taken BY HAND.**

## THE OWNER'S OWN TEST, AND IT IS THE ACCEPTANCE CRITERION

> *"Material continues separately through its real Field-to-material path. **Do
> not close that half merely because authored terrain materials render; verify
> that a Field material write changes the intended consumer.**"*

**TERRAINMAT already did the authored half** — `terrmat backed=128 orphan=0`,
`texture fragments=1216 samples=1216`, terrain draws and textures. **That is
explicitly NOT this.** A rendered authored material is the thing the owner names
as insufficient.

**What closes it: a `FIELD.WRITE.MATERIAL` write, through the real path,
changing a pixel or a consumer's observable state — demonstrated.**

## What you have to decide, and it is a BUILD not a question

1. **The encoding.** u8 triple ↔ the 32-bit lane. **Name it, version it, put it
   in ONE place, and make the reference and the RTL agree.** `compose_material`
   is the ratified law and it speaks triples; the u32 is a transport. **Never
   silently overload a field, truncate a handle, or substitute a convenient
   zero.** The directive grants explicit authority to amend a record schema by a
   **versioned extension or immutable sidecar** — use it rather than stealing
   bits.
2. **The second writer and its arbitration.** `compcache_front` has one material
   write face with one driver. Authored layer E and a field result now both want
   it. **Say what wins, why, and what happens when both arrive in the same
   cycle** — and make that rule *reachable* in a test, not merely written down.
3. **The downstream reader.** A field material that arrives and is never read
   fails the owner's test exactly as the authored triple does today.

**If the honest answer to any of these is that it cannot be done inside this
packet, say so WITH THE MEASUREMENT** — that is a full deliverable. What is not
acceptable is a lane that computes and nobody reads, which is the specific thing
the owner forbade.

## The fences

* **DO NOT REGRESS VELOCITY.** It reaches `zhao_part_collide` today —
  `terrain_veljoin_directed` 19/0, `part_terrain_tap_directed` 1297/0. It rides
  the **vertex-major** per-vertex lane stream and takes `terr_pt_fld_covers_o`.
* **Do not touch `nav_cost_o`** beyond leaving it classified and produced.
* **Do not raise `FAB_LANES`, `FAB_GROUP_PTS` or `FRONT_PTS`.** LANESCOST priced
  them and the console keeps `.FAB_LANES(1)` and the scalar front.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — the fourth Quartus
  17.0.2 inference killer, which cost `zhao_geom_arenabin` **146,414 registers
  against 1,010** for the identical circuit. Generate-**IF** infers; module scope
  infers; **the LOOP is the killer.** `check_ram_inference.py` rule 6 catches it
  now, and the checker was **100% false alarms and 100% miss** on that file
  before — **its silence is not a verdict.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## Evidence bar

* **A Field material write changing the intended consumer** — the owner's test,
  demonstrated end to end, not argued from port connectivity.
* **The encoding exercised in both directions**, with a value that is not
  symmetric under a swapped or truncated layout — **`{1,1,1}` proves nothing.**
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Nine packets this week found their own controls vacuous — one measured
  **refusals** and called it a 62× speed-up, one asserted three counters fired
  when its stimulus could not reach one, and one printed *"expected 0x1, got
  0x1"* **on failure**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — `| tail`
  reports TAIL's exit code, which LANESCOST hit on its very first use of it.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **`UNUSEDSIGNAL` IS WAIVED ACROSS WHOLE DIRECTORIES** by
  `tests/shell/v3_closure_inherited.vlt`. **A dead wire you create raises
  nothing.** Count readers by hand.
* **GATE 31: `check_console_closure_lint.py`** — run after any port change. **It
  deduplicates its source list** while `run_console_board_lint.ps1` does not, so
  a duplicated line in `fit_targets.yml` makes it say OK twice.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper mutants.**
* **Regenerating a generated file is part of the change** — five went stale this
  week, three on merges.
* **`git show` hands back INDEX content at LF while the working copy is CRLF**,
  so a diff of a correct file can claim thousands of changed lines.
* **Do not edit RTL while a build or a suite is reading it** — a suite whose
  inputs moved is not evidence in either direction.
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree**, and the other lane's tree is live.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **The encoding** — where it lives, how it is versioned, and the test that
   exercises it both ways with an asymmetric value.
3. **The arbitration rule** between authored layer E and the field result, and
   the test that reaches it.
4. **The owner's test: a Field material write changing the intended consumer.**
5. **What happened to velocity's chain**, demonstrated.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/materialpath` only.** Never `--force`.
