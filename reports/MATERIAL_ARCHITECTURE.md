# Bounded multi-sample materials — the TMU is not nerfed, the MATERIAL RULE was

Ruling 8, relayed from the reviewer via Fabian 2026-08-30, from the question:
*"with the TMU redesigns, aren't we nerfing it? We can't do what Sacrifice does
now."*

Companion to `reports/RENDERER_ARCHITECTURE.md` (rulings 1–7). Read that first
for the renderer pipeline; this file is about how many samples a fragment may
take and what combines them.

**Integration status, 2026-09-13:** the discussion before R9 records the R8
reasoning and uses TEXJOIN as its historical name for fragment material
ownership. The selected Texture V3 composition does not install TEXJOIN as a
second owner: `zhao_texture_v3own` owns the fragment lifetime and its owner-keyed
material row feeds the versioned combiner. The frozen R9 section below is the
normative Packet-B arithmetic, count, alpha, index, status, and cadence law. This
clarification changes no protected shell and makes no implementation or resource
claim.

---

## First, the direct answer

**The TMU redesign is not a nerf.** `zhao_texture_tmu_pipe` keeps every sampling
operation the serial block had — CLUT8, CLUT4, RGB565, ARGB1555, ARGB4444,
nearest and direct-colour bilinear, mip selection, repeat/clamp/mirror, raw
palette index output, and the identical fixed-point laws. It has the same request
and result ports, runs against the same untouched `zref::Tmu`, and any difference
in a sampled value is a defect in it. It reorganises the sampler into concurrent
stages; it does not reduce per-request capability.

**Status note:** the reviewer wrote this while the v2 block stood at 62/79. It is
**79/79** as of this commit — the whole serial suite, same oracle. Neither the
intermediate throughput nor the intermediate behaviour should be treated as
final; the serial block remains the reference RTL.

**But the instinct found a real hole, and it is not in the sampler.** It is the
material rule:

> "The TMU performs one primary detail sample."

That rule predates the redesign. Under it, Zhaozhou cannot reproduce Sacrifice's
terrain path, which used base tile + detail + lightmap — at least three texture
contributions per terrain pixel.

## Why one sample was chosen, and what it actually replaced

| Sacrifice contribution | current Zhaozhou replacement |
|---|---|
| main terrain tile | one CLUT8 primary sample |
| material transition / blending | Mosaic picks A or B with a stable stipple |
| lightmap | 33x33 RGB565 vertex tint per patch, Gouraud-interpolated |
| scars, blood, frost, corruption | restricted surface-sheet AUX sample |
| precise cracks, runes, shadows | actual terrain-conforming polygons |

The reasoning was sound — it spends bandwidth on what this machine is unusually
good at: geometry, deformable terrain, persistent surface state, polygon effects.
Two observations behind it hold up:

* **A sampled lightmap is not automatically necessary.** The 33x33 tint lattice
  per 32x32-cell patch, interpolated through Gouraud, already covers broad static
  lighting, ambient occlusion, faction stains and regional colour variation
  without a texture lookup.
* **An 8 km map does not itself increase samples per frame.** The screen is still
  384x240. Bigger maps increase working-set size, cache churn, streaming pressure
  and material diversity — not visible pixels. Hundreds of creatures raise
  fragment count and cache churn, but only the ones actually on screen sample.

**That justifies one sample as the cheap DEFAULT. It does not justify one sample
as the MAXIMUM.**

## What is actually missing

The lightmap has a credible substitute. **The sampled detail layer does not.**

A 64x64 tile per cell with mirrored repetition, Mosaic variation and vertex tint
may look excellent at 240p. But a separate high-frequency detail layer still
buys: close-camera ground grain; micro-rock, soil and bark-scale variation; a
less obviously repeated base tile; consistent fine texture across several
different base materials; and detail that costs no extra polygons.

**AUX does not replace it.** AUX returns `{tag, strength}` bytes for scars, masks
and effects. It returns no general RGB texture and deliberately has no format
decoder, palette, mip or filter.

So freezing the renderer at exactly one general texel forever does give up
something visually useful that Sacrifice had.

## The architecture: ONE sampler, several bounded invocations

We do not need a second TMU. We need to distinguish **one physical sampler**
from **one sampler invocation per fragment**.

A single pipelined TMU can accept multiple tagged requests belonging to one
fragment. The TMU samples textures; the fragment-lifecycle owner and material
sequencing decide how many samples a material requires and combine the results.
The historical TEXJOIN proposal supplied that role; the selected V3 integration
supplies it with `zhao_texture_v3own` and no second lifecycle owner.

