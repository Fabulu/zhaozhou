# Polygon budget — raster path rearchitecture proposal

**Status: PROPOSAL, not scheduled.** Explicitly sequenced **after** the
`DEBUG.FRAMEBLIT` integration steps 4-8 and the composed Quartus result. It is
recorded now so the reasoning is not lost, and deliberately not started, because
several of its numbers only mean something once a real composed fit exists.

Origin: owner, 2026-08-18 — "we want better polygon budget".

---

## The target

**12,000 to 20,000 post-cull triangles per frame.** That is the number the rest
of this document is trying to reach.

## What to change

Four changes, in the order they pay:

1. **Walk only the rows a triangle actually touches.** The current walk visits
   rows the triangle cannot cover. For thin and diagonal triangles — which is
   most of them in a terrain mesh — this is the dominant waste.

2. **Stop recomputing setup per tile.** Edge setup is currently redone for each
   tile a triangle lands in. It is the same triangle; the setup is invariant.
   Compute once, carry it.

3. **Pipeline the binner to one reference per clock, and enlarge the arenas.**
   Arenas go to **8k-16k triangles** and **16k-32k references**. The arena size
   is not optional at the target: at 12k-20k post-cull triangles a 4k arena
   spills, and a spill is a frame-rate cliff rather than a gradual cost.

4. **Overlap edge walking with row draining**, and **pipeline the TMU to one
   sample per clock.**

## What NOT to do

These are refusals, and each has a reason that is worth keeping:

- **Do not duplicate the raster pipeline.** A second pipe needs **171 DSPs**;
  the device has **112**. It does not fit, and no amount of scheduling makes it
  fit. This is the single most important line in the document.
- **Do not switch to 8x8 tiles.** The tile size is load-bearing elsewhere and
  the win does not justify disturbing it.
- **Do not build a separate microtriangle rasterizer yet.** It is a real
  technique and it is premature: the four changes above have to land and be
  measured first, or there is no evidence about what the microtriangle path
  would actually be relieving.

## What must happen upstream

**Enable meshlet and backface culling.** The target is *post-cull*, and without
culling the raster path is being asked to absorb work that should never have
reached it. Culling is the cheapest triangle in the budget.

---

## Why this is sequenced behind FRAMEBLIT and the composed fit

The DSP arithmetic above (171 vs 112) is the reason. Every item in the "what to
change" list consumes fabric, and the only trustworthy statement about how much
fabric is left comes from a **composed** fit — the whole design placed together,
not a block fitted in isolation. Starting this work against block-level
estimates would mean rebuilding it once the composed number arrives.

`DEBUG.FRAMEBLIT` steps 4-8 are ahead of it because they are what makes a
composed fit meaningful in the first place.

**Everything above is simulation and paper analysis. Nothing in this document
has been measured on a board.**
