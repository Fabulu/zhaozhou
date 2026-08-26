# Contract — TERRAIN.VELOCITY (Height velocity output)

> Ledger: `design/blocks.yml` · owner ZH-039 · phase 7 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` §4.2/§4.4 (the velocity lattice and the
> accumulation), `design/ops.yml` FIELD.OUT.VELOCITY (the height16 bake-back),
> `spec/form/field-ir.md` §7.1 (velocity is earth OUT-LANE 1).

## Purpose and exclusions

Produce the §4.2 velocity lattice — one height16 word per lattice vertex, 2 B,
33×33 per patch — by accumulating the Earth velocity out-lane at each vertex
and baking it back once, for TERRAIN.PATCH's composed cache neighbours and for
CPU-side collision (`column_query`'s `velocity` field, interpolated by §4.3).

Implemented as `fpga/rtl/terrain/zhao_terrain_velocity.sv`.

**What the RTL is, as built.** A stream processor that OWNS its own sweep:
33×33 lattice vertices in z-then-x scan order — the order
`compose_lattice` records velocity in and the order §4.2's lattice is addressed
in. Per vertex it consumes `lanes` velocity words (one per accepted §9.1 field
list entry, in list order), folds the ones whose footprint covers the vertex
with a saturating `fx_add`, and emits ONE height16 word.

**What it is NOT, deliberately.** No field evaluation — §4.1 forbids a second
evaluator and `zfield::interpret` is the only one. No §4.3 interpolation: that
is the consumer's, and `zref::terrain::column_query` already owns it. No
footprint rectangle store: TERRAIN.PATCH owns the §9.1 list, and a second
resident copy here would be a second implementation of one law plus 2,048
flops. No clamp at the underside: §3.4's two clamps are about a SURFACE never
punching below the modelled bottom, a rate has no such law, and inventing one
would silently zero the downward half of every wave. No VRAM port and no
lattice-sized buffer — the 2 B/vertex store belongs to whoever owns the page.

## THE SEAM — recorded, not invented

`design/blocks.yml` gives this block `inputs: [patch_state]` with
`upstream: [TERRAIN.PATCH]`. **`patch_state` as TERRAIN.PATCH actually emits it
carries no velocity at all.** It is `{top, bottom, compose_top, dirty, src_id}`
— the §3.4 height composition. The velocity number is out-lane 1 of the SAME
earth evaluations whose out-lane 0 TERRAIN.PATCH consumes (field-ir §7.1), so
the ledger line is a **routing** statement (velocity rides the same per-vertex
walk as the composition) and not an arithmetic one.

Two ports are therefore needed where the ledger names one:

| ledger | as built | why |
|---|---|---|
| `patch_state` | `lane_velocity_i` | the arithmetic input: earth out-lane 1, fx16, from FIELD.SEQ.EARTH |
| — | `lane_covers_i` | the §9.1 closed-interval answer, decided ONCE by the list's owner |

`lane_covers_i` is driven by **TERRAIN.PATCH's new `fld_covers_o`** — its
internal `cur_covers` wire, exported 2026-08-19 with this block. That export is
purely additive (one `assign`; no internal behaviour changed) and it exists so
the footprint law has exactly one implementation, per charter §29-6.
`tests/terrain/terrain_velocity_chain.cpp` runs both blocks off ONE field list
with that wire connected, and counts every cycle in which only one of the two
consumed a lane word: the count is asserted zero, so the lockstep is a
measurement and not an assumption.

**The rejected alternative** was to hold a second 16-rectangle §9.1 list in this
block, mirroring TERRAIN.PATCH's. It needs no new wire, but it re-decides a
ratified law in a second place — precisely the drift charter §29-6 exists to
forbid — and costs 16 × 4 × 32 = 2,048 flops on a device where TERRAIN.PATCH
already pays that bill once.

## Laws found

| law | where |
|---|---|
| velocity is earth OUT-LANE 1, Q16.16 | `spec/form/field-ir.md` §7.1; `zref::render::field_velocity_lane` |
| the lattice is height16-scaled, 2 B/vertex, once per frame | `spec/terrain_rules.md` §4.2 |
| the bake-back saturates | `design/ops.yml` FIELD.OUT.VELOCITY: `result_q: height16`, `rounding: saturating` |
| `fx16 → height16` = `rescale(x,8)` then saturate s16 | `spec/qformats.md` §2/§9; `zref::height16_from_fx16` |
| one rounding per result | `spec/qformats.md` §3 (single-rounding law A3b) |
| the footprint test is a CLOSED interval | `spec/terrain_rules.md` §9.1; `zref::terrain::covers` |
| the recorded order is z-then-x, vertices inner | `reference/src/zrender/terrain.cpp` |
| the 4×4 subpatch grid, border vertices shared | charter §11.1; `zref::terrain::subpatch_mask` |

## Laws CHOSEN, not found

**V1 — the accumulation is a saturating `fx_add` chain in command order over
covering lanes only, with exactly ONE bake-back at the end.**
`terrain_rules` §4.4 says "accumulated" and stops. `compose_lattice` does not
accumulate at all: it pushes one `TerrainVelocitySample` per (application,
covered vertex) and nothing in this tree consumes them, so the reduction has
never been written down anywhere. It is chosen to be the SAME reduction §3.4
already applies to the height lane, at the same vertex, from the same
evaluations — because the alternative is for the ground's measured SPEED and
its measured HEIGHT to be reduced by two different rules from one evaluation.

- REJECTED, last-writer-wins: cheaper, but two overlapping waves would make the
  speed depend on command order where the height does not, and a collision
  solver reading both would see them disagree.
- REJECTED, max-magnitude: order-independent, which is genuinely attractive,
  but it is not a velocity — two opposed waves that cancel exactly in height
  would still report the larger one's full speed. `terrain_velocity_directed`
  §3(a) is the case that separates the two rules and it is asserted, not
  described.
- REJECTED, accumulate in height16 (convert each lane, then add): two roundings
  per lane. `qformats` §3's single-rounding law rejects it outright, so this one
  the spec decided and the block only records.

**V2 — a vertex no lane covers has velocity exactly zero, and its word is
written anyway.** §4.2's lattice is 2 B for EVERY vertex, so the word must be
defined; ground no live field touches is not moving, and 0 is what not moving
is. REJECTED: leaving the previous frame's word (a persistence reading) — a
wave's trailing edge would keep a stale speed for the rest of the level, and
there is **no decay pass anywhere in this tree** to retire it (see the open
question below).

**V3 — the block owns its 33×33 z-then-x sweep**, driving `vtx_vi_o`/`vtx_vj_o`
and letting the lane producer answer, exactly as TERRAIN.BAKE owns its own.
REJECTED: taking (vi, vj) on the lane stream — it would let a producer reorder
or skip vertices and make the lattice's completeness an upstream promise rather
than a structural fact. Because the block drives the address, the test suite
can RECORD the requested order instead of assuming it.

**V4 — the lane stream is vertex-major**: `lanes` words per vertex, in list
order, vertices in sweep order. Not a new requirement — it is TERRAIN.PATCH's
chosen law 1 verbatim, and it MUST be the same one because both blocks consume
the same evaluation stream. Restated here so the two cannot drift.

**V5 — the 4×4 moving mask is PRODUCED, never CONSUMED.** `moving_mask_o` marks
the subpatches holding a vertex whose stored word is non-zero, by
`zref::terrain::subpatch_mask` (a border vertex marks both neighbours, a corner
four). REJECTED, and this is the one that matters: **gating the sweep by
TERRAIN.PATCH's incoming dirty mask**, to skip clean subpatches under a moving
wake. It would be wrong. `dirty` is `live_top != fx(base)` — DISPLACEMENT — and
velocity is the DCURVE derivative of the same envelope, so the two are out of
phase by construction: a wave's leading edge has a rate with no displacement
yet (moving, not dirty) and its crest has displacement with zero rate (dirty,
not moving). A dirty-gated velocity sweep would drop exactly the leading edge
of every wake.

That is not an argument, it is a measurement: `terrain_velocity_chain` §2 runs
both real blocks over one instant of a travelling wave and reads
**dirty mask `0x0004`, moving mask `0x0001` — disjoint**. Neither mask contains
the other. The mask this block emits is for a consumer to UNION with
TERRAIN.PATCH's, and never to be filtered by it.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset returns the block to idle, clears the accumulator, the lane index, the
sweep address, the moving mask, the output register and all three counters — no
partial vertex survives a reset. No clock-domain crossing.

## Input and output packet layouts

**start** — one per patch per frame (§4.2's "once per frame"):

| field | width | meaning |
|---|---|---|
| `start_lanes_i` | 5 | accepted §9.1 list size, 0..16 (TERRAIN.PATCH's `fields_active_o`) |
| `start_patch_id_i` | 16 | rides the sweep as `trace_patch_id_o` |
| `start_src_id_i` | 16 | rides every output word |

**the lane stream** — the block drives the address, the producer answers:

| field | width | meaning |
|---|---|---|
| `vtx_vi_o` `vtx_vj_o` | 6 | the lattice vertex the block is asking for |
| `lane_velocity_i` | signed 32 | earth out-lane 1, fx16 raw |
| `lane_covers_i` | 1 | the §9.1 closed-interval answer for THIS lane at THIS vertex |

**height_velocity out** — one §4.2 lattice word per vertex:

| field | width | meaning |
|---|---|---|
| `vv_velocity_o` | signed 16 | the stored word, height16 |
| `vv_vi_o` `vv_vj_o` | 6 | which vertex it is for |
| `vv_moving_o` | 1 | the word is non-zero |
| `vv_covered_o` | 1 | at least one lane's footprint hit this vertex |
| `vv_src_id_o` | 16 | rides the stream |

`moving_mask_o` (16) is the V5 subpatch mask, cleared at each `start` and held
until the next. `patch_done_o` is a 1-cycle pulse.

## Backpressure rules

Ready/valid on the start record, the lane stream and the output.
`lane_ready_o = (state == StLane) && (!last_lane || out_free)`: a non-final lane
is always takeable (the accumulator is a register being overwritten anyway);
the final lane needs the output register free. **With `lanes == 1` every lane is
final, so that line IS the sustained-wake path** — one lane word in, one lattice
word out, per clock. `start_ready_o` additionally waits for the output register
to drain, so a new patch can never clear the mask a previous patch's last word
still needs.

## Memory ownership

**As built: none.** The block has no VRAM port. Lane words arrive on one stream
and height16 words leave on the other. When a memory subsystem exists it is the
writer of the §4.2 velocity lattice (0.53 MiB of the terrain hot cache, §8)
via its MEM.GUARD grant. §7's ownership table does not yet name the velocity
plane at all; it names A/B/C/D/E/F/H and "the composed cache written only by
TERRAIN.PATCH". The velocity plane wants the same sentence and does not have
one — recorded here, not written into §7 by this increment.

## Q formats and rounding

The chain is fx16 (signed 32) throughout. Each `fx_add` is done at 33 bits and
narrowed with the §3 saturate, **one add at a time** — never a wide accumulate
then one narrow, which would disagree with the reference wherever a partial sum
leaves the word.

The bake-back is `saturate_s16(rescale(acc, 8))`, i.e. `(acc + 128) >>> 8` at 33
bits then a saturating narrow. Bits [32:8] of the 33-bit sum ARE the arithmetic
shift; there is no separate shifter and no unsigned promotion to be caught by.
Round-half-up means **ties go toward +infinity**, so `+128 → 1` AND `−128 → 0`
— the asymmetry is real and both are directed cases.

`fx_add` saturations are counted in `velocity_add_sats_o` (`SatLedger::add`) and
rail saturations in `velocity_rescale_sats_o` (`SatLedger::rescale`). The
differential checks both counts against the oracle's ledger, so saturation
BEHAVIOUR is compared and not merely the value.

## Latency (fixed or variable)

Variable. Per patch: 1 cycle to accept the start record, then **1 cycle per
vertex when `lanes` is 0 or 1, and `lanes` cycles per vertex otherwise**, plus
one cycle to drain the last word.

## Target throughput

The ledger asks for **1 velocity sample per clock**. **The block meets it for
the workload the console is being built for, and the numbers are measured
rather than derived** (`terrain_velocity_directed` §9 prints them, sink always
ready):

| lanes | measured |
|---|---:|
| 0 (no live field) | **1.001 clocks/sample** ✓ |
| 1 — **a wake** | **1.001 clocks/sample** ✓ |
| 2 | 2.001 |
| 4 | 4.001 |
| 16 (the §9.1 ceiling) | 16.001 |

A whole wake patch is **1,090 clocks for 1,089 vertices**, sustained — not a
burst figure: the measurement is one uninterrupted 33×33 sweep.

**The composed path is slower than this block, and that number is the one a
wake actually runs through** (`terrain_velocity_chain` §3, both blocks real):

| lanes | PATCH + VELOCITY together |
|---|---:|
| 0 | 1.001 clocks/vertex |
| 1 — **a wake** | **2.001 clocks/vertex** |
| 4 | 5.001 clocks/vertex |

The limiter is **TERRAIN.PATCH**, which spends a separate cycle ACCEPTING each
vertex before it will take that vertex's lane words, so the pair costs
`1 + lanes` where this block alone costs `max(1, lanes)`. Removing that cycle is
a TERRAIN.PATCH change (its vertex accept would have to overlap the previous
vertex's last lane), it is not attempted here, and the shortfall is stated with
its measurement rather than argued away by composition.

## Overflow and malformed-input behaviour

Every input is total. `start_lanes_i > 16` is not representable (5 bits, and the
§9.1 ceiling is 16). The `fx_add` chain saturates and counts; the bake-back
saturates to the height16 rails and counts. A lane whose `covers` bit is low is
consumed and DISCARDED — not added as zero — which is identical in value and
identical in SatLedger records, and is asserted as such with an uncovered
`INT32_MIN` lane that would saturate if it were added.

## Counters and traces

`terrain_samples_evaluated_o` (the ledger's counter; one per emitted lattice
word, frame-life, survives a patch change), `velocity_add_sats_o`,
`velocity_rescale_sats_o`, `moving_mask_o`, `trace_patch_id_o`,
`patch_done_o`, `idle_o`.

## Scalar reference function

`zref::terrain::velocity_vertex` — `reference/include/zref/zref_terrain_velocity.hpp`.

**THE LEDGER'S `reference_model: zref::TerrainVelocity` DOES NOT RESOLVE.** No
such symbol exists anywhere in the tree; the nearest things are the struct
`zref::render::TerrainVelocitySample` and the accessor
`zref::render::field_velocity_lane`, neither of which is a model of this block.
`design/ops.yml` FIELD.OUT.VELOCITY's `reference_function:
zref::fieldir::sink_out_velocity` does not resolve either — there is no
`zref::fieldir` namespace at all. Both are amended the way TERRAIN.BAKE's was
(`zref::TerrainBake` → `zref::terrain::bake_dig`), and the deviation is recorded
here.

The oracle is a THIN VIEW, and `terrain_velocity_directed` layer 1 proves it: a
real 33×33 patch with two real earth programs on overlapping footprints goes
through `compose_lattice`, its recorded velocity samples are consumed in the
order the reference pushes them (app-major, vertex z-then-x, covered only —
which the test also asserts), summed in command order, and the oracle must
reproduce every one of the 1,089 words. The three coverage classes (covered by
both / by one / by neither: 165 / 726 / 198) are asserted non-empty, so the
cross-check cannot pass by comparing two empty cases. The RTL is then run over
the reference's own lane plane.

## Directed tests

`tests/terrain/terrain_velocity_directed.cpp` — 133 checks:

1. the oracle against the EXECUTED reference (above);
2. the CONSTRUCTED bake-back boundaries, each first checked against
   `zref::height16_from_fx16` so the table is not a private copy of the RTL's
   opinion: `+128 → 1` and `−128 → 0` (both ties), `127 → 0`, `384 → 2`,
   `8388479 → 32767` clean, `8388480 → 32767` SATURATING, `−8388736 → −32768`
   clean, `−8388737 → −32768` SATURATING, `INT32_MAX`, `INT32_MIN`;
3. the fx_add chain: exact cancellation of two opposed lanes (covered, not
   moving — the case that separates V1 from max-magnitude), the constructed
   `INT32_MAX + 1` saturation, and an uncovered `INT32_MIN` lane that is skipped
   rather than added;
4. V2: a zero-lane patch still writes all 1,089 words, all zero, all uncovered;
5. V3: the block asked for exactly 1,089 vertices, in z-then-x order;
6. V5: interior / column-border / corner subpatch marking, plus sub-LSB motion
   that rounds away and marks nothing while the tie one LSB above marks;
7. backpressure on the lane side, the sink side and both — values identical,
   cycles strictly greater;
8. back-to-back patches: the mask clears, the counter accumulates;
9. the measured rate, printed.

`tests/terrain/terrain_velocity_chain.cpp` — 55 checks, both blocks real: the
closed-interval footprint edge landing EXACTLY on lattice vertices (which a
randomly placed rectangle never does), the disjoint dirty/moving masks, and the
composed rate.

## Randomized differential tests

`tests/terrain/terrain_velocity_random.cpp`, two lanes:

- **A, gameplay-shaped** — the wake: 0–4 live lanes, a diagonal coverage band
  per lane (what a moving player leaves behind), metre-scale velocities. 40
  patches / 43,560 vertices fast, 400 nightly. This lane exercises the
  ROUNDING; the height16 rails are unreachable at these magnitudes.
- **B, domain-limit** — full s32 lane words, up to the §9.1 ceiling of 16 lanes,
  coin-flip coverage. 40 / 400. This lane exercises the SATURATIONS: 47,988
  fx_add rails and 37,536 height16 rails in the fast run.

**Every exact-equality boundary is CONSTRUCTED.** Uniform random never lands on
a chosen accumulator, so every patch in both lanes carries a seeded last row of
the twelve boundary values and a seeded cancellation row, and the coverage
counters are asserted non-zero: lane A reached 468 ties, 174 rail-exact, 145
rail-first-saturating, 660 exact cancellations, 11 zero-lane patches; lane B
674 / — / 185 / 1,221 / 3. Random draw alone reaches none of the rail cases in
lane A and reaches `8388480` in lane B with probability ~2⁻³².

## Formal properties

**None written, deliberately.** The block's arithmetic is a saturating add and
a round-half-up shift, both of which the constructed directed boundaries pin
exhaustively at the points where they can differ; a BMC would restate them at
solver cost with no new information. The one property that would be worth
proving formally — "the sweep visits all 1,089 vertices exactly once and
terminates" — is a liveness statement over a 1,089-step trace, and
`terrain_bake_delta.sby` has just cost this repo a 10.7-hour BMC that never
finished. This repo banks proofs rather than manufacturing them; the sweep's
completeness is instead a *measured* fact in three test suites, which record the
requested order rather than assuming it.

## Synthesis / resource ceiling

Quartus 17.0.2 Lite per-block fit against the provisional 5CSEBA6U23I7 — see
`reports/synthesis/zhao_block_fit.json` (owned by another lane; the number this
increment measured is quoted in its run report). Virtual I/O, no board truth,
not a programmed device. Verilator `--lint-only -Wall` clean. No function-call
result is indexed anywhere in the module — the construct Verilator accepts and
Quartus 17.0 rejects, which cost GEOM.BINNER a synthesis failure that every
simulation lane passed. `fx_add_sat_fired` exists as a separate function for
exactly that reason: the saturation flag could have ridden a wider return value
and been sliced off, and that would not synthesize.

## Integration capture cases

None yet — the block has no capture surface until a memory subsystem owns the
§4.2 lattice. The `terrain-wave` / `terrain-impact` / `terrain-scars` reel
subjects are the shipping software console's live deformation and remain the
behaviour this lane must stay consistent with.

## OPEN: persistent scars or healing wake — NOT ratified anywhere

`Upheaval/docs/DESIGN.md` lists it under Wacko mode's **Open questions**:
"whether the deformation waves are persistent scars or heal behind you". It is
open in the specs too, and this contract records that rather than choosing:

- `spec/terrain_rules.md` uses "heal" **only** for the §3.4 breach law's
  `VOID_BREACHED → SOLID` transition — a bake that lifts a corner back above the
  underside. It never means a scar fading with time.
- §9's "permanence decays to a residual fraction" is the incremental-scaling
  stamp identity `(to − from) × stencil`, driven by the *stamp record's* depths.
  That is software choosing the depths, not hardware decaying anything.
- §11's "explicitly not decided" list does not mention wake decay, and no block
  in `design/blocks.yml` owns a time-decay pass over layer B.

**What the architecture already offers, without a new block.** A wake can be
built either way with what exists:

- **Healing, for free** — emit it as live earth field lanes. `live_top` is
  recomposed from `base + scar` every frame, so when the field's
  `duration_ticks` expire the ground returns exactly, with no decay pass and no
  writes to layer B at all. The cost is the §9.1 bound: **MAX_PATCH_FIELDS = 16
  live lanes per patch per frame**, so a wake long enough to need more than 16
  simultaneous segments over one patch would have its tail REJECTED (counted in
  `programs_rejected`), not faded.
- **Persistent** — emit it as `TERRAIN.BAKE` stamp records into layer B. The
  cost is the §9.2 window: **BAKE_PATCH_BUDGET = 64 records per frame**,
  enforced by backpressure, with the remainder carried FIFO to the next frame.

So the question is a game-design choice with two costed hardware roads, not a
missing mechanism — but it is genuinely undecided, and **whichever way it goes,
this block is unaffected**: velocity is a per-frame derivative of whatever the
live field says, and it is recomputed from zero every sweep (V2).

## Notes

Driven by the DCURVE-based Earth8 velocity lane (plan 1.D): `design/ops.yml`
FIELD.DCURVE returns the slope of the PWL segment containing its argument, and
`spec/form/field-ir.md` §7.4's blessed earth idiom is
`velocity = DCURVE(age_curve, phase)`. This block never sees the opcode — it
sees its result on `lane_velocity_i` — but the citation is why the lane is a
rate at all.