> **The amended rule.** Every textured fragment has a guaranteed
> one-primary-sample baseline. A bounded material recipe may request up to THREE
> samples through the same primary TMU. The Measure may remove optional samples
> under load.

That preserves the cheap fast path and restores the capability.

### Material tiers

    Tier 0  untextured        vertex colour / Gouraud only, no primary sample

    Tier 1  normal terrain    base   = sample(Mosaic winner)
                              colour = base x vertex_light
                              optional AUX surface effect
                              -- the normal distant / mid-distance path

    Tier 2  detailed near     base   = sample(Mosaic winner)
            terrain           detail = sample(detail page)
                              colour = combine(base, detail) x vertex_light
                              optional AUX surface effect
                              -- probably the visual sweet spot

    Tier 3  donor / hero      base + detail + light-or-mask sample
                              colour = combine(base, detail, light)
                              -- the literal Sacrifice-style material

Tier 2 restores the most important thing Sacrifice's extra texture supplied,
while keeping vertex lighting rather than spending a third lookup on a lightmap.
Tier 3 gives the machine the literal capability without making it the default for
every terrain pixel.

The combiner need not be programmable. A small fixed vocabulary suffices:

    MODULATE   MODULATE_2X   LERP   ADD_SAT   MASK

> **Freeze the combiner's arithmetic and rounding only after checking the
> donor's actual combination law. Do not invent "Sacrifice-compatible" blending
> from the operator names alone.**

### What the fragment owner carries

    material_recipe
    sample_count        0..3
    binding[3]
    uv_set[3]
    lod[3]
    optional blend weight

and the flow after Early-Z:

    surviving fragment
      -> allocate one fragment-owner record
      -> issue sample 0 to the single TMU
      -> issue samples 1 and 2 when the recipe requires them
      -> responses return {owner_id, sample_index}
      -> fixed-function material combiner
      -> ONE final RGB / A / index
      -> existing RASTER.FRAGMENT

**`RASTER.FRAGMENT` therefore still consumes one final texel packet** and needs
no second or third texel port — the bounded accumulation belongs before it. The
TMU's request machinery stays concerned with individual samples; the sole
fragment owner tracks fragment completion, and different fragments and samples
interleave to keep the cache and sampler busy.

## Could the console afford Sacrifice-style three-layer terrain?

On the hit path, surprisingly, perhaps.

    terrain 3-sample   276,480 x 3 = 829,440
    sky backdrop                     92,160
    stars                           128,000
    clouds                           45,000
    -------------------------------------------
    known frame                   1,094,600 samples

    1,666,667 - 1,094,600 =  572,067 raw clocks left
    against the 20% reserve target of 1,333,333:  238,733 clocks left

for creatures, objects, cache misses, queue bubbles and integration cost. **Not
luxurious. Not obviously impossible either.**

And 829,440 multiplies GEOMETRIC overdraw by three. Once Early-Z is genuinely in
front of sampling, hidden fragments issue none of those three requests, so the
real sample stream should be materially lower.

This is precisely why the synthetic profile stays, and why a stronger named one
is added: **`sacrifice_terrain_3sample`**, carrying the full 1,094,600
known-frame subtotal, plus separate creature-army and cache-thrash traces.

## Why we should still NOT triple-sample everything

**On 8 km maps with hundreds of creatures, cache locality is more dangerous than
raw arithmetic.**

A million samples into a small set of coherent terrain pages can be easier than
600,000 jumping between creature atlases, palettes, terrain tilesets, sky pages,
effect pages and mip levels. The big world increases how much content exists and
must stream, and hundreds of creatures increase how many texture IDENTITIES may
be visible at once. That is a cache problem, not a multiplier problem.

So the winning strategy is not "three samples always":

* retain three-sample capability;
* use two where detail really contributes;
* use one at distance;
* keep lighting in vertices where it is visually adequate;
* reserve sampled lightmaps and masks for hero surfaces;
* make optional detail **the first texture cost The Measure drops**.

The degradation ladder:

    3 samples -> drop the sampled light/mask
    2 samples -> drop the detail
    1 sample  -> retain the base texture
    0 samples -> Gouraud / microform / glint rung

That is a richer machine than the current one-sample law, without building an
unbounded fragment shader.

## The ruling, stated for the specs

**Do not revert or weaken TMU v2.** Its pipelining, resident palettes, nearest
bypass, mips, filtering and shared cache are exactly what make bounded
multi-sampling viable in the first place.

**Do amend the material architecture before the fragment-owner/material packet
freezes.** Charter §26's

> "The TMU performs one primary detail sample"

becomes

> "The baseline terrain recipe performs one primary sample. Bounded recipes may
> request up to three samples through the same primary TMU; optional detail and
> light/mask samples are governed by screen-space importance and are the first
> texture costs removed under pressure."

