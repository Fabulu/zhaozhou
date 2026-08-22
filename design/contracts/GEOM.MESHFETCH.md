# Contract — GEOM.MESHFETCH (Meshlet fetch and cull)

> Ledger: `design/blocks.yml` · owner ZH-037 · phase 8 · maturity SPECIFIED

## Purpose and exclusions

Fetch meshlet descriptors, reject instance bounding spheres outside every active
camera frustum, and decide LOD per governor targets.

### The cull law (owner ruling, 2026-08-22)

This block used to be specified to "cull against camera visibility sectors".
That phrase appeared exactly twice in the repository — here and in the ledger
line this file was generated from — and nothing defined what a sector was.
**It is deleted. No sector system exists.** What replaces it:

* the descriptor carries a conservative **bounding sphere**: `bound_center` and
  `bound_radius`;
* for each ACTIVE camera, transform the bound into camera space and test the
  sphere against the frustum planes;
* **reject only when the sphere is outside EVERY active camera.** In Duo, cull
  only if outside both;
* optionally carry a two-bit visibility result (camera 0, camera 1) downstream,
  so work that genuinely is camera-specific is not duplicated;
* static and rigid meshes take an **asset-generated** bound; animated creatures
  take a **conservative animation-safe instance bound**. Per-pose exact bounds
  are explicitly NOT required — a loose bound costs performance, never geometry.

**Why the coarse cull belongs HERE and not in `GEOM.CLIP`.** This block feeds
`GEOM.VDECODE` and `GEOM.POSE`, so rejecting an invisible instance here avoids
compressed vertex fetch and decode, pose work, skinning, projection, setup,
binning and rasterisation. `GEOM.CLIP` receives already-projected individual
triangles; its scissor test is deliberately cheap and comes far too late to save
any of that. The two are complementary stages, not alternatives.

**Deliberately not built:** meshlet occlusion sectors, BSP cells, portals,
island visibility grids, Hi-Z occlusion. Each needs new scene-format laws,
dynamic-update behaviour and memory structures, and for a world of floating,
deforming and rotating terrain a rigid baked visibility system could become
actively unhelpful. Another rejection bit can be added in front of this block
later without changing this law.

### What is already built

The **LOD third** of this block exists: `fpga/rtl/geometry/zhao_geom_lod.sv`,
differential-tested against the shipped `zref::creature::lod_raw` and
`zref::creature::lod_update` in `tests/differential/geom_lod_directed.cpp`
(12,530 directed evaluations, 200,000 random per nightly run, 1,267,100 checks,
mutation sweep **26 attempted / 26 accounted / 25 caught / 1 equivalent**, the
equivalence proved in `tools/sweep_geom_lod.sh`'s header rather than labelled).
It carries no divider: every quotient in the ladder feeds a comparison, and
those are cross-multiplied exactly.

**It is SEQUENCED, and that is a port-visible fact.** The five products in the
ladder -- `thresh*R`, the three rung legality products and the switch boundary
-- walk through ONE multiplier, so:

* `ready_o` is high exactly when a tick will be accepted;
* `tick_i` is IGNORED when `ready_o` is low, so a caller that ticks blindly
  cannot corrupt an evaluation in flight;
* `valid_o` is a one-cycle pulse **5 clocks** after the accepting edge, and
  `rung_o` / `hold_o` / `raw_o` are that tick's answer on that cycle;
* the inputs are latched at accept, so the caller may move on immediately.

Sustained rate is one evaluation per five clocks, or 10 M/s at 50 MHz, against a
demand of roughly 600 k/s for ten thousand live creatures at 60 Hz. The
differential CHECKS the five rather than merely waiting it out, because a
latency that changes silently is a contract break even when the answer is still
right.

The **cull third** exists too, as of 2026-08-22:
`fpga/rtl/geometry/zhao_geom_cull.sv`, against `zref::cull`
(`reference/include/zref/zref_cull.hpp`) in
`tests/differential/geom_cull_directed.cpp` — 11,090 directed instance verdicts
(17,212 checks), 4,000 random per fast run, mutation sweep **32 attempted / 32
accounted / 30 caught / 2 equivalent**, both equivalences proved in
`tools/sweep_geom_cull.sh`'s header rather than labelled.

Three things about it are worth stating here because they are decisions, not
transcription:

* **FIVE planes, not six.** `project_vertex`'s only depth condition is
  `clip.w > 0`; there is no z clip in this renderer, so `row2` is never read. A
  sixth plane would reject geometry the renderer would have drawn.
* **The length bound rounds UP.** The plane rows are not unit-length, so the
  test compares against `radius * |normal|`, and the available primitive
  (`isqrt_u64`) is an exact FLOOR square root. A floor makes rejection easier
  and deletes visible geometry at the screen edges; the ceiling only wastes
  decode work. The differential measures that it can see the difference: 1,015
  of 1,775 boundary probes answer differently under a floor.
* **The bound arrives as PORTS**, not from a descriptor. The five planes and
  their five square roots are extracted once per camera per frame (185 cycles on
  a matrix write); the per-instance path is 10 cycles and four multipliers.

Both blocks are SIMULATION ONLY. `zhao_geom_lod` has a Quartus block fit
**(1,183 ALMs / 6 DSPs / 271 registers, at `09bbe05`)**, down from 1,303 ALMs
and 18 DSPs before the products were sequenced -- both numbers measured on this
machine, at a clean worktree, on the commit each row names.
`zhao_geom_cull` has been fitted since (1,102 ALMs / 15 DSPs) and is the obvious
next candidate for the same lever: four multipliers on a path that already takes
10 cycles for a per-instance rate.

The **descriptor fetch** is still not built, and it is the reason this block
stays SPECIFIED: `zref::MeshFetch` resolves to nothing and the meshlet schema is
unfrozen. Nothing was invented to fill that gap. Three integration questions the
cull cannot answer for itself — which space `bound_centre` arrives in and how a
model transform scales the radius, the descriptor format, and who drives the
active-camera mask — are on `docs/OWNER_DOCKET.md`.

## Clock and reset semantics

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Input and output packet layouts

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Backpressure rules

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Memory ownership

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Q formats and rounding

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Latency (fixed or variable)

Variable, and only two of the block's three thirds can answer yet.

| third | latency | measured by |
| --- | --- | --- |
| LOD ladder (`zhao_geom_lod`) | **fixed, 5 clocks** from the accepting edge to the `valid_o` pulse; one evaluation in flight at a time (`ready_o` low meanwhile) | asserted on every evaluation in `tests/differential/geom_lod_directed.cpp` |
| cull (`zhao_geom_cull`) | 185 cycles per camera on a matrix write; **10 cycles** on the per-instance path | `tests/differential/geom_cull_directed.cpp` |
| descriptor fetch | UNKNOWN — unbuilt, and it is a memory path, so its latency is MEM.GUARD's before it is this block's | — |

The LOD ladder's five is a DESIGN CHOICE, not a limit: it sequences five
products through one multiplier and cost 12 of 18 DSPs to do so. If a future
consumer needs the answer in one clock, the parallel form is one commit back
(`d8278bd`) and costs 18 DSPs of 112. That trade is on `docs/OWNER_DOCKET.md`.

## Target throughput

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Overflow and malformed-input behaviour

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Counters and traces

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Scalar reference function

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Directed tests

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Randomized differential tests

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Formal properties

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Synthesis / resource ceiling

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Integration capture cases

TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).

## Notes

Meshlet limits are Phase-0 data (P2 risk 1) — schema fields stay unfrozen.
