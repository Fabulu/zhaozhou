# FINDINGS -- packet TRIMERGE (entry I13), 2026-09-23

Branch `gz/trimerge`, based on `claude/ceiling-architecture-20260912` at `f81b37a8`.

## Register and gates

| | before | after |
|---|---|---|
| `python tools/budget/completion_register.py` | **12** (8 tie-offs + 4 disconnected) | **12** |
| `python tools/maintenance/gate_sweep.py` | **RC 0** (24 gates; 2 legitimately red on inherited debt) | **RC 0** |

**The register did not move and NO PIXEL MOVED. Both are deliberate.** I13 is
refused, for the reason below, and nothing was composed.

---

## THE VERDICT: I13 IS REFUSED, AND ITS SHAPE CHANGES

I13 said two things were absent: **(a) a two-producer triangle merge into
GEOM.CLIP** and **(b) terrain's attribute packet (`invw24` + lit r/g/b)**.

**(a) IS DEAD. The merge exists, is composed in `zhao_console_core.sv`, and has
three clients.** What remains of (a) is `NCLIENT` 3 -> 4, not a build.

**(b)'s `invw24` half is real, fully specified, and cheaper than the entry
says** -- terrain's `w` is already at the core's edge, so unlike the particle arm
nothing upstream has to grow a lane.

**(b)'s COLOUR half is the only real blocker, and it is an owner decision this
tree has explicitly PARKED.**

---

## PREMISE KILLS (five; three from my own brief or the entry itself)

### 1. "GEOM.CLIP has exactly ONE triangle producer; there is NO TWO-PRODUCER MERGE" -- FALSE

`zhao_geom_clipdoor` (`fpga/rtl/geometry/zhao_geom_clipdoor.sv`, first line
*"N PRODUCERS ON ONE zhao_geom_clip INPUT"*) is instantiated in
`zhao_console_core.sv` as `u_geom_clipdoor` with **`.NCLIENT (3)`** -- slice 0
GEOM.REPLAY, slice 1 FORGE.ASSEMBLE, slice 2 PART.CLIPFEED.

**Why two prior re-measurements (gz/cfgarm 2026-09-21, gz/projclose 2026-09-22)
missed it, which is the transferable part.** Both evaluated
`u_geom_clip.tri_valid_i`, found `cl_in_valid = mw_t_valid && !cl_in_refuse_c`,
and read one wire as one producer. **That expression is still exactly true.** The
door sits UPSTREAM of `u_material_window`, and the window is a pure combinational
gate -- so a three-client arbiter and a one-wire input are the same picture from
two ends. The entry had warned about this failure *in the opposite direction*
("Anybody grepping for `rp_o_valid` at `u_geom_clip` will not find it and must
not read that as progress") and then committed its mirror image twice.
**Walk the chain; do not read the port.**

### 2. All THREE obligations the entry billed for a second producer -- DISCHARGED

1. *"it must fire `d_enter_i`"* -- the door's single output IS the window's
   input, and `d_enter_i` is `cl_in_valid && cl_in_ready`, common to every
   client. Structural.
2. *"it must carry a `{material_set, material_id}` pair -- TERRAIN HAS NEITHER"*
   -- I re-ran the search (not quoted): **still 0 hits** across
   `fpga/rtl/terrain/`. **It no longer matters.** Owner ruling 1 of 2026-09-22
   made `MATMODE_NONE_C` a lawful *declared* mode; a zero `{set,id}` pair is what
   `mode_contra_c` **requires**, not a tie-off.
3. *"`err_unpublished_o`'s premise is falsified by any second door"* -- **no
   second door was built**; every client passes THROUGH the window. Premise
   intact.

### 3. "This console holds TWO `zhao_geom_depthquant_stream`" -- there are THREE

GEOM.VATTR, FORGE.ASSEMBLE **and `zhao_part_clipfeed`**, all composed in the
core. A fourth instance is an exercised pattern, not a novelty. (The DSP argument
against it is untouched.)

### 4. MY BRIEF's `zhao_geom_wcache` premise -- FALSE

The brief said its 75-bit payload "was never widened ... a live known defect".
**It was widened 75 -> 106 on 2026-09-09**, and that file's own header calls the
widening *"a REPAIR"*. Terrain does not use the block regardless -- its shell is
`zhao_terrain_wcache`, 106 bits, carrying `fill_w_i` under an elaboration
`$fatal` on the field map. **The cheque was cashed.**

### 5. The entry's mosaic-residency bound -- FALSE about residency

The entry told the next lane to *"confirm the consumer's residency first"*,
asserting *"THIS core does not instantiate that top"*. True of **direct**
instantiation; **false about residency**, which is what the sentence was used
for. Closure, every link unconditional at module scope:

    zhao_console_core -> zhao_shell_top_v2 -> zhao_geom_bin_pipe_v2
      -> zhao_raster_tile_pipe_v2 -> zhao_raster_texture_stage_v3
      -> zhao_texture_island_v3_top -> zhao_texture_mosaic_v2

Settled not by grep but by the authoritative instrument --
`design/prod_manifest.yml`: *"zhao_texture_mosaic_v2 is reachable inside the
selected V3 root"*. **The consumer is resident.**

---

## INSTRUMENT DEFECTS FOUND

* **A stale quotation in two places at once.** `zhao_geom_clipdoor.sv`'s header
  and `design/contracts/GEOM.CLIPDOOR.md` both quoted the material window's body
  as `assign t_ready_o = t_ready_i && pass_c;`. Production is
  `assign t_ready_o = refuse_c || (t_ready_i && pass_c);` -- the `refuse_c ||`
  term is what CONSUMES a refused primitive instead of stalling it. The argument
  built on the quote is unaffected (both forms are unbuffered), but a copied
  citation went wrong in both homes simultaneously. **Both corrected in this
  packet**, with the reason beside them.
* **My own grep was an instrument that lied.** Searching
  `^\s*zhao_texture_mosaic\s` for instantiations returned only the v1 island, and
  I nearly concluded terrain's texturing path was stranded on a superseded top.
  The composed one is `zhao_texture_mosaic_v2`; the trailing `\s` excluded the
  `_v2` sibling. That is CLAUDE.md's *"a grep for `_v2$` finds half of it"*
  committed by the lane investigating it. Caught by tracing the closure by hand.
* **A subagent's "definitive NO" measured the narrower question.** One scout
  reported mosaic residency as a definitive no, having measured *direct*
  instantiation in `zhao_console_core.sv` only. Correct as measured, wrong as
  concluded. Resolved by hand against the prod manifest. **Two measurements
  disagreeing is the cheapest signal available; neither was taken on trust.**

**No new counters were added, so none is owed a demonstration that it fires.**

---

## THE OWNER DECISION, WITH EVIDENCE AND A RECOMMENDATION

**The decision:** terrain's `lit r/g/b`. Terrain owns **one signed 32-bit
scalar** flat shade (`terr_light_base_o`); GEOM.CLIP's slots 3-5 want three
channels, and since **R234 D1** they are read and delivered to the fragment.
`zhao_geom_attrpack`'s Gouraud lanes are deliberately **not** branched on
`tri_untex_i` (*"an untextured primitive is still lit"*), so R197's untex
declaration buys terrain past u/w and v/w and **does not** buy it past colour.

**Its current status is PARKED, not open.** `reports/OWNER-DECISIONS-20260920.md`
section 5 carries terrain's two absent laws and its own recommendation is
*"**Do not rule these yet, and that is the recommendation** ... not because they
are ripe."* `design/contracts/GEOM.CLIP.md` agrees from the other side: terrain's
lit r/g/b *"is terrain art content and remains the owner's (dossier decision 5)"*.

**Both ratified profiles were priced against the tree, and NEITHER is composable
today -- which is a more useful finding than "the colour is art":**

* **Untextured**, `lit(base) = (base * shade + 32768) >> 16`. Needs a **base
  colour**. In the oracle that is the patch material's `mat.r/g/b`; in RTL there
  is none -- `zhao_terrain_patch`'s vertex stream is **heights only**, and the
  projector's `mat_a`/`mat_b`/`weight` are layer-E **tile ids**, *"forwarded,
  never selected"*. The cheap-looking profile is the one with the missing
  operand.
* **Textured**, `mod_of(shade, tint, sheet)`, **exact at all-unity** by the
  oracle's own sentence. "Tint absent" has a **ratified identity** (RGB565
  `0xFFFF`, which `cell_tint` already defaults to, giving exactly 65536 in
  Q16.16), so **a unity tint is not the stand-in this entry refused three times**
  -- it is an unauthored layer at its exact identity. What blocks this route is
  **not the colour**: it is three *carriage* items -- terrain's u/v (law FROZEN
  and computable from `zhao_terrain_project`'s own input ports, but its output
  packet has nowhere to put the result), the 224-bit **aux surface context** (no
  producer anywhere, and `zhao_raster_tile_pipe_v2` **aborts the frame** on a
  non-zero one), and a terrain **material identity** so the window publishes a
  sampling material -- which `MATMODE_NONE` by construction does not.

