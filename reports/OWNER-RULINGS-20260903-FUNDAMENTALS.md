# Binding owner rulings — 2026-09-03, the fundamentals decisions

**All eight questions from `FUNDAMENTALS-DECISIONS-NEEDED.md` are ruled.**
This file is the authority; the contracts defer to it.

The core message, in the owner's words:

> **Yes to expressive terrain, fireballs that illuminate things, and real
> liquid surfaces — but each gets the smallest bounded architecture that
> produces the visible result.**

---

## D-1 · Unclamped flat-shade primitive — **(a)**, WITH A REFINEMENT

Add a signed unclamped primitive; keep the existing function as a
**bit-identical wrapper**:

    shade_flat_tri_dir_unclamped(normal, light_dir) -> signed shade

    shade_flat_tri_dir(normal, light_dir) {
        return clamp01(shade_flat_tri_dir_unclamped(normal, light_dir));
    }

**All existing golden CRCs must remain unchanged.** That is how the refactor is
known to be correct.

### THE REFINEMENT, which corrects what the contracts said

I had written "additive terms must be summed before the clamp". **That is true
only within ONE light.** The ruled composition law:

    for each directional/local light i:
        raw_i = shade_flat_tri_dir_unclamped(n, light_dir_i)
        ndl_i = clamp01(raw_i + normal_detail_i)     // detail is THIS light's
        rgb_accum += light_colour_i * ndl_i

    rgb_accum += ambient
    rgb_accum += non-directional spill / global flash
    final_rgb  = saturate(rgb_accum)

Why the distinction matters, stated by the owner:

* a terrain **detail normal may brighten a face whose base normal is slightly
  turned away from that same sun**, so base + detail combine before clamping;
* **a second sun or fireball is an independent light. Its negative dot product
  must become zero; it must not subtract illumination contributed by another
  light.**
* ambient, broad spill and global flashes are **colour contributions, not
  signed directional terms**.

> **The binding rule:** the shared flat-shade primitive gains a signed unclamped
> form. Clamping occurs **once per independent light**, after that light's base
> and detail terms are combined. Coloured contributions are then accumulated,
> and final colour saturation occurs after ambient and spill are added. The
> existing clamped function remains as a bit-identical wrapper.

This keeps `TERRAIN.NORMALMAP` out of the generic terrain/creature normal
function while making multiple suns and local spell lights mathematically sane.

---

## D-2 · Generic cartridge resources — **(A)**

| kind | resource | section type |
|---|---|---|
| **10** | `TEXTURE_PAGE` | `0x000E` |
| **11** | `MATERIAL_SET` | `0x000F` |
| **12** | `MESH_STREAM` | `0x0010` |

Allocated after the current `CLIP_BANK` section. **The ABI generator owns the
final emitted constants and regeneration.** `.zpak` already reserves kinds
10–255, so these are ordinary additive families.

**`TEXTURE_PAGE`** — immutable sampled bytes plus interpretation: dimensions,
format, mip count and offsets, row/level strides, texel payload, page integrity
identity, optional subtype for palette data. **Palettes use this same
publication and generation machinery** rather than growing an unrelated loader.

**`MATERIAL_SET`** — an immutable table indexed by `material_id`. An entry
carries zero-to-three sample bindings, texture and palette page handles, TMU
format/filter/wrap/mip state, combiner recipe and weight, raster state,
cel/toon participation, ink participation, fog exemption, optional AUX use.
**This is the step between `DrawForm`'s handle, a meshlet's `material_id`, and
the concrete sample descriptors the texture machinery expects.**

**`MESH_STREAM`** — immutable geometry: meshlet descriptors, vertex stream,
local-index stream, offsets and counts, format and generation metadata.

### Family pages become manifests, not private loaders

    family resource
       |- references MESH_STREAM
       |- references MATERIAL_SET
       '- MATERIAL_SET references TEXTURE_PAGE

`CREATURE_FORM`, `SKY_SET`, terrain tilesets and future object forms **may
reference** these, and **may not embed a second family-specific interpretation**
of the same texture, material or mesh concepts.

`MEM.UPLOAD` copies all of them as opaque immutable bytes under one residency,
generation, integrity and publication law. **It does not need to know whether
the bytes depict Zixx, a cliff, a water surface or a fireball.**

---

## D-3 · Cache coherence — **(b)**, generation-tagged caches

    cache tag = physical line tag + residency generation

Applies to texture and palette lines, material-record caches, later
mesh/descriptor caches, and **the decoded pose cache — which must distinguish
the clip-bank generation IN ADDITION to `{type, clip, frame, sub}`**.

Publishing a new mapping makes every old entry **structurally unable to match**.
The texture cache's invalidate port stays useful for reclaiming space and hit
rate, but **correctness may not depend on an uploader remembering an ad-hoc
invalidate list.**

**Use the existing 16-bit residency generation.** Before wrapping and reusing a
generation, perform an **epoch transition and global cache invalidation**.
**Silent generation wrap is forbidden.**

> Generation tags provide coherence; explicit invalidation is an optimisation
> and wrap-management tool.

---

## D-4 · Depth quantisation — **separate named block**

`GEOM.DEPTHQUANT`, owning exactly one conversion: Q16.16 `1/w` + selected
profile → `invw24`, once per projected vertex, **before** clipping, parameter
storage and rasterisation.

It may physically sit beside — or eventually inside — the project wrapper, but
**remains a separate ledger row, contract, oracle and test target so unfinished
depth work cannot hide inside an otherwise green `GEOM.PROJECT`.**

