# CELLCARRY — I13's last blocker is CARRIAGE, and the consumer is already resident

**Branch `gz/cellcarry`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I13` and `zhao_terrain_normalmap` are two of the register's five, and
`zhao_terrain_normalmap` is the LAST entry on the `BUILT BUT NOT CONNECTED`
list.**

## Read the entry, and know that TWO packets have already refused it

`grep -n '^// I13\.'` — never a line number. It is the longest entry in the file:
**ten packets have re-measured it and six found one of its own sentences false.**
The newest paragraph wins.

**TERRTRI refused it on 2026-09-26 and was right to.** Composing
`zhao_terrain_normalmap` alone would move the register 5 → 4 while changing not
one pixel — *"a prefix of a chain whose last link does not exist"*, this file's
first prohibition. **Do not close the entry that way, and if you end up in the
same position, refuse it again and say so.**

## What is already TRUE, so you do not re-derive it

Four things a 2026-09-22 reader would call blockers are gone:

* **The merge is dead as a blocker.** `zhao_geom_clipdoor` is composed with
  `.NCLIENT (3)`; terrain is a **fourth client**, not a new mechanism.
* **Terrain's u/v is built and composed** — `zhao_terrain_uvlane`, Q16.16 tile
  units, on `terr_uv_*` under the triangle's own `src_id`. It refuses walls
  deliberately: `terrain_rules` 6.6's wall U needs FORGE.CLIFF's rim plan walk,
  whose emission stage is not written, and the tessellator emits top and
  underside only.
* **The aux surface context has a producer** — `zhao_texture_sheetmod`, with the
  sheet share widened 2 → 3 clients (TERRAINAUX).
* **The colour is NOT the blocker.** `mod_of(shade, tint, sheet)` is exact at
  all-unity by the oracle's own sentence, and **"tint absent" has a RATIFIED
  IDENTITY** — RGB565 `0xFFFF`, which `cell_tint` already defaults to, giving
  exactly 65536 in Q16.16. A unity tint is **not** the stand-in this entry
  refused three times; it is an unauthored layer at its exact identity.

## THE BLOCKER, and it is why this packet is called CELLCARRY

The mosaic's `{tile_a, tile_b, weight}` is built from `base_rgb[23:16]` /
`[15:8]` plus `recipe_weight` — **and both of those are PER SPAN.** Terrain's
layer-E triple is **PER CELL**. There is no carriage between them.

**That is the whole of it, and unlike every earlier version of this entry the
CONSUMER IS RESIDENT.** The trimerge note settled it: the closure is this file →
`zhao_shell_top_v2` → `zhao_geom_bin_pipe_v2` → `zhao_raster_tile_pipe_v2` →
`zhao_raster_texture_stage_v3` → `zhao_texture_island_v3_top` →
`zhao_texture_mosaic_v2`, **every link unconditional at module scope**, and
`design/prod_manifest.yml` says it outright: *"zhao_texture_mosaic_v2 is
reachable inside the selected V3 root."*

**So this is a producer-and-path job with a real last link**, which is exactly
what the last two refusals did not have.

**And re-measure that before you build on it.** A grep for `zhao_texture_mosaic`
followed by whitespace does NOT find the composed one — it is the `_v2` — which
is CLAUDE.md's *"a grep for `_v2$` finds half of it"* in this entry's own subject
matter. In this tree "X does not exist" runs false at a rate near one in two, and
every packet this week found at least one.

## The second half: `invw24`, fully specified

Terrain's raw `w` is already at this module's edge on `proj_out_aw_o/bw_o/cw_o`
([30:0] fx16 — **not** 1/w, that is the separate `proj_out_ad_o/bd_o/cd_o` lane),
with the profile on `proj_fill_profile_o`. What is needed is a
`zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4` pair and a `pack_attr`
analogue. **`zhao_forge_assemble`'s `u_dq`/`u_rcp`/`pack_attr` is the template**,
and the console already holds two depthquant streams, so a second instance is a
route the tree has exercised — the entry records that neutrally, against its own
recommendation of an arbiter.

## The fences

* **`GEOM_CLIP_ATTRS` STAYS 7.** *"The slots are EMPTY, not SPARE"*, and
  narrowing would *"close the distance to a gap by deleting the place the answer
  lands."*
* **No unnamed flat colour stand-in.** `terrain_rules` 6.5 makes layer-H tint
  PER-VERTEX by ratified spec. If a flat value is ever right on the ALM budget it
  must be **NAMED a stand-in in the RTL with 6.5 cited beside it** — and it is an
  escalation, because the owner's directive prohibits *"remove Gouraud/detail
  normals"*.
* **§5 of `OWNER-DECISIONS-20260920.md` does NOT fence you.** Decision Record 3
  in `reports/OWNER-RULINGS-20260919-EVENING.md` rules it out: §5's own header
  says *"PARKED, NOT LIVE"* and *"it asks the owner for nothing"*, and both its
  premises have expired. An earlier brief of mine said otherwise and is corrected
  there. **The engineering decision is yours; the capability is not.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`** — the default is the
  SHIPPING part, the console's `@edgeclose` row is on the SIZING part, and I
  differenced those two today and read a 247-block DSP fall as a win.

## The fixture, stated so you do not plan your evidence around it

**`raster pixels=2560` will not move**, and a packet quoting the console smoke as
its evidence is quoting a fixture that never reaches its path: the smoke's
terrain pages fail their CRC, so no page becomes resident and terrain emits no
triangle. **A terrain arm lands with an ACCEPTANCE BENCH** —
`tests/prod/terrainaux_acceptance.cpp` (758 checks ending in a pixel that
changes) and `tests/prod/partmat_acceptance.cpp` are the patterns.

**But re-measure that too.** TERRAINAUX repaired the terrain fixture
(`degenerate=128 → 0`, `refs_taken 128 → 256`) in the same merge that refused
this entry, so whether it changes what the smoke can reach is a measurement.

## Evidence bar

* **A pixel that changes**, through real production modules wired port-for-port
  as `zhao_console_core` wires them, on an acceptance bench of your own.
* **The per-cell triple arriving at `zhao_texture_mosaic_v2`** with the right
  values for the right cell — checked against the layer-E source, not against
  your own carriage bookkeeping.
* **Cross-check against the oracle** (`reference/src/zrender/terrain.cpp`), which
  builds no material and no nav lattice and so cannot settle everything.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison**. A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  driver polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — it
  classifies by scanning prose, and a packet relabelled an entry this week by
  writing "TIED TO ZERO" about a tie it had just removed.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* leaves `$LASTEXITCODE` carrying the previous command's value.
* **`gate_sweep` does not run the console smoke controls.** Run them yourself
  with `@splat`; `& script $sw` binds the switch as a positional path and
  silently does nothing.
* **Regenerate `zhao_prod_top.sv` after ANY port change** and re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **The register before and after, measured BARE**, and whether
   `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **What moved** — the pixel, the stimulus, the chain, the oracle it agrees
   with. **If nothing moved, say that FIRST and say why**, and refuse rather than
   compose a prefix.
3. **Whether the mosaic consumer was really resident**, re-measured.
4. **Every claim in this brief or the entry you found FALSE.** Ten packets have
   each found at least one; expect to.
5. **What you refused**, especially any stand-in you declined to ship unnamed.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/cellcarry` only.** Never rebase or push
   the shared branch, and never `--force`.
