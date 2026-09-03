# The boring 3D fundamentals audit

**Owner brief, 2026-09-03, after the normal-map investigation found that
production terrain had no lighting at all.** The owner's framing, which is the
useful part:

> The pattern is not "we forgot twenty expensive GPU features." It is more
> specific: **we built impressive endpoints and sometimes forgot the boring
> organ connecting them.** Normal maps exposed this because the terrain already
> had a normal generator — but nothing used those normals to light anything.
> The same species of gap exists elsewhere.

And the reassurance that goes with it, which should not be lost either:

> Most of the newly identified gaps are **connective tissue, not another giant
> computation island.** The only one with real capacity teeth is lighting, and
> even there the correct answer is bounded per-vertex work and measured
> sequencing — not a surprise modern shader core.

---

## THE RULE THIS FILE EXISTS TO ENFORCE

Every capability gets one row across the **complete chain**:

    authoring bytes -> cartridge page type -> HPS owner -> upload/residency
    -> command or resource handle -> producing block -> transport packet
    -> consuming block -> cache-coherence rule -> reference implementation
    -> differential test -> production manifest entry

**A row is RED if even one arrow is `???`.** Not "unfinished" — *unowned*. The
difference matters: the ledger already knows it lacks `GEOM.MESHFETCH`,
`GEOM.LOOM`, the particle blocks and `POST.COMPOSITE`. Those are unfinished and
visible. The dangerous ones are the arrows with **no named owner**, because
nothing in any status report is counting them.

Every one of today's discoveries would have shown as a red row immediately:

* normals produced but never consumed;
* animation bytes stored but no uploader;
* material IDs carried but never resolved;
* index offsets emitted but never consumed;
* fog supposedly applied by a block with no colour input;
* cache invalidation existing but no publisher driving it.

---

## THE RED ROWS

### R1 — mesh index → triangle assembly · **CRITICAL, NO OWNER** · verified

The most basic omission in the list, and confirmed against the tree rather than
taken on trust:

* `GEOM.MESHFETCH`'s descriptor carries `vertex_offset`, `index_offset`,
  `vertex_count`, `triangle_count`, `material_id`;
* `zhao_geom_vdecode.sv` accepts **neither `index_offset` nor
  `triangle_count`** — grep returns nothing. It is the vertex side only;
* `zhao_geom_setup.sv` expects a **complete triangle**: `tri_ax_i`, and the two
  other corners;
* **`tri_ax_i` is driven only from `zhao_shell_top.sv` and
  `zhao_geom_bin_pipe.sv`** — i.e. from a harness. The binner's own `tri_idx_r`
  is a triangle index inside a tile-reference list, which is a different thing;
* the ledger has **zero** blocks matching `GEOM.(ASSEMBLE|INDEX|TRI)`.

So the authoritative path is:

    MESHFETCH
      |- vertex_offset --> VDECODE --> SKIN/PROJECT
      '- index_offset  --> ??? -----> triangle A/B/C --> CLIP/SETUP

Nobody owns: fetching three local u8 indices per triangle, selecting the
corresponding decoded/projected vertices, assembling A/B/C, carrying material
and attributes alongside, issuing all `triangle_count` triangles, or Duo-view
projected-vertex selection.

**This is finite, not terrifying** — a streaming index walker is *read index
triplet → fetch three cached projected vertices → emit triangle descriptor*.
But it needs an owner **now**, either by expanding `GEOM.MESHFETCH` to own
topology walking or by creating `GEOM.ASSEMBLE`. **It must not be allowed to
emerge accidentally as miscellaneous logic inside `GEOM.PARAMBUF`.**

### R2 — the complete lighting route · **CRITICAL** · partly caught today

**Terrain**: caught this session. `TERRAIN.NORMALS` is UNIT_VERIFIED at 41,731
checks and nothing consumed its output; `TERRAIN.PROJECT` has no colour port;
the composed shell contains no terrain at all. Now assigned to
`TERRAIN.SHADE` — **contracted, oracle written, RTL not built**, and blocked on
the clamp decision in R6 below.

**Creatures and ordinary meshes**: the same seam is still open.
`GEOM.VDECODE` decodes a bind-space normal and says it travels a parallel
lighting path; `GEOM.SKIN` transforms **positions only**; the actual
`zhao_geom_project.sv` takes positions, view and source id — **no normal,
colour, light set, tint or fog**. `SKIN.NORM` and `CREATURE.LIGHT` are designed
in a report and are not built blocks.

**So the ledger's description of `GEOM.PROJECT` as "projection + lighting" is
aspirational. The source implements projection.**

The route that must become explicit:

    VDECODE normal -> rigid transform / two-weight SKIN.NORM -> world normal
                   -> GEOM/CREATURE.LIGHT -> vertex RGB
                   -> PROJECT -> CLIP -> ATTRSTEP -> fragment

