# Zixxtrixx remake — visual / animation / asset-pipeline reconnaissance — Findings

**Agent:** R1, visual/animation/asset-pipeline recon (read-only)
**Created:** 2026-08-26
**Parent Task:** RUN-20260826-1615-zixxtrixx-remake-concept-match
**Status:** Complete

Every claim is labelled **MEASURED** (read in a file / computed from quoted
code) or **INFERRED**. Where the repo lacks something it says **NOT PRESENT**
rather than guessing. Paths are relative to `C:\programmieren\zencrifice\`.

---

## Summary

Three things are true and they are not the things the first pass believed.

1. **The cracks are a topology bug, not a weight bug and not a hardware
   limit.** `build_ring_part` emits *every* vertex as `b0 == b1, w0 == 64` —
   rigid. Nothing in the authoring path has ever produced a 2-bone blend. The
   2-influence machinery (`SkinVertex`, `skin_vertex`, `clamp_3to2`, GEOM.SKIN
   RTL) is complete, tested, and **used only by unit tests**. The smallest
   honest fix is a per-ring bone/weight lane on `RingPart` — tooling only,
   no silicon.
2. **RGB565 is innocent. The lighting is the culprit.** Worst RGB565
   quantisation error across every candidate colour is **4/255**. But the
   creature's entire shading model is *one* white directional light, no fill,
   multiplied as a scalar and quantised to 1/16 steps — and on a horizontal
   body **half the surface sits flat at the 0.25 ambient floor** while unity
   is mathematically unreachable. That is what the saturation push was
   compensating for.
3. **The texture path exists on both ends and is missing exactly one splice
   in the middle.** `raster_tri` already samples CLUT8 + RGB565 palette and
   modulates by a Q16.16 factor; the TMU RTL samples five formats with mips.
   `compose_creatures` never builds a `TextureSpan`, `Meshlet` has no page id,
   and **no asset tool anywhere converts a drawing into a texture page.**

---

# A. WHY THE CURRENT MODEL DOES NOT MATCH THE CONCEPT

I opened both sheets with the Read tool and measured against the displayed
2000x1454 rendition (original 7015x5100, x3.51).

## A.1 What the sheets actually show — MEASURED by looking

**`Upheaval/creature/Zixxtrixx/Concept/Side.png`**

| feature | measurement (displayed px) | ratio |
|---|---|---|
| whole composition | x 100->1745, y 380->1310 | **1.77 : 1 landscape** |
| head blob | ~395 long x ~413 tall | — |
| body ribbon thickness | ~230-260 | head is **1.6-1.8x body** |
| eye disc | ~190 diameter, centred (1510, 645) | **0.46 x head height** |
| eye position | ~centre of head blob, slightly forward | — |
| pink dorsal edge stripe | ~55-70 of the ~240 ribbon | **25-28% of cross-section** |
| tail blades | **TWO**, ~450-500 long, ~55-70 at base | **~28% of body thickness** |

Pupil is a **red-orange vertical wavy slit with a kink**, not a round pupil.
A small black spike/notch sits at the fork junction. Two ink "S" doodles
top-right are motion sketches, not anatomy.

**`Upheaval/creature/Zixxtrixx/Concept/Front.png`**

| feature | measurement (displayed px) | ratio |
|---|---|---|
| head disc | ~410 wide x ~480 tall | taller than wide |
| body column at neck | ~270 | **head = 1.52 x body width** |
| each eye | ~105 x ~165 | **26% head width, 34% head height** |
| eye centre height | y ~ 590 | **57% down the head** |
| pink cap boundary | y ~ 490 at centre | **top 36% of the head** |
| blue throat teardrop | extends ~170 px BELOW the head | **~35% of head height** |
| mouth | ~95 wide at y ~ 735 | **23% head width** |

The orange eye rings **break the head's black outline at both extremes** — the
eyes ARE the widest point of the skull. Tail prongs read **one green, one
pink** — the blades are flat plates with the dorsal-pink on one face.

## A.2 THE POSTURE RECONCILIATION — INFERRED from MEASURED geometry

`REMAKE-BRIEF.md` and MODELINGGUIDE §1 disagree about whether the side sheet
shows an upright posture. **Both are right.** The side sheet is drawn as a
*decorative horizontal composition* (1.77:1 landscape). **Rotate it 90 deg CCW
and you get exactly the upright S the guide asks for**: tail forks resting
lower-left, body rising through an S, head at the top. That is precisely what
Front.png independently shows — tail prongs left, body making a J, head on
top. Do not treat the horizontal layout as the posture.

## A.3 The structural mismatches — MEASURED against `tools/reel/zixxtrixx.h`

These are structural, not aesthetic.

**1. Three prongs where the concept has two.** `zixxtrixx.h:210-212`
(`kBProngA/B/C`) and `zixxtrixx.h:521` (`for (int k = 0; k < 3; ++k)`).
Both sheets show **two**. Confirmed independently by `REMAKE-BRIEF.md:44`.

**2. The prongs are deliberately square; the concept's are flat blades.**
`zixxtrixx.h:104` — `constexpr int kProngSegs = 4;   // 4 = square cross section = BLOCKY`.
A 4-sided rotationally-symmetric prism cannot make a flat blade. The concept
blades are ~4:1 flat plates with a pink edge on one face.

**3. Every colour region is GEOMETRY, and it costs a third of the bone budget.**
`zixxtrixx.h:427-437` (pink cap part), `:494-506` (8 ridge parts), `:203`
(`kBRidge0`, `kRidgeSegs = 8`). Of **28 bones, 9 (32%) exist only to paint
colour** — `kBCap` plus `kBRidge0..7`. Of **31 ring parts** counted from
source, **14 (45%)** exist only to paint colour (cap 1, ridge 8, prong tips 3,
eye rims 2). The concept's pink is a *painted edge stripe*, not a tube chain.

**4. `RingSpec` cannot make the shapes the concept needs — NOT PRESENT.**
`reference/include/zref/zref_creature.hpp:313-317`:
```cpp
struct RingSpec {
  int32_t y;       // fx16 along the part axis
  int32_t radius;  // fx16
  uint8_t segments;
};
```
There is **no centre offset and no second radius**. MODELINGGUIDE:136 says
*"Use offset and elliptical rings where they are sufficient"* — **neither
offset nor elliptical rings exist in the repo.** Every ring is a circle
centred on the part axis, so a broad flattened skull, a flat tail blade and a
ribbon-shaped body are all unreachable without a tooling change. `align` is
also **per-part, not per-ring** (`zref_creature.hpp:337`), so a part cannot
twist along its length either.

**5. The eyes cannot influence the silhouette the way the concept's do.**
`zixxtrixx.h:81-83`: `kEyeR = 132`, `kEyeRimR = 158` against `kHeadRMax = 335`
— eye/head 0.39 by radius. The concept's eyes *break the head outline*; here
they are discs stuck onto a body of revolution. The first run found exactly
this by rendering (`RUN-20260826-0617/TASK_LOG.md:136-141`).

**6. Deliberate, documented deletions from the concept.**
`zixxtrixx.h:85-89` — the mouth: *"REMOVED"*.
`zixxtrixx.h:125-129` — `kOrange` (212,121,96): *"is GONE, not unused ... the
subject could not fit the 256-colour law with it. This is the one place the
concept art lost a colour outright."*
`zixxtrixx.h:130-132` — the blue->green throat blend: *"cost a whole shading
band against the 256-colour ceiling."*
All three are concept features; all three were cut for a GIF gate.

**7. Six sides, chosen for GIF.** `zixxtrixx.h:61-65`: *"6, not 8. Every
distinct face normal is a distinct shade and every shade is a palette entry
against the 256-colour law."* `compile_creature` permits **3..32** segments
(`creature_core.cpp:450-451`), so 12 is legal today.

**8. Horizontal ground snake, not an upright S.** `zixxtrixx.h:264-266` — the
skeleton is a pure `-kSegLen` chain along -X with `kHeadRise = 132` mm of lift
on the head bone only. There is no S in the rest pose at all; the only body
shape ever seen is whatever the clip's per-frame quats produce.

---

# B. CONTINUOUS BENDABLE BODY

## B.1 The single decisive line — MEASURED

`reference/src/zcreature/creature_core.cpp:342-344`:
```cpp
    for (const BuiltVert& bv : ring_cache.back()) {
      cur.verts.push_back(SkinVertex{bv.x, bv.y, bv.z, part.bone, part.bone, 64, bv.u, bv.v});
    }
```
`b0 = part.bone`, `b1 = part.bone`, `w0 = 64`. **Every vertex the ring builder
has ever produced is rigid.** Caps too (`:392`, `:403`). No code path in
`build_ring_part` can emit anything else, because `RingPart` carries exactly
one bone (`zref_creature.hpp:338`).

## B.2 Can a vertex carry two bones at non-64/0 today? YES — but not from the authoring path

