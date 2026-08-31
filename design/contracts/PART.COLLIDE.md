# Contract — PART.COLLIDE (Particle collision)

> Ledger: `design/blocks.yml` · owner ZH-063 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Plane and heightfield collision following the LIVE deformed terrain; bounce/slide/stick/die transitions.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons the particle in
flight. Terrain height/normal arrive as a sampled port, so no state survives.

## Input and output packet layouts
### In
The updated particle from `PART.UPDATE`, its species descriptor, and the
collision sources.

### Collision sources in v1 — closed
* simple planes;
* **the live deformed terrain heightfield and normal.**

"Live deformed" is the interesting half: the particle collides with the terrain
as it is *this tick*, including a wave or a crater that has just formed. That is
what makes debris settle into a fresh scar instead of hovering over the old
surface.

### The response enum — FROZEN, owner ruling 2026-08-31 §2.3

| response | behaviour |
|---|---|
| `IGNORE` | no response |
| `DIE` | remove on first accepted contact |
| `STICK` | snap to contact, zero relative velocity, remain until lifetime or a descriptor-defined stuck lifetime ends |
| `SLIDE` | remove the inward normal component, keep tangential, apply descriptor friction |
| `BOUNCE` | reflect the inward normal component by descriptor restitution, apply tangential damping |

**Selected explicitly by the species descriptor. Hardware must not guess from
speed, colour, particle size or material** — the ruling says so, and the reason
is that an inferred response is a behaviour nobody authored and nobody can
reproduce from the asset.

## Backpressure rules
Ready/valid, house hygiene. One particle in flight; a stall is safe.

## Memory ownership
**None.** Terrain height and normal arrive as sampled ports from
`TERRAIN.PATCH`; planes arrive as parameters. This block owns no memory and
performs no fetch, which keeps the live-terrain read on the terrain block's
side where its residency is already solved.

## Q formats and rounding
Positions and velocities in the same format `PART.UPDATE` emits. Restitution,
friction and damping are descriptor constants in fx16.

Reflection and projection are the standard forms; one rounding per emitted
component, round-half away from zero.

**A contact must not leave the particle inside the surface.** After response the
particle is placed at the contact point, and the contact point is computed so
that re-testing it in the same tick would not report a contact. That is what
makes "one response per tick" safe rather than a source of jitter.

## Latency (fixed or variable)
**Fixed** per particle. One response, no iteration, no search — by ruling.

## Target throughput
**One collision test per clock** (ledger), matching the two stages upstream so
the particle path streams end to end at 32,768 clocks per tick.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| unknown response enum | **refuse**, count. Never default to `IGNORE`: a particle that should have died and instead flew on is a visible bug with no error |
| terrain sample unavailable | treat as no contact for this tick, count it. Do not stall the particle path on a terrain read |
| particle already inside the surface at entry | resolve to the contact point and count it; this is the case a previous tick's response should have prevented, and counting it is how that regression becomes visible |

**One response is resolved per particle per tick.** Further contact is handled on
a later tick. The ruling is explicit — *"do not build an unbounded contact
loop"* — because an iterating collider has no bounded execution time and cannot
sit in a fixed-latency stream.

## Counters and traces
* `polygon_particles`, `soft_particles` (ledger)
* `contacts_by_response[5]`
* `contacts_terrain`, `contacts_plane`
* `already_inside_at_entry` — the regression signal named above
* `terrain_sample_unavailable`

## Scalar reference function
`zref::ParticleCollide` (ledger). Owns the five responses, the contact-point
placement and the refusal taxonomy.

**Moving terrain bodies, when they arrive:** body surface velocity enters the
relative-velocity calculation. The ruling names this explicitly so that the
oracle is written with a relative-velocity form now, rather than an
absolute-velocity form that would have to be rewritten later. **Canonical
gameplay physics stays on the HPS** — this block is presentation.

## Directed tests
`tests/particles/part_collide_directed.cpp`.

* each of the five responses against a hand-computed outcome, on a plane and on
  a heightfield;
* **`STICK` really stops** — zero relative velocity, and the particle is still
  at the same point many ticks later;
* **`BOUNCE` with restitution 0 equals `SLIDE` with friction 0** for a
  head-on contact. Two paths, one answer — the kind of cross-check that catches
  a sign error in the normal component;
* the contact point is outside the surface: re-testing it in the same tick
  reports no contact. This is the anti-jitter property;
* a particle already inside at entry: resolved and counted;
* unknown response enum: refused, no motion;
* **live deformed terrain**: the same particle, the same position, a terrain
  that changed this tick — the contact must follow the new surface.

## Randomized differential tests
`tests/particles/part_collide_random.cpp`, RTL against `zref::ParticleCollide`.

Random approach angles, speeds and terrain slopes, biased toward **grazing
contacts and near-vertical walls** — the two cases where a normal-component
split is most fragile and where uniform random sampling produces almost none.

Report the response mix. Run twice for determinism.

## Formal properties
`tests/formal/part_collide_once.sby`:

* **at most one response per particle per tick** — the ruling's bound, as a
  property. This is what guarantees fixed latency;
* after any response, the particle is not inside the surface;
* `DIE` emits no further motion for that particle;
* handshake hygiene; reset drops in flight.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,800 ALMs, 12 DSPs, 0 M10K.**

The DSPs are the dot products and the reflection. No memory: terrain arrives as
a sample.

## Integration capture cases
* **debris settling into a fresh crater** — the live-deformed-terrain case, and
  the one that justifies the whole feature. Debris must land on the new surface,
  not the old one.
* **32,768 particles over varied terrain** — confirms one-per-clock with a real
  slope distribution.
* **a `STICK` population over many ticks** — nothing drifts.
* replay equality, as the other particle blocks require.

## Notes

leaf (results return through PART.STATE's writeback path).
