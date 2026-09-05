# Contract — MATERIAL.RESOLVE (Material record resolution)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::material::resolve` — **WRITTEN 2026-09-05**, `reference/include/zref/zref_material_resolve.hpp`

## Purpose and exclusions

MATERIAL.RESOLVE turns `{material_set, material_id, quality_tier}` into the
concrete description a texture fetch needs.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R3.** The audit's
finding, in its own shape — all the nouns exist and the verb does not:

    material_set + material_id  ->  ???  ->  sample_count, recipe, texture
                                             bases, palette bases, TMU modes,
                                             wrap/filter/mips, raster state,
                                             toon/ink flags

`DrawForm` carries a `material_set` handle. Meshlet descriptors carry
`material_id`. `TEXTURE.FRAGROB` expects every one of those fields **already
resolved** and explicitly refuses to own material arithmetic. Nothing performs
the lookup.

**This block is deliberately ONLY the lookup.** The audit splits the job in two
because they are different machines:

| | what it is | where it lives |
|---|---|---|
| **MATERIAL.RESOLVE** | a table lookup and a small cache | **this contract** |
| **TEXTURE.COMBINE** | arithmetic over 0–3 returned samples | the island architecture's registered II=1 combiner |

**Exclusions, each a specific refusal:**

* **No combiner arithmetic.** The recipes are FRAGROB's downstream problem and
  the island brief already budgets a separate block for them.
* **No texture fetching.** It returns *bases and modes*; `TEXTURE.TMU` fetches.
* **No LOD selection.** It returns the mip policy; the sampler picks the level.
* **No authoring.** It reads a record; the asset compiler writes one.

## The record, and why it must be frozen before RTL

A material record resolves to exactly what FRAGROB's input packet needs, so the
two cannot drift:

    sample_count      u2    0..3, the ruling limit
    recipe            u3    the fixed combiner recipes
    recipe_weight     unit8
    for sample 0..2:
        binding_slot        generated width
        binding_generation  u8
        tmu_mode      u4    nearest/bilinear/CLUT/direct
        wrap          u2    repeat / clamp / mirror
        mip_policy    u2
    palette_base      u32   for CLUT modes
    raster_state      u32   carried to the triangle descriptor
    flags             u8    toon, ink, alpha-test

**Widths are indicative and the record is NOT frozen by this file.** Freezing
it is step 2 of the owner's priority order and belongs with the cartridge
decision below, because a record that the asset compiler cannot emit is not a
record.

## THE CARTRIDGE QUESTION — RULED, D-2, 2026-09-03

The `.zpak` resource-kind registry has programs, source maps, sky sets, terrain
pages, tone banks, island pages/tables, creature forms and clip banks. It has
**no generic texture-page or material-set kind** — while creature parts claim
texture pages, terrain names tilesets, and `DrawForm` takes a material-set
handle.

Three options, any of which can work:

* **A** — generic `TEXTURE_PAGE` + `MATERIAL_SET` + `MESH_STREAM` resources
* **B** — every family page carries its own texture/material/mesh subpages,
  under one common nested-resource layout
* **C** — a generic immutable `RESOURCE_BLOB` plus typed manifests

**RULED: A.** `spec/cartridge.md` §4a now allocates them:

| kind | resource | section type |
|---|---|---|
| **10** | `TEXTURE_PAGE` | `0x000E` |
| **11** | `MATERIAL_SET` | `0x000F` |
| **12** | `MESH_STREAM` | `0x0010` |

**So this block's input is no longer hypothetical.** A `MATERIAL_SET` is an
immutable table indexed by `material_id`, uploaded like every other resource,
and family pages **reference** it rather than embedding their own
interpretation. Palettes are a **subtype of `TEXTURE_PAGE`**, inheriting the
same publication and generation machinery instead of a private loader.

**The record layout inside `MATERIAL_SET` is still not frozen** — the draft
above is a draft, and the ABI generator owns the emitted constants.

## Input and output packet layouts

**In:** `{ material_set, material_id, quality_tier }`, ready/valid.
**Out:** the resolved record above, ready/valid, plus `hit`/`miss` evidence.

## Backpressure rules

Ready/valid. A miss stalls the requester rather than returning a default —
**there is no sensible default material.** A guessed material draws the wrong
surface confidently, which is worse than a stall.

## Memory ownership

Reads material records from **local SDRAM**, in a region owned by the render
resource arena and uploaded through `MEM.UPLOAD` like every other immutable
asset. Owns a **small direct-mapped cache** of recently resolved records.

**RULED, D-3, 2026-09-03: the cache tag includes the residency generation.**

    cache tag = physical line tag + residency generation

Publishing a new material table therefore makes every cached record from the
old one **structurally unable to match**. No flush is required for correctness;
an invalidate input remains legal for reclaiming space. The generation is the
existing **16-bit** residency generation, and **silent wrap is forbidden** — a
wrap requires an epoch transition and global invalidation.

## Q formats and rounding

None of its own.

## Latency (fixed or variable)

`variable`. A hit is a small fixed number of clocks; a miss takes the arbiter's
latency. **Materials are resolved per meshlet, not per fragment**, which is
what makes a small cache sufficient — the same material serves every triangle
of a meshlet and usually many meshlets.

## Overflow and malformed-input behaviour

* **A `material_id` past the set's count is REFUSED and counted.** Not clamped
  to zero: material 0 is a real material and drawing with it hides the bug.
* **A `material_set` handle that is not resident is a residency fault**, not a
  stall-forever. It is the frame-publication law's business, exactly as with
  animation banks: the frame is not published and the previous one repeats.
* **A record whose `sample_count > 3` is malformed** — the ruling limit is
  three — and is refused.

## Scalar reference function

**WRITTEN 2026-09-05** — both, in
`reference/include/zref/zref_material_resolve.hpp`. They could not be written
earlier: the record they return was not frozen. It is now
`zhao_abi::ZhMaterialRecord` (32 B), emitted by the ABI generator, and this
oracle reads it rather than restating it.

## Directed tests

**WRITTEN 2026-09-05** — `tests/texture/material_resolve_directed.cpp`, 32
checks, green. Every case this section named, with one refinement found while
writing them: **`sample_count == 4` cannot be tested, because the frozen layout
makes it unrepresentable.** It is two bits. A malformed record is therefore
exercised through the things the layout DOES permit — a set reserved control
bit and a non-zero reserved word — which is a better test than the one asked
for, because it checks a case that can actually occur.

The coherence case is the one that matters: a republish at a new generation is
followed by a resolve that MISSES and returns the new record, with **no flush
performed**, because D-3's tag is what makes the old line structurally
unmatchable and a flush would hide a tag bug rather than prevent one.

## Randomized differential tests

Planned, against the scalar model, with a coverage guard on the refusal classes.

## Integration capture cases

None on hardware.

## Synthesis / resource ceiling

Expected **low**: a small cache, a registered lookup, a legality compare. The
audit's estimate for the whole gap is "probably low–moderate". It has no
arithmetic and no wide datapath.

## Notes

The surviving TEXJOIN behaviour returns **sample 0 for every recipe**, and the
three-sample terrain recipes were absent from that RTL entirely. So when this
block starts returning real `sample_count` and `recipe` values, the combiner
must exist to consume them — **the two are one piece of work in two contracts**,
and shipping the resolver alone would make the machine confidently fetch
samples nothing combines.