**The runtime is ready.** `creature_core.cpp:240-256`:
```cpp
  // 2-weight: w0*(A v) + w1*(B v) EXACTLY in s128 (the skin product is
  // never rounded before the blend), then ONE rescale(.,22) ...
  const mat3x4fx& B = palette[v.b1];
  const int32_t w1 = 64 - v.w0;
    *o[i] = rescale_s32(v.w0 * pa + w1 * pb, 22, L, &SatLedger::mul);
```
**The hardware is ready.** `design/contracts/GEOM.SKIN.md:90-98` specifies the
identical `rescale(w0*pa + w1*pb, 22)`, and
`RUN-20260823-0937-geom-skin-dsp-rearchitecture/SPEC_v1.md:91` records that
*"the six weight multiplies contributed approximately zero"* — two-influence
skinning is **free** in silicon.

**The 3->2 compile gate is ready.** `clamp_3to2`, `creature_core.cpp:661-757`,
emits blended vertices at `:727` —
`SkinVertex cv{s.x, s.y, s.z, bi[0], bi[1], n0, 0, 0}`.

**And it is dead code in production.** Grepping `clamp_3to2` across the tree
returns its declaration, its definition, and **five calls, all in
`tests/geometry/creature_core.cpp:466-501`.** Nothing else calls it.
`compile_creature` (`zref_creature.hpp:399-400`) takes only
`const std::vector<RingPart>&` — **there is no way to hand it pre-skinned
vertices or a custom mesh.**

Note also that `clamp_3to2` writes `u = 0, v = 0` (`:727`) and `SourceVertex`
has no texcoord lane at all (`zref_creature.hpp:417-421`) — **the 3->2 path
currently destroys UVs.**

## B.3 Point by point against the four things the brief asks for

**shared boundary vertices between sections — AVAILABLE within a part, ABSENT
across parts.** A single `RingPart` already shares each interior ring between
the pair below and the pair above (`creature_core.cpp:366-367`,
`hi = add_ring(ri+1); lo = hi - n`). Even the **meshlet split duplicates the
seam ring at identical positions** (`:364` `add_ring(ri)`, comment *"the split
stays watertight"*) — **meshlet splitting is NOT a crack source.** But across
parts there is no sharing at all; each part is compiled independently and
offset by its own bone's bake (`:494-501`). MEASURED.

**no internal caps — ALREADY AVAILABLE.** `caps` is per-part
(`zref_creature.hpp:336`); the current body sets `p.caps = 0` except the last
(`zixxtrixx.h:487`). One long part has no internal caps by construction.
MEASURED.

**per-vertex 2-bone blended weights — BLOCKED.** `RingPart` has one `bone`;
`build_ring_part` hardcodes `64`. See B.1. MEASURED.

**continuous longitudinal UVs — BLOCKED, and worse than it looks.**
`creature_core.cpp:326-329`: `v_lane_of(ri) = ri*255/(n_rings-1)` — V runs
0..255 **per part**, restarting at every part. And `build_ring` `:281`:
`out[k].u = ang >> 8` — **U is the ring angle and nothing else.** So the
texture today runs *around* the animal and repeats *along* it: exactly
backwards from the concept's dorsal stripe. MEASURED.

## B.4 Why the cracks are as big as they are — COMPUTED

Body part `j` spans backward from bone `j` to bone `j+1`'s origin. Bone `j+1`
rotates; part `j`'s trailing ring does not follow it. Peak per-joint pitch
from `build_attack` (`kAttackArc = 6620`, peak key `{27,-1120}`, rear weight
`w = j/10`); gap ~ `2*R*sin(theta/2)`; camera scale `k=240000, dist=8 m,
W=384` -> **87.9 px/m**:

| joint | per-joint pitch | body radius | gap | gap in pixels |
|---:|---:|---:|---:|---:|
| 4 | -16.3 deg | 156 mm | 44.2 mm | 3.9 px |
| 6 | -24.4 deg | 132 mm | 55.9 mm | 4.9 px |
| **8** | **-32.6 deg** | **108 mm** | **60.6 mm** | **5.3 px** |
| 10 | -40.7 deg | 84 mm | 58.5 mm | 5.1 px |

At joint 8 the body is 216 mm (19 px) thick and the hole is 61 mm (5.3 px) —
**over a quarter of the local diameter.** Accumulated chain rotation is 224
deg, matching the comment at `zixxtrixx.h:305-311`. This is a real geometric
opening; no texture can cover it.

## B.5 The smallest honest change — INFERRED, tooling only

**Add a per-ring bone/weight lane to `RingSpec`, and let `build_ring_part`
emit blended vertices. Nothing else.**

```
struct RingSpec { int32_t y; int32_t radius; uint8_t segments;
                  uint8_t b0; uint8_t b1; uint8_t w0; };   // +3 bytes
```
In `build_ring_part`'s `add_ring` lambda replace
`{..., part.bone, part.bone, 64, ...}` with the ring's own `{b0, b1, w0}`, and
change the bind-space offset at `creature_core.cpp:494-501` from "offset by
`part.bone`'s bake" to authoring rings directly in creature-global bind space
(equivalent to offsetting by `b0` when `w0 == 64`). Add `v_base`/`v_span` to
`RingPart` so V runs continuously across a chain instead of restarting per
part.

Why this is the smallest:

* **zero silicon** — GEOM.SKIN already implements the blend;
* **zero runtime** — `skin_vertex` already takes the 2-weight path;
* **no new compile gate** — `w0 + w1 = 64` is structural by construction, and
  `clamp_3to2` stays available for a future 3-influence importer;
* it makes the "continuous flexible chain" a **reusable primitive** — one
  `RingPart` spanning N bones with a weight ramp is a snake, a tail, a
  tentacle or a long neck;
* it deletes 9 bones and 14 parts of pure colour geometry once the texture
  lane lands (A.3.3).

Validators to add with it, all compile-time: `w0 <= 64`;
`b0/b1 < bone_count`; every interior ring appears exactly once (no duplicate
seam positions across parts); V strictly increasing along the chain;
`caps == 0` on any ring that is not a chain end; and a **max-bend pose sweep**
asserting that no vertex pair sharing a bind position ever separates.

---

# C. TEXTURE PATH — WHAT EXISTS, AND THE ONE MISSING ARTEFACT

## C.1 What already exists end to end — MEASURED

**Sampler, reference side.** `reference/src/zrender/internal.hpp:126-133`:
```cpp
struct TextureSpan {
  const Tileset* ts = nullptr;
  uint8_t tile_a = 0, tile_b = 0, weight = 0;
  bool mosaic = false;
  int32_t mod_r = 1 << 16, mod_g = 1 << 16, mod_b = 1 << 16;
};
```
`raster_tri(..., const TextureSpan* tex = nullptr)` (`internal.hpp:155-157`) —
**the texturing overload is already in the ONE rasteriser the creature uses.**
`ScreenV` already carries `int32_t u, v` in Q16.16 (`internal.hpp:76-77`).
Sample+modulate is `rast.cpp:249-255` (CLUT8 index -> RGB565 palette entry ->
expand -> multiply by `mod_*`). Terrain uses it today. `mod_* == 0x10000` is
**exact unity** (`internal.hpp:122-124`).

**Sampler, hardware side.** `fpga/rtl/texture/zhao_texture_tmu.sv:409-413`
enumerates `FMT_CLUT8 / FMT_RGB565 / FMT_CLUT4 / FMT_ARGB1555 / FMT_ARGB4444`;
`:219-228` gives the mode word with `FILTER` (nearest/bilinear), `WRAP_U/V`,
`LOG2W/H`, `MAX_LEVEL`, `MIP_EN`. Mips: **up to 16 levels, floor-selected**,
`level = min(lod >> 4, max_level, min(log2w, log2h))` (`:130-142`), no
trilinear blend. CLUT+bilinear is hardware-downgraded to nearest with an error
pulse (`:174-180`, `:470`). The palette is fetched **through the same 1 KiB
texture cache** (`ST_PAL`, `:404`, `:640`; geometry `zhao_texture_cache.sv:42-44`)
at `pal_base + idx*2` — up to 256 entries, 512 B, **per request**, not a
global CLUT.

**The format already prescribes creature textures.** `spec/creature_rules.md:47`
— *"One texture (CLUT8 page) per part; optional 1-bit alpha key."*
`creature_rules.md:157` — kind 8 = *"compiled part table (part -> meshlet ids,
**texture page**, alpha flag)"*.

**`SkinVertex` does carry usable u/v** — `zref_creature.hpp:284`,
`uint8_t u = 0, v = 0`, filled by `build_ring` (`creature_core.cpp:281-282`).
8-bit each; see C.3 for the caveat.

## C.2 The missing seams — MEASURED, three of them

