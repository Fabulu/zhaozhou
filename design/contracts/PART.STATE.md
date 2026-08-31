# Contract — PART.STATE (Particle state stream)

> Ledger: `design/blocks.yml` · owner ZH-042 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Stream 128-bit particle state (provisional packing, plan Q3) from HPS DDR, write back spawned/died particles.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. The HPS DDR side crosses in
`MEM.HPS.BRIDGE`, not here.

Reset abandons the stream in flight. **The ping-pong buffers are NOT cleared** —
they hold the last committed generation, and clearing them would destroy the
simulation rather than restart it. What resets is the pointer state, the
compaction cursor and the in-flight tags.

A tick is atomic at the buffer level: the read buffer is never written and the
write buffer is never read within one tick, so a reset mid-tick loses that tick
and leaves the previous generation intact.

## Input and output packet layouts
### The particle record — FROZEN, owner ruling 2026-08-31 §2.1

**128 bits exactly.**

| field | bits | note |
|---|---|---|
| position | 54 | 3 × 18 |
| velocity | 33 | 3 × 11 |
| age | 10 | |
| species | 7 | index into the species descriptor table |
| size | 6 | |
| spin | 6 | |
| flags | 4 | |
| variation | 8 | the per-particle hash input |
| **total** | **128** | |

**Everything else lives in the species descriptor**, not in the particle:
lifetime, update recipe, collision response, curves, material, child-spawn
rules. 7 bits of `species` therefore buys 128 distinct behaviours at zero
per-particle cost, which is why the record fits at all.

### Randomness — stateless, and this is a correctness rule not an optimisation

> "Randomness is stateless deterministic hashing from stable identifiers and
> variation. Do not use a tiny evolving random seed whose sequence changes when
> a particle is culled or processed in a different batch."

An evolving per-particle seed makes the simulation depend on **iteration
order**, so culling one particle silently changes every later one. Determinism
is the whole capture/replay contract, so this is a hard rule. Use the existing
full-domain hash with `{particle_id, variation, tick, lane}` as inputs.

### Streams

Dense sequential ping-pong in HPS DDR. Per tick:

1. **survivors are compacted first**, in stream order;
2. **children are appended after survivors**;
3. on exhaustion, **survivors always outrank new children**.

That ordering is the ruling's and it is also what makes the stream dense: a
compaction that interleaved children would need a second pass or a gap list.

## Backpressure rules
Ready/valid, house hygiene on both channels.

The block is a memory streamer, so its real backpressure is `MEM.HPS.BRIDGE`'s
grant. It must not stall the update pipeline for a write that has not been
granted: writes are posted into a bounded staging FIFO and the tick continues.

**If staging fills, the tick stalls rather than dropping a particle.** Dropping
under backpressure would make the result depend on memory timing, which is the
same determinism failure as an evolving seed.

## Memory ownership
**Owns the two particle buffers in HPS DDR** and nothing else on chip.

At the required tier — **32,768 active particles** × 16 B — each buffer is
**512 KiB**, so 1 MiB for the pair. The stretch tier of 65,536 doubles that to
2 MiB and the ruling gates it explicitly on physical board bandwidth, not on
this block's willingness.

**The traffic is the number that matters**, and it should be stated before
anyone is surprised by it: 32,768 particles read + written every tick at 16 B is
**1 MiB per tick**. At 60 Hz that is **63 MB/s** sustained, before species
descriptors, before spawn appends, and before every other client. That is the
figure the stretch tier must be argued against.

Species descriptors are read-only and small; they belong in an on-chip table
loaded per frame, not re-fetched per particle.

## Q formats and rounding
Position is 18 bits per axis and velocity 11 — both **deliberately narrow**, and
the contract must state what they mean before RTL fixes it by accident.

The scales are a Class-C decision the ruling did not make, because they are
capture-visible: they determine what a particle position *means*. Until they are
ruled, this block **carries the bits without interpreting them** — it is a
streamer, and `PART.UPDATE` is where the arithmetic lives.

What this block does assert: **the packing is exact and lossless**. A record
read and written back untouched must be bit-identical, which is a testable
property that does not depend on the scale question.

## Latency (fixed or variable)
**Variable**, and dominated entirely by HPS DDR. The block's own work — unpack,
route, repack — is a fixed small number of clocks.

The contract deliberately does not quote an end-to-end figure. This is a memory
streamer behind a bridge that has its own arbitration, and a latency number
measured without the rest of the frame's traffic would be a fiction.

## Target throughput
**One particle per clock** (ledger).

