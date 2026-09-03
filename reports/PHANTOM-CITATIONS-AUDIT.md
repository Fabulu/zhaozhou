# Phantom citations: 13 contracts point at tests that do not exist

**Every one of these will bite the person who builds that block, at the moment
they build it.** Four bit me today, in four consecutive blocks, which is what
prompted counting the rest.

Measured 2026-09-03 by walking every `design/contracts/*.md`, extracting each
backticked `tests/…` path and each `zref::…` symbol, and checking whether the
file exists on disk and whether the symbol appears anywhere under
`reference/`.

## Why it matters, stated once

The ledger already refuses these — `V17`, *"a symbol nobody defined is a
phantom citation"* and *"an existing file that is not about the cited reference
model is an alias, not evidence"*. But **V17 only fires at
`REFERENCE_COMPLETE` and above.** Every block below that maturity can carry a
citation to a file nobody wrote, indefinitely, and nothing complains.

So these are not failures today. They are **failures scheduled for the day
someone builds the block**, and they arrive as a surprise in the middle of
other work — which is exactly how they arrived four times today: `TERRAIN.MIPGEN`
(`zref::terrain::mipgen`), `TERRAIN.RESIDENCY` (a random test that tested the
*prototype*), `GEOM.VDECODE` (`zref::VertexDecode` plus two test files), and
`POST.GATHER` (`zref::PostGather` plus two test files).

## The count

| | |
|---|---|
| contract-cited **test files** that do not exist | **35**, across 13 contracts |
| contract-cited **`zref::` symbols** with no definition | **45**, across ~24 contracts |

## The test files

| contract | a path it cites that is not there |
|---|---|
| `FORGE.PRIM.md` | `tests/geometry/forge_prim_directed.cpp` |
| `GEOM.LOOM.md` | `tests/geometry/geom_loom_directed.cpp` |
| `GEOM.MESHFETCH.md` | `tests/geometry/geom_meshfetch_directed.cpp` |
| `GEOM.VDECODE.md` | `tests/geometry/geom_vdecode_random.cpp` |
| `MEASURE.HISTOGRAM.md` | `tests/measure/measure_histogram_directed.cpp` |
| `PART.COLLIDE.md` | `tests/particles/part_collide_directed.cpp` |
| `PART.LADDER.md` | `tests/formal/part_ladder_stability.sby` |
| `PART.SPAWN.md` | `tests/particles/part_spawn_directed.cpp` |
| `PART.STATE.md` | `tests/particles/part_state_directed.cpp` |
| `PART.UPDATE.md` | `tests/particles/part_update_directed.cpp` |
| `POST.COMPOSITE.md` | `tests/post/post_composite_directed.cpp` |
| `TWOD.PLANE.md` | `tests/twod/twod_plane_directed.cpp` |
| `TWOD.SPRITE.md` | `tests/twod/twod_sprite_directed.cpp` |

Note `tests/post/…` and `tests/twod/…`: **those directories do not exist at
all.** The compositor tests live in `tests/compositor/`. A contract citing a
directory the repository has never had is a strong sign the citation was
written from the block's name rather than from the tree.

## The symbols

`zref::TerrainBake`, `zref::MeasureTokens`, `zref::ParticleState`,
`zref::TwoPlanes` and the rest return **no match anywhere under
`reference/`** — not a differently-namespaced definition, not a renamed one,
nothing. Spot-checked four by hand before writing this down, because a
substring search is a weak instrument and a wrong number here would be its own
kind of phantom.

Full list in the audit output; the recurring ones are
`zref::TerrainBake` (cited by five contracts) and `zref::MeasureTokens` (three).

## What to do about it, and what NOT to do

**Do not bulk-delete the citations.** Most of them name evidence that *should*
exist — the contract is right that `PART.UPDATE` needs a directed test. The
citation is a promise, and the fault is that nothing tracks whether the promise
was kept.

**Do not bulk-create empty files to satisfy the checker.** That converts a
visible phantom into an invisible one, and the ledger would go green.

The honest options, in order of cost:

1. **Extend V17 to fire below `REFERENCE_COMPLETE`** as a warning rather than an
   error, so a phantom is visible from the day it is written rather than from
   the day it blocks someone. This is the cheap one and it is the one that
   changes the pattern.
2. **When a block is built, fix its citations as part of building it** — which
   is what happened four times today, and cost perhaps twenty minutes each
   because the ledger caught it immediately rather than a reader catching it
   later.
3. Write the missing oracles ahead of the blocks. Expensive, and premature for
   blocks whose rulings may still move.

**This report does not fix anything.** It is a count, so the next person to
open one of those thirteen contracts is not surprised by it.
