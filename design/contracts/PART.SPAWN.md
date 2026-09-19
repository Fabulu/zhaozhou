# Contract — PART.SPAWN (Deterministic child spawn)

> ## SUPERSEDED NUMERIC LAW — READ BEFORE ANY Q-FORMAT SENTENCE BELOW
>
> **This contract describes the particle position and velocity scales as an open
> Class-C question. They were ruled.** Amendment C2 (2026-09-02, `QFMT_VERSION`
> 2 → 3, owner ruling R3 of
> `reports/OWNER-RULINGS-BUILDABILITY-20260902.md`) replaced `spec/qformats.md`
> §10 whole:
>
> | | what this contract still implies | C2 (binding) |
> |---|---|---|
> | position | unruled, Class C | **s18, S 9.8 m**, LSB 1/256, relative to the population origin |
> | velocity | unruled, Class C | **s11, S 2.8 m/tick**, LSB 1/256 |
>
> Both carry the same eight fractional bits, so integration is `pos + vel` with
> no shift — which is the fact a reader of the stale text would go looking for
> and not find.
>
> `PART.EXPAND.md` and `PART.SOFT.md` already carry a banner of this shape.
> These two did not, which is why a block was built in September 2026 citing
> "the scale is unruled" as its reason for doing nothing.

> ## AMENDED — THE EVENTS ARRIVE FROM **PART.COLLIDE**, NOT PART.UPDATE. RULED.
>
> Raised as CONTESTED 2026-09-19 and **ruled the same day**:
> `reports/RULING-I4-COLLISION-SPAWN-20260919.md`, owner *"Go with your
> recommendations on the whole goal run."* This banner replaces the contested
> one; the paragraphs below are the record of what was wrong.
>
> **The amendment, and it is two sentences of this contract:**
>
> | | this contract's text | binding after the ruling |
> |---|---|---|
> | **In** | events *"from `PART.UPDATE`"* | **from `PART.COLLIDE`** |
> | parent record | the particle as PART.UPDATE left it | the **POST-CONTACT** record, i.e. `pout_c` |
>
> **The event's CONTENT is unchanged.** The four events stay FROZEN at birth /
> age marker / collision / death (owner ruling 2026-08-31 §2.4); the count, the
> order and the 16-child bound are untouched. Only the announced ORIGIN moves,
> and it moves because it was wrong.
>
> **Why it was wrong.** **In** required *"The parent particle plus its
> spawn/death events from `PART.UPDATE`"*, and the FROZEN event list includes
> **collision**. But `PART.COLLIDE.md` places PART.COLLIDE strictly DOWNSTREAM
> of PART.UPDATE and calls it a leaf, and `PART.UPDATE.md` excludes the collision
> response from its own oracle and from its input list. PART.UPDATE therefore
> could not know a collision had occurred, so it could not author event bit 2.
>
> The measured consequence in the composed console: `spawn_by_event2_o` was
> **structurally stuck at zero**. Spawn-on-collision — sparks on impact — did not
> work, and no test said so, because the counter read zero exactly as it would if
> no collision had happened. **Do not quote any reading of it taken before
> 2026-09-19 as evidence.**
>
> **Where the child is placed** is the physics question no contract answered.
> Ruled: **POST-CONTACT**, because it is what PART.COLLIDE already computes (so
> no second placement law enters the tree), because pre-contact puts the spark
> inside the surface the particle just hit, and because `kPartStuck` already
> describes a particle at rest ON the surface. It is reversible by one named
> constant — `CHILD_AT_POST_CONTACT` in `fpga/rtl/particles/zhao_part_collide.sv`
> — and CLAUDE.md's art law applies: judge it by looking at sparks in motion.
>
> In RTL: PART.UPDATE's three own event bits cross PART.COLLIDE on the same
> register enable as the record, PART.COLLIDE fills bit 2 from its own
> `c_contact_o`, and `c_spawn_record_o` is the parent record this block reads.

> Ledger: `design/blocks.yml` · owner ZH-064 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Deterministic child-particle spawning (seeded, ordered, budgeted) writing back into the state stream.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons the parent in
flight and any children it had queued but not emitted. **A partially emitted
child set is never committed** — children are appended as a group, so a reset
cannot leave half a burst in the stream.

## Input and output packet layouts
### In
The parent particle plus its spawn/death events from `PART.COLLIDE`, and the
parent's species descriptor.

> **AMENDED 2026-09-19** (ruling I4). This sentence read *"from `PART.UPDATE`"*.
> PART.UPDATE cannot observe a collision, so it could not author one of the four
> frozen events; PART.COLLIDE carries PART.UPDATE's other three across and fills
> that one in. The parent particle is the **post-contact** record. See the banner
> at the head of this file.

### Spawn events — FROZEN, owner ruling 2026-08-31 §2.4

A species descriptor may name child species for exactly four events:

    birth        a bounded age marker        collision        death

**Maximum 16 children per event.**

The ruling adds the clarification that keeps this from looking like a
limitation: *"Large spell casts may seed large populations directly from the
HPS; the 16-child limit governs local particle-to-particle bursts and therefore
does not limit a tornado, explosion or god spell to 16 visible particles."*

