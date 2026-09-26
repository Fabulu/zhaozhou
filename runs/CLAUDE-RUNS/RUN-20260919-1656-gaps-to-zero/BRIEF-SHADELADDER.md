# SHADELADDER — I13, and BOTH "unsettled laws" are settled. They are UNBUILT.

**Branch `gz/shadeladder`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**The owner's instruction, 2026-09-26: close every gap before the console is
fitted.** A fit of a design with open gaps measures a machine that is not the
machine. `I13` and `zhao_terrain_normalmap` are two of the five, and
`zhao_terrain_normalmap` is the last entry on the `BUILT BUT NOT CONNECTED` list.

## Three packets have refused this entry. Read why, then read this.

`grep -n '^// I13\.'` — never a line number. TERRAINAUX, TERRTRI and CELLCARRY
each refused, each for a better reason than the last, and **each was right to**:
composing the normal map alone moves the register while changing no pixel.

**CELLCARRY's refusal is the one you are answering, and its framing is off by
one word.** It reported *"two ARITHMETIC LAWS are unsettled, and a law must be
settled before a wire is laid."* The first half of that sentence is wrong and the
second half is exactly right.

**BOTH LAWS ARE SETTLED. NEITHER IS BUILT.**

* **The shade ladder is FROZEN IN THE ORACLE, with its own comment naming it.**
  `reference/src/zrender/terrain.cpp`:
  ```cpp
  const int32_t shade_q = (shade + 8191) >> 14;   // the palette ladder (0..4)
  const __int128 prod = static_cast<__int128>(shade_q << 14) * tint_q * sheet_q;
  return static_cast<int32_t>(div_rhu_s128(prod, static_cast<__int128>(1) << 32));
  ```
  **ONE rounding over `shade_q × tint × sheet` in s128.** Nothing is awaiting a
  decision; the RTL does not do it. Zero hits tree-wide, and CELLCARRY's positive
  control fired.
* **The S8.24 bound is MANDATED IN `spec/qformats.md`**, in the table, in the
  column headed for exactly this: `u/v_over_w | s32 | S 8.24 | **saturate** |
  round-half-up`. **The spec already says saturate.** The RTL has no saturate and
  no clamp on that multiply.

**So there is nothing to ask anybody. There are two things to build**, and
building them is what makes it safe to lay the wire CELLCARRY correctly refused
to lay without them.

## What is already TRUE — re-measured by three packets, do not re-derive

* **The merge is not a blocker**: `zhao_geom_clipdoor` is composed with
  `.NCLIENT (3)`; terrain is a **fourth client**.
* **Terrain's u/v is built and composed** (`zhao_terrain_uvlane`, `terr_uv_*`,
  Q16.16 tile units, same `src_id`). It refuses walls deliberately — wall U needs
  FORGE.CLIFF's rim plan walk, whose emission stage is unwritten, and the
  tessellator emits top and underside only.
* **The mosaic consumer is RESIDENT**, re-measured by CELLCARRY: `u_mosaic` at
  `zhao_texture_island_v3_top.sv:1303`, the triple at `:1097-1099`.
* **The carriage is a THREE-FILE job, not eleven** — CELLCARRY measured that the
  entry's *"no carriage at all"* is wrong: **32 bits already ride the flat
  request per triangle.**
* **The terrain arm IS exercised.** 256 references taken, 256 coordinate packets
  emitted, **zero degenerate**. Terrain contributes no pixels because **its
  triangles reach no raster** — not because the bench is blind. That older caveat
  of mine was stale and is corrected.
* **`invw24` is fully specified**: terrain's raw `w` is already at the core's
  edge on `proj_out_aw_o/bw_o/cw_o` ([30:0] fx16 — *not* 1/w). Needs a
  `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4` pair and a `pack_attr`
  analogue; **`zhao_forge_assemble`'s `u_dq`/`u_rcp`/`pack_attr` is the
  template**, and this console already holds two such streams.

## The job, in order

1. **Build the ladder** — bit-exact against `mod_of`, **one rounding**, not two.
   The composed path today would be two 8-bit `unit_mul` roundings and no ladder;
   that is the divergence.
2. **Build the saturate** on the perspective multiply, per `qformats.md`.
3. **Then** the carriage, `invw24`, and the fourth clipdoor client.

**If 1 and 2 land and 3 does not, that is a good packet.** Say so and stop — the
laws being implemented is the thing that unblocks everyone after you.

## And one seam is WIDER than recorded

CELLCARRY found a **false PRESENCE**, which is rarer and worse than a false
absence: `OWNER-DECISIONS` §2 says the normal map needs *"no port change on a
composed block"*, citing `zhao_raster_texjoin_v2` — **zero instantiations, marked
not shell-connected, and no `detail_i`/`detail_o` anywhere under
`fpga/rtl/texture` or `fpga/rtl/raster`.** The normal map needs a detail port
**built into a composed block**. Do not plan around that sentence.

## The fences

* **`GEOM_CLIP_ATTRS` STAYS 7.** *"The slots are EMPTY, not SPARE."*
* **No unnamed flat colour stand-in.** `terrain_rules` 6.5 makes layer-H tint
  PER-VERTEX; broadcasting the scalar shade into slots 3..5 **removes Gouraud**,
  which the owner's directive prohibits. That is an escalation, not a choice.
* **Do not compose `zhao_terrain_normalmap` for a free register point.** A prefix
  of a chain whose last link does not exist is this file's first prohibition, and
  three packets have refused exactly that.
* **§5 of `OWNER-DECISIONS-20260920.md` does NOT fence you** — Decision Record 3.
* **Do NOT start a console or full-device fit**, and **every map you quote must
  name its `-Device`.**

## Evidence bar

* **Bit-exactness against the oracle**, over a real vertex sweep, with a
  **positive control that MOVES the result** — an identity that cannot fail is
  not evidence. This is the whole reason the ladder matters: get it wrong and the
  pixel is wrong against a capture-exact law while every gate passes.
* **The saturate demonstrated saturating**, on stimulus that exceeds the bound —
  and a negative control inside it that does not.
* **A pixel**, if you reach step 3, on an acceptance bench of your own.
  `tests/prod/terrainaux_acceptance.cpp` is the pattern.
* **Prove every counter you quote**; a guard unreachable with legal stimulus
  needs a **committed mutant** under `tests/mutants/`, renamed so no source list
  elaborates it, polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's.**
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`, and
  note that a hashtable splat through `powershell -File` stringifies the switches
  so they never start — that has produced false greens and false reds here on the
  same day.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE.**
2. **The ladder, bit-exact against `mod_of`**, with the control that moves.
3. **The saturate, demonstrated saturating.**
4. **How far you got on carriage/`invw24`/the fourth client** — and if the answer
   is "not at all", say it plainly; items 2 and 3 are worth the packet.
5. **Every claim in this brief or the entry you found FALSE.**
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/shadeladder` only.**
