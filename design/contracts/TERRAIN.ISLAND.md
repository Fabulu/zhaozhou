# Contract — TERRAIN.ISLAND (Island patch directory)

> Ledger: `design/blocks.yml` · gpu clock · maturity REFERENCE_COMPLETE
> RTL: `fpga/rtl/terrain/zhao_terrain_island_dir.sv`
> Reference model: `zref::island::Directory` — `reference/include/zref/zref_island.hpp`
> Test: `tests/terrain/island_dir_rtl_directed.cpp`

## Purpose

Answers one question about one patch of one island: **is there ground here, and
if so which page holds it?** The four answers are `RESIDENT` (with a page
handle), `OPEN_SKY`, `OUT_OF_EXTENT` and `BAD_PITCH`.

**Exclusions.** It does not store pages, does not load them, does not evict and
does not decide policy. `TERRAIN.RESIDENCY` owns the set-associative store and
who may use a handle; `SW.STREAM` owns what gets published and evicted. This
block owns the *meaning* of that store's answer for an island, and nothing else.

## Why it exists at all: a miss has no meaning on its own

An 8 km island at the canonical 2.0 m pitch is 125 × 125 = **15,625 patches**,
of which the shipped island has **793** of ground — the figure `terrain_rules`
§1.4 costs out from its 3.25 km², derived independently in
`tests/terrain/island_8km_directed.cpp`.

So **94.9% of every honest query finds nothing**, and that is the normal case,
not a failure. The reference states the rule directly:

> NOT A MISS. Sky is the ordinary answer for most of an island's grid, and
> reporting it as a failure would make the normal case look like an error and
> hide the ones that are.

A store that raised a fault on every miss would raise fifteen thousand of them
per full sweep, and a genuine eviction fault — the kind that means a frame is
about to read a page somebody else owns — would be invisible inside that number.
`OPEN_SKY` is therefore a first-class outcome with its own counter.

## Why it composes the store rather than replacing it

The obvious hardware reading of a sparse 125 × 125 grid is a table with an entry
per patch. **The arithmetic refuses it immediately.** A dense handle store,
addressed on a power-of-two row stride so the index is a shift rather than a
multiply, is 128 × 128 × 32 bits = **524,288 bits = fifty-two M10Ks** on a
553-block device — to hold a structure whose occupancy peaks near 800.

The sparse store already exists and is UNIT_VERIFIED: `TERRAIN.RESIDENCY`'s
256-set, 4-way directory over the canonical key
`{resource_epoch, island_id, patch_ix, patch_iz}`. This block is the extent and
pitch gates plus the outcome mapping. Its whole cost is a comparator tree and
four counters.

That is also why the reference model is a `std::map` rather than an array: it
mirrors a cache, and a dense reference would have quietly implied a dense
design.

## The outcome order, and why it is load-bearing

`zref::island::Directory::find` tests in exactly this sequence, and the RTL
mirrors it:

| order | condition | outcome |
|---|---|---|
| 1 | `pitch_log2` outside {−1, 0, +1, +2} | `BAD_PITCH` |
| 2 | `ix` or `iz` outside `[0, extent)` | `OUT_OF_EXTENT` |
| 3 | store miss | `OPEN_SKY` |
| 4 | store hit | `RESIDENT` + handle |

**Pitch outranks extent**, and that is not arbitrary tidiness: a descriptor
naming a pitch the machine does not have cannot be trusted to say what its
extent *is*. Reporting `OUT_OF_EXTENT` for a coordinate checked against an
untrustworthy bound would be a confident wrong answer. The directed test asserts
this exact case — an out-of-extent coordinate on an illegal pitch must come back
`BAD_PITCH`.

**An out-of-extent coordinate never reaches the store.** The store is keyed on
truncated 16-bit indices, so a coordinate outside the extent could alias onto a
real patch and return another island's ground. The gate is not an optimisation.

**A negative coordinate is outside, not enormous.** The extent test is signed,
verbatim from the reference. Dropping the sign wraps a small negative into the
middle of the grid.

## Pitch and scale

Legal `pitch_log2` is −1, 0, +1, +2. The canonical +1 gives a 2.0 m pitch and,
on the 32 × 32-cell lattice, a **64 m patch** — which is what makes 8 km come to
125 patches per axis. An illegal value is **refused, never clamped**: rounding a
malformed descriptor to the nearest legal scale puts an island at a size nobody
asked for, and it would do so silently.

## Interface shape

One query in flight. The residency answers in order, so the proof that an answer
belongs to its query is trivial; a deeper pipeline is a measurement away, not a
guess away. The query coordinate is **captured at acceptance** and the gates read
the captured value — the descriptor is frame-scoped and may be read live, but the
coordinate may not, which is the ingress-capture rule
(`tools/rtl/check_ingress_capture.py`).

The answer carries the query's own tag, so a block answering the *previous*
query cannot pass its test.

## Counters

`island_resident`, `island_open_sky`, `island_out_of_extent`, `island_bad_pitch`
— one per outcome, mirroring `zref::island::Ledger` field for field. The
directed test compares all four against the oracle's ledger rather than checking
they are non-zero, and additionally requires that a run genuinely contained
resident, sky **and** out-of-extent answers, so the comparison cannot be
satisfied by one outcome repeated.

## Scalar reference function

`zref::island::Directory::find` (`reference/include/zref/zref_island.hpp`),
with `zref::island::Ledger` as the counter oracle.

**The oracle is the OUTCOME MAPPING, not the store.** That distinction is the
same one `TERRAIN.RESIDENCY` draws and it is deliberate here for a sharper
reason: the mapping is where a wrong answer is *silent*. A set-associative
directory that loses a page fails loudly the moment a frame reads it, but
`OPEN_SKY` returned where `RESIDENT` was correct simply draws nothing, on a
grid where drawing nothing is the answer 94.9% of the time. There is no
picture in which that is visible. So the mapping gets a definition outside the
RTL that implements it, and the differential compares every outcome, every
page handle, every answer tag and all four counters against it.

`tests/terrain/island_dir_rtl_directed.cpp` — 21 checks:

* the directed cases, which pick coordinates to sit on named edges — the
  extent boundary, a negative coordinate, an illegal pitch, and the ordering
  case where an out-of-extent coordinate on an illegal pitch must report
  `BAD_PITCH`;
* a **full grid sweep** of all 15,625 patches: RTL 793 resident / 14,832 sky,
  oracle identical. This is the check that a mapping bug cannot hide in, since
  it visits every input the block has;
* a **3,000-draw randomised phase** on coordinates nobody chose, a quarter of
  them deliberately out of extent and split between negative and beyond —
  the negative half being where an unsigned compare would wrap a small
  negative into the middle of the grid and return another patch's ground.

The randomised phase's first version reported 907 mismatches: exactly the 37
resident plus the 870 sky, i.e. every in-extent draw and no other. **A defect
that lands on precisely one clean partition of the input is a bench artefact,
not a block defect** — it was the bench redrawing the coordinate while
`q_valid` was held, which both breaks the payload-stability half of
ready/valid and desynchronises the expected-answer list from what the block
consumed. The check that now guards it asserts one answer per *accepted*
query, so the comparison is aligned rather than merely the same length.

## What is not yet established

The block has not been fitted, and it has not been composed with the real
`TERRAIN.RESIDENCY` — the directed test models the store so that the *mapping*
is what is under test, which is the block's actual job. Composing the two is a
separate step with a separate question, and the residency has its own
differential.