So 16 is a bound on **cascade fan-out**, not on spectacle. That distinction is
what makes the bound safe to enforce in hardware.

### Ordering — exact, and it is the determinism contract

    parent stream order   ->   event order   ->   child index 0..N-1

**One spawn generation per tick.** A child may spawn on a later tick, never
recursively in the same one. That is what bounds the work: without it, a single
tick could cascade unboundedly and no fixed-latency stage could contain it.

## Backpressure rules
Ready/valid, house hygiene. A parent's children are emitted as a group; the
block holds the group until the consumer has taken all of it, so a stall
mid-group cannot interleave another parent's children and break the ordering
above.

## Memory ownership
**None.** Children are appended into `PART.STATE`'s write stream; that block
owns the buffers. The species table is read as a port.

## Q formats and rounding
Child position and velocity are derived from the parent's, in the same formats,
subject to the same open Class-C scale question recorded in `PART.UPDATE`.

**Child variation comes from the stateless hash**, seeded by
`{parent_id, event, child_index, tick}` — never from a running counter. A
counter would make a child's identity depend on how many particles were
processed before it, which is the iteration-order dependence `PART.STATE`'s
contract forbids for exactly this reason.

## Latency (fixed or variable)
**Variable in a bounded way**: 1 clock for a parent with no children, up to 16
for a full burst. Bounded is the property that matters, and it follows directly
from one generation per tick.

## Target throughput
**One spawn decision per clock** (ledger) for parents without children.

The worst case is a species that spawns 16 on every tick, which would cost 16×
the parent stream. That is a **content** bound, not a hardware one, and the
counters below are what make it visible before it becomes a frame-rate mystery.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| capacity exhausted | **survivors outrank children; accepted earlier parents outrank later ones; later children are dropped deterministically** — the ruling's priority, in that order |
| child count > 16 | refuse the group, count it. Do not emit the first 16 — a truncated burst is a different effect, silently |
| unknown child species | refuse the group |
| recursive spawn in the same tick | impossible by construction, and asserted |

**Deterministic dropping means by stream order, never by arrival.** The same
input must drop the same children on a repeat run, which is asserted directly by
running each random case twice.

## Counters and traces
* `polygon_particles` (ledger)
* `children_requested`, `children_emitted`, `children_refused` — the ruling's
  three, kept separate so a budget overrun shows as a shape
* `spawn_by_event[4]`
* `groups_refused_by_reason[3]`
* `max_children_in_tick` — the content-bound signal named above

## Scalar reference function
`zref::ParticleSpawn` (ledger). Owns the four events, the 16 bound, the exact
ordering, the stateless child hash and the drop priority.

It does **not** own what a child *is* beyond its derivation from the parent —
the child's own behaviour is its species descriptor's, and this block only
places it into the stream.

## Directed tests
`tests/particles/part_spawn_directed.cpp`.

* each of the four events fires exactly once for its condition;
* 16 children accepted; **17 refuses the whole group**, and specifically does
  not emit the first 16;
* ordering: two parents each spawning on two events, and the emitted stream
  matches parent → event → index exactly;
* **no recursion**: a child whose species also spawns on birth produces nothing
  more this tick, and produces its own children on the next;
* exhaustion with mixed survivors and children: survivors all retained,
  children dropped from the end, and the **same** children dropped on a repeat;
* child variation differs between siblings but is reproducible — the same parent
  and tick gives the same 16 variations.

## Randomized differential tests
`tests/particles/part_spawn_random.cpp`, RTL against `zref::ParticleSpawn`.

Random species graphs with random spawn depths, run **to and past capacity** —
a spawn test that never exhausts is testing the easy half. Report the
requested/emitted/refused mix.

Run each case twice and compare, as every particle block requires.

## Formal properties
`tests/formal/part_spawn_order.sby`:

* **no child is emitted before any survivor** — the ruling's priority as a
  safety property;
* the emitted order is exactly parent → event → child index, under arbitrary
  backpressure;
* **at most one generation per tick** — no child emitted this tick can itself
  produce a child this tick. This is what bounds the block's work;
* a group is emitted whole or not at all;
* handshake hygiene; reset drops in flight without committing a partial group.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,200 ALMs, 2 DSPs, 0 M10K.**

Two DSPs at most: child derivation is a small transform of the parent, and the
hash is logic, not arithmetic. If this block grows DSPs, child *behaviour* has
migrated in from `PART.UPDATE`, where it belongs.

## Integration capture cases
* **a cascade species** — children that spawn children, over many ticks.
  Confirms the generation bound holds and the population converges rather than
  exploding.
* **a capacity-exhaustion frame** — the priority is visible and repeatable.
* **a Level-9 spell** — the case the ruling explicitly carves out: the HPS seeds
  the population directly, so this block's 16-child bound must be nowhere near
  the limiting factor. If it is, the carve-out was misread.
* replay equality.

## Notes

Determinism: identical state+program ⇒ identical children (capture-exact).
