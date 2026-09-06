# Contract — TERRAIN.COMPCACHE (composed-lattice patch front)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_compcache_front.sv`
> Reference model: `zref::terrain::ComposedLattice` — `reference/include/zref/zref_terrain.hpp`
> Test: `tests/terrain/compcache_front_rtl_directed.cpp`

## Purpose

Hold one patch's composed lattice on chip and serve it to the tessellator.
`TERRAIN.PATCH` pushes one `patch_state` record per lattice vertex;
`TERRAIN.TESS` reads `h`, `wx`, `wz` and cell substance through **registered**
ports at one datum per clock. This block is the join.

**It is the missing middle, named as missing by both neighbours.**
`TERRAIN.PATCH`'s contract says under *Integration capture cases*: "**None
yet** … nothing downstream consumes `patch_state` in RTL — `TERRAIN.TESS`
exists but reads a lattice through its own memory ports … because the
composed-height cache that would sit between them does not exist yet.
Composing the two is the next increment." `TERRAIN.TESS`'s says "Not yet
composed: `TERRAIN.PATCH` upstream (the composed-height cache that would…)".
`reports/Missingterrain` lists it third among the gaps between the terrain
organs and an 8 km world.

## Exclusions

It does not compose (`TERRAIN.PATCH`'s law), does not tessellate, does not
allocate the 256 SDRAM slots, does not load or evict pages, and does not decide
which patches are visible. It is a store with two ports and a swap.

## Why only ONE patch is on chip

The full cache is 256 live patches of composed heights plus velocity:

    256 × 2,178 B heights = 544.5 KiB
    + 544.5 KiB velocity  = 1,089 KiB = 8.92 Mbit

against **5.53 Mbit of M10K on the whole device — 161%.** It cannot be on
chip, and `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §2.5 already puts the
bulk in the terrain hot-cache pool in local SDRAM where `terrain_rules` §8
budgets it.

But SDRAM cannot answer a registered port at one datum per clock, which is
exactly what `TERRAIN.TESS` requires. So the shape is forced: **bulk in SDRAM,
one patch staged on chip, double-buffered** so patch N+1 fills while TESS eats
patch N.

    2 parities × 2 surfaces × 1,089 vertices × 32 b = 139,392 bit ≈ 14 M10K
    2 parities × 1,024 cells × 2 b                  =   4,096 bit ≈  1 M10K

The SDRAM backing attaches later on the **fill** side without changing the
serve ports — which is the point of drawing the boundary here.

## Why it stores 33 + 33 world positions and not 1,089 pairs

TESS asks for `(vi, vj)` and wants back `h`, `wx`, `wz`, which reads like a
world position per vertex: 1,089 × 64 b per parity, about 14 M10K on top of the
heights, doubling the block.

**It is separable, and that is a law rather than an inference.**
`zref::terrain::ComposedLattice` declares `wx` per lattice *column* and `wz`
per lattice *row*, and states the precondition outright:

> The lattice must be axis-aligned monotone (identity/axis placement —
> island-datum space, the space §4.3 is written in).

The existing composed test reads exactly that way — `lat_.wx[lat_vi_]`,
`lat_.wz[lat_vj_]` (`tests/terrain/terrain_lod_tess.cpp`). Storing a pair per
vertex would be storing 1,089 copies of 66 numbers.

**Rotated sheets are on the owner's feature list and would break this** — but
not silently. The placement space is axis-aligned by the reference's own stated
precondition, so a rotated sheet is a change §4.3 must make *first*. When it
does, the fix here is a 2×2 basis and four multiplies at one datum per clock,
which is affordable; it is not a redesign.

## `patch_state` carries no vertex index. The order IS the index.

The output port is `{top, bottom, compose_top, dirty, src_id}` — no `(vi, vj)`.
A record's identity is its position in the stream, matched against the order
the vertices were submitted. That is sound: the compose lane is a single
in-order lane, "1 cycle per vertex with no live field, and 1 + n cycles with n
accepted field lanes", which cannot reorder.

Sound is not checked, so it is checked three ways: the write cursor counts,
`a_fill_no_overrun` refuses a 1,090th record rather than wrapping onto vertex
zero, and the differential feeds the **vertex index through `src_id`** and
requires it to come back matching — pinning the positional contract to a value
rather than to a count.

## One record is TWO write clocks, and ready is low on the second

A record carries both surfaces of one vertex and the store has a single write
port, so top and bottom go in on consecutive clocks. `st_ready_o` is therefore
asserted only on phase 0, and the bottom is written on phase 1 from the value
captured at acceptance.

**The first draft held ready up for both phases.** Under ready/valid that means
the producer advances, so the *next* record's top would have been written into
this vertex's bottom plane. Every height would still have been a real composed
height and every count would have matched; the underside would simply have been
the neighbouring vertex's top surface. A lattice wrong by one vertex is a
terrain that renders — which is why this is written down rather than fixed
quietly.

## Poison, not zero

A request outside the grid, or a read with no patch served, returns
`0x5BADF00D` on `h`, `wx` and `wz`. Zero is a legal height *and* a legal world
coordinate, so returning zero lets a consumer that reads without asking, or
asks off the edge, produce a plausible flat triangle. `0x5BADF00D` is the value
`tests/terrain/terrain_lod_tess.cpp` already drives for exactly this case, so a
consumer that trips it sees a value it has been able to recognise since before
this block existed.

Substance has no spare encoding in two bits and inventing one would change
TESS's port, so it returns 3 (§3.3 gives 0 = SOLID, so 3 is not the dangerous
default) and **the counter is the alarm**, not the value.

## Counters

`fill_records` (records into the current fill), `patches_filled`,
`patches_served`, `fill_overrun` (records past 1,089 — refused, never wrapped),
`lat_oob`, `cs_oob`.

`patches_served` is incremented in its own statement rather than in the
else-arm of the handover. A release landing on the same clock as a handover
takes the first branch, and folding the count in there would lose exactly the
patch retired at the busiest moment — the steady state, where a fill finishes
as a patch is released, every patch. The counter would have under-reported
precisely when the pipeline was working.

## Scalar reference function

`zref::terrain::ComposedLattice` (`reference/include/zref/zref_terrain.hpp`),
with `substance()` as the cell-plane oracle.

**The oracle is the STORE's contents and addressing, not the composition.**
What can go silently wrong here is the mapping — a transposed `vj*33 + vi`, a
surface plane swapped, a parity served while it is still being filled — and
every one of those returns real composed heights from the wrong place. There is
no picture in which a lattice wrong by one vertex is visible; it renders as
terrain. So the differential compares every vertex of every surface, and the
cell plane, against the reference structure rather than sampling.

## What is not yet established

Not fitted; not composed with the real `TERRAIN.PATCH` or `TERRAIN.TESS` (that
composition is the next step and is the whole reason this block exists); the
SDRAM backing store on the fill side is not built, so today a harness plays the
part of the 256-slot cache. The overflow law for a 257th patch in one frame is
still `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` OPEN question 6 and is not
this block's to decide — the front holds one patch and has no opinion about how
many the frame wanted.
