# TERRTRI — I13's terrain triangle, now that two of its three carriage items are built

**Branch `gz/terrtri`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I13` and `zhao_terrain_normalmap` are two of the register's six, and
`zhao_terrain_normalmap` is the LAST entry on the `BUILT BUT NOT CONNECTED`
list.**

## Read the entry, and budget real time for it

`fpga/rtl/prod/zhao_console_core.sv`, entry **I13** — find it with
`grep -n '^// I13\.'`, never by line number. It is the longest entry in the file
because **nine packets have re-measured it and six of them found one of its own
sentences false.** Read all of it, including the amendments that contradict
earlier amendments. The entry is an instrument, not a narrative, and the newest
paragraph wins.

**Do not brief yourself off this document instead of that one.** What follows is
only the state of play as I merged it.

## What is now BUILT that the entry's older paragraphs say is missing

Three things a 2026-09-22 reader would have called blockers are closed:

* **The merge is DEAD as a blocker.** `zhao_geom_clipdoor` is composed as
  `u_geom_clipdoor` with `.NCLIENT (3)` — GEOM.REPLAY, FORGE.ASSEMBLE,
  PART.CLIPFEED. **Terrain is a fourth client, not a new mechanism.** Every
  sentence above the 2026-09-23 trimerge note saying "one producer chain through
  `u_material_window`'s gate, no merge" is reading the wrong side of the window.
* **Terrain's u/v is BUILT AND COMPOSED.** `zhao_terrain_uvlane` leaves this
  module on `terr_uv_*` in Q16.16 tile units, tagged with the same `src_id` the
  triangle and the light carry. It deliberately refuses walls — `terrain_rules`
  6.6's wall U accumulates rim length in lattice scan order and needs
  FORGE.CLIFF's rim plan walk, whose emission stage is not written. The
  tessellator emits top and underside only, so that is the whole debt today.
* **The aux surface context has a producer**, as of the TERRAINAUX merge:
  `zhao_texture_sheetmod`, with `zhao_surface_sheetshare` widened 2 -> 3 clients
  and the shell's seven `pg_*` ports added. The entry's *"224-bit aux surface
  context (NO producer anywhere)"* predates that. **Re-measure it; do not take
  my word and do not take the entry's.**

## What is left, as best I can establish

1. **`invw24` — fully specified, no upstream growth needed.** Terrain's raw `w`
   is already at this module's edge on `proj_out_aw_o/bw_o/cw_o` ([30:0] fx16 —
   NOT 1/w, that is the separate `proj_out_ad_o/bd_o/cd_o` lane), with the
   profile on `proj_fill_profile_o`. What is needed is a
   `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4` pair and a `pack_attr`
   analogue. **`zhao_forge_assemble`'s `u_dq`/`u_rcp`/`pack_attr` is the
   template**, and note that this console already holds two depthquant streams,
   so a second instance is a route the tree has exercised — the entry records
   that neutrally, against its own recommendation of an arbiter.
2. **The fourth clipdoor client**, with the light needing no join FIFO: the
   composer already rate-locks the two streams, taking a terrain reference only
   when replay AND light can both accept it.
3. **Lit r/g/b — and READ THIS BEFORE YOU PLAN AROUND IT**, because the entry's
   own latest measurement moves it somewhere surprising.

### The colour is not the blocker on the textured route

Both ratified profiles were priced against the tree and **neither is composable
today, which is a different and more useful finding than "the colour is art"**:

* **UNTEXTURED** — `lit(base) = (base * shade + 32768) >> 16`. It needs a BASE
  COLOUR, and **there is no terrain material colour anywhere in RTL**:
  `zhao_terrain_patch`'s vertex stream is heights only, and the projector's
  `mat_a`/`mat_b`/`weight` are layer-E tile ids, *"forwarded, never selected"*.
  **The cheapest-looking profile is the one with the missing operand.**
* **TEXTURED** — `mod_of(shade, tint, sheet)`, exact at all-unity by the
  oracle's own sentence. **"Tint absent" has a RATIFIED IDENTITY** — RGB565
  `0xFFFF`, which `cell_tint` already defaults to, giving exactly 65536 in
  Q16.16. **A unity tint is therefore NOT the stand-in this entry refused three
  times; it is an unauthored layer sitting at its exact identity.**

So what blocks the textured route is **carriage**, and TERRAINAUX measured the
remaining item: the mosaic's `{tile_a, tile_b, weight}` is built from
`base_rgb[23:16]` / `[15:8]` plus `recipe_weight`, **and both are PER SPAN** — so
terrain's **per-cell** layer-E triple has **no carriage at all.** That, and not a
colour ruling, is the live blocker.

**R197's untex declaration does NOT buy you past this.**
`zhao_geom_attrpack`'s Gouraud lanes are deliberately not branched on
`tri_untex_i` — *"an untextured primitive is still lit"* — so untex buys a
terrain triangle past u/w and v/w and **not** past lit r/g/b. After R234 D1 those
slots are read and delivered to the fragment. Terrain must produce three channels
and owns **one scalar**, `terr_light_base_o`.

## The fences

* **`GEOM_CLIP_ATTRS = 7` STAYS 7.** "Four slots nobody reads" is stale (six of
  seven have readers since D1), and even when it was true the entry refused the
  narrowing in advance: *"the slots are EMPTY, not SPARE"*, and narrowing would
  *"close the distance to a gap by deleting the place the answer lands."*
* **Do not invent a per-vertex terrain colour and do not ship a flat stand-in
  unnamed.** `spec/terrain_rules.md` 6.5 makes layer-H tint PER-VERTEX by
  ratified spec. If a flat stand-in is the right call on the ALM budget — and it
  may be — **it must be NAMED a stand-in in the RTL with 6.5 cited beside it**,
  or the next reader inherits a Gouraud law silently implemented as a constant.
* **`reports/OWNER-DECISIONS-20260920.md` §5 PARKED terrain's two absent laws**
  with its own recommendation *"Do not rule these yet"*. You may not un-park
  them. You MAY establish that one of them is no longer a ruling question because
  its operand exists — which is exactly what the unity-tint identity above
  argues. **State which you are doing.**
* **A prefix of a chain whose last link does not exist is "a tie-off wearing a
  composition's clothes"** — the file's first prohibition. The previous lane
  built nothing for exactly this reason and was right to. **If you reach that
  position, stop and say so; a refusal with evidence outranks a moved number.**

## The fixture trap, stated so you do not plan your evidence around it

**`raster pixels=2560` WILL NOT MOVE, and a packet quoting the console smoke as
its evidence is quoting a fixture that never reaches its path.** The smoke's
terrain pages fail their CRC, so no page becomes resident and terrain emits no
triangle at all. A terrain arm lands with an **ACCEPTANCE BENCH** — the way
PARTMAT proved the particle arm in `tests/prod/partmat_acceptance.cpp`, and
TERRAINAUX proved the aux arm in `tests/prod/terrainaux_acceptance.cpp` with 758
checks ending in a pixel that changes.

**But re-measure that too**: TERRAINAUX repaired the terrain fixture in the same
merge (`degenerate=128 -> 0`, `refs_taken 128 -> 256`) after finding
`zhao_terrain_seq` walks a set once and skips non-resident patches, so the
compose door never opens and `compcache_front` serves poison `32'h5BADF00D`.
Whether that changes what the smoke can reach is a measurement, not an
assumption.

