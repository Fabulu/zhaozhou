# GEOM.MESHFETCH — the cull, derived before any RTL

> Everything here is **simulation and design**. No hardware has run any of it.

The owner ruled the law on 2026-08-22: conservative per-camera frustum rejection
of an instance **bounding sphere**, before vertex decode, rejecting only when the
sphere is outside **every** active camera. "Visibility sectors" is deleted. This
note works out what that means in this machine's actual conventions, so the
implementation starts from arithmetic rather than from a shape.

## 1. The clip convention is already fixed, and it is not the textbook one

From `zref::render::project_vertex` (`reference/src/zrender/rast.cpp:43`), which
is the law GEOM.PROJECT is verified against:

```
clip = mat4_vec4(vp, {x, y, z, 1})     // clip_i = sum_j vp[i][j] * v[j]
if (clip.w.raw <= 0) return o;         // behind the eye — Phase-3 cull
ndc_x = clip.x / clip.w                // -1..+1
ndc_y = clip.y / clip.w                // +Y NDC = +Y canvas ROW (top-left origin)
```

Three things matter and each would be easy to get wrong from memory:

* `mat4fx` is **row-major** and `mat4_vec4` computes `M·v` with `v` a column, so
  the clip components are dot products with the matrix ROWS. Plane extraction
  therefore uses row sums, not column sums.