**One common lighting vocabulary with different normal producers** — not
terrain, creatures and static objects each inventing their own final colour
law. This is the largest potential new expense and the most shareable; the
existing creature-light work already notes position skinning at ~12 clocks per
weighted vertex, and **the accumulator does not need ten permanent light
engines merely because ten light terms are legal.**

### R3 — material resolution · **CRITICAL, NO OWNER**

All the nouns exist and the verb does not:

    material_set + material_id  ->  ???  ->  sample_count, recipe, texture
                                             bases, palette bases, TMU modes,
                                             wrap/filter/mips, raster state,
                                             toon/ink flags

`DrawForm` carries a `material_set` handle, meshlet descriptors carry
`material_id`, and `TEXTURE.FRAGROB` expects all of the above **already
resolved** — it explicitly refuses to own combiner arithmetic. The surviving
TEXJOIN behaviour still returns sample 0 for every recipe.

Split into two, because they are different jobs:

* **`MATERIAL.RESOLVE`** — table lookup from `{material_set, material_id,
  quality tier}` to concrete bindings and recipe. **Ownerless today.**
* **`TEXTURE.COMBINE`** — arithmetic over the returned zero-to-three samples.
  Already anticipated by the island architecture as its own registered II=1
  pipeline.

Need not be expensive: a material record in local SDRAM, a small cache, a
registered lookup. But the format and the authority must exist.

### R4 — the cartridge has no generic texture/material story · **CRITICAL**

The `.zpak` resource-kind registry has programs, source maps, sky sets, terrain
pages, tone banks, island pages/tables, creature forms and clip banks. It has
**no generic texture-page or material-set kind** — yet creature parts claim
texture pages, terrain names tilesets, and `DrawForm` takes a material-set
handle.

One of these must be chosen deliberately:

* **A** — generic `TEXTURE_PAGE` + `MATERIAL_SET` + `MESH_STREAM` resources
* **B** — every family page contains its own texture/material/mesh subpages,
  with one common nested-resource layout
* **C** — a generic immutable `RESOURCE_BLOB` plus typed manifests

Any can work. **What cannot work is every asset compiler inventing where its
texture bytes live and expecting `MEM.UPLOAD`, `MATERIAL.RESOLVE` and
`TEXTURE.CACHE` to somehow agree.**

### R5 — upload is caught; **cache coherence is the missing second half**

`MEM.UPLOAD` now has a contract and a reference law (this session), with the
owner's two corrections applied: a fresh unpinned destination slot rather than
in-place overwrite, and the HPS source as a capability with a hard 32-bit
reachability rule. **No RTL yet.**

Two things remain:

1. **It must be the path for ALL immutable render assets** — animation banks,
   texture pages, palette pages, material tables, mesh descriptors,
   vertex/index streams, terrain pages, sky assets, Sunder resources — not an
   animation-only pipe.
2. **Publishing a new mapping must make every cache incapable of returning the
   old bytes.** `TEXTURE.CACHE` already has invalidate inputs and notes that
   uploaded palette pages otherwise leave stale data — **and nothing drove that
   invalidate.** The complete transaction:

       allocate fresh unpinned destination -> copy -> wait for every VRAM
       write to retire -> verify CRC -> invalidate old physical lines (or bind
       cache identity to the new generation) -> publish mapping atomically
       -> pin for READY frames

   Mainly making ownership and visibility **true rather than implied**.

### R6 — the depth pipeline disagrees with itself · **CRITICAL CORRECTNESS**

Verified independently this session: `zhao_geom_project.sv` emits `out_d_o`,
documented as **"Q16.16 1/w"**. **Twelve RTL files consume `invw24`.** No
`depth_profile` port exists anywhere in the tree, though the ABI now carries a
two-bit profile in `SetView`.

Not a capacity concern — a correctness one. One named stage must own: profile
selection, Q16.16 reciprocal → `invw24`, scale/shift/saturation, behind/far
handling, the matching ZRef function, and exact capture-visible profile
identity. **`GEOM.DEPTHQUANT`, or explicitly part of `GEOM.PROJECT`. Not
integration glue.**

`DEPTH_PROFILE_NEXT_STEPS` steps 5–6 are open and step 5 is described there as
"the only thing that was ever actually blocked"; the docket lists 1–4 done and
is silent on 5–6, so it reads closed. **It is not.**

### R7 — fog exists in law, not in wires · **HIGH**

The environment spec is mature — directional sun, ambient, tint, linear fog,
capture state. But `SetEnvironment` is a **reserved** command, not an
implemented one. `RASTER.FRAGMENT` says it does no fog because the incoming
vertex colour is already fogged — and `GEOM.PROJECT` has **no colour or
environment input** with which to have fogged it.

