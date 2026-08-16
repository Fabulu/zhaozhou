# Contract — SW.CPUCOLL (CPU-side collision and canonical scars)

> Ledger: `design/blocks.yml` · owner ZH-036 · phase 7 · maturity SPECIFIED

## Purpose and exclusions

Terrain collision, canonical scar state and navigation on the ARM side, derived from the exact same field semantics the FPGA evaluates.

Phase-1 scope: this software block is wave-1-active. Its contract is authoritative NOW; the headings below that name C++/RTL artifacts describe the shape of the evidence to come, and no maturity advance happens without that evidence being committed (rules V2/V3).

## Input and output packet layouts

The query API is `zref::terrain::column_query` (spec/terrain_rules.md §4.3
— normative pseudocode): `(island, wx, wz) → {class SOLID|VOID|OUT, top,
bottom, velocity, matA, matB, weight, sheet}`. Consumers: unit ground
placement, `rotateOnGround` slope tilt (two extra taps along facing/side —
spec/creature_rules.md), projectile/unit fall-through (VOID/OUT → ballistic;
below `island_datum + min(bottom) − KILL_MARGIN` → removal event), nav-grid
refresh from gameplay-cell dirty bits (breach/heal events from the bake).

Canonical-scar mirror: the sim runs the SAME `zref::TerrainBake` function
over the same stamp stream (TERRAIN.BAKE contract) — layers B/D on the sim
side are bit-identical to VRAM by construction and asserted differentially.

## Backpressure rules

Backpressure: `none`.

## Memory ownership

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Q formats and rounding

Exactly terrain_rules §3.4/§4.3: fx16 lattice interpolation (two
single-rounded MADs per query, qformats §3), height16 ↔ fx16 conversions
per qformats §2/§9. The sim NEVER evaluates a field program at a non-lattice
point (lattice law, terrain_rules §4.1) — that law, not testing, is what
keeps physics equal to pixels (sacengine's 2.0-vs-3.0 Erupt drift is the
donor's cautionary bug, recon S1 §4).

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: gameplay-rate queries.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Directed tests

Planned: `(tbd)`.

## Randomized differential tests

Planned: `(tbd)`.

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

One semantics, two consumers (FPGA terrain + CPU collision) — never re-derived by hand (§29-6).
