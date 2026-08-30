# GEOM.BINNER's capacity is two orders of magnitude short of the game

Written 2026-08-30, following ruling 4 of `reports/RENDERER_ARCHITECTURE.md`
("preserve its safe overflow wall, make capacities build parameters, instrument
real scenes, then fit two meaningful capacity points rather than guessing") and
Fabian's emphasis that the maps are 8 km with hundreds of textured creatures and
objects.

**No RTL changed for this file.** It is arithmetic against the shipped
parameters, written down because the numbers are not close and nobody should
discover that during an integration.

---

## What is there today

    parameter TRI_CAP    = 128     triangles per frame
    parameter CHUNKS     = 256     chunks in the reference arena
    parameter CHUNK_REFS = 4       references per chunk
    -> 1,024 tile references per frame
    parameter GRID_W/H   = 24/24   576 tiles (Z60 384x240 needs 24 x 15 = 360)

They are already build parameters, which is half of what ruling 4 asks for. The
overflow wall is already safe and already instrumented: the current triangle is
abandoned, `overflow_o` LATCHES, and `triangles_culled_o` counts — the block's
header spends a page on exactly this. So nothing here is a defect. The numbers
are simply sized for proving the laws, not for the game.

## What one frame actually needs

**Terrain alone exceeds TRI_CAP by an order of magnitude.** A terrain patch is
32x32 cells over a 33x33 lattice, and a cell is two triangles:

    one patch            32 x 32 x 2  =  2,048 triangles
    TRI_CAP                              128
    -> a SINGLE patch is 16x the whole frame budget

and a frame draws many patches. Add the sky drum, the cloud sheet, the sun quad,
star quads, and then the thing the maps exist for:

    hundreds of creatures x one meshlet each (charter 15: up to 96-126
    triangles) is TENS OF THOUSANDS of triangles before objects.

**References are worse than triangles.** A reference is one (triangle, tile)
pair, so a triangle spanning N tiles costs N. Terrain triangles near the camera
are large and cross several tiles each; the sky backdrop covers the whole grid.
A full-canvas quad alone is 2 triangles and up to 360 references at Z60 — a
third of the entire arena for two triangles.

    CHUNKS x CHUNK_REFS = 1,024 references
    a full-screen backdrop = up to 720 of them

## What that means, stated plainly

* **These are not "tune it later" numbers.** They are ~2 orders of magnitude
  short for triangles and comparable for references. The gap is not going to be
  closed by a sweep of 128 vs 256.
* **The overflow behaviour is correct and will fire constantly.** Latching
  `overflow_o` and culling the excess triangle is the right response to running
  out; it is not a rendering strategy. A frame that drops most of its geometry
  is not a frame.
* **So this is an architecture question, not a capacity sweep.** Roughly:
  * how many triangles can the frame arena hold in M10K, at what depth of
    reference chunk;
  * whether terrain is submitted as triangles at all, or whether the binner
    learns a patch/strip primitive so 2,048 triangles become one record;
  * whether the drain becomes multi-pass over tile bands, so the arena holds a
    BAND's references rather than a frame's;
  * how MEASURE.TOKENS' budget interacts — it is the block that decides what
    does not get drawn, and it should be deciding that, not the arena's
    high-water mark.

## What to measure before choosing

The instrumentation already exists and is not being used: `tile_references_o`,
`max_tile_list_depth_o`, `arena_used_o`, `triangles_culled_o` and `overflow_o`.
Ruling 4 asks for real scenes, and the scenes that matter are the ones the
reviewer already named for the TMU traces — terrain+sky, maximum stars, creature
army, giant near camera, beams/storm, Duo, adversarial thrash.

**Run those through `zref` first and count triangles and (triangle, tile) pairs
per frame in software.** That costs nothing, needs no RTL, and turns this file's
estimates into measurements. Only then is a capacity frontier worth fitting.

## What this does NOT block

The render path works and is tested at the sizes it is built for:
`render_pipe_directed` and `render_fb_directed` drive 1 and 2 triangles over a
8x6 grid, and the shell draws through the real guard and arbiter. Nothing above
invalidates any of that. It says the arena is sized for a test and not for a
game, and that the fix is a design decision rather than a bigger constant.

---

## Appendix: TriangleContext is an ENABLER, not a speed-up

Ruling 4 also proposes storing a full TriangleContext so `RASTER.EDGEWALK` stops
recomputing edge setup for every tile reference. Measuring what that would
actually save, from the block's own header:

    5 setup cycles + 16 walk cycles + 0..16 drain beats = 21..37 per tile job

The 5 setup cycles are one shared 23x23 cross-product unit issuing the AREA and
then the three edge values at the tile corner. Of those:

* **the area is genuinely tile-independent** and is recomputed per reference;
* **the three edge values are NOT.** They are the edge functions evaluated at
  THIS tile's corner. Handing EDGEWALK prepared `kx/ky/kc` replaces three cross
  products with `kc + kx*px + ky*py` — six multiplies against three, on a shared
  multiplier. That is not obviously fewer cycles.

So the honest saving is **about one setup cycle in five**, i.e. 3-5% of a tile
job — not the "a 30-tile triangle pays setup 30 times" saving the ruling implies.

**Its real value is that it gives per-triangle ATTRIBUTE state somewhere to
live**, which is the actual prerequisite for the attribute-bearing geometry seam
(ruling 5) and therefore for anything textured. It should be built for that
reason and costed as that, not sold as a throughput win.