**The fog arithmetic is specified. The fog carrier is missing.**

And an unresolved visible-semantic decision that must be ruled **before the
colour packet freezes**: the general law is `lighting → fog → interpolate →
toon`, which can quantise fog into hard toon bands. The cel path probably wants
`unfogged lighting → interpolate → toon → texture → fog`.

### R8 — no frozen contact-shadow law · **HIGH VISUALLY, ~ZERO HARDWARE**

Full shadow maps are deliberately absent, which is defensible. What is missing
is a settled replacement for the basic job: **make a creature look attached to
the ground.** Especially for a game of floating islands and airborne creatures.

The proposed cheap law, to be frozen:

| range | shadow |
|---|---|
| near hero | projected low-poly hull or 8–16 vertex ellipse, conformed from a few terrain height taps |
| near army | 4–8 vertex blob |
| mid | tiny dark splat |
| far | none |

Rendered as ordinary transparent terrain-biased geometry through the main
renderer. **No shadow-map hardware, no new framebuffer, no shadow unit.** Can
cost essentially zero new raster hardware and make more perceptual difference
than several expensive material effects.

### R9 — local spell lighting is prose, not a contract · **IMPORTANT FOR THIS GAME**

`SetEnvironment` deliberately holds one directional sun and ambient, no dynamic
point lights. The creature report proposes an HPS spatial light catalogue,
stable top-K per creature, bounded sequenced accumulation, weak lights folded
into coloured spill, huge flashes global. **Still a report.**

> If giant magical fireballs bloom but cast no coloured light onto nearby
> creatures, terrain or structures, the effects can look pasted onto the world.

Three separate targets need an explicit decision:

| receiver | sensible v1 law |
|---|---|
| creatures / objects | bounded top-K diffuse lights |
| terrain | sun + broad spill / global flash; **not** every point light |
| particles / spells themselves | emissive/additive + existing glow tags |

### R10 — water and lava: a route, no material law · **OPTIONAL BUT UNRESOLVED**

`TWOD.PLANE` is explicitly **not** a general world-depth plane; its contract
says water/lava intersecting ordinary geometry must be triangles through the
main renderer. That is fine — no water hardware needed. But if the game wants
ponds, rivers or lava, freeze the cheap v1: low-poly surface, scrolling texture,
alpha or additive, optional vertex waves, depth test on with an **explicit**
depth-write policy, no reflection, no refraction, no screen-space copy, explicit
sorting rule.

**If the game does not need liquid surfaces, say so and forget them.** The
danger is the stale ledger phrase "water/lava" making us assume a solution
exists when the plane engine refuses the relevant depth behaviour.

---

## WHAT WE DID **NOT** FORGET

Real capabilities, not vague intentions: strict depth test and write; stencil;
alpha test/cutout; replace / alpha / additive / additive-modulated blending;
texture and vertex modulation; nearest and bilinear; CLUT4/CLUT8 and direct
colour; mip selection; repeat, clamp and mirror; sky, clouds and additive sun;
the frame transaction and previous-frame repeat; tile lifecycle and framebuffer
publication.

And these are **consciously excluded rather than forgotten**: MSAA, anisotropic
filtering, OIT, shadow maps, arbitrary shaders, a second unrestricted TMU,
general tangent-space normal maps, true near-plane polygon clipping.

Two riders on that list:

* the near-plane law **drops an entire triangle when any vertex is behind the
  eye**. Deliberate, but it needs a close-camera stress reel for giants, Zixx,
  beams and large terrain triangles, because it can visibly pop geometry.
* the terrain normal-map unit **does not generalise**. It is cheap precisely
  because a heightfield's tangent frame is world-axis-aligned. For cel-shaded
  creatures, **explicitly refuse general tangent-space normal maps in v1**
  rather than let someone infer that the terrain block generalises.

---

## PRIORITY ORDER (owner's)

1. Name and contract the mesh-index/triangle assembler. **(R1)**
2. Freeze material records, cartridge representation and `MATERIAL.RESOLVE`.
   **(R3, R4)**
3. Freeze the complete normal/lighting route for terrain, rigid meshes and
   creatures. **(R2)**
4. Finish the general `MEM.UPLOAD` transaction and resource/cache-coherence
   law. **(R5)**
5. Close the depth-profile and environment/fog seams. **(R6, R7)**
6. Adopt cheap geometry-based contact shadows. **(R8)**
7. Explicitly choose yes/no for local spell lighting, water, and non-terrain
   normal maps. **(R9, R10)**

**Then continue the texture-island recovery.**

Note that 1–7 are almost entirely *contract and format* work, which does not
need the Quartus toolchain and can therefore proceed alongside fits.
