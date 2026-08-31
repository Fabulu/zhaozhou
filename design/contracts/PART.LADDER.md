# Contract — PART.LADDER (Representation ladder)

> Ledger: `design/blocks.yml` · owner ZH-043 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Choose each particle's representation by projected size and budget (soft sprite vs streak vs polygon particle), consuming governor targets.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`.

**Reset clears the hysteresis and hold state**, which is a visible consequence
and is worth stating: after a reset every particle re-selects from scratch, so
the first frame may show representation changes that the hold would normally
have suppressed. That is correct — the alternative is stale hold state deciding
against a camera that no longer exists.

## Input and output packet layouts
### In
`{ particle, projected_size, camera_id, governor_target, prev_rung, hold_count }`

### Out
`{ particle, representation, changed }`

### The ladder — FROZEN, owner ruling 2026-08-31 §2.5

    meshlet  ->  triangle/shard  ->  ribbon/streak  ->  soft sprite  ->  glint  ->  culled

### The answer that shapes the whole block

> **Yes: a particle may change representation while it is alive.**
> The simulation state never changes because of the representation. Selection is
> **per camera and per frame**, so the same particle may be a triangle in one Duo
> view and a glint in the other.

Both halves matter. *Representation may change* means every transition has to be
made continuous. *Simulation never changes* means this block is **downstream of
everything** and feeds back into nothing — it cannot reseed, respawn or perturb
a particle, and the formal properties below enforce that.

Per-camera selection means the output is **per (particle, camera)**, not per
particle. In Duo that is two decisions for one simulation record.

### Provisional thresholds (Class B — evidence-driven defaults, not ABI)

| representation | projected size |
|---|---|
| meshlet | ≥ 18 px diameter |
| triangle/shard | ~6–18 px |
| ribbon/streak | trail ≥ 4 px, narrow enough to read as a line |
| soft sprite | ~2–8 px |
| glint | ~0.5–2 px |
| culled | < 0.5 px unless semantically protected |

Species may apply small threshold biases. **"Semantically protected" is a
species flag, not an inference** — a tiny but important particle stays visible
because its asset says so.

## Backpressure rules
Ready/valid, house hygiene. One decision in flight. The hold/hysteresis state is
indexed per (particle, camera), so a stall never loses it.

## Memory ownership
Owns the **per-(particle, camera) hold state**: `prev_rung` and `hold_count`.

At 32,768 particles × 2 cameras × ~1 byte that is **64 KiB**, which does not
belong in M10K beside the renderer — same conclusion as pose palettes and the
Loom parent store. It travels with the particle record's soft state or lives in
the SDRAM hot region.

**This is the block's one real cost and it is storage, not logic.** A design that
put the hold state on chip would be choosing 64 KiB of M10K to avoid a few
bytes per particle of DDR traffic, and that trade should be measured, not
assumed.

## Q formats and rounding
Projected size in fx16 pixels. Thresholds and biases in the same format.

Comparisons are **cross-multiplied, never divided** — the same technique
`zhao_geom_lod` already uses, where every quotient in the ladder feeds a
comparison and so the block carries no divider. Reuse that, because a divider
here would be pure waste for the same reason.

## Latency (fixed or variable)
**Fixed.** A comparison ladder with no iteration. The Class-B trade is sequenced
versus flat, exactly as in `zhao_geom_lod`, which took five products through one
multiplier at a cost of 12 of 18 DSPs.

## Target throughput
**One ladder decision per clock** (ledger) — but note the unit: per (particle,
camera).

At 32,768 particles in Duo that is 65,536 decisions, **~4.9 % of a
1,333,333-clock frame**. Double the other particle stages, and that is inherent
to per-camera selection rather than a fault.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| `projected_size` negative or NaN-equivalent | refuse, count; select `culled` for that frame only, and **do not update hold state** — a bad sample must not poison the next frame's hysteresis |
| governor target unattainable at the lowest rung | select `culled`, count it. The governor asked for less than the cheapest representation costs |
| `prev_rung` out of range (stale or corrupt state) | treat as no history: select fresh, count it |

