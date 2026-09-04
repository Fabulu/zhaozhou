# Contract — TEXTURE.COMBINE (Material combiner)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::material::combine` — **WRITTEN 2026-09-05**, `reference/include/zref/zref_material.hpp`

## Purpose and exclusions

TEXTURE.COMBINE performs the fixed recipe arithmetic over the zero-to-three
samples a fragment asked for, producing the single RGB and alpha that
`RASTER.FRAGMENT` blends.

**Written 2026-09-03.** It is the other half of `MATERIAL.RESOLVE`, and that
contract says why they are one piece of work in two files:

> The surviving TEXJOIN behaviour returns **sample 0 for every recipe**, and
> the three-sample terrain recipes were absent from that RTL entirely. So when
> this block starts returning real `sample_count` and `recipe` values, the
> combiner must exist to consume them — shipping the resolver alone would make
> the machine confidently fetch samples nothing combines.

`TEXTURE.FRAGROB` explicitly refuses to own this arithmetic, and
`reports/islandrearchitecture5.md` §3.3 already budgets it as **its own
registered II=1 pipeline** — 650 ALM, 500 registers, 1–4 M10K, 0–2 DSP.

**Exclusions, each a specific refusal:**

* **No sampling.** The samples arrive; `TEXTURE.TMU` fetched them.
* **No resolution.** The recipe arrives; `MATERIAL.RESOLVE` looked it up.
* **No blending against the framebuffer.** That is `RASTER.FRAGMENT`'s, and the
  distinction matters: this combines *sources with each other*, that combines
  the *result with what is already there*.
* **No toon quantisation.** `RASTER.TOON` owns the cel band.
* **No fog.** See the ordering question in
  `reports/FUNDAMENTALS-DECISIONS-NEEDED.md` D-5 — fog's position relative to
  toon is an open owner decision and this block must not pre-empt it.

## The recipes

Six encodings, matching the constants already in `zhao_raster_texjoin_v2.sv`
and `zhao_texture_fragrob.sv` so nothing is renumbered:

| id | recipe | arithmetic |
|---|---|---|
| 0 | `PASSTHRU` | sample 0 unchanged |
| 1 | `MODULATE` | `s0 * s1` |
| 2 | `MODULATE2X` | `s0 * s1 * 2`, saturating |
| 3 | `LERP` | `lerp(s0, s1, recipe_weight)` |
| 4 | `ADD_SAT` | `s0 + s1`, saturating |
| 5 | `MASK` | `s0` where `s1` passes, else transparent |

**A seventh encoding is REFUSED and counted**, not treated as passthrough. The
recipes are a closed set by ruling, and quietly accepting an unknown one is how
a content bug becomes a shipped picture nobody questions.

## Q formats and rounding

Every sample is RGB888 plus `alpha8`. Products use **unit8** semantics —
`spec/qformats.md` §2: **value = raw/256, so 255 is the largest representable
and NOT 1.0.** A modulate by 255 therefore darkens very slightly, which is the
ratified behaviour and must not be "fixed" to 255/255.

**One rounding per result**, round-half-up. `zhao_raster_blend`'s existing
unit8 arithmetic is the precedent and should be reused rather than restated —
the console already has one unit8 multiply law.

## Input and output packet layouts

**In**, ready/valid: `{ sample_count[1:0], recipe[2:0], recipe_weight (unit8),
s_rgb[3] (24), s_a[3] (8), has_aux, aux_rgb, aux_a, frag_tag }`.

**Out**: `{ rgb (24), a (8), frag_tag }`.

`frag_tag` rides through untouched so FRAGROB can retire in allocation order —
this block must not reorder, and a reorder here would be invisible in a
triangle count and obvious in a capture CRC.

## Backpressure rules

Ready/valid, **II=1** per the island budget: one fragment per clock, fully
pipelined. It is the last arithmetic before the fragment leaves the texture
island, so a stall here backs up the whole island.

