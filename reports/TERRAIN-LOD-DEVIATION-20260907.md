# The LOD deviations — the last block between a page and a selected mesh

*2026-09-07. Written after composing PAGESTREAM → PATCH → COMPCACHE → TESS →
NORMALS and finding the chain stops one block short of TERRAIN.LOD. Every line
number below was read from the tree.*

---

## The gap, stated exactly

`TERRAIN.LOD` **does not read the lattice.** Its subpatch descriptor arrives
with the deviations already computed:

```
zhao_terrain_lod.sv:197-199
    input logic [23:0] sp_dev1_i,      // |fine − coarse| at level 1, fx16
    input logic [23:0] sp_dev2_i,
    input logic [23:0] sp_dev3_i,
```

and `zref::terrain::LodSubpatch` defines them
(`reference/include/zref/zref_terrain_lod.hpp:92-105`):

> `dev[L]` = the largest |fine − coarse| height deviation this subpatch would
> suffer at level L, fx16 world units, unsigned. `dev[0]` is ZERO BY DEFINITION.

**Nothing computes them.** Not in `fpga/rtl`, and not in `zref` either:

* the only driver of `sp_dev1_i` anywhere in the tree is
  `zhao_prod_top.sv:3740`, taking `u53_src[154 +: 24]` — and
  `zhao_prod_top.sv:3690` is `assign u53_src = {16{u53_lfsr_q}}`. **An LFSR.**
* every test supplies them by hand: `tests/terrain/lod_dev.hpp:139-141` writes
  `s.dev[1..3]` straight from a C++ struct the case author filled in.

So this is not a missing wire and not merely missing RTL. **The quantity has no
executable definition anywhere in the repository** — only a sentence.

## The law is DERIVABLE, and that is the good news

Two ratified functions already say how a coarse height is formed, and TESS uses
both:

```
zref_terrain_tess.hpp:173  coarse_height(ha, hb) = ha + div_rhu(hb - ha, 2)
zref_terrain_tess.hpp:212  morph_case(job, vi, vj) -> 0 none, 1 x-mid, 2 z-mid,
                                                      3 diagonal mid
```

At level *L* with stride `s = 1 << L`, a vertex is either carried by the coarse
level (`vi` and `vj` both multiples of `2s`) or is a midpoint of one of three
kinds, and its coarse height is `coarse_height` of the relevant pair. So

```
dev[L] = max over the subpatch's vertices of |fine(v) - coarse_L(v)|
```

needs **no new arithmetic at all** — one comparison and one `coarse_height` per
vertex per level, over 81 vertices × 3 levels per 8×8-cell subpatch. That is
243 subtractions and 243 magnitude-compares; no divides beyond the existing
`div_rhu`, no DSP.

## TWO RULINGS, AND THEY ARE THE WHOLE REASON THIS IS A REPORT

### 1. Which quantity is `dev[L]` — the morph deviation or the mesh deviation?

`morph_case` returns **0 on subpatch boundary vertices**, and its own comment
says why at length:

> GEOMORPH APPLIES ONLY STRICTLY INSIDE THE SUBPATCH — CHOSEN … A vertex on a
> subpatch boundary is shared with a neighbour that has its OWN morph factor …
> Leaving boundary vertices unmorphed makes crack-safety hold unconditionally.

So there are two defensible readings of *"the deviation this subpatch would
suffer"*, and they differ exactly on the boundary ring:

| reading | what it measures | boundary vertices |
|---|---|---|
| **morph deviation** | how far the interior moves during a transition | excluded — they never move |
| **mesh deviation** | how far the coarse *mesh* departs from the fine one | included — the coarse mesh drops them and interpolates across |

The second is the larger number, always, and is the one a projected-error
selector would want if the question is *"will the player see the difference"*.
The first is the one that matches what actually moves on screen during a
geomorph. `morph_case`'s own comment already names the visible consequence of
the split — *"the subpatch interior moves while its border does not, so the
border reads as a shallow crease bounded by the level's own height deviation"* —
which reads as an argument for the second, but stops short of saying so.

**Not decided here.**

### 2. When is it computed — at load, or per frame?

`live_top` is `compose_top` plus the live field lanes. `compose_top` is
`base + scar` clamped, and both are page bytes that do not change until a bake.
So:

* **at load time**, once per page, alongside the mips: 16 subpatches × 243
  operations = about 3,900 per page, next to the 6,726 clocks the page load
  already costs. Free.
* **per frame**, because a live field lane moves `live_top` and therefore moves
  the deviation: the same work × 60 Hz × however many patches are live. At T7's
  32 pages a frame that is ~125,000 operations a frame just for the selector's
  input.

The difference is about **sixty times**, and it is not a performance question
with an obvious answer: a load-time deviation is *wrong* for a patch with an
active deformation field, and terrain deformation is the feature the whole
FIELD.SEQ.EARTH lane exists for.

A third option exists and should be considered rather than discovered later:
compute at load from `compose_top`, and recompute **only for patches whose
`subpatch_dirty_o` mask is non-zero** — which `TERRAIN.PATCH` already produces,
per subpatch, for exactly this class of question (`terrain_rules` §4.4, *"we
recompute exactly where the ground moved"*). That is the cheapest reading that
is not also wrong, and it costs one more consumer of a signal that already
exists.

**Not decided here either.**

## What it would take once ruled

A block on the same shape as `TERRAIN.MIPGEN`: streaming, storage-free,
consuming the composed lattice as it goes past and emitting 16 subpatch records
of three deviations each.

It sits in the same place MIPGEN does — behind `TERRAIN.PAGESTREAM` — if the
answer to ruling 2 is "at load"; behind `TERRAIN.PATCH`'s composed stream if the
answer is "per frame", because that is where `live_top` exists. **The two
answers put the block in two different places in the chain**, which is why it is
worth ruling before it is written rather than after.

Either way it needs a `zref` model first, because there is nothing to be a
differential against: `zref::terrain::lod_deviation(lattice, ox, oz)` returning
the three numbers, written from `coarse_height` and `morph_case` so the law has
one implementation rather than two.

## What this report does not claim

It does not claim the arithmetic is hard — it is 243 subtract-and-compare per
subpatch and no DSP. It claims the **definition** is one sentence with two
readings, that nothing in the repository has ever executed either of them, and
that the choice between them decides where the block goes.