Keep Mosaic as the baseline transition method. Keep vertex tint as the normal
lightmap replacement. Keep AUX for scars. **Restore an optional direct detail
layer, and retain a three-sample donor/hero recipe.**

### In one paragraph

Are we nerfing the TMU? **No.** Are we presently nerfing the material system
relative to Sacrifice? **Yes — if the one-sample rule remains an absolute
maximum.** Do we need Sacrifice's exact three samples everywhere? **Probably
not.** Do we need the ability to issue two or three bounded samples when the
picture benefits? **Yes.** And this is the moment to correct it, because the
fragment-owner and production fragment-material packets have not been frozen.

---

# FROZEN — MATERIAL COMBINER V1 (owner ruling R9, 2026-09-02)

`MATERIAL_RECIPE_VERSION = 1`.

**These are Zhaozhou-native v1 recipes. Do NOT label them Sacrifice-exact.**
The document above spent its length arguing that we should stop waiting for an
unspecified donor law, and this section is that argument being closed: the
recipes below are ours, chosen, and frozen.

## The four exact helpers

Every RGB operation is component-wise. Intermediates are widened before shifts
and saturation.

    rescale_s(x,8)   = (x + 128) >>> 8                    signed arithmetic; ties toward +infinity
    unit_mul8(a,b)   = (a*b + 128) >> 8
    modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)            one direct multiply/round/shift
    lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w, 8))  w unit8, raw/256

`modulate2x8` is exactly the direct `(a*b+64)>>7` law. It is not a rounded
unit multiply followed by doubling. `rescale_s` uses a signed arithmetic right
shift; adding 128 before `>>> 8` makes exact half ties round toward positive
infinity, including negative deltas.

## The eight recipes and exact legal counts

| id | name | legal sample count | RGB | A |
|---|---|---:|---|---|
| 0 | `PASSTHRU` | 0 or 1 | count 0: `admitted_base.rgb`; count 1: `s0.rgb` | count 0: `admitted_base.a`; count 1: `s0.a` |
| 1 | `MODULATE` | exactly 2 | `unit_mul8(s0.rgb, s1.rgb)` | `s0.a` |
| 2 | `MODULATE2X` | exactly 2 | `modulate2x8(s0.rgb, s1.rgb)` | `s0.a` |
| 3 | `LERP` | exactly 2 | `lerp8(s0.rgb, s1.rgb, recipe_weight)` | `s0.a` |
| 4 | `ADD_SAT` | exactly 2 | `sat_u8(s0.rgb + s1.rgb)` | `s0.a` |
| 5 | `MASK` | exactly 2 | `s0.rgb` | `unit_mul8(s0.a, s1.a)` |
| 6 | `TERRAIN_DETAIL_LIGHT` | exactly 3 | `unit_mul8(modulate2x8(s0.rgb, s1.rgb), s2.rgb)` | `s0.a` |
| 7 | `TERRAIN_DETAIL_MASK` | exactly 3 | `modulate2x8(s0.rgb, s1.rgb)` | `unit_mul8(s0.a, s2.a)` |

Only PASSTHRU admits counts 0 or 1. Recipes 1–5 admit exactly two samples and
recipes 6–7 admit exactly three. Count 0 reads no sample at all: it returns the
base RGB and alpha captured at fragment admission, not `s0` and not a null
texture. Both terrain recipes apply `modulate2x8` to the first `s0`/`s1` layer;
only `TERRAIN_DETAIL_LIGHT` then unit-multiplies that RGB by `s2`.

## The rules that travel with every multi-sample recipe

* **Sample 0 is the base sampled colour and owns alpha when it exists**, unless
  the recipe names a mask. PASSTHRU count 0 instead uses admitted base RGB/A.
* **The output palette index is zero at count 0 and exactly `sample0.index` at
  every legal nonzero count.** Later samples and palette RGB never replace it.
* **Final status is exactly eight bits:** bitwise-OR the full status bytes from
  committed required TMU planes and the committed required AUX plane, then OR
  `{7'b0,material_refused}`. Unrequired and uncommitted planes contribute
  nothing; current producers use bit 0 for `SOURCE_REFUSED` and preserve bits
  7:1 for typed expansion.
* **The V3 top stores the exact 46-bit material row**, separate
  `material_refused` and material-generation arrays, and authoritative
  `owner_required_mask_m[3:0]` plus owner-mask generation, all written on
  `own_adm_accept`; the mask captures exact `own_adm_req`. It separately stores
  `{descriptor_usable,owner_generation}` in a per-owner descriptor-trust sidecar
  only on descriptor-response handshake. `recipe_weight` and admitted base RGB/A
  remain in row46; none is re-derived from live inputs or added to owner
  functional ports.