## Memory ownership

None. It is arithmetic on values handed to it.

## Latency (fixed or variable)

`fixed`. The pipeline depth is an implementation choice; the throughput is not.

## Overflow and malformed-input behaviour

* **`sample_count == 0`** produces the fragment's vertex colour unchanged —
  an untextured surface is legal and common, and must not require a dummy
  sample.
* **A recipe naming more samples than `sample_count` supplied** is malformed
  and refused: `MODULATE` with one sample cannot multiply by a sample that was
  never requested. Counted, not silently degraded to passthrough — which is
  exactly what the surviving TEXJOIN does today and why a wrong material looks
  plausible.
* **Saturation is reported.** `ADD_SAT` and `MODULATE2X` saturate by design;
  the count tells content authors when a recipe is clipping constantly.

## Scalar reference function

**WRITTEN 2026-09-05** — `zref::material::combine(recipe, weight, samples,
count, base, frag_tag, ledger)` in `reference/include/zref/zref_material.hpp`.
It calls `zref::unit_mul` rather than restating the unit8 product, as this
contract requires.

**One thing this contract leaves open, and the oracle refuses to guess.** The
overflow section says `sample_count == 0` returns "the fragment's vertex
colour", but the In-packet list has no vertex-colour field — the only
non-sample colour in it is `has_aux / aux_rgb / aux_a`. The reference therefore
takes that colour as an explicit `base` parameter instead of binding it to a
packet field. When the RTL is written, whichever field carries it is passed in
and the two agree by construction; hard-coding a guess into the arbiter would
be the worse error.

## Directed tests

**WRITTEN 2026-09-05** — `tests/texture/material_combine_directed.cpp`,
35 checks, green. Every case this section asked for, plus one the writing of it
turned up:

* each of the six recipes against hand-computed values at the unit8 corners
  (0, 1, 128, 255) — including that modulate by 255 is **not** identity;
* **and the exact boundary of that, swept across all 256 inputs.**
  `unit_mul(a,255) = floor((255a+128)/256)`, which equals `a` precisely when
  `a <= 128`. So modulate by 255 is **identity for every a ≤ 128 and `a-1`
  above it** — it does not "always darken", it darkens for less than half the
  range. Two drafts of that comment were wrong before the sweep was written;
  a test comment that misstates the law it pins is worse than none, because the
  assertion still passes and nothing ever contradicts it;
* `sample_count == 0` returning vertex colour unchanged;
* a recipe demanding more samples than supplied, refused and counted;
* a seventh recipe encoding refused;
* `frag_tag` preserved through every path, because retirement order depends on
  it;
* saturation reported for `ADD_SAT` and `MODULATE2X`.

## Randomized differential tests

Planned, against the scalar model over random recipes, counts and sample
values, with a coverage guard that every recipe and every refusal class was
actually reached — the repository has shipped random tests that never hit their
interesting case more than once.

## Integration capture cases

None on hardware. **The composed case is `MATERIAL.RESOLVE` → `TEXTURE.TMU` →
FRAGROB → COMBINE**, which is the first point at which a real material produces
a real pixel.

## Synthesis / resource ceiling

The island budget's own line: **650 ALM, 500 registers, 1–4 M10K, 0–2 DSP**,
with the §3.4 tripwire `reject DSP > 2`. Registered in
`design/fit_targets.yml` with that rule when the RTL exists.

## Notes

**It must reuse the existing unit8 multiply rather than define a second one.**
That law already exists, and `zhao_raster_fragment` was explicitly built to
call it rather than restate it. A second unit8 multiply is the same defect
class as the duplicated flat-shade law found earlier today: two arithmetics
that agree until they do not, with nothing to say which is right.

**This block is why the terrain three-sample recipes matter.** They were absent
from the surviving TEXJOIN RTL entirely, so terrain material work has been
blocked on a combiner nobody had written — another instance of the audit's
pattern: an endpoint built, the connecting organ missing.
