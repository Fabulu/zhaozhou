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

---

## Addendum: the Field IR sequencers are constrained, not merely unbuilt

`spec/form/field-ir.md` §1 carries a grep-audit law (charter §29-6) that changes
what "build FIELD.SEQ.*" is allowed to mean:

> Field IR *op semantics* exist in exactly two places — the C++ generic
> interpreter (`zfield::interpret`) and the TS interpreter … There is and shall
> be no third implementation: no hand-written per-program evaluator, no "faster"
> fused C++ variant, **no RTL-side re-derivation ahead of the profile engine
> (which will consume the same serialized bytes)**. A reviewer greps for the
> op-name switch outside those two files and must find none.

Read carefully, this is a design constraint rather than a prohibition. RTL is
foreseen — the parenthesis names "the profile engine" and says it consumes the
same serialized bytes. What is forbidden is a sequencer that re-derives op
semantics: a per-program hardwired evaluator, or an RTL opcode switch written
from the spec by hand.

So the five sequencer blocks are not just expensive, they have a required shape:
**a byte-code engine that executes `.zprog` images**, differentially verified
against `zfield::interpret` on the committed `.zvec` corpus. Anything that reads
like a second implementation of the op table will fail the grep audit by
construction, however well it tests.

Two consequences worth planning around:

1. The three shared op blockers (`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB`) come with
   that engine and are differentials against the same interpreter — they are not
   separate work.
2. The engine is one block's worth of effort that unblocks five. That makes it
   better value than its size suggests, and it is the reason the "first
   `FIELD.SEQ.*` sequencer" sits third on the cheapest-next list above rather
   than last.

`FIELD.PROGCACHE` is clear of this constraint: it caches and validates programs
and never evaluates one. Its validation half is already law —
`zfield::decode` with thirteen named error classes — so only its cache policy
needs deciding.

---

## Addendum 2: the cheap blocks are gone, and what that leaves

Since the map above was written, five greenfield blocks were built and verified —
`GEOM.PROJECT`, `FIELD.PROGCACHE`, `PART.EXPAND`, `PART.SOFT`, plus both halves of
`GEOM.POSE`. Four of the five were **kind-1** phantoms: the law already existed
under another name and only had to be found, cited and pinned.

**A systematic scan says there are no more of those.** Every remaining
`reference_model` was checked against the reference tree for a law shipped under a
different name. The results:

| Block | Is the law already shipped? |
| --- | --- |
| `GEOM.VDECODE` | **No.** Meshlets hold plain `SkinVertex` — there is no compressed form anywhere, and the ledger says the format belongs to `SW.TOOLS.ASSET`: *"one spec, two ends"*. |
| `POST.GATHER`, `POST.COMPOSITE` | **No.** `zref_aux.hpp` says of the distortion map that *"the offset arithmetic belongs to whoever"* — it is explicitly unassigned. Bloom, flash and grading exist only in the star/sky path, which is a different block's law. |
| `TWOD.SPRITE`, `TWOD.PLANE` | **No.** `blit_pattern_8x8` is a form-marker blit, not a HUD sprite pipeline with descriptors, affine and CLUT paths. |
| `GEOM.LOOM`, `GEOM.WARP` | **No.** Transform-graph evaluation and Warp8 deformation are unimplemented in software as well. |
| `PART.SPAWN/STATE/UPDATE/COLLIDE` | **No.** `zref::render::Particle` is a draw-time snapshot the renderer is HANDED. Nothing simulates particles. |
| `PART.LADDER` | Partly. The seven rungs are charter §9 and the counter lanes are `zref::measure`, but the ledger says the thresholds are *"provisional until Phase-10 evidence"* — the numbers are explicitly not ratified. |

## So the remaining 37 split three ways, and only one is mine to do alone

**1. Needs a spec another block owns.** `GEOM.VDECODE` is the clear case: the
vertex compression format has two ends and the pack side is `SW.TOOLS.ASSET`'s.
Inventing one end unilaterally would create exactly the kind of unratified law
this project keeps catching. `PART.LADDER`'s thresholds are the same shape —
recorded as provisional pending evidence that does not exist yet.

**2. Needs the Field IR engine.** Five blocks, one engine, required shape already
documented in addendum 1. This is large but it is unambiguous work: a byte-code
engine over `.zprog`, differentially verified against `zfield::interpret` on the
committed `.zvec` corpus. **It is the single highest-value remaining item** and
nothing about it needs a decision from anyone.

**3. Needs behaviour decided.** The four particle-simulation blocks, the two
compositor blocks and the two 2D blocks have no law in software, no ratified spec
section, and no donor behaviour to extract. Each one means choosing how the game
behaves — how a particle spawns, ages and collides; what bloom looks like — and
then writing that choice down as the reference before any RTL. That is design
work, and the choices belong to the person whose game it is.

## The honest statement of scope

"Finish the full hardware" is not one more sitting's work. Group 2 is the next
substantial thing I can do without input. Group 3 is roughly a dozen blocks whose
*behaviour* has never been decided, and doing them well means deciding it
deliberately rather than having me invent it and record the invention as law.
