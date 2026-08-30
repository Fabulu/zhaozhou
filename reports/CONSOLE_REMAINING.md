# What is actually left to build

An audit of `design/blocks.yml` against the RTL tree, the test tree and the
CONTRACTS, 2026-08-31. It exists because the ledger's own summary is misleading
in both directions, and "finish the console" needs a real list rather than a
status field.

**This file was wrong on its first pass and is corrected below.** The first
version listed seven blocks as "real, unblocked hardware work" on the strength
of their ledger entries. Reading their contracts changes that to **zero**. The
first-pass reasoning is kept at the end, because the way it was wrong is the
useful part.

---

## The headline

`design/blocks.yml` holds **92 blocks** and reports 37 SPECIFIED. Of those:

| | count | |
|---|---|---|
| `blocked_on: hardware` | 6 | waiting for a board, not for work |
| software / profile entries | 12 | behind hardware by standing direction |
| **already built, ledger stale** | **2** | should be advanced |
| waiting on owner decisions about behaviour | 10 | particles, 2D, compositor |
| **have no usable contract** | **7** | see below |

**The number of blocks that can be built today from what is written down is
zero.** Not because they are hard — several are small — but because in every
case the laws they would have to obey do not exist yet.

---

## Built, but still listed SPECIFIED — advance these

| block | the RTL | the gate |
|---|---|---|
| `TERRAIN.PATCH` | `zhao_terrain_patch.sv` | `terrain_patch_directed`, composed 33x33 dual patch |
| `GEOM.WCACHE` | `zhao_geom_wcache.sv` over `zhao_vertex_arena.sv` | 73-check differential, mutation sweep, inductive formal proof |

**The audit method is itself a finding.** Searching the RTL for each block's
contract path found only `TERRAIN.PATCH` and reported `GEOM.WCACHE` as unbuilt,
because `zhao_vertex_arena` implements it without citing it. Searching for a test
named after the block found both. Neither method alone is reliable: a block can
be finished under any name.

---

## Waiting on owner decisions about behaviour

Standing direction: *"Do NOT invent game behaviour for the particle-simulation,
compositor or 2D blocks."*

`PART.STATE`, `PART.UPDATE`, `PART.COLLIDE`, `PART.SPAWN`, `PART.LADDER`,
`TWOD.PLANE`, `TWOD.SPRITE`, `POST.GATHER`, `POST.COMPOSITE`, `POST.ECHO`.

---

## The seven that looked buildable, and what each is actually missing

| block | what is missing |
|---|---|
| `INPUT.SNAC` | contract is a **stub: 15 TODO sections** — clocks, packets, backpressure, Q formats, refusals, all unwritten |
| `FORGE.PRIM` | **15 TODO sections** |
| `GEOM.VDECODE` | **15 TODO sections**, and the compressed vertex FORMAT is not pinned anywhere in `spec/` |
| `GEOM.WARP` | **15 TODO sections** |
| `GEOM.LOOM` | **15 TODO sections**; the purpose line is a list of BEHAVIOURS (gait, formations) that the particle rule covers |
| `GEOM.MESHFETCH` | contract has a real cull ruling and a real latency table, but **clocks, packets, memory ownership, Q formats and throughput are all TODO**. Two of its three thirds are already built (`zhao_geom_lod`, `zhao_geom_cull`); the third is a descriptor fetch whose memory layout does not exist |
| `MEASURE.HISTOGRAM` | **a documented refusal, not a gap** |

### `MEASURE.HISTOGRAM` deserves quoting, because it is the model

Its contract already explains at length why it was deliberately not started: it
would need **four stacked inventions** — the error metric, the bucket
boundaries, the cutoff rule, and a Version-2 input to a governor just built to
Version 1 — and none has a law anywhere in the tree. Its own words:

> a small block built on four invented laws is worse than no block, because the
> inventions become ratified by being built.

That is the correct answer for all seven, and the contract for one of them
already says so.

### And the five `FIELD.SEQ.*` entries

`EARTH`, `WARP`, `FLOW`, `FORMATION`, `STAMP` are **Field IR programs, not
datapaths.** The engine they run on is built and heavily optimised. They are
SPECIFIED because no program has been authored, and authoring one is closer to
content than to hardware.

---

## What is genuinely open, and it is all decisions

1. **`wmin`, `wmax`, `scale`** — `OPEN-SPEC-DEPTH-QUANTISATION.md`. Blocks
   GEOM.PROJECT's attribute carry, the renderer's last step-6 piece.
2. **The binner arena capacity** — `BINNER_CAPACITY_FOR_8KM_MAPS.md`. An army
   needs ~150x the triangle capacity and ~25x the references.
3. **What `276,480` counts** — `PER_PIXEL_BUDGET.md`. Decides whether two
   per-pixel blocks need replicating at all.
4. **Seven contracts to write**, or a ruling that a subset may be specified by
   whoever builds them.

---

## Honest summary

The console's remaining hardware is **not blocked on effort**. It is blocked on
**specification**: seven contracts that are stubs, ten blocks whose behaviour is
a design decision, six waiting on a board, and four open numeric or policy
questions.

What is finished and measured is substantial — the renderer's per-pixel path is
built, composed end to end and priced against a frame — and what remains cannot
honestly be advanced by writing more RTL against contracts that say TODO.

---

## Appendix: how the first pass got this wrong

The first version of this file read `design/blocks.yml`, filtered by `maturity`,
`kind` and `blocked_on`, cross-checked the RTL and test trees, and concluded
seven blocks were ready to build. Every one of those steps was correct. **It
never opened the contracts.**

The ledger says what a block IS. The contract says whether anyone has decided
what it must DO. A block can be unblocked, unbuilt, untested, small, and still
completely unbuildable — and only the contract shows it.

This is the same shape as two other errors made the same day: a setup cost
measured over the wrong interval, and an existing 268-line test nearly
overwritten because a file's absence was assumed rather than checked. In all
three the available evidence was consistent with the wrong answer, and the fix
was the same — go and look at the thing itself.
