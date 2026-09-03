# Contract — FORGE.SHADOW (Contact shadows, as ordinary geometry)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::forge::shadow_*` — PLANNED AND NOT WRITTEN

## Purpose and exclusions

FORGE.SHADOW emits a small terrain-conforming transparent hull under a creature
or object, so it looks **attached to the ground**.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R8.** Full shadow
maps are deliberately absent from this console and that is defensible. What was
missing is any settled replacement for the most basic visual job there is — and
for a game of **floating islands and airborne creatures**, "is that thing
touching the ground or hovering above it" is not a refinement, it is legibility.

The audit's estimate is the reason this is worth doing now: **very small if
done as geometry**, and *"can make more perceptual difference than several
expensive material effects."*

**Exclusions, and they are the whole design:**

* **No shadow map.** No depth pass from the light, no shadow buffer, no second
  view.
* **No new framebuffer and no new raster hardware.** The output is ordinary
  transparent geometry through the main renderer.
* **No shadow unit.** This is a primitive generator, a sibling of
  `FORGE.PRIM`, not a lighting stage.
* **No self-shadowing, no shadows cast onto other creatures, no shadows from
  terrain onto terrain.** Contact only.
* **No occlusion query.** Whether the ground is really below is answered by a
  few height taps, not by visibility.

## The frozen ladder

| range | shadow |
|---|---|
| **near hero** | projected low-poly hull, or an 8–16 vertex ellipse, conformed from a few terrain height taps |
| **near army** | 4–8 vertex blob |
| **mid** | tiny dark splat |
| **far** | none |

The ladder is a **coarseness floor selected by the governor**, exactly as
`PART.LADDER` treats particle representation — the same idea and the same
refusal to let a distant creature spend near-hero geometry.

## Input and output packet layouts

**In**, per shadow caster: `{ world_x, world_z, radius, strength, rung,
src_id }` — where `rung` selects the ladder step and `strength` is a unit8 the
content author owns.

**Terrain height taps in**: the block asks for a small fixed number of heights
around the caster and conforms the hull to them. **Fixed count per rung**, so
the cost is bounded and knowable rather than dependent on terrain roughness.

**Out**: a vertex stream to `GEOM.SETUP`, in a **declared deterministic order**
— the same rule `FORGE.PRIM` obeys, and for the same reason: two orderings
produce the same picture and different capture CRCs.

## Backpressure rules

Ready/valid. It is a background producer: a stalled shadow must never delay a
creature. Under pressure the governor lowers the rung, which is a coarser
shadow rather than a missing one — **a shadow that vanishes is worse than a
crude one**, because the creature appears to take off.

## Memory ownership

None. It reads terrain heights through the ordinary terrain path.

## Q formats and rounding

Positions fx16 world units, as everything else in the geometry path.
`strength` is unit8 (value = raw/256, so 255 is the largest representable and
not 1.0).

## Latency (fixed or variable)

`variable` — a height tap takes the terrain path's latency.

## Overflow and malformed-input behaviour

* **A caster with no ground beneath it** — over a void cell, off the island
  edge, or above the keel — emits **no shadow**, and that is correct rather
  than a fault. An airborne creature over a chasm should not have a shadow
  pasted at some default height.
* **A radius of zero** emits nothing.
* **Height taps that disagree wildly** (a cliff edge under the caster) still
  conform: the hull follows them. It may look odd on a knife-edge, and that is
  a content problem, not a hardware refusal.

## THE ONE THING THAT MUST NOT BE GOT WRONG

**Depth bias, and it must be authored rather than accidental.** CLAUDE.md's
ground-contact law applies directly: clipping through the ground must be
authored, never accidental, and *a belly resting at exactly zero reads as
hovering*. A shadow at exactly the terrain height z-fights with it; a shadow
biased too far reads as floating detached from its caster.

So the bias is a **named, editable constant per rung**, and its correctness is
decided **by looking** — a shadow that measures right and looks detached is
wrong.

## Scalar reference function

**PLANNED AND NOT WRITTEN**: `zref::forge::shadow_hull(rung, x, z, radius,
heights[])` returning the vertex list in emission order, and
`zref::forge::shadow_rung(distance, governor_floor)`.

## Directed tests

**PLANNED AND NOT WRITTEN**: each rung's exact vertex and triangle count; the
emission order deterministic and identical across two runs and under stalls; a
caster over a void emitting nothing; the governor floor overriding a near rung;
and the height-tap count fixed per rung regardless of terrain.

## Randomized differential tests

Planned, against the scalar model, over caster positions including island edges
and breach holes.

## Integration capture cases

None on hardware. **And a look-gate, because this is art**: a creature walking
across flat ground, a slope, a cliff edge and a breach, at 240p, watched in
motion. A contact shadow either sells the contact or it does not, and no
measurement decides that.

## Synthesis / resource ceiling

Expected **very small**: a vertex generator with a fixed table per rung, plus
the tap request logic. It has no arithmetic beyond placing vertices. The audit's
own assessment: *"may cost essentially zero new raster hardware."*

## Notes

The material architecture already mentions precise shadows as terrain-conforming
polygons; this contract is the **automatic, per-creature** case that had no
rule. The two are compatible: an authored precise shadow is content, this is
the default every creature gets for free.
