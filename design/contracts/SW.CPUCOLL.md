# Contract — SW.CPUCOLL (CPU-side collision and canonical scars)

> Ledger: `design/blocks.yml` · owner ZH-036 · phase 7 · maturity SPECIFIED

## Purpose and exclusions

Terrain collision, canonical scar state and navigation on the ARM side, derived from the exact same field semantics the FPGA evaluates.

Phase-1 scope: this software block is wave-1-active. Its contract is authoritative NOW; the headings below that name C++/RTL artifacts describe the shape of the evidence to come, and no maturity advance happens without that evidence being committed (rules V2/V3).

## NAVIGATION — implemented 2026-09-26 (the rest of this block is not)

`reports/OWNER-DECISION-20260926-I34-NAV.md` moved navigation truth and its
query service here and **superseded** the FPGA's `TERRAIN.COMPOSED_NAV`
publication. `FIELD.WRITE.NAV` and every one of its semantics are PRESERVED;
only the consumer moved.

**The API.** `zref::nav::Service` — `reference/include/zref/zref_nav.hpp`,
implementation `reference/src/znav/nav_service.cpp`, shipped inside
`zhao_zref`.

```
set_terrain(patch, xform) / terrain_changed()
add_field(program, ZhCmdTerrainField) -> id   // accepted-command order
remove_field(id) / clear_fields() / begin_tick(tick)
query(fx16 wx, fx16 wz)  -> Result
query_cell(int ci, int cj) -> Result
Result { passable, block, cost, authored_cost, ground_y, slope_cos,
         fields_covering, fields_applied, generation }
```

**Hard passability** is the column class (`kOut` / `kVoid` / `kSolid`, §3.2)
plus the ruling-R1 collision normal against `Policy::min_slope_cos`, taken from
the SAME `column_pick` as the cost. **A cost never decides it** —
`compose_nav`'s own docstring forbids it, and a blocked location reports
`kBlockedCost` (INT32_MAX) so a caller that ignores the flag still cannot route
through a hole.

**Composed cost** is `compose_nav(authored, deltas, n)` per lattice vertex in
command order, then the §4.3 interpolation of that lattice at the query point
(`zref::terrain::plane_interp`, the same call `column_query` uses for height),
plus a slope surcharge. The authored baseline and the slope rule are
`zref::nav::Policy` — named, editable GAME RULES, not ratified arithmetic.

**Nothing is re-derived.** Evaluation is `zfield::interpret` through
`zref::render::compose_lattice` (the ONE §4.1 evaluation); coverage is
`zref::terrain::covers`; the intake bound is `kMaxPatchFields`; the reduction is
`zref::terrain::nav_vertex`, which forwards to `zref::fieldir::compose_nav`.

**Callers today:** `zgame::Wizards` (the console's only `zcon::GameTruth` —
passability refuses a step, cost scales it) and `runtime/desktop/desktop_main`,
which owns the terrain resource and advances the service through
`zcon::TickObserver`. `Upheaval`'s `uph_engine` links the same `zhao_zref`
(`Upheaval/CMakeLists.txt:56,:108`), so its `TerrainSet` and creature AI reach
it without a second copy.

**What is NOT done in this block:** the canonical B/D scar mirror, and the
per-beam occlusion DDA. The ledger maturity therefore stays `SPECIFIED`.

## Input and output packet layouts

The query API is `zref::terrain::column_query` (spec/terrain_rules.md §4.3
— normative pseudocode): `(island, wx, wz) → {class SOLID|VOID|OUT, top,
bottom, velocity, matA, matB, weight, sheet}`. **As shipped, `ColumnResult`
carries `{cls, top, bottom}` and the other fields are reached separately —
`collision_normal` for slope (ruling R1), `plane_interp` for any per-vertex
plane. The sentence above describes the SPEC's record, not the C++ struct, and
the difference has been read as a missing implementation before.** Consumers: unit ground
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

**Navigation (settled 2026-09-26).** The service BORROWS the canonical
`render::TerrainPatch`; the sim owns it, and a service that copied the terrain
would be a second thing to keep in step. It OWNS one derived cache per
registered patch: the composed lattice plus a per-vertex composed-cost plane
plus the recorded nav samples — **60,120 bytes measured for one 33×33 patch
with four live fields** (`tests/nav/nav_service_directed.cpp` section 6, which
reads `cache_bytes()` rather than estimating). Nothing is written back to VRAM
and nothing is published; the FPGA is not a writer of this state and the CPU is
not a writer of the fabric's (`zhao_terrain_writeback.sv` ruling T4).

The cache key is `(terrain_epoch, field_epoch, tick-while-any-field-is-listed)`.
The tick is excluded only when the accepted list is empty, so an ANIMATED field
re-evaluates every tick and untouched terrain is free across ticks.

TODO — collision and the canonical scar mirror have not been written and their
memory ownership is still open.

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

**Navigation (settled 2026-09-26).**

* **Cost saturates, never wraps.** `compose_nav` clamps to the int32 range after
  EVERY delta and floors at zero ONCE on the way out. Three `INT32_MAX` deltas
  give `INT32_MAX`.
* **Command order is observable at saturation** and is preserved: the accepted
  list appends in command order and `remove_field` erases rather than
  swap-and-pops.
* **A seventeenth field is REJECTED and COUNTED** (`fields_rejected()`), never
  evicted and never silently clamped — the §9.1 intake law.
* **An absent out-lane is not a write of zero.** A program declaring fewer than
  four out-lanes contributes no delta and raises `fields_covering` without
  `fields_applied`.
* **A resource miss** (`program == nullptr`) contributes nothing, exactly as
  `compose_lattice` already treats it.
* **A degenerate triangle** returns `Block::kDegenerate` and is IMPASSABLE. The
  flattering reading is "probably fine"; that is where a router would squeeze
  through.
* **Outside the registered patch** is `Block::kOut`, impassable. There is no
  multi-patch directory in this packet and the service does not pretend to have
  one.

TODO — collision and the canonical scar mirror have not been written.

## Directed tests

* `tests/nav/nav_service_directed.cpp` — **113 checks**, the owner's five
  acceptance items against the PRODUCTION API, plus the §9.1 intake bound,
  out-lane presence, generation coherence and the CPU-work / memory
  measurement. It decodes the real committed `crater_ring` spell from
  `compiler/tests/generated/`. Every "unchanged" assertion carries a
  `[control]` that moves on the same fixture.
* `tests/runtime/wizards_nav_directed.cpp` — **77 checks**, the WIRED path:
  `zgame::Wizards` through `zcon::Session`. Includes the positive control that
  a replay WITHOUT the `zcon::TickObserver` hook DIVERGES, so the hook is shown
  to be load-bearing rather than asserted to be.

Collision and the scar mirror: `(tbd)` — not written.

## Randomized differential tests

Navigation: `(tbd)`. The composed cost is checked against
`zref::fieldir::compose_nav` directly at chosen points, including both orders of
a saturating pair, but there is no randomized sweep against the fabric's
`zhao_field_sinks` yet. **That is an honest gap and is recorded as one.**

Collision and the scar mirror: `(tbd)` — not written.

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

One semantics, two consumers (FPGA terrain + CPU collision) — never re-derived by hand (§29-6).
