# Contract — GEOM.WCACHE (projected-vertex arena)

> Ledger: `design/blocks.yml` · owner ZH-040 · phase 8 · maturity SPECIFIED

## Purpose and exclusions

**A bounded, direct-indexed vertex arena. NOT a cache.** It exists so a vertex
shared by several primitives is projected ONCE and replayed, and so the two views
share that work.

Owner ruling 2026-08-24 fixes the shape, and the exclusions are the load-bearing
part:

* **no associative tags, no LRU, no arbitrary eviction** — a lookup is an index,
  not a search;
* **no coordinate-key lookup.** Comparing 96-bit position triples to decide
  identity is forbidden: it is expensive, and it answers "same place" when the
  question is "same vertex";
* **`src_id` is not overloaded.** It stays opaque passthrough. Identity is
  carried by explicit new fields;
* it is **not a general cache hierarchy**, and this contract does not authorise
  one.

**Why a plain arena suffices:** the producer already knows each vertex's index. A
terrain patch is a 33x33 lattice and the tessellator holds `vi`/`vj` before it
expands them into world coordinates. A skinned mesh has a vertex number.
Identity is *given*, never *inferred*, so an associative structure would be
paying to rediscover something the producer never lost.

## Clock and reset semantics

Single `clk`, synchronous to `gpu_clk`. Active-low `rst_n` clears `sealed`,
`generation` and every valid bit; it does **not** clear the payload memory —
`QUARTUS_GOTCHAS` 10, a reset that touches the array prevents inference. A lookup
into an unwritten slot is refused by the valid bit, not by the contents.

## Input and output packet layouts

### FILL channel (write)

| field | width | meaning |
| --- | --- | --- |
| `fill_valid_i` / `fill_ready_o` | 1 / 1 | ready/valid |
| `fill_arena_i` | `ARENA_W` | which arena |
| `fill_gen_i` | `GEN_W` | generation of that arena |
| `fill_index_i` | `INDEX_W` | vertex index **within** the arena |
| `fill_payload_i` | `PAYLOAD_W` | the projected result to replay |

### LOOKUP / REPLAY channel (read)

| field | width | meaning |
| --- | --- | --- |
| `look_valid_i` / `look_ready_o` | 1 / 1 | ready/valid |
| `look_arena_i` `look_gen_i` `look_index_i` | as above | the key, all explicit |
| `rep_valid_o` | 1 | a result is presented |
| `rep_hit_o` | 1 | 1 = payload valid, 0 = deterministic MISS |
| `rep_payload_o` | `PAYLOAD_W` | valid only when `rep_hit_o` |
| `rep_refuse_o` | 1 | the lookup was **malformed**, not merely absent |

`rep_hit_o` and `rep_refuse_o` are distinct on purpose: a miss says *project this
vertex*, a refusal says *the caller asked something illegal*. Collapsing them
would let a bug look like a cold cache forever.

### ORIGIN channel — the datum, and the reason this block exists at all

| field | width | meaning |
| --- | --- | --- |
| `org_we_i` | 1 | write an arena's datum |
| `org_arena_i` | `ARENA_W` | |
| `org_x_i` `org_y_i` `org_z_i` | 32 each | **full-width** fx16 origin |
| `org_x_o` `org_y_o` `org_z_o` | 32 each | the datum of `look_arena_i` |

**Position is carried REBASED: one full-width origin per arena, bounded LOCAL
coordinates per vertex.** The projector folds the origin into its per-arena
translation and multiplies the *local* coordinates.

> **The absolute coordinate must NOT be reconstructed before the multiplied row
> terms.** `M·(O + d) = M·O + M·d`, and `M·O` is a per-arena constant computed
> once. Rebuilding `O + d` and multiplying that throws away the entire reason for
> the representation: the operand reaching a DSP would be world-sized again.
> Measured 2026-08-24 — `32x27` costs 3 DSPs, exactly what `32x32` costs, so a
> wide operand on *either* side forfeits the cheap band.

## Backpressure rules

One lookup per clock is the target. `look_ready_o` may fall only when a fill is
using the write port in the same cycle and the chosen banking cannot absorb both;
whether that ever happens is a **derived** property of the banking decision, not
an owner choice. Fill and lookup are separate channels precisely so the producer
never has to serialise them by hand.

## Memory ownership

