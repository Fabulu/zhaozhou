# Polygon budget — rearchitect the triangle front end

**Status: PROPOSAL, not scheduled.** Explicitly sequenced **after**
`DEBUG.FRAMEBLIT` steps 4–8 and the composed Quartus result. Recorded now so
the reasoning survives, and deliberately not started, because most of its
numbers only mean something once a real composed fit exists.

Origin: owner, 2026-08-18 — "we want better polygon budget".

**Everything below is paper analysis and simulation. Nothing here has been
measured on a board.**

---

## The workload is unusual, and that is the whole argument

- 240p means the screen holds only **92,160 pixels** in Z60.
- A 5,000-triangle creature filling 150×150 pixels averages about **4.5 visible
  pixels per authored triangle**.

So high-detail creatures are predominantly a **microtriangle problem**: many
triangles, comparatively few fragments. The current design wastes cycles *per
triangle* regardless of how tiny that triangle is, and that is the part worth
changing.

## Where the throughput goes today

**The edge walker**, per triangle × tile job:

| Stage | Clocks |
| --- | --- |
| setup | 5 |
| row walk | **exactly 16**, always |
| row drain | 0–16 |

**21–37 clocks even when the triangle covers one or two pixels**, and it accepts
only one job at a time.

**The binner** is still a bring-up implementation:

- only **128** stored triangles;
- only **1,024** triangle–tile references;
- **2.83 clocks per emitted reference**, against an intended one per clock.

**The TMU** sustains:

- one direct-colour sample per **4** clocks;
- one CLUT sample per **6** clocks;

against an intended one sample per clock. Until that is pipelined, extra polygon
throughput just runs into a texture bottleneck.

There is a lot of performance available here **without a much larger GPU**.

## The four changes, in the order they pay

### 1. Walk only the rows a triangle can actually touch

The binner already knows the scissored bounding box. Pass the tile-local first
and last rows into the edge walker.

    current:   5 setup + 16 rows + drain
    improved:  small fixed start + bounding-box height + drain

A triangle spanning two pixel rows should cost roughly two walk clocks, not
sixteen. For close-up creatures, where triangles are one to four pixels tall,
**this is probably the single largest gain**.

Trade-offs: changes the binner→edge-walker packet; needs new exact-coverage
tests around tile and row boundaries; adds a little control logic; does **not**
meaningfully increase DSP use; **preserves the existing top-left fill rule
exactly**.

It gives most of the benefit people seek from 8×8 tiles without quadrupling the
tile count and tile-list entries.

### 2. Stop recomputing triangle setup for every tile

`GEOM.SETUP` already calculates the edge coefficients and the binner receives
them. Preserve the setup descriptor rather than recomputing it per tile — it is
the same triangle and the setup is invariant.

### 3. Pipeline the binner, and enlarge the arenas

To one reference per clock, with arenas of **8k–16k triangles** and
**16k–32k references**. The arena size is not optional at the target: at
12k–20k post-cull triangles a 128-triangle / 1,024-reference arena spills, and a
spill is a frame-rate cliff rather than a gradual cost.

### 4. Overlap edge walking with row draining, and pipeline the TMU

To one sample per clock.

## What NOT to do

- **Do not duplicate the raster pipeline.** A second pipe needs **171 DSPs**;
  the device has **112**. It does not fit, and no scheduling makes it fit. This
  is the most important line in the document.
- **Do not switch to 8×8 tiles.** Change 1 buys most of that benefit without
  quadrupling the tile count.
- **Do not build a separate microtriangle rasterizer yet.** It is a real
  technique and it is premature: the four changes have to land and be measured
  first, or there is no evidence about what it would be relieving.
- **Do not chase 50,000 triangles at 240p.** Much of that scene would be deeply
  subpixel, and the extra fabric would produce less visual value than polygon
  particles, terrain deformation, persistent scars, better shadows, spell
  geometry, or stable 60 Hz headroom.

## The target

**12,000–20,000 post-cull triangles per frame**, with **meshlet and backface
culling enabled** — the target is *post-cull*, and culling is the cheapest
triangle in the budget.

Expected return: plausibly **two to four times** the useful creature-triangle
throughput, with controlled resource growth.

## What it means for creature assets

Authoring limits are unchanged by the redesign:

| Class | Authored highest tier |
| --- | --- |
| Ordinary creature | 3,000–4,096 |
| Large or elaborate creature | 4,000–6,000 |
| Giant / boss / showcase | 6,000–8,192 |
| Exceptional cinematic model | up to 10,000, only after benchmarks |

**Every creature can contain an excellent highest tier. The Measure decides
whether that tier is currently affordable.**

A normal battle:

| | submitted |
| --- | ---: |
| one selected close creature | 4,000 |
| two nearby important creatures | 1,500 each |
| ten ordinary units | 300 each |
| forty distant units | 40 each |
| terrain, spells, particles | the remaining budget |

That is roughly **11,600 creature triangles while displaying fifty-three
creatures**.

## The order of work

Sequenced by the owner, and the order is the point:

1. Finish `DEBUG.FRAMEBLIT` steps 4–8 and get the **composed Quartus result**.
2. Reduce the existing **171-DSP** estimate below the device ceiling, with
   headroom.
3. Build one canonical high-poly benchmark: an 8,192-triangle boss source
   model, a 4,096-triangle ordinary close creature, a hundred aggressively
   LODded distant creatures, and representative terrain, textures and spell
   effects.
4. Implement active-row edge walking.
5. Preserve setup descriptors instead of recomputing per tile.
6. Pipeline and enlarge the binner.
7. Pipeline the TMU.
8. **Measure again.**
9. Add a second tile/coverage context **only if** counters still show coverage
   setup as the binding stage.

## Why it waits

The 171-vs-112 DSP arithmetic is the reason. Every item above consumes fabric,
and the only trustworthy statement about how much fabric remains comes from a
**composed** fit — the whole design placed together, not blocks fitted in
isolation. Starting against block-level estimates would mean rebuilding once the
composed number arrives.

`DEBUG.FRAMEBLIT` steps 4–8 come first because they are what makes a composed
fit meaningful.

> Rearchitect for microtriangles, yes. Build two complete GPUs, no.