**All downstream consumers receive only the canonical `invw24`. No consumer
performs its own profile conversion.**

---

## D-5 · Fog — **carry a factor, apply after material shading**

`SetEnvironment` is promoted from reserved to implemented when this lands.

**Do not carry an already-fogged vertex colour.** Carry **unfogged lit RGB** and
a **fog factor**, computed once per vertex from the frozen view/fog law,
transported through clipping and `GEOM.PARAMBUF`, interpolated through
`ATTRSTEP`.

One unified final ordering:

| | ordinary material | cel material |
|---|---|---|
| 1 | lighting | lighting |
| 2 | interpolate lighting + fog factor | interpolate lighting + fog factor |
| 3 | — | **toon quantisation** |
| 4 | texture/material combination | texture/material combination |
| 5 | **fog final source RGB** | **fog final source RGB** |
| 6 | framebuffer blend | framebuffer blend |

**Fog is applied to the final source colour before alpha or additive blending.**
Fog-exempt classes — sky family, HUD, deliberately emissive/additive effects —
take an **explicit bypass**.

This prevents two ugly errors: **fog quantised into hard toon bands**, and
**texture modulation multiplying the fog colour itself**.

---

## D-6 · Fireballs light the world — **yes**

> A giant magical fireball does not illuminate only itself like a pasted-on
> billboard.

| receiver | v1 lighting |
|---|---|
| creatures and ordinary objects | deterministic bounded **top-K** coloured diffuse lights |
| terrain | sun + **broad local spill and global flash**; not every point light per fragment |
| particles and spell surfaces | emissive/additive materials + glow tags |

The HPS may hold arbitrarily many emitters and **deterministically** selects the
bounded set for visible receivers using intensity, projected importance,
authored priority, **stable source-ID tie-breaking, hysteresis and minimum hold
time**.

For creatures and objects:

* **4** local lights guaranteed for a near/full-detail receiver;
* **6** targeted for hero and bosses;
* **8** is the descriptor maximum;
* ambient and broad coloured spill need **no** directional dot product;
* **first degradation** is dropping expensive face-normal response on weaker
  lights, **then** dropping the weakest local light.

**One sequenced accumulator evaluates the selected terms. K legal lights does
not mean K permanently instantiated light engines.**

Terrain gets a cheaper low-frequency representation — broad spill around nearby
major effects, global flash envelopes for explosions and lightning. **It does
not evaluate every spark against every terrain fragment.**

**No dynamic shadow casting from these lights in v1.** The diffuse colour
response is the important perceptual connection.

---

## D-7 · Liquids — **yes, ordinary geometry through the main renderer**

Never `TWOD.PLANE`; that engine explicitly refuses arbitrary depth-tested world
planes.

**Water v1:** low-poly triangulated surface + optional vertex-wave displacement
+ one or two scrolling bounded texture samples + alpha blend, **depth test ON,
depth write OFF**, deterministic coarse **back-to-front** ordering.

* break large water into **bounded patches**;
* sort at **patch/surface granularity, not per triangle**;
* avoid mutually intersecting transparent sheets in authored v1 content;
* no reflection, no refraction, no scene-colour copy, no screen-space
  distortion;
* shoreline foam may use geometry, AUX surface marks or an additive overlay.

**Lava v1:** opaque or near-opaque base — **depth test ON, depth write ON**,
scrolling base/detail material — with an optional emissive/glow overlay
afterward: **depth test ON, depth write OFF, additive, glow tag**.

Vertex waves are generated **before projection** and need no liquid-specific
raster hardware.

> The material law must explicitly identify water transparency versus lava's
> opaque/emissive base. **"Liquid" is not one ambiguous blend mode.**

---

## D-8 · General tangent-space normal maps — **REFUSED in v1**

The terrain normal-map block is a **specialised exception** whose economy
depends on the heightfield having a world-axis-aligned tangent frame. **That
does not generalise** to arbitrary props, skinned creatures or twisted surfaces.

For v1: terrain may use the specialised detail-normal path; creatures and props
use authored/skinned vertex normals; fine appearance comes from geometry,
textures, toon treatment and bounded lighting. **No tangent/bitangent vertex
attributes, no arbitrary mesh normal texture sample, no tangent-frame
reconstruction in the fragment path.**

Reconsidering it requires an explicit later architecture with a tangent-basis
format, skinning rules, interpolation cost, sample budget and a measured visual
case. **The terrain block is not precedent.**

---

## The near-plane obligation

**The close-camera stress reel is MANDATORY before the v1 geometry path
freezes.** It must include: the camera entering or grazing Zixx's spring pose;
giant limbs and body geometry crossing the camera; long beams passing through
the near plane; large terrain/cliff triangles; water surfaces; and fast camera
motion through each case.

**The owner judges the moving footage at native 240p.** If whole-triangle
rejection visibly removes unacceptable chunks, the response is either a bounded
proper near-plane clipper, or a proven content/tessellation restriction that
makes the pop acceptably small.

> **"Documented simplification" is not itself an acceptable visual result.**

---

## Immediate implementation order (the owner's)

1. **D-1** — add the signed unclamped primitive, preserve the clamped wrapper,
   **and prove the old goldens unchanged.**
2. **D-2** — freeze the three generic resource families and make family assets
   reference them.
3. **D-3** — thread residency generation through resource handles and every
   derived cache.
4. **Record D-4 … D-8 now** so no agent silently chooses different defaults
   while the texture-island recovery continues.