**Seam 1 — `compose_creatures` never textures anything.**
`reference/src/zcreature/creature_sim.cpp:451-462` is the entire creature
triangle path:
```cpp
        const int32_t shade = quant_shade(ambient_floor(
            render::shade_flat_tri(a.wx, a.wy, a.wz, b.wx, b.wy, b.wz, c.wx, c.wy, c.wz, L)));
        render::TriMode tm;  // opaque: depth test + write
        render::raster_tri(surf, vpp, a.s, b.s, c.s, sat_u8((m.r * shade + 32768) >> 16),
                           sat_u8((m.g * shade + 32768) >> 16), sat_u8((m.b * shade + 32768) >> 16),
                           tm);
```
No `TextureSpan`. `ScreenV.u/v` are never written — the `PV` struct at
`:438-442` carries only `s, in, wx, wy, wz`. **`SkinVertex.u/v` are computed
by the ring builder and read by nobody. They are dead data today.**

**Seam 2 — `Meshlet` has no page id.** `zref_creature.hpp:304-308`:
```cpp
struct Meshlet {
  std::vector<SkinVertex> verts;
  std::vector<uint8_t> idx;           // 3 * tri_count vertex indices
  uint8_t r = 128, g = 128, b = 128;  // part material (the CLUT8 page stand-in)
};
```
The comment says it outright: the flat RGB **is a stand-in for a CLUT8 page**
that was never built.

**Seam 3 — and this is THE one — NO ASSET TOOL PRODUCES A PAGE.**
`tools/pack/` contains **one file: `.gitkeep`**.
`design/contracts/SW.TOOLS.ASSET.md:12-14` puts it out of scope explicitly:
*"Excluded (L4 lane, later waves): meshlet/LOD/microform packing, **procedural
texture baking** ..."*, and `:42-43` *"The packer re-serializes, never
computes."* NOT PRESENT anywhere in `tools/`, `compiler/` or `native/` (empty):

* PNG decoder — **NOT PRESENT**
* palette builder / colour quantiser — **NOT PRESENT**
* mip-chain generator — **NOT PRESENT** (despite the TMU shipping mip
  selection and `zhao_texture_tmu.sv:19` calling mipmaps *mandatory*)
* CLUT4 / ARGB1555 / ARGB4444 encoder — **NOT PRESENT**
* tileset -> VRAM swizzle — **NOT PRESENT** (`zref_render.hpp:164-165`)

The **only** thing in the repo that produces a `Tileset` is
`tools/reel/zhao_reel.cpp:768-830`, `island_tileset()` — LCG integer noise
with 17 hand-typed palette triples, **generated inside the GIF exporter**,
level 0 only. `zref_render.hpp:166-169`:
```cpp
struct Tileset {
  uint16_t palette[256] = {};        // RGB565 LE halfwords
  uint8_t tiles[256][64 * 64] = {};  // CLUT8 indices per tile
};
```
Single level. **No mips in the reference renderer at all.**

## C.3 What it takes to get one hand-drawn crayon page onto this creature

**THE EXACT MISSING ARTEFACT: a deterministic `PNG -> { CLUT8 tile page,
RGB565 palette, mip chain }` converter, plus the `Meshlet.page` field and the
`TextureSpan` splice in `compose_creatures`.** Everything on both sides of it
already ships.

Minimum viable path, each step a named seam (INFERRED):