* **Admission owner mask is the trust root.** `O` means the stored owner-mask
  generation matches the joined owner; `M` means the material generation
  matches; `D` means response-captured descriptor usability is true and its
  sidecar generation matches. Material and descriptor masks are independently
  compared with `owner_required_mask_m`. If O and both copies are valid/equal,
  expand normally unless count-only `material_refused` forces refusal. If O is
  valid and either copy is invalid or mismatched, always use the owner mask and
  force every owner-required source to typed terminal refusal. Only invalid O is
  reset-lifetime: feed no expander and enter the island/owner reset barrier.
* **Row46 never travels through the expander.** Expansion receives only owner
  mask plus force-refusal control. At owner combine admission, a tagged,
  credit-reserved synchronous material-read pipeline/FIFO accepts at most one
  valid owner ticket per ready clock and preserves owner+row+generation under
  stalls. A valid generation/mask uses row46. With valid O but invalid/mismatched
  material, it derives count and AUX from owner mask, reads exactly those
  required planes so full status and sample-0 index survive, zeroes weight/base,
  sets `material_refused=1`, and chooses an illegal recipe for that count:
  recipe 0 for count 2, recipe 1 for counts 0/1/3. This produces
  J1/status-refused/loud output. Invalid O admits no combine and takes the reset
  barrier. Pipeline state reduces into `material_read_idle_w`; arithmetic leaf
  state reduces into `combine_leaf_idle_w`; their conjunction is the one existing
  `q_combine_idle` term, with both sides independently fire-tested. Invalid
  output-owner bits are never read.
* **A sample-count mismatch is the only malformed material encoding** and is an
  asset error, not a mode to fall back from. Every 3-bit recipe ID is assigned
  exactly once to recipes 0–7.
* **MASK multiplies alpha continuously** as `unit_mul8(s0.a,s1.a)`; it is not a
  nonzero-alpha gate. Recipes 1–4 and 6 preserve `s0.a`; recipe 7 multiplies
  `s0.a` by `s2.a`.

## Malformed assets: count mismatch only

Every recipe code is assigned; “malformed material” in R9 means only that the
sample count is illegal for its recipe. Material/descriptor copy failure uses
owner-mask-driven source refusals while O is valid; only invalid owner-mask
identity is reset-lifetime. Neither case redefines the stored `material_refused`
count bit. **Reject count mismatches before sealing.** If one reaches hardware: set typed
`SOURCE_REFUSED`, raise the sticky recoverable frame fault, complete every
admitted owner obligation through its typed refusal path, and repeat the
previous complete frame. The terminal diagnostic value is deliberately loud
`RGB=24'hFF00FF, A=8'hFF`, never a plausible partial recipe or admitted base
colour. It drains normally; it is not publishable.

## Where the combiner lives and what it costs

The Packet-B combiner is a registered owner-keyed engine, not a large
combinational case on retirement. One physical arithmetic datapath may issue at
most **one paired phase per clock**. It does not accept or complete one arbitrary
multi-phase material job per clock.

Each accepted job is classified exactly once after all required-source commits:

    source_status_dirty = any committed required TMU/AUX status[7:0] is nonzero
    J1 = source_status_dirty
      || material_refused
      || status-clean legal PASSTHRU(count 0/1), ADD_SAT, or MASK job
    J2 = status-clean, !material_refused, legal MODULATE, MODULATE2X, LERP,
         or TERRAIN_DETAIL_MASK job
    J3 = status-clean, !material_refused, legal TERRAIN_DETAIL_LIGHT job

    job_count    = J1 + J2 + J3
    phase_demand = J1 + 2*J2 + 3*J3

Any job with any required-source nonzero status is J1 regardless of recipe;
`material_refused` is also J1 regardless of recipe. J2/J3 are legal-count and
status-clean. Issued and completed phase
counters must each equal `phase_demand` after drain. A homogeneous clean J1
stream may complete one job per clock after fill; clean J2 consumes two issue
clocks per job and clean J3 consumes three. A one-job-per-clock claim for J2 or
J3 requires a later measured architecture with additional physical phase
capacity and a correspondingly revised law.

## Status of the RTL as of this freeze

`zhao_raster_texjoin_v2.sv` historically declared recipes 0–5 and returned
sample 0 for every non-`PASSTHRU` recipe while raising
`combiner_unfrozen_o`; recipes 6 and 7 were absent. That block remains historical
source/oracle context, not the selected fragment owner. Packet B instead
specifies `zhao_texture_material_combine_v3` against this complete R9 law and the
single-owner V3 path. This architecture text makes no claim that that RTL,
its tests, or its resource measurements already exist.
