# Contract — PART.UPDATE (Particle update)

> Ledger: `design/blocks.yml` · owner ZH-062 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Fixed integration recipes (integrate/gravity/drag/attract/orbit/vortex/wind/shockwave/spline-flow) driven by flow-field results.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons the particle in
flight; no state survives it, because every value this block needs arrives with
the particle or from the species table.

## Input and output packet layouts
### In
The 128-bit record from `PART.STATE`, the species descriptor, and a bounded
Field/FLOW acceleration sample.

### Out
The same record, updated, plus deterministic spawn/death events for
`PART.SPAWN`.

### The recipe vocabulary — CLOSED, owner ruling 2026-08-31 §2.2

    integrate      gravity      linear drag    attraction
    repulsion      orbit        vortex         wind
    shockwave      spline flow  colour curve   size curve

**Twelve operations and the list is closed for v1.** A species gets ordinary
integration plus optional gravity/drag plus **one** bounded primary motion
recipe — not a program. A Field/FLOW result may add a bounded acceleration, and
the ruling is explicit that this "does not turn PART.UPDATE into a programmable
particle processor."

That sentence is the architecture. The moment a species can chain recipes, this
block needs an instruction stream, a program cache and a bound on execution
time, and it becomes a second Field engine.

**New recipes are additive, versioned extensions. A recipe id is never silently
reinterpreted** — an old capture must replay under the recipe it recorded.

## Backpressure rules
Ready/valid, house hygiene. A stall is safe at any point: this block holds one
particle, and the particle carries its own state.

## Memory ownership
**None.** Reads the species table (on-chip, loaded per frame, owned by
`PART.STATE`) and Field samples as ports. Writes nothing to memory.

That is what makes the block a pure function of its inputs, which is what makes
it differentially testable at all.

## Q formats and rounding
This is where the particle arithmetic lives, and therefore where the position
and velocity scales must finally be pinned — `PART.STATE` deliberately carries
those bits without interpreting them.

**They are Class C and the ruling did not make them.** 18 bits of position and
11 of velocity are capture-visible: they decide what a particle position *means*
and they cannot change once a capture exists. This contract records the question
rather than answering it, and the block cannot be built until it is answered:

* the metres-per-unit scale for the 18-bit position axis, and therefore the
  world extent a particle may occupy;
* the units of the 11-bit velocity and the tick length it integrates over.

Everything else follows the tree's existing laws: one rounding per emitted
value, round-half away from zero, declared here and generated where it is a
table. Curves are lookups, not evaluated polynomials — a curve is authored
content and a table keeps it exact.

## Latency (fixed or variable)
**Fixed** per particle. There is no data-dependent branching: every recipe
resolves to the same eight-step order, with unused steps contributing identity.

Ledger says variable; that predates the closed vocabulary. **Fixed is the
stronger property and it should be asserted**, because a fixed-latency update is
what lets `PART.COLLIDE` and `PART.SPAWN` be simple streaming stages behind it.

## Target throughput
**One particle update per clock** (ledger). At the required 32,768 tier that is
32,768 clocks, ~2.5 % of a frame — matching `PART.STATE`, which is the point:
the two stages stream together and neither should be the other's bottleneck.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| unknown recipe id | **refuse the particle**, count it. Never fall back to `integrate` — a silently different motion is worse than a missing particle |
| species index out of range | refuse; `PART.STATE` should have caught it, and catching it twice is cheap |
| velocity saturates 11 bits | **clamp and count**. Wrapping would reverse a particle's direction, which reads as a physics bug rather than a numeric one |
| age reaches 2^10 | the particle dies; lifetime is bounded by the field width and the species cannot exceed it |

The saturate-don't-wrap rule is the one to enforce in RTL review: an 11-bit
velocity is narrow enough that fast particles will reach it.

## Counters and traces
* `polygon_particles`, `soft_particles` (ledger)
* `recipe_histogram[12]` — which of the closed twelve a frame actually uses
* `velocity_saturations` — if this is ever large, the scale question above was
  answered wrongly
* `particles_died_by_age`, `particles_refused`

## Scalar reference function
`zref::ParticleUpdate` (ledger). Owns the twelve recipes, the eight-step order
and the saturation rules. Not the collision response (`zref::ParticleCollide`)
and not the spawn decision (`zref::ParticleSpawn`), which are separate stages
with separate oracles.

### The order is the law, not an implementation detail

    1. advance age, evaluate lifetime
    2. read species constants and external Field/wind input
    3. evaluate the selected force/motion recipe
    4. apply drag
    5. integrate velocity and position
    6. resolve at most ONE collision response for the tick
    7. evaluate size and colour curves
    8. emit deterministic spawn/death events

Drag after the recipe and before integration; curves after integration; events
last. Reordering any pair changes the trajectory, so this list is reproduced in
the oracle and asserted, not left to the implementation to imply.

## Directed tests
`tests/particles/part_update_directed.cpp`.

* each of the twelve recipes in isolation against a hand-computed step;
* **the eight-step order**, proved by a case where two orderings differ:
  a particle with both drag and a strong recipe, where drag-before-recipe and
  drag-after-recipe give measurably different velocities;
* velocity saturation: clamps, counts, and **does not wrap** — assert the sign
  is preserved, which is what a wrap would destroy;
* age at 2^10 − 1 then one more tick: dies;
* unknown recipe id: refused, and no motion applied;
* a Field/FLOW acceleration at its bound, and at zero, giving identical results
  to the no-Field path when zero.

## Randomized differential tests
`tests/particles/part_update_random.cpp`, RTL against `zref::ParticleUpdate`.

Random species, recipes and Field inputs, biased toward the saturation
boundaries rather than uniform — a uniform velocity distribution almost never
saturates, so it would test everything except the case that bites.

Report the recipe and saturation mix. Run each case twice for determinism, as
`PART.STATE` requires.

## Formal properties
`tests/formal/part_update_bounds.sby`:

* **velocity never wraps** — for all inputs, the emitted velocity is the
  saturated value and its sign matches the unsaturated result's sign;
* age is monotonic within a life and a dead particle emits no further motion;
* every accepted particle produces exactly one output or one refusal;
* handshake hygiene; reset drops in flight.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 2,500 ALMs, 16 DSPs, 0 M10K.**

The DSP figure is the real question and it is a Class-B trade: twelve recipes
sharing sequenced multipliers, or laid out flat for one-per-clock. `zhao_geom_lod`
already demonstrated the lever — five products through one multiplier cost 12 of
18 DSPs — and the same measurement should be made here before choosing.

Zero M10K: the species table belongs to `PART.STATE`.

## Integration capture cases
* **32,768 particles across many species** — confirms one-per-clock holds with a
  realistic recipe mix rather than a single hot recipe.
* **a shockwave frame** — the recipe with the widest dynamic range, and the one
  most likely to saturate velocity.
* **replay equality** on a particle-heavy frame, as `PART.STATE` requires.

## Notes

Recipes are spec constants; forces come from FIELD.SEQ.FLOW, never ad-hoc math.
