# Zhaozhou Measurement Rules — MEASURE.HISTOGRAM's metric

**Status:** ratified 2026-09-20 by owner ruling **R70**
(`reports/OWNER-RULINGS-20260919-EVENING.md`), provisional at the
coordinator's recommendation. Single law for *what number goes in a bucket*.
Where this file and any other text disagree, this file wins for the histogram's
event metric.

This file exists because `design/contracts/MEASURE.HISTOGRAM.md` opens its list
of inventions with the one thing it did not make:

> "the error metric — what number goes in a bucket. Charter says 'candidate
> error buckets' and stops."

The block was then built **metric-agnostic on purpose**: it "takes an unsigned
magnitude of `EW` bits and declares nothing about what it measures." That is a
good property of the block and a hazard at the seam, and the hazard is the
reason this file is a spec and not a comment. A metric-agnostic organ cannot
notice that it is being fed the wrong quantity. **Nothing downstream can catch
a wrong choice here**, so the choice is written down where it can be checked
against rather than inferred from whatever happens to be wired.

---

## 1. The v1 metric: the terrain page-load LOD deviation

**An event is one subpatch's deviation record**, produced at PAGE LOAD by
`zhao_terrain_lodfeed` from the lattice TERRAIN.MIPFEED streams past
TERRAIN.MIPGEN, walked by `zhao_terrain_loddev`.

| lane | quantity | width | reference model |
|---|---|---|---|
| 0 | `dev1` — deviation of the subpatch at LOD level 1 | u24 | `zref::terrain::lod_deviation` |
| 1 | `dev2` — level 2 | u24 | same |
| 2 | `dev3` — level 3 | u24 | same |
| 3 | *not valid* | — | — |

Each is the **unsigned world-space magnitude**, in the lattice's height units,
by which a subpatch decimated to that level departs from the fine lattice. It
is the MESH reading (`DEV_INCLUDE_BOUNDARY = 1`), owner ruling **R22**. The
reading is a named parameter, not a constant, so the owner's pick stays a
one-line change (CLAUDE.md rule 6).

**Sixteen events per surface per page**, one per subpatch. Only surface 0 is
measured: `zhao_terrain_lod` law 7 gives the underside the top's level and its
`sp_*` port has no surface field, so a surface-1 record could never be read.
Those samples are counted on `surface1_samples_o` and dropped — the same
decision, not a second one.

## 2. `src_id` records WHICH SOURCE an interval came from

The histogram's `ev_src_id_i` carries the **page's** source id, 16 bits, and
`snap_src_id_o` reports the id of the last event accepted into an interval.

**It is the id of the walk, not of the fill.** The walk that emits a page's
records runs behind the stream that fills the buffer, so a page may already
have started arriving when the previous page's records are emitted (that is
what `lattices_dropped_o` counts). Taking the streaming page's id would stamp
page B's id on page A's deviations, and because the histogram is
metric-agnostic **nothing could ever see it**. `zhao_terrain_loddev` echoes the
`start_src_id_i` it was started with; `zhao_terrain_lodfeed.w_src_id_o` is that
echo, and it travels with the walk by construction.

**A source id above 65,535 ALIASES.** T5's record field is a `u32` and this
port is 16 bits. The narrowing is the histogram's limit, declared here rather
than hidden at the instance that makes it.

## 3. The width adaptation is an ADAPTATION, and this is why

`zhao_measure_histogram` is instantiated at `EW = 32`; the deviations are 24
bits. Lanes 0–2 carry them **zero-extended 24 → 32**; lane 3's `lane_valid` is
held LOW, so it contributes no event rather than contributing a zero one.

The block bins by `log2` of the magnitude with `SUB_BITS` mantissa bits. The
bin index of an unsigned `x` is the position of its top set bit, and
zero-extension adds no set bits, so **the widening moves no value into a
different bucket**. It changes the range the histogram could represent and
nothing else.

Had the adaptation been a shift, a truncation or a saturation it would have
been a law and would have needed a ruling of its own. It is guarded at the
instance (`zhao_console_core.sv`, an elaboration `$fatal` on `LANES != 4` or
`EW < 24`) rather than trusted, because `4'b0111` names the wrong lanes
silently if `LANES` ever moves.

## 4. What v1 is NOT, and what v2 will be

Charter §9 Version 1 has the ARM predict a **pixel-error threshold per camera**
from these counters. That is **not** what this metric is: a page deviation is a
world-space quantity and carries no camera.

R70 ratifies the page deviation anyway, for a stated reason: the ARM's
Version-1 job is to predict a refinement threshold from prior counters, and a
page deviation **is** a candidate error bucket for that decision. The
screen-space pixel error per camera is the **v2** refinement, and it needs a
projector-side residual that nothing in this tree computes — GEOM.LOD-class
work (ruling R68), not a wiring job.

**Consequence for whoever adds the v2 source:** the two are different
quantities in one organ, so they MUST be separable. `src_id` is how — an
interval records which source its events came from, and a reader that mixes a
terrain page id with a camera id has read this section and chosen to.

## 5. What this metric does NOT license

* **`RASTER.FRAGMENT`'s `fragment_error_o` is not an error magnitude.** It is
  `s1_v_r && !rd_valid_i`, a one-bit tilestore-read protocol flag that "should
  never fire", and it carries no value. `design/blocks.yml`'s declared
  `inputs: [fragment_error]` points at it, and that declaration is wrong.
* **`ev_expected_fx_i` / `ev_actual_fx_i` are DEBUG.TRACE's ports**, not this
  block's. There is no expected, no actual and no difference anywhere on
  MEASURE.HISTOGRAM's event port. Two separate refusals in
  `zhao_console_core.sv` were argued from that confusion.
* **Do not build a second deviation producer.** One exists, it is
  differentially tested (`terrain_lodpath_directed`, 286 checks), and a second
  would be the duplication `tools/budget/uncashed_cheques.py` check 3 exists to
  catch.

## 6. Where it is exercised

`tests/terrain/terrain_lodhist_directed.cpp` drives the composed arrangement —
`zhao_terrain_lodfeed` → the §3 widening → `zhao_measure_histogram` — with a
lattice that moves, and checks that a height pushed in as a page-load sample
lands in the bin `zref::terrain::lod_deviation` predicts for it.

**The console smoke does NOT exercise it, and that is measured rather than
assumed.** `tests/prod/run_console_core_smoke.ps1` plays zero pages whose CRC
cannot match the declared `expected_page_crc32c`, so TERRAIN.PAGELOADER's
`fin_ok` never rises, TERRAIN.MIPREQ issues no job, and the fine stream never
moves: the bench prints `mipreq requests=0 … samples_sent=0` on its `mip` line
for exactly this reason. What the smoke asserts instead are the two properties
that hold at zero and at a thousand — no fine sample without a lattice start,
and every record lodfeed emits accepted by the histogram.
