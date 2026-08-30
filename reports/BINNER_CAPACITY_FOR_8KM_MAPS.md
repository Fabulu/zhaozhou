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

## MEASURED, 2026-08-30 — `tools/render/count_bin_load.cpp`

The estimates above are now measurements. GEOM.BINNER's binning law IS
`zref::Binner`, so counting (triangle, tile) pairs through the shipped oracle
counts exactly what the hardware would have to store. Z60 384x240, 24x15 tiles:

| scene | triangles | vs 128 | references | vs 1,024 | deepest tile |
|---|---:|---:|---:|---:|---:|
| sky backdrop, 2 triangles | 2 | 0.0x | **396** | 0.4x | 2 |
| one terrain patch, 32x32 cells | 2,048 | **16.0x** | 4,080 | 4.0x | 12 |
| creature army, 200 x 96 tris | 19,200 | **150.0x** | 23,912 | **23.4x** | **341** |
| giant near camera, 126 tris | 126 | **1.0x** | **25,704** | **25.1x** | 126 |

There is no camera, visibility or LOD in the tool, so each row is an UPPER bound
for the geometry it describes and a LOWER bound for a real frame, which carries
several of these at once.

### The two limits fail INDEPENDENTLY, which the estimate did not show

* **The giant fits in TRI_CAP and destroys the arena.** 126 triangles is 1.0x
  the triangle budget -- it would be accepted -- and they generate 25,704
  references, 25x the arena. Sizing TRI_CAP alone would not have caught it.
* **The army destroys TRI_CAP and is comparatively easy on references.** 19,200
  triangles is 150x, but its references are 23x -- *less* than the giant's, from
  152x more triangles, because an army is many SMALL things.
* **Two triangles can eat 40% of the arena.** The sky backdrop needs 396
  references for 2 triangles.

So there is no single number to raise. **A triangle budget and a reference
budget are different resources with different worst cases**, and any capacity
decision has to state both and name which scene each is sized for -- exactly
the shape the Field engine ended up with, where the wall was whichever resource
refused rather than the one that looked busy.

### And the deepest tile list is 341

A chunked list at `CHUNK_REFS = 4` walks 86 chunks to drain that one tile, one
pointer chase each. That is a latency the drain pays per tile, and it is not
visible in either total. `max_tile_list_depth_o` already reports it and nothing
reads it.

## What to measure before choosing

The instrumentation already exists and is not being used: `tile_references_o`,
`max_tile_list_depth_o`, `arena_used_o`, `triangles_culled_o` and `overflow_o`.
Ruling 4 asks for real scenes, and the scenes that matter are the ones the
reviewer already named for the TMU traces — terrain+sky, maximum stars, creature
army, giant near camera, beams/storm, Duo, adversarial thrash.

**Run those through `zref` first and count triangles and (triangle, tile) pairs
per frame in software.** That costs nothing, needs no RTL, and turns this file's
estimates into measurements. Only then is a capacity frontier worth fitting.

## THE DECIDING ARITHMETIC: a frame-resident triangle arena cannot be built

`TRI_ENT_W = 16 + 6*21 = 142 bits` a triangle, and the device has **553 M10Ks =
5.53 Mbit**. So:

| triangles held | arena bits | M10K | share of the device |
|---:|---:|---:|---:|
| 128 (today) | 18 Kbit | ~2 | 0.3% |
| 2,048 (one terrain patch) | 291 Kbit | ~29 | 5% |
| 19,200 (the measured army) | 2.73 Mbit | ~273 | **49%** |
| 25,000 | 3.55 Mbit | ~355 | **64%** |

**References are cheap by comparison.** A reference is a triangle index plus the
chunk's `next` pointer, so 25,000 references in 4-reference chunks is about
6,250 x (4x15 + 13) = 456 Kbit, roughly **46 M10K — 8%**. That asymmetry is the
whole finding: the arena's expensive half is the one holding VERTICES.

### Which kills the obvious fixes, in order

* **Raising TRI_CAP does not work.** Holding one measured army costs half the
  device's memory, and that is before the terrain it is standing on, the sky,
  the effects, and every other RAM in the console.
* **A patch/strip primitive for terrain does not rescue it either.** Collapsing
  2,048 terrain triangles into one record is a large, real saving — and the
  army is still 19,200 records of creature geometry with no grid structure to
  exploit. Terrain is not what breaks this.
* **TriangleContext makes it strictly worse.** Storing edge coefficients and
  attribute planes per triangle widens the record that is already the problem.

### So the arena must stop being frame-resident

The conclusion the numbers force is that a bounded arena cannot hold a frame,
and therefore **the overflow wall should become a FLUSH, not a cull**:

    bin until the arena is full
      -> drain the tiles that have references
      -> reset the arena
      -> continue binning the SAME frame

Nothing is dropped; the frame simply takes more passes over the tile grid, and
each pass pays the tiles it touches. That is the standard sort-middle answer to
bounded binning memory, and this block is already most of the way there: it has
a chunked arena, a safe wall that abandons cleanly, and a drain that walks tiles
in order. What it lacks is the ability to drain and then KEEP GOING.

Two things that change and must be designed rather than assumed:

* **A tile may now be visited more than once per frame**, so RASTER.TILE_PIPE's
  brand-new one-clear-one-resolve-per-tile lifecycle needs a third state: the
  SECOND pass over a tile must NOT clear it, and must resolve into what the
  first pass left. That is exactly the `job_first_i`/`job_last_i` pair already
  built, driven across passes instead of within one.
* **Submission order still has to hold.** Terrain's painter order is
  semantically observable, so a flush boundary must not reorder two triangles
  that land in the same tile.

**This is an owner-level decision and the numbers are the input to it, not a
substitute for it.** What is no longer open is whether a bigger constant would
do: it would not, and the reason is 49% of the device for one army.

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