* **`+Y NDC maps to +Y canvas row**, i.e. downward (video_rules §2, top-left
  origin). There is no Y flip anywhere. A cull that assumes the usual
  Y-up NDC gets top and bottom swapped — and, because both are symmetric about
  zero, it would still look *plausible* on centred content and fail only near
  the top and bottom edges.
* There is **no near/far z clip here at all**. The only depth condition is
  `w > 0`. So the frustum this machine actually uses has **five** planes, not
  six, and inventing a sixth would reject geometry the renderer would have drawn.

## 2. The five planes are row combinations of `vp`

With `clip.x = row0·v`, `clip.y = row1·v`, `clip.w = row3·v`, the visible volume
is `-w <= x <= w`, `-w <= y <= w`, `w > 0`. Rearranged, each condition is a plane
`p·v >= 0`:

| plane | 4-vector |
| --- | --- |
| left | `row3 + row0` |
| right | `row3 - row0` |
| bottom | `row3 + row1` |
| top | `row3 - row1` |
| near (w>0) | `row3` |

No new camera model, no new law: these are the same view-projection the renderer
projects with, read a different way. That is the property a conservative cull
needs — it cannot disagree with what actually gets drawn.

`row2` is unused, which is the same fact as "there is no z clip", stated in the
arithmetic instead of in prose.

## 3. The sphere test

For a plane `p = (a,b,c,d)` and a sphere `(centre, r)`, the sphere lies wholly
outside iff

```
a*cx + b*cy + c*cz + d  <  -r * |(a,b,c)|
```

Reject the instance for a camera iff that holds for ANY of the five planes.
Reject the instance outright iff it is rejected for EVERY active camera. In Duo
that is "outside both", exactly as ruled.

## 4. THE ROUNDING MUST GO ONE SPECIFIC WAY, AND IT IS THE OPPOSITE OF THE HABIT

`|(a,b,c)|` is irrational in general, so it has to be bounded. The available
primitive is `isqrt_u64`, the ratified **exact floor** square root
(qformats §7.2), and §7.4's `normalize3_approx` already does exactly the
computation needed — an exact s128 sum of squares fed to `isqrt_u64`. So the
primitive and its usage pattern both exist.

**But floor is the wrong direction here.** Using `floor(|n|)` makes the
right-hand side `-r*|n|` *larger* (less negative), which makes the rejection
test easier to satisfy — the block would reject spheres that are actually
visible. A too-tight cull does not cost performance, it **deletes geometry**,
and it does so only near screen edges where it reads as objects popping out of
existence.

The safe form takes an **upper** bound on the length:

```
len_lo = isqrt_u64(a*a + b*b + c*c)          // exact floor
len_hi = len_lo + (len_lo*len_lo < sumsq)    // ceil
reject if  dot < -r * len_hi
```

A loose bound costs a little wasted work; a tight one loses geometry. This is
the same asymmetry the owner already ruled on for the bound itself ("a loose
bound merely costs performance"), applied to the arithmetic.

## 5. What exists and what has to be written

| piece | status |
| --- | --- |
| `mat4fx`, `mat4_vec4`, the §2 single-rounding-per-row law | shipped, `zref_fixp.hpp:309` |
| the clip convention and `w > 0` | shipped, `project_vertex` |
| exact sum-of-squares into `isqrt_u64` | shipped pattern, §7.4 `normalize3_approx` |
| `bound_radius` for creatures | shipped, `CreatureType`, from `isqrt_u64` |
| plane extraction, the sphere test, the ceil bound | **to write** |
| the meshlet descriptor format (where `bound_centre` lives) | **owner/undecided** |

So the reference is a small composition of ratified pieces rather than a new
subsystem — which is what makes this kind 1 rather than kind 2, despite `grep
frustum` returning nothing across the whole reference tree.

## 6. Two things NOT to do

* Do not add a far plane or a near-z plane. The renderer has neither.
* Do not normalise the planes by dividing them (the textbook move). Dividing
  four fx16 components by a length introduces four roundings per plane in a
  direction nobody has analysed; comparing against `r * len_hi` needs one bound
  and keeps the plane exact.

## 7. The derivation is CHECKED, before any RTL exists

`cull_check.cpp` (same scratch directory) compiles header-only against
`zref_fixp.hpp` and `zref_trig.hpp` and drives three different perspective
matrices (60 deg 4:3, 90 deg 1:1, 35 deg 16:9):

```
plane equivalence : 600000 checked, 0 mismatches
conservatism      : 60000 spheres, 26796 rejected, 0 WRONGLY rejected
looseness         : 344 kept although empty (acceptable: costs work, not geometry)
```

**Claim 1 is now measured, not asserted.** A point passes all five plane tests
exactly when `project_vertex`'s own conditions say it is visible, across 600,000
random points. If the row combinations were wrong -- most plausibly by swapping
top and bottom, which the downward-Y convention invites -- this is where it
would show.

**Claim 2 is PROVEN, and the sampling only corroborates it.** Rejection uses
`len_hi = ceil(sqrt(sumsq)) >= |n|`, so `dot < -r*len_hi` implies
`dot < -r*|n|`, which is the true "sphere lies wholly outside" condition. The
60,000-sphere sample finding zero wrong rejections is consistent with that, but
the guarantee comes from the direction of the bound rather than from the sample
-- which matters, because a sample can always miss a sphere that pokes into the
frustum by a sliver.

**The 0.6% looseness is the acceptable failure**: those spheres are kept when
they could have been rejected, costing a little decode work and no geometry.

None of this touches the repo's build tree; it is a standalone check written to
answer the question before committing to an implementation.

## 8. How the camera reaches the block: copy GEOM.PROJECT, do not invent

`zhao_geom_project` already loads a per-view matrix, and the cull must use the
same route or the shell ends up with two ways to say the same thing:

```systemverilog
input logic        cfg_we_i,
input logic        cfg_view_i,    // which of the two views
input logic [ 4:0] cfg_addr_i,
input logic [31:0] cfg_data_i,
```

with the address map (zhao_geom_project.sv ~line 220):

| addr | meaning |
| --- | --- |
| 0..15 | `mat[view][addr[3:0]]` -- the 4x4 fx16 matrix, row-major |
| 16 | viewport x0 in bits 11:0, y0 in bits 27:16 |
| 17 | viewport w in bits 11:0, h in bits 27:16 |

**The cull needs 0..15 and NOT 16..17.** Rejection happens in CLIP space, and
the viewport only maps NDC to pixels afterwards — a sphere outside the clip
volume is outside it whatever the viewport does. Taking the viewport would be
harmless but would imply a dependency that does not exist.

### Where the plane extraction should live

The five planes are per camera per frame, not per instance. Extract them ONCE
when the matrix is written (or on the first evaluation after a write) and hold
them, along with the five `len_hi` bounds — that is five sums of squares and
five `isqrt` per view per frame, against potentially thousands of instances.
Putting either on the per-instance path would be the same mistake the LOD
ladder avoided by turning its divides into comparisons.

### The block boundary

Follow `zhao_geom_lod`: the caller owns the per-instance data. The cull takes
`bound_centre` (three fx16) and `bound_radius` (fx16) as PORTS and returns a
reject bit plus the two-bit per-camera visibility the owner ruled. It does not
fetch descriptors — the descriptor FORMAT is still undecided, and a block that
takes its bound as a port is not blocked on that decision.