At the required tier that is 32,768 clocks per tick — **2.5 % of a
1,333,333-clock frame**. The particle system is not clock-bound at the
guaranteed tier; it is **bandwidth**-bound, per the 63 MB/s above.

That asymmetry is worth stating plainly, because it means optimising this
block's clocks-per-particle is the wrong lever, and shrinking the record or
avoiding the round trip is the right one.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| capacity exhausted | **survivors retained, later children dropped deterministically** — the ruling's priority, and "deterministically" means by stream order, never by arrival |
| `species` index out of range | refuse the record, count it; do not fabricate a species |
| staging FIFO full | **stall**, never drop |
| a read response arrives after reset | discard by tag |

Counters record **requested, emitted and refused** children separately, which is
the ruling's wording and is what makes a budget overrun visible as a shape
rather than as a single number.

Note this block's overflow is the *soft* kind: dropping a queued child is
expected behaviour under load. The hard-overflow law — fault, drain, repeat the
previous frame — belongs to geometry capacity, not here.

## Counters and traces
* `polygon_particles`, `hps_ddr_bytes_by_client` (ledger names)
* `particles_live`, `particles_died`, `particles_spawned`
* `children_requested`, `children_emitted`, `children_refused`
* `bytes_read`, `bytes_written` — the bandwidth question, measured
* `stall_cycles_memory`, `stall_cycles_consumer` — separately
* `species_histogram[128]` — which species a real frame actually uses

## Scalar reference function
`zref::ParticleState` (ledger `reference_model`).

Owns the 128-bit pack/unpack, the compaction order, the survivor-over-child
priority and the refusal taxonomy. It does **not** own the update arithmetic or
the spawn rules — those are `zref::ParticleUpdate` and `zref::ParticleSpawn`,
and duplicating them here would create a second law.

## Directed tests
`tests/particles/part_state_directed.cpp`.

* **pack/unpack round trip is bit-exact** for every field at its extremes. With
  128 bits fully assigned there is no slack, so an off-by-one in any field
  offset corrupts its neighbour — this test is what catches that;
* the record is **exactly 128 bits**: assert the sum, so adding a field without
  removing one fails here rather than in silicon;
* compaction order: survivors dense and in stream order, children strictly
  after;
* exhaustion: survivors retained, children dropped, and **the same input drops
  the same children** on a repeat run — determinism, asserted directly;
* a culled particle does not change any later particle's hash. This is the
  stateless-randomness rule, and it is the single most valuable case in the
  file, because its failure mode is a replay that diverges hours later;
* `species` out of range: refused, counted, no fabrication.

## Randomized differential tests
`tests/particles/part_state_random.cpp`, RTL against `zref::ParticleState`.

Random populations with random death and spawn patterns, run to capacity and
past it. The generator must report its **refusal and drop mix**, so a change
that makes exhaustion unreachable shows as a shift rather than as silent lost
coverage — the same requirement as the geometry blocks.

**Run every random case twice and compare.** Determinism is this block's core
property and a differential against a reference does not test it; only
re-running does.

## Formal properties
`tests/formal/part_state_priority.sby`:

* **a survivor is never dropped while a child is emitted** — the ruling's
  priority, as a safety property rather than a test case;
* the output stream is **dense**: no gap between survivors, and children
  strictly after the last survivor;
* no record is emitted twice within a tick;
* handshake hygiene on both channels;
* reset leaves the previous generation intact — the buffers are not cleared.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 900 ALMs, 0 DSPs, ≤ 4 M10K.**

Zero DSPs is a real constraint: this block streams and repacks, it does not
compute. A DSP in its fit means arithmetic has migrated here from
`PART.UPDATE`.

M10K is for staging and the species table only. The particle buffers are in HPS
DDR by the ruling and must not drift on chip — at 512 KiB each they could not
fit anyway, which is a useful hard stop.

## Integration capture cases
* **32,768 particles, the required tier**, sustained for many ticks — measures
  the 1 MiB/tick round trip against real bandwidth alongside every other client.
  This is the trace that decides whether the 65,536 stretch tier is arguable at
  all.
* **a capacity-exhaustion frame** — more children requested than space. Confirms
  survivors are retained and the same children are dropped on a repeat run.
* **a species mix change mid-frame** — the histogram moves, nothing else does.
* **replay equality**: capture a particle-heavy frame, replay it, require
  bit-identical output. The stateless-randomness rule exists for this and
  nothing else proves it.

## Notes

particle128 layout provisional (charter §13); Phase 10 revisits seed starvation.