Nothing here can refuse a *particle*; the simulation is upstream and untouchable.
The worst this block does is show it differently for one frame.

## Counters and traces
* `lod_representation_counts`, `polygon_particles`, `soft_particles` (ledger)
* `transitions[6][6]` — which rung pairs actually occur. A busy off-diagonal
  means hysteresis is too tight
* `transitions_suppressed_by_hold`
* `culled_by_size`, `culled_by_governor` — distinguished, because they mean very
  different things about a frame
* `semantically_protected_kept`

## Scalar reference function
`zref::RepresentationLadder` (ledger). Owns the six rungs, the thresholds, the
hysteresis and hold rules, and the per-camera independence.

Shares nothing with `zref::creature::lod_*` beyond technique: creature LOD picks
a mesh, this picks a representation *kind*, and merging them would couple two
ladders that the ruling deliberately keeps separate.

## Directed tests
`tests/particles/part_ladder_directed.cpp`.

* each rung selected at the centre of its band, and at both edges;
* **hysteresis**: a particle oscillating around a boundary must not change rung
  more than once per ~20 % of band width. Sweep it slowly across and count
  transitions — the count is the assertion;
* **four-frame hold**: a legitimate rung change is suppressed for three frames
  and applied on the fourth. Assert the exact frame, not "eventually";
* **per-camera independence**: one particle, two cameras at different distances,
  two different rungs in the same frame. This is the Duo law and the case a
  single-camera implementation silently passes;
* **the simulation is untouched**: run a population with the ladder enabled and
  with it forced to a single rung, and require the *simulation state* after N
  ticks to be bit-identical. This is the ruling's "simulation state never
  changes because of the representation", tested directly;
* `culled` below 0.5 px, and a semantically protected particle NOT culled at the
  same size;
* a bad `projected_size`: culled this frame, hold state unchanged.

## Randomized differential tests
`tests/particles/part_ladder_random.cpp`, RTL against `zref::RepresentationLadder`.

Random size trajectories — including deliberately adversarial ones that hover on
a boundary and that cross several bands in one frame. Uniform random sizes
almost never sit on a boundary, which is the only place this block is hard.

Report the transition matrix. Run twice for determinism.

## Formal properties
`tests/formal/part_ladder_stability.sby`:

* **no transition within the hold window** — for any input sequence, a rung
  change is followed by at least four frames without another for that
  (particle, camera);
* **the ladder is monotonic in size**: a larger projected size never selects a
  cheaper representation, for equal history. Without this a particle could get
  *simpler* as it approaches, which is the most visible possible bug;
* **no feedback**: no output of this block reaches any simulation state. The
  strongest property here, and the one that makes per-camera selection safe at
  all;
* handshake hygiene; reset clears hold state.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,000 ALMs, 6 DSPs, 0 M10K.**

Zero M10K is deliberate and is the interesting constraint: the 64 KiB of hold
state must live off chip. Six DSPs for cross-multiplied comparisons, following
`zhao_geom_lod`'s sequencing lever if the fit wants it.

## Integration capture cases
* **a particle receding from the camera through every rung** — the transition
  sequence is monotonic, each hold is honoured, and no pop is visible. Judge this
  from a **contact sheet of every frame**, not sampled stills; a single bad
  transition frame is exactly what uniform sampling misses.
* **Duo with cameras at different distances** — two rungs for one particle,
  simultaneously, and no interference between them.
* **32,768 particles in Duo** — 65,536 decisions, confirming the ~4.9 % figure.
* **the simulation-invariance capture**: the same seed with the ladder forced to
  each rung in turn, requiring identical simulation state every time.

## Notes

Ladder thresholds provisional until Phase-10 evidence.