1. **Converter** (new; `tools/pack/`, or a Python peer of `togif.py`): crayon
   PNG -> 256-entry RGB565 palette + CLUT8 indices, deterministic (fixed
   median-cut or a fixed authored palette; **no dithering**, matching
   `togif.py`'s palette-exact discipline), plus a box-filtered mip chain built
   **in palette space** so the smallest mip keeps blue/yellow/orange/green/pink
   instead of averaging to mud (MODELINGGUIDE:214-216).
2. **`Meshlet` gains `uint8_t page`**; `RingPart` the same; `compile_creature`
   copies it through.
3. **`compose_creatures` builds a `TextureSpan`** with `mod_r/g/b = shade` —
   the *existing* Q16.16 modulate — and writes `ScreenV.u/v` from
   `SkinVertex.u/v`.
4. **UV authoring**: the per-ring lane from B.5.
   **Caveat, MEASURED:** `u/v` are 8-bit, so a page is 256 texels per axis as
   addressed from the vertex. For a 3.5 m creature that is ~14 mm/texel — fine
   for crayon, insufficient for fine detail. Either accept it or widen to
   `uint16_t` tool-side and let the packer narrow per meshlet. `ScreenV.u/v`
   are already Q16.16 so the raster does not care.
5. **Seam placement**: U is `ang >> 8` with `align` controlling phase
   (`creature_core.cpp:275`, `:281`). Set `align` so the wrap lands on the
   belly.

**Donor precedent for the page shape — MEASURED**, `docs/OWNER_DOCKET.md:1330-1335`:
637 `.SXTX` assets, *"width is essentially always 256, but height is arbitrary
— 9 to 799 ... only 81 of 637 (12.7%) are power-of-two in both axes"*, with V
computed as `ring.texture / textureMax`. Sacrifice's pages are **vertical
atlas strips, one per body part, V running the length of the part** — exactly
the longitudinal-V law the brief wants. The donor got joint-continuous texture
by putting the joint *inside* a part. Note it does **not** solve part-to-part
UV matching; a shared body page with the seam underneath is **our** design,
not a copy. Label it as ours.

**Caution — MEASURED:** `zhao_raster_fragment.sv:69-70` still asserts
*"TEXTURE.TMU DOES NOT EXIST"*, while `zhao_texture_tmu.sv:355-362` documents
its outputs as *"These three fields ARE zhao_raster_fragment's
`frag_texel_rgb_i` ..."*. The comment is stale and no top level was found
wiring the two. Do not read it as evidence of a gap in the TMU.

---

# D. COLOUR — THE HIGHEST PRIORITY

## D.1 The real framebuffer format — MEASURED

**RGB888 internally, resolved once to RGB565 with 4x4 ordered Bayer dither.**

Working surface: `internal.hpp:50-53` — *"charter §8: RGB888 working, Q16.16
depth"*, `std::vector<uint8_t> rgb;  // w*h*3`.
Tile store (RTL): `zhao_raster_tilestore.sv:28-32` — *"`[63:40]` 24-bit RGB
working colour"*. Blender carries 8 bits/channel (`zhao_raster_blend.sv:80-86`).

Resolve: `zhao_raster_resolve.sv:210-212`:
```systemverilog
  // video_rules.md §3: [15:11] R, [10:5] G, [4:0] B.
  logic [15:0] px565;
  assign px565 = {c_r5, c_g6, c_b5};
```
Oracle, `reference/src/zrender/resolve.cpp:64-85`:
```cpp
inline constexpr uint8_t kBayer4[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5},
};
      const uint8_t b = kBayer4[y & 3][x & 3];
      const uint32_t r5q = (static_cast<uint32_t>(r) * 31 + b * 16 + 8) / 255;
      const uint32_t g6q = (static_cast<uint32_t>(g) * 63 + b * 16 + 8) / 255;
```
Called once at `render_frame.cpp:500`. Bayer phase is **absolute (surface),
not tile-local** — `zhao_raster_resolve.sv:191-195`. Scanout consumes 565
as-is with no CLUT (`design/contracts/VIDEO.SCANOUT.md:29`).

**The presentation gamut is 65,536 colours, ordered-dithered. There is no
palette anywhere in the framebuffer path.**

> Doc drift worth a one-line fix: `zhao_raster_resolve.sv:10,13-15,199-200`,
> `zhao_raster_quant.sv:9,15-19,27-28`, `design/contracts/RASTER.RESOLVE.md:62`
> and `reference/include/zref/zref_tileresolve.hpp:9` all still state green's
> pre-2026-08-18 dither amplitude (`B*32+16`). **The shipping RTL is correct** —
> `zhao_raster_resolve.sv:203-208` instantiates all three at `AMP=16, RND=8`.
> `RASTER.RESOLVE.md` contradicts itself between lines 62 and 74.

## D.2 Authored r,g,b -> pixel: the complete transform — MEASURED

For a creature triangle, in order:

```
1.  lambert = clamp( n.L , 0, 1 )                shade_flat_tri, terrain.cpp:61-99
                L = (1,2,1)/sqrt(6), normalised   internal.hpp:180-185
                    kLightX=26758  kLightY=53521  kLightZ=26758
                FLAT PER TRIANGLE. no vertex normals. no specular. no fill.
                the light is WHITE - ONE scalar applied to R, G and B alike.

2.  s = 0.25 + 0.75 * lambert                    ambient_floor, creature_sim.cpp:330-332
        return 16384 + ((shade * 49152 + 32768) >> 16);

3.  s = round(s * 16) / 16, clamped [0,1]        quant_shade, creature_sim.cpp:321-326
        int32_t q = (shade + 0x800) >> 12 << 12;
        // comment: "palette law: a creature contributes <= 17 shades per
        //           material; the tool counts and enforces"

4.  pixel_c = sat_u8( (authored_c * s + 32768) >> 16 )   creature_sim.cpp:459-461

5.  RGB565 + 4x4 Bayer dither at resolve                 resolve.cpp:76-85
```

**There is no other input.** No light colour, no ambient colour, no rim, no
emissive, no fog on the creature path. The sky contributes **zero** light —
`zhao_reel.cpp` sets `s.sun_energy = zref::fx16{0}` and the sun/cloud layers
are never requested.

The creature path *does* get ambient (step 2); terrain **top surfaces do
not** — `terrain.cpp:447-451` applies the identical floor only to walls and
undersides. Do not conflate the two when comparing renders.

## D.3 WHAT THE SATURATION WAS COMPENSATING FOR — COMPUTED

Three answers. The third is the real one.

**(i) NOT RGB565. Worst quantisation error is 4/255 = 1.6%.** Round-half-up
5/6/5 with bit-replication expansion, before dither:

| colour | in | 565 index | back to 8-bit | max channel error |
|---|---|---|---|---:|
| `kGreen` flank | 116,205,147 | 14,51,18 | 115,207,148 | **2** |
| `kPink` dorsal (saturated) | 206,130,175 | 25,32,21 | 206,130,173 | **2** |
| `kBlue` head | 20,163,213 | 2,40,26 | 16,162,214 | **4** |
| `kYellow` eye (saturated) | 250,226,92 | 30,56,11 | 247,227,90 | **3** |
| sheet pink (raw) | 226,203,221 | 27,50,27 | 222,203,222 | **4** |
| sheet yellow (raw) | 246,236,167 | 30,58,20 | 247,235,165 | **2** |
| `kOrange` (deleted) | 212,121,96 | 26,30,12 | 214,121,99 | **3** |

With the Bayer dither the average error is smaller still. **RGB565 is not a
constraint on this creature and never was.**

**(ii) The gain never reaches unity on a body of revolution.** For a cylinder
with a horizontal axis the face normal is `n = (0, cos t, sin t)` and
`n.L = (2 cos t + sin t)/sqrt(6)`, whose maximum is `sqrt(5)/sqrt(6) = 0.9129`.
After the ambient floor and the 1/16 quantiser the **brightest reachable face
on the body is 15/16 = 0.9375. Unity is mathematically unreachable.**

**(iii) THE REAL ANSWER — half the animal is a flat, formless 25% silhouette,
and a purely multiplicative white light turns a pastel into grey.**
Twelve faces of a 12-sided horizontal body, through the exact chain:

| face | lambert | after ambient | quant | `kGreen` | raw sheet pink | pink chroma spread |
|---:|---:|---:|---:|---|---|---:|
| 0 deg | 0.8167 | 0.8625 | 14/16 | (102,179,129) | (198,178,193) | 20 |
| 30 deg | 0.9114 | 0.9336 | **15/16** | (109,192,138) | (212,190,207) | 22 |
| 60 deg | 0.7619 | 0.8214 | 13/16 | (94,167,119) | (184,165,180) | 19 |
| 90 deg | 0.4083 | 0.5562 | 9/16 | (65,115,83) | (127,114,124) | 13 |
| **120-270 deg (6 faces, HALF the body)** | **0.0000** | **0.2500** | **4/16** | **(29,51,37)** | **(57,51,55)** | **6** |
| 300 deg | 0.0547 | 0.2911 | 5/16 | (36,64,46) | (71,63,69) | 8 |
| 330 deg | 0.5031 | 0.6273 | 10/16 | (73,128,92) | (141,127,138) | 14 |

Read the bold row. **Six of twelve faces — a full half of the surface — are
identically 4/16.** They are not shaded; they are one flat dark colour with
zero form. The animal is lit from one side with no fill, no bounce and no sky
term, so half of it is a silhouette.

Now the chroma column. The raw sheet pink (226,203,221) has a channel spread
of **23 counts** at full light and **6 counts** at the ambient floor. Six out
of 255 is *not a colour*; it is grey. A pastel is *defined* by being near
white, and a purely multiplicative scalar preserves the hue **ratio** exactly
while collapsing the absolute chroma toward zero. **That is the "grey helmet"
the first pass reported** (`RUN-20260826-0617/TASK_LOG.md:143-149`), and
pushing to 206,130,175 (spread **76** at full, **19** at floor) was the only
lever available.

It was compensating for **a one-light, no-fill, multiply-only shading model** —
not for RGB565, and not really for the 1/16 quantiser either.

**The brief already anticipated this** (MODELINGGUIDE:249): *"Do not solve
pastel colours becoming muddy merely by pushing saturation harder. First
correct the material response, lighting and texture."* The evidence supports
that instruction exactly. The fix is a **coloured ambient/fill term** (a cool
sky-colour ambient instead of a grey scalar) and/or a raised ambient floor
and/or dropping `quant_shade` — all in `creature_sim.cpp:321-332`, about 12
lines, reference-side, **no silicon**.

## D.4 Where the 256-colour law is enforced, and whether it is only a GIF gate

**The gate itself — MEASURED, one site.** `tools/reel/zhao_reel.cpp:2135-2142`:
```cpp
  // ---- the palette law: a shipped GIF must be palette-exact ----
  if (pal.count() > 256) {
    std::fprintf(stderr,
                 "%s: %zu unique colours (> 256) - the frame set cannot be encoded "
                 "palette-exactly. Re-author the scene; NEVER fall back to palettegen.\n",
                 sub.name, pal.count());
    return 3;
  }
```
A **hard build failure, exit 3**, over the **union of every frame** of a
subject (`PaletteSet`, `:117-137`; `pal.add_frame` in the loop at `:2108`,
`pal.count()` after it at `:2136`), firing in `--check` too because it sits
before the `if (!g_write)` return at `:2155`.

**Is it purely a GIF-export gate? NO. Three parts.**

**(a) In hardware and in the framebuffer there is no 256-colour limit at all.**
Direct RGB565 out of resolve, no CLUT in scanout, RGB888 through the tile
store and blender (D.1). The TMU's palette is **per-request**
(`req_pal_base_i`, `zhao_texture_tmu.sv:338`) — a per-texture CLUT8 page, not
a global scene palette. Confirmed **NOT PRESENT** in RTL, contracts, or the
reference renderer's output path.

**(b) But it has already leaked INTO the reference renderer, in two places.**
The most consequential finding in this section:

* `reference/src/zcreature/creature_sim.cpp:319-326` — `quant_shade`, applied
  unconditionally to **every creature triangle**, its comment naming the
  cause: *"palette law: a creature contributes <= 17 shades per material; the
  tool counts and enforces"*. This bands the lighting permanently, GIF or not.
* `reference/src/zrender/terrain.cpp:462-464` — *"The palette ladder: the
  flat-shade weight is quantised to 2 bits `((shade + 8191) >> 14)` BEFORE
  modulation so the modulated palette stays inside the 256-colour capture
  law"*. Costs terrain ~8% of its dynamic range (0.75 instead of 0.8167).
  `terrain.cpp:562-564` suppresses a whole second tint family for the same
  reason.

**(c) And it has already amputated the content, irreversibly, in source.**
Removing the export gate restores none of these:

| what was lost | site |
|---|---|
| `kOrange` eye rim — a concept colour, deleted outright | `zixxtrixx.h:125-129` |
| the mouth | `zixxtrixx.h:85-89` |
| the blue->green throat transition | `zixxtrixx.h:130-132` |
| 6 body sides instead of 8+ | `zixxtrixx.h:61-65` |
| the animated terrain field, deleted from both subjects | `zhao_reel.cpp:2940-2943`, `:2978-2981` |
| the dusk sky gradient, flattened | `zhao_reel.cpp:2974-2977` |
| **the slither halved to ONE gait cycle per orbit** | `zhao_reel.cpp:2908-2911` |
| the island texture lane, stripped for flare subjects | `zhao_reel.cpp:964-968` |
| a light distance changed purely for palette count | `zhao_reel.cpp:1108-1120` |

**Measured headroom today** (`Upheaval/website/scratch-reel/*.txt`): slither
**239/256** at 64 frames; strike **249/256** at 96 frames. Seven colours of
headroom on the strike. The law is not a distant ceiling — it is binding on
every decision in the lane.

**Verdict: the ENFORCEMENT is a GIF gate; the CONSEQUENCES are not.** The
parent must (1) demote the gate to a post-approval fallback, (2) delete
`quant_shade` from `creature_sim.cpp` or make it opt-in, and (3) treat every
row of table (c) as content to restore, not as settled design.

---

# E. HOW SACRIFICE AVOIDS VISIBLY DISCONNECTED BODY SECTIONS

## E.1 The measurement that settles it — MEASURED

`docs/OWNER_DOCKET.md:1599-1607`, over **317,234 ring-vertices across all 93
donor models**:

| influences | share |
|---|---|
| 1 bone | 65.07% |
| 2 bones | 32.41% |
| **3 bones** | **2.51%** |

**34.92% of every vertex in Sacrifice is influenced by more than one bone.**
If the donor's bodies were rigid parts that number would be exactly zero.
`OWNER_DOCKET.md:1609-1611` adds that the clipped 2.51% *"are the seam
vertices (shoulders, hips, neck), where the error is most visible."*

**Sacrifice avoids disconnected body sections by not having any.** The visible
model is one continuous skinned shell; joints deform rather than pivot.

Structures, `Upheaval/creature/06-DONOR-AND-SACENGINE.md:23-26`:
```d
struct Bone     { Vector3f position; size_t parent; Vector3f[8] hitbox; }
struct Position { size_t bone; Vector3f offset; float weight; }
struct Vertex   { int[3] indices_; Vector2f uv; }
struct BodyPart { Vertex[] vertices; uint[3][] faces; Texture texture; }
```
**`BodyPart` carries no bone field at all.** Bones reach vertices only through
the shared `Position` pool. `BodyPart` is a **texture/material unit, not a rig
unit.** `:34-36` — *"up to **3** bone influences per vertex, weights arriving
as `weight/64.0f` so **6-bit**, one texture per body part."*

Part counts confirm it structurally (`Upheaval/docs/ANIMATION-NOTES.md:556-558`,
`:607-610`): Phoenix **29 bones / 6 body parts**; Manahoar **17 bones /
5 parts**; roster median **19 bones / 6 parts**. Six parts cannot rigidly
cover 29 bones.

## E.2 The weight encoding, and a trap — MEASURED

`OWNER_DOCKET.md:1300-1302`: *"Sacrifice's weights are a raw `ubyte` scaled by
`weight/64.0f` ... and **all 256 raw values occur**, so its blend is an
*affine* sum of independently-weighted bone-space offsets, not a convex
combination."* **/64 is the SCALE, not the normaliser** — a ubyte of 255 is a
weight of 3.98. `:1304`: *"the importer must normalise each vertex's weights
to sum to 64."* Our `SkinVertex` stores only `w0` and derives `w1 = 64 - w0`
(`zref_creature.hpp:283`), so normalisation is structural for us — but donor
meshes cannot be ingested unmodified (`OWNER_DOCKET.md:1623-1624`).

## E.3 A mis-attribution in our own tree — MEASURED, and it authored this bug

`reference/include/zref/zref_creature.hpp:338`:
```cpp
  uint8_t bone = 0;     // rigid part: one bone per part (donor law)
```
Repeated verbatim at `Upheaval/creature/01-RING-CONSTRUCTION.md:58` and built
into a whole section at `02-SKELETON-AND-SKINNING.md:23-30` (*"the body-part
decomposition **is** the rig ... There is no such thing as a single part that
deforms along its length."*).

**"(donor law)" is false.** No source supports it and the 317,234-vertex
census refutes it. One-bone-per-`RingPart` is a *zhaozhou reference-builder*
convenience that got labelled as a donor constraint — and the first Zixxtrixx
was then authored against it: `RUN-20260826-0617/TASK_LOG.md:105`, *"28 bones,
**~38 rigid ring parts**"*.

**Also stale:** `spec/creature_rules.md:120-122` and
`02-SKELETON-AND-SKINNING.md:55-57` both say the donor's weight distributions
*"could not be measured"*. That was true in
`RUN-20260816-0046/FINDINGS-S2-sacengine-creatures.md:3` and was **superseded
2026-08-23** by the `OWNER_DOCKET` census. The §3 gate remains correct; only
its justification is out of date.

## E.4 What 3 influences implies for our 2-influence path — INFERRED from measured premises

**Two is not a compromise for a serpent. It is exactly the right number.**

1. The donor's 3-influence vertices are at **shoulders, hips, neck**
   (`OWNER_DOCKET.md:1310-1311`, `:1610`) — **branch points**, where three or
   more bone segments meet in a vertex's neighbourhood.
2. **A serpent chain has no branch points.** In `b0 -> b1 -> ... -> bn` every
   interior joint has valence 2; the natural support of any vertex's weight
   function is `{bi, bi+1}` and nothing else. The clamp error is not "small";
   it is **identically zero**, because a third weight has nothing to bind to.
3. Only **2.51%** of donor vertices needed 3, across a roster of bipeds,
   quadrupeds and wingeds — all with far worse topology than a snake.
4. The **65.07% single-bone case IS our mid-segment case.** Mid-segment rings
   at 64/0, joint rings ramping — the donor's distribution reproduced exactly.
5. **Where Zixxtrixx does branch:** the fork->prongs junction and the
   head/cap/eye cluster. With two prongs (per the concept) the fork is a
   valence-3 node. Either keep the prongs as rigid attachments, or push the
   blend region one ring away from the junction so no single vertex needs all
   three.
6. **Two influences add NO positional error over rigid.** The full
   `w0*(Sa v) + w1*(Sb v)` is evaluated in s128 and rounded **once** with
   `rescale(.,22)` (`zref_creature.hpp:288-293`, `GEOM.SKIN.md:90-98`).
   **A crack cannot come from the blend arithmetic.**
7. **The decisive point:** with shared boundary vertices a crack is impossible
   *regardless of weights* — a shared vertex has one position, computed once,
   used by triangles on both sides. **Weights buy the smooth silhouette;
   shared vertices buy the closed surface.** Different fixes for different
   symptoms, and the current model is missing both.

## E.5 Other donor facts bearing on this run — MEASURED

* **30 Hz keys, 60 Hz sim, each key held exactly 2 ticks, no interpolation
  ever** — `FINDINGS-S2:23` (*"No interpolation between keyframes
  (`renderer.d:1303` TODO). Ships and looks fine."*), `ANIMATION-NOTES.md:68-69`.
* **No blending, no cross-fade, no IK.** Masked by **64 authored slots**.
* **Quaternion storage `short[4]` = 8 B/bone/frame**, 12 B/frame header
  (`sxsk.d:19-24,61-64`). Identical to ours.
* **Creature LOD: NONE.** `OWNER_DOCKET.md:1643-1647` — *"Creatures had no LOD
  in the original (one mesh each), and the `lod` field ... is **not** an LOD
  level"*. This **corrects** `SACRIFICE-NOTES.md:639-642`. Our ladder is ours.
* Bones **11-32**, median 19. Mesh vertices **1,600-10,500** per creature;
  ring-vertex roster median **2,771**; ring counts median **152**
  (`ANIMATION-NOTES.md:556-558`, `:609`; `SACRIFICE-NOTES.md:64-66`).
  **Our proposed LOD0 at ~750 verts is a quarter of the donor's median.**
* **Caveat on all structural claims:** `06-DONOR-AND-SACENGINE.md:50-51` —
  sacengine's character model loader is *"slightly incomplete"*, and the
  influence census reads through that loader. The *existence* of multi-bone
  vertices does not depend on the unparsed section; the exact 65/32/2.51 split
  does.

---

# F. PROPOSED ANIMATION VOCABULARY

## F.1 The quaternion problem, first — MEASURED

**There is NO quaternion multiply anywhere in this repository.** The complete
quat surface is:

* `quat16_identity()` — `zref_creature.hpp:75`
* `quat16_quantize(w,x,y,z)` — `:85`, impl `creature_core.cpp:29-42`
* `quat16_axis_angle(ax,ay,az,half_sin,half_cos)` — `:92`, impl `:43-47`
* `quat16_to_mat3(q, out, L)` — `:114`, impl `:49-68`

No Hamilton product, no slerp, no nlerp, no compose. **NOT PRESENT.**
(`zhao_geom_pose_decode.sv:34` mentions "ONE multiply engine" — that is the
3x4 *matrix* multiply, not a quaternion product.)

**That is precisely why the current attack pops.** `zixxtrixx.h:342-343`:
```cpp
      c.quats[base + kBSpine0 + j] =
          (pitch > 600 || pitch < -600) ? quat_z(pitch) : quat_y(lat);
```
A joint switches **axis** on a threshold. At the crossing the rotation jumps
from `Ry(lat)` to `Rz(+-600)` in a single 30 Hz key with no interpolation.

### The minimal deterministic integer helper

Lanes are S 1.0.14 (`kQuatOne = 16384`, `zref_creature.hpp:72`). A product of
two lanes is S2.0.28; **one** `rescale(.,14)` lands back on a lane, giving the
single-rounding law (qformats §3 / A3b) with no intermediate quantisation:

```cpp
// Hamilton product of two quat16s. 16 s32 products, |sum| < 4*2^28 < 2^31,
// exact in s64. ONE rescale per lane, then hemisphere-canonicalise.
quat16 quat16_mul(const quat16& a, const quat16& b) {
  const int64_t aw=a.q[0], ax=a.q[1], ay=a.q[2], az=a.q[3];
  const int64_t bw=b.q[0], bx=b.q[1], by=b.q[2], bz=b.q[3];
  const int64_t w = aw*bw - ax*bx - ay*by - az*bz;
  const int64_t x = aw*bx + ax*bw + ay*bz - az*by;
  const int64_t y = aw*by - ax*bz + ay*bw + az*bx;
  const int64_t z = aw*bz + ax*by - ay*bx + az*bw;
  // rescale(.,14): S2.0.28 -> S1.0.14 lane, round-half-up, saturate,
  // then reuse the existing hemisphere canonicalisation.
}
```

It belongs in `creature_core.cpp` beside `quat16_axis_angle`, it is
**authoring-only** (clip construction), it touches **no silicon and no
runtime**, and it should be tested against a double oracle over the same
3,600-rotation sweep `tests/geometry/creature_core.cpp` already uses.

**Do NOT renormalise** — `creature_rules.md` §2.2 / `zref_creature.hpp:105-113`
froze "no renormalisation" for the decode. Composing 3-4 quantised quats will
drift the norm; measure it, and if it matters, compose in fx16 and quantise
**once** at the end rather than quantising each factor.

**Note also, MEASURED:** bone rest rotations are **identity** — `Bone` is
`{parent, tx, ty, tz}`, a pure translation chain (`zref_creature.hpp:142`;
consequence documented at `zixxtrixx.h:28-31`). Every non-quarter-turn bind
angle — the prong splay, the S stance, the head tilt — must be **baked into
every key of every clip**. With `quat16_mul` this becomes
`quat_mul(bind_q[b], anim_q[b])` and stops being an authoring tax.

## F.2 Caterpillar locomotion — vertical and longitudinal

Replace the lateral yaw entirely. Per spine joint `j`, with phase
`ph_j = f*(65536/keys) - j*(waves*65536/segs)`:

```
pitch_j = A_pitch * sin(ph_j)                // about Z: the arch. DOMINANT.
roll_j  = A_roll  * sin(ph_j + quarter turn) // secondary life, A_roll ~ A_pitch/6
yaw_j   = A_yaw   * sin(ph_j / 2)            // sway, A_yaw ~ A_pitch/8
q_j     = quat_mul(quat_mul(quat_z(pitch_j), quat_y(yaw_j)), quat_x(roll_j))
```

Fixed composition order (Z then Y then X), **every frame, every joint, no
thresholds** — that alone removes the pop.

* **Amplitude.** Must read at 384x240. The body is ~19-36 px thick, so an
  arch lifting the mid-body by **one full body diameter** is ~36 px and
  unmissable. Working target: peak mid-body rise 1.0-1.5x body diameter.
* **Longitudinal bunching.** `Bone` has no scale lane and `SCALE` is a Loom
  node, not a clip lane — so "compress and release" must come from **arching**
  (an arch shortens the ground projection) plus **root displacement**. `Clip`
  already carries `root` as 3 x fx16 per frame (`zref_creature.hpp:191-198`).
  **Use it.** Drive `root.x` with a non-uniform per-cycle advance so the
  animal surges when the arch passes rather than sliding at constant speed —
  that is the anti-foot-slide term and it is free.
* **Ground contact.** `kSlitherSpeed = 7` mm/frame today (`zixxtrixx.h:154`)
  is root translation in the *reel*, not in the clip. Moving it into
  `Clip.root` makes the gait self-contained and loopable.
* **Head hold.** Keep the idea at `zixxtrixx.h:145` (`kSlitherHeadHold`) but
  **compose** it rather than replacing an axis: the head counter-pitches to
  stay level while the neck arches under it.
* **Keys.** 40 keys ~ 1.33 s per cycle at 30 Hz. Two full cycles must be
  visible in the website preview (MODELINGGUIDE:285).

## F.3 Triple salto mortale, ENDING AS A STRAIGHT SPEAR STRAIGHT DOWN

Fabian's words, MODELINGGUIDE:6 — *"salto up, become like a straight spear and
smash down with real power."* This is the detail the distilled workplan lost,
and it dictates the last third of the clip: **the terminal pose is the whole
animal straight, vertical, prongs down.**

Proposed shape, ~110 keys ~ 3.7 s at 30 Hz:

| phase | keys | what the body does |
|---|---:|---|
| **coil** | 0-14 | front braces, rear gathers; total chain curvature ramps to ~+120 deg (a tight C behind the head) |
| **loop 1** | 14-38 | chain sweeps through -360 deg; rear-weighted, head is the pivot |
| **loop 2** | 38-58 | faster (-360 deg in 20 keys vs 24) |
| **loop 3** | 58-74 | fastest (-360 deg in 16 keys) — the committed rotation |
| **STRAIGHTEN** | 74-84 | **every joint drives to zero simultaneously.** The S unwinds into a rigid vertical line, prongs down, above and in front of the head. This is the spear. |
| **impact** | 84-86 | `kEvAttack` at 84. Hold 2-3 keys — the hit-stop. |
| **recoil** | 86-96 | a fast shudder travelling head-ward |
| **settle** | 96-110 | back to the S stance; **last pose must equal frame 0** |

Two things worth stating plainly:

1. **The straighten is the whole point and it is the easy part** — it is
   "curve every joint to 0 over 10 keys". The hard part is arriving at loop 3's
   exit with the chain wound to exactly `-3 x 360 deg` so that zeroing it
   produces a *vertical* spear rather than a diagonal one. Author the total
   accumulated curvature as a **single curve** and distribute it per joint by
   weight, as `build_attack` already does (`zixxtrixx.h:333-336`) — **but sum
   the weights and normalise.** The current code's accumulated rotation is
   `kAttackArc * sum(w) = 5.5x` the per-joint value; that arithmetic trap is
   documented at `zixxtrixx.h:146-152` and cost the first pass an iteration.
2. **`Clip.root` must lift and drop the animal.** A somersault that never
   leaves the ground reads as a wriggle. Root Y up through loops 1-2, down hard
   through the straighten. Free; already in the format.

**Prototype both variants** the brief asks for (head-as-pivot vs
whole-body-wheel) and choose on readability at 384x240, not realism.

**Pose-cache pressure — INFERRED, flag it:** the decoded-pose cache holds
**<=128 (type, clip, frame) tuples ~ 192 KiB** (`GEOM.POSE.md:16`,
`creature_rules.md:83-88`). A 110-frame attack played by 20 instances at
different phases occupies up to 20 tuples for one clip, versus 48 keys today.
Not fatal, but a battle full of Zixxtrixx mid-somersault is a new worst case,
and `cache_hits`/`cache_misses` (`GEOM.POSE.md:84`) is the metric to watch.

## F.4 Falling flail loop

~48 keys, seamless. Deliberately asymmetric — the brief forbids clean sine
motion (MODELINGGUIDE:357).

```
pitch_j = A1*sin(ph_j)       + A2*sin(2*ph_j + 1/3)   // two incommensurate terms
yaw_j   = A3*sin(ph_j + 1/2) + A4*sin(3*ph_j)
roll_j  = A5*sin(ph_j * 2/3)                          // corkscrew, 2:3 vs pitch
```
Choose harmonic ratios so the *whole* pattern closes over 48 keys but no
single joint looks periodic. Head and tail counter-rotate (opposite sign on
`A3` at `j=0` vs `j=n`). Tail blades splay by animating the two prong bones in
antiphase. Compose with `quat16_mul`, same fixed order.

## F.5 30 Hz held poses vs 60 Hz presentation interpolation

MODELINGGUIDE §8 asks for an A/B. **Do the axis fix and the camera slowdown
FIRST** — INFERRED, but on strong evidence: the current pop is an *axis
discontinuity* (F.1), not an interpolation deficit, and the current camera
completes a full revolution in 3.2 s (H.2). Both mask the question. The donor
shipped 30 Hz held with no interpolation and it *"looks fine"*
(`FINDINGS-S2:23`) — but the donor never authored a triple somersault.

If interpolation is still wanted after the fix, `quat16_mul` is **not** what
it needs — an nlerp is: lerp the four lanes in fx16, one `isqrt`-based
normalise, quantise once. Presentation only; gameplay truth stays at 30 Hz. A
per-clip flag is acceptable (locomotion off, attack and fall on).

---

# G. COSTS

Against the real limits: meshlet <=64 verts / <=126 tris
(`zref_creature.hpp:300-301`); <=32 bones (`creature_rules.md:44`); 24 MB
meshlet+LOD+animation pool and 192 KiB pose cache (`creature_rules.md:79-88`).

## G.1 Meshlet packing law — COMPUTED by replicating `build_ring_part`

Greedy fill per `creature_core.cpp:356-365`, continuous 22-ring shell:

| sides | meshlets | verts | tris | rings per meshlet |
|---:|---:|---:|---:|---:|
| 6 | 3 | 144 | 252 | 10 |
| 8 | 3 | 192 | 336 | 8 |
| 10 | 5 | 260 | 420 | 6 |
| **12** | **6** | **324** | **504** | **5** |
| 14 | 7 | 392 | 588 | 4 |

**At 12 sides the vertex cap binds before the triangle cap** (5 rings = 60
verts / 96 tris) — a 94% vertex fill and 76% triangle fill. Good packing, no
waste. **12 sides is the sweet spot; 14 drops to 4 rings/meshlet.**

## G.2 Proposed LOD0, 12 body sides, continuous shell — COMPUTED

| part | rings x sides | meshlets | verts | tris |
|---|---|---:|---:|---:|
| body + neck, **one continuous chain** | 24 x 12 | 6 | 348 | 552 |
| head shell (needs a non-circular profile) | 9 x 14 | 3 | 155 | 238 |
| eye rim x2 | 3 x 10 | 2 | 62 | 100 |
| eyeball x2 | 4 x 10 | 2 | 84 | 160 |
| tail blade x2 | 6 x 8 | 2 | 98 | 176 |
| **LOD0 TOTAL** | | **15** | **747** | **1,226** |
| *current model, same formula* | | *31* | *451* | *618* |

**Fifteen meshlets against thirty-one.** The continuous shell is *fewer*
meshlets than the segmented one despite 66% more vertices, because the current
model's 10 body parts + 8 ridge parts are 18 two-ring meshlets averaging
12 verts each — 19% of the vertex cap. **Every one of the 31 meshlets today is
a separate draw of a 12-vertex stub.**

Against the donor: **747 verts vs a roster median of 2,771** ring-vertices
(`ANIMATION-NOTES.md:558`). We are at 27% of Sacrifice's median creature.
There is room.

## G.3 Bones — COMPUTED

| | current | proposed |
|---|---:|---:|
| root | 1 | 1 |
| head | 1 | 1 |
| spine | 10 | **14** (finer bend for the salto) |
| fork | 1 | 1 |
| prongs | 3 | **2** (concept) |
| eyes | 2 | 2 |
| skull cap | **1** | **0** — texture |
| dorsal ridge | **8** | **0** — texture |
| **total** | **28 / 32** | **21 / 32** |

**The texture lane buys back 9 bones and spends 4 of them on bend smoothness,
leaving 11 spare.** That is the strongest budget argument for doing the
texture pipeline in this run rather than deferring it.

## G.4 Animation bytes — COMPUTED

Format: 12 B root + 8 B/bone/frame (`creature_rules.md:66-69`). At 21 bones:
**180 B/key.**

| clip | keys | bytes |
|---|---:|---:|
| caterpillar locomotion | 40 | 7,200 |
| triple salto (ends as spear) | 110 | 19,800 |
| falling flail loop | 48 | 8,640 |
| idle / stance | 32 | 5,760 |
| **total** | **230** | **41,400 B ~ 40 KiB** |

*(current: 28 bones x 80 keys = 18,880 B)*

**40 KiB against a 24 MB pool is 0.17%.** Animation bytes are not a constraint
and never will be for one creature. Sixty creatures at this budget is 2.4 MB —
10% of the pool.

## G.5 Texture pages — ESTIMATED (INFERRED; no tool exists to measure)

Following the donor's vertical-atlas shape (`OWNER_DOCKET.md:1330-1335`) and
the `Tileset` container (64x64 CLUT8 tiles + 256 x RGB565 palette):

| page | size | CLUT8 bytes | + mips (x4/3) | palette |
|---|---|---:|---:|---:|
| body (long strip, V along the animal) | 128 x 512 | 65,536 | 87,381 | 512 |
| head / throat | 256 x 256 | 65,536 | 87,381 | 512 |
| tail blades | 128 x 128 | 16,384 | 21,845 | 512 |
| eyes | 64 x 64 | 4,096 | 5,461 | 512 |
| **total** | | **151,552** | **~202,068 B ~ 197 KiB** | 2,048 |

**~200 KiB is 0.8% of the 24 MB pool.** Comfortably affordable, and CLUT8 at
one byte per texel is 2x cheaper than RGB565 direct — worth measuring both as
MODELINGGUIDE:189 asks, but CLUT8 is the right default for flat crayon
regions.

**Caveat, MEASURED:** `SkinVertex.u/v` are `uint8_t`, capping a page at 256
texels per axis *as addressed from the vertex*. A 128x512 body strip needs
either a 16-bit tool-side UV that the packer narrows per meshlet, or the page
split along the chain. Flag it before authoring.

## G.6 Pose cache

192 KiB = 128 tuples x 32 bones x 48 B, exactly (`GEOM.POSE.md:16`). At 21
bones a tuple is 1,008 B, so the *bytes* are fine; the **tuple count** is the
scarce thing, and F.3's 110-key attack is the new pressure. See F.3.

## G.7 The LOD ladder — one new obligation, MEASURED

`compile_creature`'s micro rung decimates automatically:
`creature_core.cpp:522-529` keeps *"every 2nd ring ... first and last always"*
and halves segments (min 3). **For a continuous weighted chain this will drop
alternate rings including joint rings**, which can leave a blend ramp with a
hole in it and re-open the exact problem we are fixing. LOD1/LOD2 must either
(a) pin joint rings as non-droppable, or (b) re-derive the weight ramp after
decimation. **This is a real new obligation the segmented model never had.**

---

# H. THE PRESENTATION LANE

## H.1 How the reel produces frames — MEASURED

**The C++ reel writes exactly one format and it is not an image.**
`tools/reel/zhao_reel.cpp:82-95`, `write_rgb` — an 8-byte header (`u32 w LE`,
`u32 h LE`) then `w*h*3` RGB888 bytes, one file per frame as `%04u.rgb`
(`:2112-2116`), plus `palette.rgb` (`:2149-2152`) and `meta.txt` (`:2198-2201`).

**PNG / GIF / PPM / BMP / WebP / WebM / MP4 / APNG / ffmpeg in the C++ reel:
NOT PRESENT.** Grep returns only prose in comments.

**The RGB888 bytes carry RGB565 content.** The canvas is 5-6-5
(`zref_render.hpp:111`) and the reel bit-replicates it back out at
`zhao_reel.cpp:99-104` (`unpack565`). So even a "full colour" export is
65,536-colour content in a 24-bit container — which is honest, and is exactly
what the brief means by *"preserve exact RGB565-derived frames"*.

**Can it emit full-colour PNG today? YES — downstream, in Python, and it
already does.** `Upheaval/website/tools/topng.py:49-67` hand-rolls a genuine
24-bit truecolour PNG on stdlib zlib (`IHDR ... 8, 2` = depth 8, colour type 2,
filter 0 per row, *"keeps pixel art byte-exact"*). Verified on disk:

| file | size | bytes |
|---|---|---:|
| `public/renders/zixxtrixx-slither.png` | 1152x720 (3x NN) | 26,195 |
| `public/renders/zixxtrixx-strike.png` | 1152x720 | 25,689 |
| `public/renders/native/zixxtrixx-slither.png` | **384x240 native** | 11,528 |
| `public/renders/native/zixxtrixx-strike.png` | 384x240 | 10,708 |
| `public/renders/zixxtrixx-slither.gif` | | 870,759 |
| `public/renders/zixxtrixx-strike.gif` | | 1,236,117 |

**Full-colour PNG frame output is already a solved problem.** MODELINGGUIDE:226
is satisfiable today with zero new code.

## H.2 `sub.orbit` — MEASURED

**It is a `bool`. It has no units. And it spins the WORLD, not the camera.**

`zhao_reel.cpp:977-980`:
```cpp
  // orbit: yaw the world by f * (65536/frames) per frame - one exact turn
  // per loop when frames divides 65536 (the integer step keeps the loop
  // seamless); the sky rotates with it so the world-fixed sun sweeps round
  bool orbit = false;
```
Consumed twice per frame — the view matrix (`:1986-1991`) and the sky dome
(`:2024-2028`), both computing `theta = f * 65536 / frames` and
post-multiplying `rot_world_yaw(theta)` (`:243-251`). Both must agree or the
sky slides off the terrain.

**Consequence, and it explains a knob nobody would guess:** because the yaw is
about the *world origin*, a creature must be offset by its own body centre or
it orbits about its own head — `zixxtrixx.h:161-171`, `kBodyCentreX = +1387`.

**Revolutions per clip — COMPUTED** (GIF rate `FPS = 20`, `togif.py:42`):

| subject | frames | duration | revolutions |
|---|---:|---:|---:|
| `zixxtrixx-slither` | 64 | **3.2 s** | **1.00** |
| `zixxtrixx-strike` | 96 | **4.8 s** | **1.00** |

**The camera completes a full revolution in 3.2 seconds over exactly one gait
cycle.** That is the "absurdly fast orbit" the brief names, quantified.

## H.3 How to express a slow orbit — the seam is small

Today `orbit` is all-or-nothing: exactly one turn over `frames`. **NOT
PRESENT:** any azimuth offset, start angle, revolutions count, partial orbit,
look-at target, camera keyframe track, or FOV-in-degrees.

Minimal change: replace `bool orbit` with a rational `orbit_num/orbit_den`,
and change both consumers to
`theta = f * 65536 * num / (frames * den)`. Then:

* **beauty orbit, one revolution over 12-16 s** at 20 fps = **240-320 frames**
  with `orbit = 1 turn`. At 30 Hz keys x 2 that is 120-160 keys — **3-4
  caterpillar cycles per revolution**, satisfying MODELINGGUIDE:378
  ("several complete animation cycles per revolution") directly.
* **fixed camera is already free:** `orbit` defaults to `false`
  (`zhao_reel.cpp:980`) and fixed-camera creature subjects already ship
  (`subject_creaturepop()`, `:3003-3009`). **Delete one line.**

Camera knobs today (MEASURED, `cam_pitch` at `zhao_reel.cpp:253-289`):
`cam_k` (Q16.16 projection scale, **not** an angle; zixxtrixx uses 240000),
`cam_eye`/`cam_dist` (**whole metres** — `E = eye_m << 16`; no sub-metre
placement), `cam_ps`/`cam_pc` (Q16.16 sin/cos pitch pair, default ~26 deg
down), `cam_bias`, `shake[]`. A `cam_pull` lerp exists (`:1827-1836`), gated
on creature subjects only. The zixxtrixx aim point is
`13 - 0.4877*8 ~ 9.1 m` (`:2924-2926`).

The five presentation subjects MODELINGGUIDE §10 asks for are therefore
**four `orbit = false` copies plus one slow-orbit copy** of the existing
subject factory, once `orbit` becomes rational.

## H.4 WebM / WebP instead of GIF — what it takes

**GIF is mandatory in exactly one file.** `togif.py:126-137,171-173` hardcodes
the ffmpeg call and the `.gif` names. Everything else is format-agnostic:

* `assemble.py` performs **two** checks — the file exists on disk (`:62`,
  hard-fail at `:64-68`) and `MAX_TABS = 6` (`:76-81`) — then emits a bare
  `<img src="..." loading="lazy">` (`:97-99`). **No extension check, no MIME
  check.**
* `deploy.ps1` never touches renders — `assemble.py`, the `noindex` gate
  (`:50-52`), `wrangler pages deploy` (`:61-62`).

Therefore:

* **Animated WebP or APNG: a new encoder script plus a `creatures.json` edit.
  Zero changes to the site generator.** Cheapest route to full colour, and
  `<img>`-compatible.
* **WebM/MP4: additionally needs `assemble.py` to emit `<video>`** with
  `autoplay muted loop playsinline` and a poster. Small, but a template change.
* Keep `style.css:106` `image-rendering: pixelated` on whatever replaces it —
  a smoothed upscale misrepresents the hardware.
* Keep the **decode-and-compare-byte-for-byte** verification `togif.py:136-137`
  performs. It is why the shipped GIFs are provably palette-exact, and the
  discipline should survive the format change.

## H.5 Two lane defects found in passing — MEASURED

1. **`--check` never validates the zixxtrixx subjects.** The `--check` list
   (`zhao_reel.cpp:3094-3123`) runs 28 subjects and ends at
   `subject_creaturepop()`. `subject_zixx_slither()` and
   `subject_zixx_strike()` are **absent**, despite both carrying pinned golden
   CRCs (`0x46759455` at `:2952`, `0xAB9FE046` at `:2989`). `subject_orbit()`
   is likewise missing. **The two shipped creature subjects have goldens the
   regression suite never checks.**
2. **Six PNGs sit in `public/renders/` that `.gitignore:14-16` says should not
   be there** (*"Only the encoded GIF belongs in public/renders/"*) and that
   neither `creatures.json` nor `index.html` references — ~74 KB of dead
   weight in the deploy. `togif.py:176-181` writes them anyway.

---

# Recommendations

Ordered by leverage; all evidence-backed above.

1. **Add a per-ring `{b0, b1, w0}` lane to `RingSpec` and emit blended
   vertices from `build_ring_part`.** ~20 lines, tooling only, no silicon.
   This is the crack fix. Add the six validators from B.5, including a
   max-bend pose sweep. (B)
2. **Delete `quant_shade` from `creature_sim.cpp:321-326`, or make it
   opt-in.** It is a GIF accommodation living permanently in the reference
   renderer. Same for the terrain palette ladder at `terrain.cpp:462-464`. (D)
3. **Fix the lighting before touching a single authored colour.** Half the
   body sits flat at the 0.25 ambient floor with zero form. Add a *coloured*
   fill/ambient term and raise the floor. Then reset `kPink` and `kYellow` to
   the sheet values and re-measure. Do not push saturation again. (D)
4. **Build the PNG -> CLUT8 + palette + mip converter.** It is the one missing
   artefact; both ends of the texture path already ship. It pays for itself in
   9 bones and 14 parts. (C, G.3)
5. **Add `quat16_mul`** beside `quat16_axis_angle`, authoring-only, tested
   against a double oracle. Compose every joint's pitch/yaw/roll in a fixed
   order every frame. The threshold switch at `zixxtrixx.h:342-343` goes. (F)
6. **Make `orbit` rational** and split the presentation subjects. A slow orbit
   is 240-320 frames at one turn; a fixed camera is deleting one line. (H)
7. **Two prongs, twelve sides, flat blades.** The concept says two; twelve is
   the meshlet sweet spot; blades need an offset/elliptical `RingSpec` or a
   custom-mesh tooling path, **because neither exists today**. (A, G.1)
8. **Correct the record:** delete `(donor law)` from `zref_creature.hpp:338`,
   rewrite `01-RING-CONSTRUCTION.md:58` and
   `02-SKELETON-AND-SKINNING.md:23-30, 55-57`, and update
   `creature_rules.md:120-122` — the donor is **34.92% multi-bone** and its
   distributions **were** measured (`OWNER_DOCKET.md:1601-1607`). That
   mis-attribution is what authored the cracks. (E)
9. **Wire the zixxtrixx subjects into `--check`.** They have goldens nobody
   verifies. (H.5)
10. **Doc drift:** four files still state the pre-2026-08-18 green dither
    amplitude; `RASTER.RESOLVE.md` contradicts itself between lines 62 and 74;
    `zhao_raster_fragment.sv:69` asserts the TMU does not exist. Shipping code
    is right in all three cases. (D.1, C.1)

---

# Files Created in This Directory

None beyond this report.

---

# Files Examined

**Concept art (opened as images, measured):**
`Upheaval/creature/Zixxtrixx/Concept/Side.png`, `Front.png`

**Authoritative task text:**
`zhaozhou/reports/MODELINGGUIDE`;
`Upheaval/creature/Zixxtrixx/REMAKE-BRIEF.md`;
`runs/CLAUDE-RUNS/RUN-20260826-1615-.../SPEC_v1.md`

**The model and the lane:**
`zhaozhou/tools/reel/zixxtrixx.h` (all 539 lines);
`zhaozhou/tools/reel/zhao_reel.cpp`;
`zhaozhou/spec/creature_rules.md` (all 283 lines)

**Reference implementation:**
`reference/include/zref/zref_creature.hpp`;
`reference/src/zcreature/creature_core.cpp`, `creature_sim.cpp`;
`reference/src/zrender/internal.hpp`, `terrain.cpp`, `rast.cpp`,
`resolve.cpp`, `render_frame.cpp`;
`reference/include/zref/zref_render.hpp`, `zref_texture.hpp`,
`zref_tilestore.hpp`, `zref_tileresolve.hpp`;
`tests/geometry/creature_core.cpp` (the `clamp_3to2` call sites)

**Contracts:**
`GEOM.SKIN.md`, `GEOM.POSE.md`, `TEXTURE.TMU.md`, `RASTER.RESOLVE.md`,
`VIDEO.SCANOUT.md`, `SW.TOOLS.ASSET.md`

**RTL:**
`fpga/rtl/texture/zhao_texture_tmu.sv`, `zhao_texture_cache.sv`;
`fpga/rtl/raster/zhao_raster_resolve.sv`, `zhao_raster_quant.sv`,
`zhao_raster_fragment.sv`, `zhao_raster_tilestore.sv`, `zhao_raster_blend.sv`;
`fpga/rtl/geometry/zhao_geom_pose_decode.sv`

**Donor reconnaissance:**
`Upheaval/creature/01-` through `06-DONOR-AND-SACENGINE.md`, `README.md`;
`Upheaval/docs/SACRIFICE-NOTES.md`, `ANIMATION-NOTES.md`;
`zhaozhou/docs/OWNER_DOCKET.md` (§1285-1345, §1597-1662 — the asset census);
`runs/CLAUDE-RUNS/RUN-20260816-0046-.../FINDINGS-S2-sacengine-creatures.md`;
`runs/CLAUDE-RUNS/RUN-20260823-0937-geom-skin-dsp-rearchitecture/SPEC_v1.md`;
`runs/CLAUDE-RUNS/RUN-20260826-0617-zixxtrixx-first-creature-model/TASK_LOG.md`

**Website lane:**
`Upheaval/website/tools/topng.py`, `togif.py`, `assemble.py`;
`Upheaval/website/creatures.json`, `deploy.ps1`, `public/style.css`,
`public/index.html`, `public/renders/`, `scratch-reel/*.txt`