### RECOMMENDATION -- a sequence, not a ruling

Under R234 D1 the owner has already taken the expensive option once and said
*"we just want the full capability"*, so the flat stand-in is very likely the
wrong answer again. But **the textured profile is not one decision.** It is:

1. terrain u/v carriage (frozen law -- engineering),
2. an aux surface context producer (engineering),
3. a terrain material identity (engineering),
4. **then** colour, which can ship at the tint's exact identity **honestly**,
5. and layer-H tint **content** -- the only art item -- last.

**THE PARKED OWNER DECISION IS NOT BLOCKING THE NEXT THREE PIECES OF WORK.**
Every pass that has stopped at I13 stopped at item 5 instead of item 1. That
reordering is this packet's main offering, and it does not require the owner to
rule anything today.

---

## A CONSTRAINT THE ENTRY DID NOT CARRY

**The console smoke cannot prove a terrain arm.** Every terrain page the smoke
plays fails its CRC, so no page becomes resident and terrain emits **no triangle
at all**. A terrain arm must land with an **acceptance bench**, as PARTMAT proved
the particle arm with `tests/prod/partmat_acceptance.cpp`, and
`raster pixels=2560` **will not move**. A packet planning to quote the smoke as
its evidence is planning to quote a fixture that never reaches its path. Named
now so it is priced rather than discovered at the end.

---

## WHAT I DID NOT DO, AND WHY

* **I did not build the depthquant pair, the packer or the fourth clipdoor
  client**, though all three have worked templates and could have been written
  today. With slots 3-5 carrying an invented colour they would be a **prefix of a
  chain whose last link does not exist** -- *"a tie-off wearing a composition's
  clothes"*, the campaign's first prohibition -- and the register would have
  moved while not one pixel did.
* **I did not narrow `GEOM_CLIP_ATTRS`.** Re-affirmed a fourth time.
* **I did not run a Quartus fit.** Every question here was structural and
  answered in seconds.
* **I did not add a counter**, so none is owed a firing demonstration.

## FILES CHANGED

* `fpga/rtl/prod/zhao_console_core.sv` -- the I13 entry, re-measured and
  corrected (comment only; no RTL).
* `fpga/rtl/geometry/zhao_geom_clipdoor.sv` -- stale header quotation corrected
  (comment only).
* `design/contracts/GEOM.CLIPDOOR.md` -- the same quotation, same correction.
* this file.