## Evidence bar

* **A pixel that changes**, through real production modules wired port-for-port
  as `zhao_console_core` wires them, on an acceptance bench of your own.
* **Cross-check against the oracle** — `reference/src/zrender/terrain.cpp`, and
  note it builds no material and no nav lattice, so it cannot settle everything.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison**. A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  driver polarity inverted so it passes when the counter FIRES. Register positive
  controls with the right polarity.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — it
  classifies by scanning prose, and a recent packet relabelled an entry by
  writing "TIED TO ZERO" about a tie it had just removed.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's**.
* **Regenerate `zhao_prod_top.sv` after ANY port change** and re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit** —
  `mutant_copy_drift` keys on commit time and reverting does not clear it.
* **Verilator lint-clean is not Quartus-synthesizable.**
* **DO NOT start a console or full-device fit.** Verilator answers your questions
  in seconds; a new block is in no running fit's closure.
* **A grep for a phrase is not a search for the concept**, and this entry is
  where that law was coined twice — `zhao_texture_mosaic` followed by whitespace
  does not find the composed `_v2`, and every other `ATTRS` hit was the substring
  inside `ATTRSETUP`. Open the file that would own the capability.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **The register before and after, measured BARE**, and whether
   `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **What moved** — the pixel, the stimulus, the chain, the oracle it agrees
   with. If nothing moved, say that first and say why.
3. **Which of this brief's and the entry's claims you found FALSE.** Nine packets
   have each found at least one; expect to.
4. **Whether the textured route's carriage question is answerable** without
   un-parking §5's decision, and on what evidence.
5. **What you refused**, especially any stand-in you declined to ship unnamed.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/terrtri` only.** Never rebase or push
   the shared branch, and never `--force`.