The arena owns one inferred simple-dual-port memory of `DEPTH x PAYLOAD_W`, plus
a valid bitmap and a per-arena `generation` register. Direct-indexed: the address
IS `fill_index_i`. Synchronous read, no reset on the array, no byte enables — the
three properties `QUARTUS_GOTCHAS` 10 measured as decisive for inference.

**A map reporting `blockMemoryBits = 0` for this block is a FAILED implementation
however green its tests are.** That rule found a block at 229% of the device.

## Q formats and rounding

The arena performs **no arithmetic**. It stores and returns payload bits
verbatim, so it introduces no rounding and cannot drift from the reference. The
origin words are fx16 (S 1.15.16) and are likewise stored verbatim.

## Latency (fixed or variable)

Fixed: **one clock** from an accepted lookup to `rep_valid_o`, being the
synchronous memory read. No variable-latency path exists because there is no miss
handling — a miss is answered in the same cycle a hit would be.

## Target throughput

One lookup per clock at 100 MHz. The demand that motivates the block: a 33x33
terrain patch presents **6,144** vertex references across its triangles while
holding only **1,089** distinct vertices, so replay removes ~82% of projector
work before any width or rate optimisation is considered.

## Overflow and malformed-input behaviour

**Every one of these is a deterministic refusal, and NONE may alias into another
vertex:**

| condition | response |
| --- | --- |
| `look_index_i >= DEPTH` | `rep_refuse_o`, no memory read |
| `look_arena_i >= ARENAS` | `rep_refuse_o` |
| lookup while the arena is **not sealed** | `rep_refuse_o` |
| `look_gen_i` != the arena's current generation | `rep_refuse_o` (**stale**) |
| `fill_index_i >= DEPTH` | fill dropped, `arena_overflow_o` sticky |
| fill to a sealed arena | fill dropped, `arena_overflow_o` sticky |

Sealing exists so "the producer has not written this yet" and "this vertex does
not exist" are different answers. Generation exists so a reused arena cannot
serve last frame's vertices — the failure a plain valid bit cannot catch.

## Counters and traces

`wcache_hits_o`, `wcache_misses_o`, `wcache_refusals_o` (u32, saturating),
`arena_overflow_o` (sticky). Refusals are counted separately from misses: a
rising refusal count is a **caller bug**, a rising miss count is normal cold
traffic.

## Scalar reference function

`zref::geom::VertexArena` in `reference/include/zref/zref_geom_wcache.hpp`,
providing `seal`, `fill`, `lookup` and `origin` with identical refusal semantics.
The RTL differential replays a recorded command stream against it.

## Directed tests

Fill/lookup round trip; miss on unfilled index; refuse on out-of-range index;
refuse on out-of-range arena; refuse before seal; refuse on stale generation;
**generation wrap** (a wrapped generation must not resurrect stale data); fill to
a sealed arena dropped and counted; overflow sticky; hit and fill in the same
cycle; origin write/read per arena; **payload returned bit-identical**.

## Randomized differential tests

Random interleavings of fill/seal/lookup/origin against `VertexArena`, including
deliberately malformed keys at a high rate — the refusal paths are the ones a
random test would otherwise never reach.

## Formal properties

1. a lookup NEVER returns a payload written under a different `{arena, index}`;
2. a lookup NEVER returns a payload written under a different generation;
3. `rep_hit_o` implies the slot was filled since the last seal of that arena;
4. an out-of-range or unsealed lookup performs **no memory read**;
5. `arena_overflow_o` is sticky;
6. covers: a hit, a miss, each refusal class, and a generation wrap.

Property 1 is the one that matters: **it must never wrap into another vertex.**

## Synthesis / resource ceiling

Inferred block memory, **0 DSPs**, and the block is a store — an ALM count in the
hundreds means the memory did not infer and the implementation has failed.

## Integration capture cases

`PROJECT+WCACHE` composed fit, per the ruling's delivery order, once the RTL
exists. That measurement — not a leaf or pair fit — is what may be cited about
this block's timing (`QUARTUS_GOTCHAS` 12: a small design in a large device
measures its own placement).

## Notes

Built as a **reusable parameterised arena primitive** with a `GEOM.WCACHE` shell
around it. Terrain may later instantiate the same primitive at its own depth and
payload width. **Same primitive does not mean one physically shared memory**, nor
one arena arbitrating unrelated traffic.

Banking (single vs ping-pong) is **derived** from the one-lookup-per-clock target
and the measured workload, and is recorded here when measured — not chosen ahead
of the evidence.
