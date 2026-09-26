# CARRIAGE — I13's item 3, with both laws BUILT and the drain objection DISSOLVED

**Branch `gz/carriage`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Four packets have refused `I13`. You are the first with the ground cleared.**
Read the entry (`grep -n '^// I13\.'`, never a line number) — it is the longest
in the file and its last sections are SHADELADDER's and FABRICSINK's, both from
2026-09-26, both measurements.

## What changed under this entry in ONE day

* **BOTH ARITHMETIC LAWS ARE BUILT.** SHADELADDER landed
  `fpga/rtl/terrain/zhao_terrain_shademod.sv` (`mod_of` bit-exact, **FIVE**
  rungs, one rounding, 0 DSP, 17 clocks) and
  `fpga/rtl/geometry/zhao_geom_overw_sat.sv` (the S8.24 saturate on the s32
  coordinate), plus `zref::geom_over_w_wide` proven a strict extension of the
  ratified narrow form. **Both are registered as OPEN deferrals with explicit
  delete conditions — they are waiting for exactly this packet.**
* **The entry's "FOUR values" was FALSE — there are FIVE rungs `{0,1,2,3,4}`**,
  and **rung zero is reachable and BLACK**. A 2-bit ladder could not express a
  top-surface triangle turned from the sun. That is fixed in the entry and in
  the built block; **do not reintroduce a 2-bit assumption anywhere.**
* **THE DRAIN OBJECTION IS MEASURABLY WRONG, in your favour.** FABRICSINK
  measured `zhao_material_window.sv:415-420`'s `match_c`: five terms — mode,
  vertex_alpha, frag_state, material_set, material_id — and **`base_rgb` and
  `recipe_weight`, the exact bits the mosaic slices, are NOT among them.** A
  **per-triangle** triple on the rider costs **ZERO DRAINS.** The drain price is
  real only for per-cell `{material_set, material_id}`, which nobody proposes.
* **The route is REAL and already per-triangle at the boundary.** The authored
  layer-E triple walks **eight composed hops** —
  `zhao_console_core.sv:24057 → 24746 → 17418 → 17720 → 17759/17085 → 17855 →
  17889/17067 → 18102 → 18134/10522` — and dies at `proj_out_*`, a dangling
  top-level output of core **and** board (`:2721-2742`, `:5433-5452`).
  **Per-triangle is the granularity "never interpolate identifiers" requires**,
  so the hard part is already right.
* **Past that boundary the mosaic's material bytes are a COMPILE-TIME
  CONSTANT** — `MAT_BASE_RGB_C = 24'hFF_FF_FF` (`:26924`) → flat request
  (`:26937/26944`) → `zhao_raster_texture_stage_v3.sv:397` →
  `zhao_texture_island_v3_top.sv:1097-1098` → `u_mosaic` (`:1303`).
  `req_mat_a_i == req_mat_b_i == 8'hFF` on **every fragment drawn today.**
* **CELLCARRY measured the carriage is a THREE-FILE job, not eleven**: 32 bits
  **already ride the flat request per triangle.**

**So: the consumer is resident, the route is live and per-triangle, the laws
exist as blocks, and the carriage costs no drains.** If you refuse this, refuse
it on something none of those five packets found — and say so plainly.

## The job

1. **Compose the two built blocks** and carry the per-triangle layer-E triple
   across `proj_out_*` into the flat request, replacing `MAT_BASE_RGB_C`.
2. **`invw24`** — terrain's raw `w` is at the core's edge on
   `proj_out_aw_o/bw_o/cw_o` ([30:0] fx16, **not** 1/w). It needs a
   `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4` pair and a `pack_attr`
   analogue. **`zhao_forge_assemble`'s `u_dq`/`u_rcp`/`pack_attr` is the
   template**, and this console already holds two such streams.
3. **The fourth `zhao_geom_clipdoor` client** — it is composed with
   `.NCLIENT (3)`; terrain is a fourth, not a new mechanism.

