# Contract — MATERIAL.RESOLVE (Material record resolution)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/texture/zhao_material_resolve.sv` — **BUILT 2026-09-19**
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

## The RTL, and the one ruling it waits on

**BUILT 2026-09-19** -- `fpga/rtl/texture/zhao_material_resolve.sv`, enforced by
`tests/texture/material_resolve_rtl_directed.cpp` (91 checks, differenced
against the oracle rather than restating it).

**THE CARTRIDGE QUESTION ABOVE WAS RULED ON 2026-09-03 AND THE LEDGER WENT ON
CITING IT AS A BLOCKER UNTIL 2026-09-19.** This file said `RULED: A` in its own
section above; `design/blocks.yml` said "BLOCKED ON A CARTRIDGE DECISION
(audit R4)" and recorded both tests as "PLANNED -- NOT WRITTEN" while the oracle
suite had been green for a fortnight. A contract and its ledger row disagreed
for sixteen days and nothing compared them.

**SLOT -> EXTENT WAS RULED ON 2026-09-19** -- `spec/memory_rules.md` §5f.1:
*a published slot is named by the handle index of the resource it holds*, so
the residency directory is `{index:24}` keyed with row
`{slot, base, extent, kind}`, and `zhao_mem_upload` publishes all five
(`publish_index_o`, `publish_slot_o`, `publish_base_o`, `publish_extent_o`,
`publish_tag_o`). Base and extent were never missing VALUES -- MEM.UPLOAD
already bounds-checked `req_vram_addr_i` and `req_len_i` against
`cfg_region_*` before writing a byte, then dropped them. The only genuinely
absent field was the KEY.

**What the block waited on, kept because it is the evidence for that ruling**,
and it was a different ruling from R4:

* the Memory ownership section above says records are read "from **local
  SDRAM**, in a region owned by the render resource arena and uploaded through
  `MEM.UPLOAD` like every other immutable asset". Both halves of that are true
  and neither yields an ADDRESS.
* `spec/memory_rules.md` 5f, under "It is a knob, and what is NOT decided
  here", leaves open "the pool's internal layout (descriptors vs index streams
  vs vertex records)". So no law turns a handle's 24-bit index into a base
  inside `RENDER.ASSET_POOL`.
* `MEM.UPLOAD`'s publication is `{publish_slot_o[7:0],
  publish_generation_o[15:0], publish_tag_o[7:0]}` -- a slot and a generation,
  carrying **neither a base address, nor an extent, nor a resource kind**.
* `spec/commands.zidl` has no command that publishes a material set; by D-2's
  design the route is the generic `.zpak` resource path.

**The ruling needed, stated so it could be made:** *for a resource kind
published by `MEM.UPLOAD`, what maps `{kind, handle index}` to `{base,
extent}` in local SDRAM?* One sentence in §5f plus two fields on that
publication closed it, and §5f.1 is that sentence.

**WHAT IS LEFT IS COMPOSITION, AND IT IS FOUR SEAMS RATHER THAN ONE RULING.**
`reports/OWNER-DOCKET-20260919.md` item 4 called the ruling "the last thing
between the texture island and sampling anything". That is too strong, and each
of the four remaining is an entry somebody had already written down:

1. **MEM.UPLOAD is composed nowhere.** `zhao_hps_arbiter` carries exactly two
   clients and both are taken in BOTH instances -- CMD.DMA and DEBUG.FRAMEBLIT
   in `zhao_shell_top_v2`, TERRAIN.CMD and TERRAIN.PAGELOADER in
   `zhao_console_core`'s `u_terr_hps_arb`. A third is an owner ruling, and core
   entry I27 already records it as one.
2. **The record fetch wants a third ENGINE1 requester.**
   `zhao_geom_mem_adapter` has exactly two, GEOM.MESHFETCH and GEOM.ASSETFETCH.
3. **The resolve REQUEST has no honest producer.** `cmd_draw_material_set_o`
   (core entry I41) and `geom_mf_job_*` (I36) are both boundaries while
   CMD.SCHEDULER's draw path is absent, and joining the two live wires that ARE
   present would pair meshlet N's triangles with meshlet M's material -- the
   join core entry I39 refuses by name, because GEOM.MESHFETCH's result
   register has moved on by the time the meshlet is offered.
4. **`tri_flat_request_i` wants the binding page's three fields besides**,
   which the section below says are not ours.

Until then the block's `dir_*` (residency directory write) and `mem_*`
(record fetch) ports are **real ports driven by nobody** -- the standing
`zhao_console_core` entry I39 gives GEOM.ASSEMBLE's descriptor fields. It is
therefore **not composed**: composing it would move core entry I20's
`tri_flat_request_i` boundary rather than close it, and a zero flat request is
a LEGAL profile, so a composer inventing plausible constants would produce a
picture and prove nothing.

### Three fields this contract implied were ours and are NOT

Found while writing the projection, against `zhao_render_texture_pkg.sv` and
`zhao_texture_binding_resolver_v2.sv`. `zhao_console_core` entry I20 said
"Every one of those fields is MATERIAL.RESOLVE's output"; that is too strong:

* `palette_slot`, `palette_generation` and `response_class` belong to the
  **binding page**. The binding resolver holds them in `binding_row_t` and
  takes the request's copies as **witnesses it checks**
  (`witness_mismatch_o`). A resolver that emitted them would manufacture that
  mismatch. This contract's record does carry `palette_base`, which is a
  different thing -- the CLUT's address, not the slot.
* `lod_q4_4` is excluded by this contract's own Purpose and exclusions
  section ("No LOD selection. It returns the mip policy; the sampler picks the
  level").
* `base_rgb`/`base_alpha` are vertex colour and the 224-bit
  `aux_surface_ctx` is terrain world context; zero is their legal non-terrain
  value.

### A narrowing the frozen layout contains, and the consumer already expects

`MaterialSample.binding_slot` is **u16**; the flat request's
`base_binding_selector` is **u8**. The RTL detects and counts the overflow on
`rsp_selector_overflow_o` rather than truncating, because the binding resolver
already has `req_selector_overflow_i` waiting for it. A silent truncation
would name binding 0 for slot 256 and sample the wrong page with every gate
green.

### `count_legal` is deliberately not implemented here

The recipe/count pairing is the **combiner's** law and it refuses and counts it.
Implementing it here would be two implementations of one rule. It is OBSERVED
without refusing, on `recipe_count_mismatch_o`.

### Owner ruling D-3 has a committed positive control

D-3's tag is a STRUCTURAL property -- the stale line is "structurally unable to
match" -- not a guarded state with a counter, so no legal stimulus can make the
shipping resolver fail its coherence case.
`tests/mutants/zhao_material_resolve_gen8tag_mutant.sv` narrows the tag to the
low 8 bits of the generation, which is the width a `handle32` carries and
exactly what a reader who stopped at `{index:24, generation:8}` would write.
Its driver has inverted polarity and passes on the stale hit. It does.

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
