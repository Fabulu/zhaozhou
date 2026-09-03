# Contract — MATERIAL.LIQUID (Water and lava, as ordinary geometry)

> Ledger: material law, not a block — no RTL, no ledger row of its own
> Ruled: **D-7, 2026-09-03**
> (`reports/OWNER-RULINGS-20260903-FUNDAMENTALS.md`)

## What this is, and why it is not a block

**Liquids exist. They need no hardware.**

They are **ordinary triangulated world surfaces through the main renderer**, and
**never** `TWOD.PLANE` — that engine explicitly refuses arbitrary depth-tested
world planes, and the audit's R10 finding was that the stale ledger phrase
*"water/lava"* was letting people assume a solution existed behind an engine
that declines the relevant depth behaviour.

So this file is a **material law**, not a contract for a machine. It exists so
that "liquid" stops being one ambiguous blend mode.

## Water v1

    low-poly triangulated surface
      + optional vertex-wave displacement
      + one or two scrolling bounded texture samples
      + alpha blend
      + depth test ON
      + depth write OFF
      + deterministic coarse back-to-front ordering

**Rules, each one a real constraint rather than advice:**

* **break large water into bounded patches** — an unbounded sheet cannot be
  sorted or budgeted;
* **sort at patch/surface granularity, not per triangle.** Per-triangle sorting
  is the expensive answer to a problem patch sorting already solves;
* **avoid mutually intersecting transparent water sheets in authored v1
  content.** This is a content rule, and stating it is cheaper than building
  order-independent transparency, which is on the refused list;
* **no reflection, no refraction, no scene-colour copy, no screen-space
  distortion.** Each of those needs a framebuffer read this console does not
  do;
* **shoreline foam** may use geometry, AUX surface marks, or an additive
  overlay — three legal answers, all of them ordinary.

## Lava v1

Lava is **not water with a different texture**, and conflating them is the
specific error this file prevents.

The base is **opaque or near-opaque**:

    depth test ON
    depth write ON
    scrolling base/detail material

with an **optional emissive/glow overlay rendered afterward**:

    depth test ON
    depth write OFF
    additive blend
    glow tag

**Depth write is the difference.** Water writes no depth because things are
visible through it; lava writes depth because it is a surface. Getting that
backwards gives water that occludes what is beneath it, or lava you can see
into.

## Samples and waves

One or two samples suffice: **water** can use a lerped or modulated pair,
**lava** a base plus additive/modulated detail. Both are inside the ruled
zero-to-three sample budget and need no new recipe.

**Vertex waves are generated before projection** and therefore need **no
liquid-specific raster hardware** — they are ordinary vertex displacement, and
the existing geometry path already carries them.

## What the material record must say

The `MATERIAL_SET` entry (`spec/cartridge.md` §4a, kind 11) must **explicitly
identify water transparency versus lava's opaque/emissive base** through its
existing fields: blend mode, depth-write flag, and glow participation.

> **"Liquid" is not one ambiguous blend mode.**

## Cost

**No new hardware.** Two blend modes that exist, a depth-write flag that
exists, a glow tag that exists, and vertex displacement the geometry path
already does. The cost is content: patches, ordering and the authoring rule
about intersecting sheets.

## If the game does not want liquids

Then this file is deleted and the phrase is removed from the ledger. The ruling
chose yes — but the reason the question mattered was never the cost. It was that
a stale phrase in a ledger is indistinguishable from a plan.