**A pixel that changes is the deliverable.** If you get the triple to the mosaic
and `invw24` defeats you, say so — that is still the entry moving for the first
time in four passes.

## The fences

* **`GEOM_CLIP_ATTRS` STAYS 7.** *"The slots are EMPTY, not SPARE."*
* **No unnamed flat colour stand-in.** `terrain_rules` 6.5 makes layer-H tint
  **per-vertex**; broadcasting the scalar shade into slots 3..5 **removes
  Gouraud**, which the owner's directive prohibits outright. If a flat value is
  ever right it must be **NAMED a stand-in in the RTL with 6.5 cited beside
  it**, and it is an escalation.
* **Do not compose `zhao_terrain_normalmap` for a free register point.** A
  prefix of a chain whose last link does not exist is this entry's first
  prohibition and four packets have refused it.
* **`OWNER-DECISIONS` §2's "no port change on a composed block" is a FALSE
  PRESENCE** — it cites `zhao_raster_texjoin_v2`, which has **zero
  instantiations** and no `detail_i`/`detail_o` anywhere. The normal map needs a
  detail port **built into a composed block**. Do not plan around that sentence.
* **§5 of `OWNER-DECISIONS-20260920.md` does NOT fence you** — Decision Record 3.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`.**

## And one live claim nobody has settled

*"Terrain has no material identity"* is being carried through this tree as
*"terrain CANNOT present one material"*. **FABRICSINK found the second claim
false four ways** — `tileset_id` (`terrain_rules.md:143`, `hdrread.sv:53`), the
oracle's rim and underside runs (`terrain.cpp:673-675`, `773-775`), weight
0/255, and ruling **R13 explicitly refusing it**. The literal zero-hit grep
measures **a naming boundary between lanes, not an absence.** If your work turns
on it, measure it; do not inherit either version.

## Evidence bar

* **A pixel that changes**, through real production modules wired port-for-port
  as `zhao_console_core` wires them, on an acceptance bench of your own —
  `tests/prod/terrainaux_acceptance.cpp` is the pattern.
* **The per-cell triple arriving at `zhao_texture_mosaic_v2` with the right
  values for the right cell**, checked against the layer-E source, not against
  your own carriage bookkeeping.
* **The ladder and the saturate still bit-exact once composed** — SHADELADDER's
  benches are committed; run them against the composed path, and note its
  lesson: **rung centres are exactly where the ladder is invisible**, so a
  control placed there is vacuous.
* **Prove every counter you quote**; a guard unreachable with legal stimulus
  needs a **committed mutant** under `tests/mutants/`, renamed so no source list
  elaborates it, polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's** — `| tail` reports `tail`'s status and two packets misread a
  register that way today.
* **GATE 31: `tools/quartus/check_console_closure_lint.py`.** Run it after any
  port change — 32 s, and it refuses IMPLICIT/MODMISSING/PINMISSING on the real
  fit closure. It exists because a dead clock shipped this morning.
* **`check_entry_claims` keys on PROSE.** Writing "X is not composed" about a
  composed module in RTL comments turns the gate red even when you are quoting a
  claim to reject it. **Reword rather than baseline.**
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`.
* **`zhao_console_core.sv` is a SHARED HOT FILE and another packet is in it.**
  Stage your **hunk**; never `git add <file>`; never `git checkout --`.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether
   `zhao_terrain_normalmap` left the `BUILT BUT NOT CONNECTED` list.
2. **The pixel** — the stimulus, the chain, the oracle it agrees with. **If
   nothing moved, say that FIRST and say why**, and refuse rather than compose a
   prefix.
3. **How far `invw24` and the fourth clipdoor client got.**
4. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
5. **What you refused**, especially any stand-in you declined to ship unnamed.
6. **Anything you got wrong and caught yourself.** SHADELADDER found its own
   named control was VACUOUS and FABRICSINK caught a truncated grep that would
   have made its refusal look stronger. That is the standard.
7. Branch and commit hash. **Push `gz/carriage` only.** Never `--force`.
