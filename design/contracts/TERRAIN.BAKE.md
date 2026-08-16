# Contract — TERRAIN.BAKE (Persistent scar bake + breach law)

> Ledger: `design/blocks.yml` · owner ZH-036 · phase 7 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` §3.4 (world-identity wave). This block
> is the ONLY writer of the scar plane (layer B) and the cell-state plane
> (layer D) — every permanent wound, every breach, every heal is born here,
> deterministically, and mirrored bit-exactly by the sim.

## Purpose and exclusions

Bake Scar Scribe stamp results and expiring field programs into the
persistent scar delta (height16, incremental-scaling law: apply
`(to − from) × stencil`, so interrupted casts un-apply and permanence decays
to a residual fraction — terrain_rules §9), then evaluate the breach law
(terrain_rules §3.4): a SOLID cell with no_bake = 0 whose composed top
equals bottom at all four corners becomes VOID_BREACHED; a bake that lifts
any corner heals it back to SOLID. Canonical scars are mirrored by
SW.CPUCOLL (same function, sim side).

Exclusions: never touches authored planes (A/C), never stamps sheets
(SURFACE.STAMP), never composes live fields (TERRAIN.PATCH).

## Clock and reset semantics

`gpu` domain; synchronous reset aborts an in-flight bake WITHOUT partial
writes (bake commits per dirty-rectangle, double-buffered then flipped).

## Input and output packet layouts

- `stamp_results` (SURFACE.STAMP / FIELD.SEQ.STAMP): stamp records
  {patch_id, stencil handle, from fx16, to fx16, footprint} in command order.
- Output `baked_scars`: dirty-rectangle notifications {patch_id, rect,
  breach_events[]} to TERRAIN.PATCH (and via capture, to the sim mirror).

## Backpressure rules

ready/valid; bakes are strictly ordered (command order — replay law).

## Memory ownership

Exclusive writer of layers B and D via its MEM.GUARD grant. Reader of A/C.

## Q formats and rounding

Scar bake-back: fx16 → height16 by `rescale(x, 8)` + saturate s16
(qformats §2 conversions — round-half-up bake-back). Breach equality is
tested AFTER the §3.4 clamp in fx16 (exact equality: composed == bottom).
The no_bake clamp bounds scar so composed top stays ≥ bottom + one height16
LSB on protected cells.

## Latency (fixed or variable)

Variable per stamp footprint; bounded by footprint ≤ patch.

## Target throughput

1 bake texel (lattice vertex) per clock.

## Overflow and malformed-input behaviour

Saturating height16 writes recorded (qformats §5 mirror counters); a stamp
naming a non-resident patch is a safe no-op + counter; breach/heal events
on VOID_AUTHORED cells are impossible by construction (§3.4: authored void
never becomes ground).

## Counters and traces

`surface_texels_touched`; trace payload includes breach/heal cell events
(the discrete transitions are first-class trace/capture facts).

## Scalar reference function

`zref::TerrainBake` — the SAME function runs sim-side in SW.CPUCOLL's
mirror (canonical scars, charter §6); differential tests assert
FPGA-vs-sim bit equality of layers B and D after arbitrary bake sequences.

## Directed tests

`tests/terrain/terrain_bake_directed.cpp`: incremental-scaling identities
(apply a→b→a = identity), interruption un-apply, residual decay, breach
birth at exactly-four-corner equality, heal round-trip, no_bake clamp.

## Randomized differential tests

`tests/terrain/terrain_bake_random.cpp`: random stamp sequences vs zref;
capture-replay of breach events byte-exact.

## Formal properties

No partial commit: a dirty rectangle is either fully old or fully new at
every observable boundary (double-buffer flip law). Writer exclusivity
rides `mem_guard_no_escape`.

## Synthesis / resource ceiling

`geometry_mantle` group (charter §25).

## Integration capture cases

Phase-7 gate: settled impact leaves persistent shape + surface scars
(charter §23); breach-birth + fall-through capture (terrain_rules §3.5).

## Notes

Canonical-scar ownership is shared with SW.CPUCOLL (ZH-036); the bake is
capture-exact. Transient fields can NEVER breach — only bakes write layer D
— so permanent holes are always born under an impact event whose FX masks
the discrete transition (terrain_rules §3.4, design consequence).
