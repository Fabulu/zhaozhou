# What is actually blocking every remaining block

**Date:** 2026-08-21
**Method:** each of the 24 remaining `SPECIFIED`, non-deferred, non-hardware-
blocked RTL blocks was trial-advanced to `REFERENCE_COMPLETE` in a scratch copy
of `design/blocks.yml`, the ledger check run, the errors recorded, and the file
restored. Dashboard-staleness errors are filtered out as noise.

## The headline

**There are no cheap advances left.** `TERRAIN.BAKE` was the last block that was
finished but merely unrecorded — it advanced today with no work beyond
regenerating a diagram. `SURFACE.SHEET` and `TERRAIN.VELOCITY` each needed one
real test written. Everything after them needs the block itself built.

Today's advances went: `GEOM.SKIN`, `DEBUG.TRACE`, `SURFACE.SHEET`,
`TERRAIN.BAKE`, `TERRAIN.VELOCITY`, plus both halves of `GEOM.POSE`. That
exhausted the backlog of *built-but-unrecorded* work.

## The three shapes, and how many of each

### A. Greenfield — 16 blocks

`FIELD.PROGCACHE`, `GEOM.MESHFETCH`, `GEOM.PROJECT`, `GEOM.VDECODE`,
`GEOM.WCACHE`, `GEOM.LOOM`, `GEOM.WARP`, `MEASURE.HISTOGRAM`, all seven `PART.*`,
`FORGE.PRIM`, `TWOD.PLANE`, `TWOD.SPRITE`, `POST.GATHER`, `POST.COMPOSITE`.

Identical error shape every time:

- **V6** — both declared test paths do not exist;
- **V17** — the `reference_model` is a phantom, *and* the contract names no
  `zref::` symbol under "Scalar reference function".

So each one needs, in order: a reference (or a forward to the real law), the
contract's reference section, RTL, a differential, a random lane, a mutation
sweep. That is the full DEBUG.TRACE treatment, sixteen times.

**One of them is much cheaper than the rest.** `GEOM.PROJECT` cites
`zref::GeomProject`, which is a phantom — but `zref::render::project_vertex` is
real, is what `TERRAIN.PROJECT` already uses, and `TERRAIN.PROJECT` is already
`UNIT_VERIFIED` with working RTL. The ledger even records why the two are
separate: *"Kept separate from GEOM.PROJECT by architect ruling (1.D): merging
later is a trivial edit."* This is a kind-1 phantom with a proven neighbour.

### B. Blocked on the Field IR sequencers — 5 blocks

`FIELD.SEQ.EARTH`, `FIELD.SEQ.FLOW`, `FIELD.SEQ.FORMATION`, `FIELD.SEQ.STAMP`,
and `TERRAIN.PATCH` downstream of them.

All four sequencers report the **same three** V10 blockers:
`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB` — the Field IR's base arithmetic ops, none
of which has a differential test. They are shared, so writing those three unlocks
the op layer for all four at once.

But the op tests are differentials, and a differential needs RTL to differ
against. No `FIELD.SEQ.*` block has any. **So the three op tests are not the
blocker; the sequencer RTL is**, and the op tests come with the first sequencer
that exists.

`TERRAIN.PATCH` is a different case, documented in
`reports/PHANTOM_REFERENCES.md`: its three remaining op blockers are sinks that
belong to `FIELD.SEQ.EARTH`, and it is blocked on that block being built rather
than on anything of its own.

### C. Registered-but-intentionally-late — 1 block

`MEASURE.HISTOGRAM`. The ledger's own note says it: *"Charter §12 calls this
Version 2: registered now, built late."* Its blockers are real but so is the
decision not to build it yet.

## What this means for order

The wave order does not change, but the *cost* per block just became uniform and
much higher, and it is worth saying plainly: from here, "get through the waves"
means writing sixteen blocks, not clearing a backlog.

The cheapest next steps, in order:

1. **`GEOM.PROJECT`** — kind-1 phantom, real oracle, and a working sibling to
   pattern-match against.
2. **`FIELD.PROGCACHE`** — a cache, and the pose cache built today is a close
   structural analogue (tags, residency, counters, delegated storage).
3. **The first `FIELD.SEQ.*` sequencer** — expensive, but it unblocks four
   blocks plus `TERRAIN.PATCH`, and brings the three shared op tests with it.

The `PART.*` family (seven blocks) and the compositor family (four) are each a
coherent chunk that would be better done together than interleaved, since they
share references.
