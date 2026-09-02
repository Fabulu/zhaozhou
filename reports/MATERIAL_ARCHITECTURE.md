# Bounded multi-sample materials — the TMU is not nerfed, the MATERIAL RULE was

Ruling 8, relayed from the reviewer via Fabian 2026-08-30, from the question:
*"with the TMU redesigns, aren't we nerfing it? We can't do what Sacrifice does
now."*

Companion to `reports/RENDERER_ARCHITECTURE.md` (rulings 1–7). Read that first
for the renderer pipeline; this file is about how many samples a fragment may
take and what combines them.

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
fragment. The TMU samples textures; TEXJOIN and material sequencing decide how
many samples a material requires and combine the results.

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

### What TEXJOIN carries

    material_recipe
    sample_count        0..3
    binding[3]
    uv_set[3]
    lod[3]
    optional blend weight

and the flow after Early-Z:

    surviving fragment
      -> allocate TEXJOIN record
      -> issue sample 0 to the single TMU
      -> issue samples 1 and 2 when the recipe requires them
      -> responses return {record_id, sample_index}
      -> fixed-function material combiner
      -> ONE final RGB / A / index
      -> existing RASTER.FRAGMENT

**`RASTER.FRAGMENT` therefore still consumes one final texel packet** and needs
no second or third texel port — the bounded accumulation belongs before it. The
TMU's ROB stays concerned with individual sample order; TEXJOIN tracks fragment
completion, and different fragments and different samples interleave to keep the
cache and sampler busy.

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

**Do amend the material architecture before TEXJOIN freezes.** Charter §26's

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
picture benefits? **Yes.** And this is the moment to correct it, because TEXJOIN
and the production fragment-material packet have not been frozen.

---

# FROZEN — MATERIAL COMBINER V1 (owner ruling R9, 2026-09-02)

`MATERIAL_RECIPE_VERSION = 1`.

**These are Zhaozhou-native v1 recipes. Do NOT label them Sacrifice-exact.**
The document above spent its length arguing that we should stop waiting for an
unspecified donor law, and this section is that argument being closed: the
recipes below are ours, chosen, and frozen.

## The three primitives

    unit_mul8(a,b)   = (a*b + 128) >> 8
    modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)
    lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w, 8))     w unit8, raw/256

## The eight recipes

| id | name | samples | RGB | A |
|---|---|---|---|---|
| 0 | `PASSTHRU` | 0 or 1 | `s0.rgb` | `s0.a` |
| 1 | `MODULATE` | 2 | `unit_mul8(s0, s1)` | `s0.a` |
| 2 | `MODULATE2X` | 2 | `modulate2x8(s0, s1)` | `s0.a` |
| 3 | `LERP` | 2 | `lerp8(s0, s1, recipe_weight)` | `s0.a` |
| 4 | `ADD_SAT` | 2 | `sat_u8(s0 + s1)` | `s0.a` |
| 5 | `MASK` | 2 | `s0.rgb` | `unit_mul8(s0.a, s1.a)` |
| 6 | `TERRAIN_DETAIL_LIGHT` | 3 | `unit_mul8(modulate2x8(s0, s1), s2)` | `s0.a` |
| 7 | `TERRAIN_DETAIL_MASK` | 3 | `modulate2x8(s0, s1)` | `unit_mul8(s0.a, s2.a)` |

**Count 0 means `has_texture = 0` and no sample is read at all** — not a sample
of a null texture.

## The rules that travel with every multi-sample recipe

* **Sample 0 is the base and owns alpha**, unless the recipe names a mask.
* **The output palette index is `sample0.index`.**
* **Error and status bits are ORed over all required samples.** A recipe is as
  broken as its worst sample.
* **`recipe_weight` is stored in the TEXJOIN record**, not re-derived.
* **A sample-count mismatch or an unknown recipe is a material-asset error**,
  not a mode to fall back from.

## Malformed assets

**Reject them before sealing.** If one reaches hardware: raise a **sticky frame
fault** and repeat the previous complete frame. **Do not emit a plausible
placeholder texel** — a plausible wrong texel is the failure this whole document
exists to argue against, and it is invisible in exactly the way that costs days.

## Where the combiner lives

**Its own registered II = 1 pipeline.** Not a large combinational case on
TEXJOIN's retirement path — which is what `zhao_raster_texjoin_v2.sv` currently
has, deliberately marked unfrozen, and what must be replaced before that block
is production.

## Status of the RTL as of this freeze

`zhao_raster_texjoin_v2.sv` declares recipes 0–5 with matching ids and returns
sample 0 for every non-`PASSTHRU` one, raising `combiner_unfrozen_o`. That flag
was the right call and is now discharged by this section: the arithmetic exists,
so the block can implement it. **Recipes 6 and 7 do not exist in the RTL at all**
and are the three-sample terrain cases the whole document was written for.
