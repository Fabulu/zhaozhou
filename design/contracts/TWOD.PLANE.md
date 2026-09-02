# Contract — TWOD.PLANE (Twin Horizons planes)

> Ledger: `design/blocks.yml` · owner ZH-068 · phase 11 · maturity SPECIFIED

## Purpose and exclusions

Two scanout/tile-aware world planes, in **two declared roles only**: a BACKDROP
beneath the resolved world, and an ATMOSPHERE sheet over it in the post stream.

**Not a "world-space depth plane".** The purpose line used to name one, and
naming a typical use is how a restriction gets read as permission — ruling R4
is explicit that there is **no arbitrary depth test and no depth write in v1**.
Water, lava, a landscape, or any plane that must *intersect* ordinary geometry
is **triangles through the main renderer**, not a plane.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons work in flight
and clears staging; no descriptor state survives a reset, because descriptors are
re-sent per frame.

## Input and output packet layouts
### The two-slot limit is REAL — owner ruling 2026-08-31 §3.1

> Two plane descriptors are a real v1 limit, not a placeholder. **Use one
> time-multiplexed restricted plane engine.** Do not instantiate two full engines
> and do not let it become a second unrestricted TMU.

Both halves are the design. Two slots is the *interface*; one time-multiplexed
engine is the *implementation*, and they are separately binding — building two
engines would satisfy the first and violate the second.

### Plane descriptor

`{ slot, enable, format, base, stride, width, height, wrap_u, wrap_v,
   affine[6], line_scroll_base, view_mask, palette_id,
   role[1:0], blend_mode[1:0], opacity }`

**`role`, `blend_mode` and `opacity` added 2026-09-02 by ruling R4.**

### The two roles — and the two that are refused

| role | when | blending | depth |
|---|---|---|---|
| **0 BACKDROP** | beneath the resolved 3D world | **must be REPLACE** | no test, no write |
| **1 ATMOSPHERE** | post stage 4 — over the displaced world, **before** bloom/grade | ALPHA or ADD | no test, no write |
| **2, 3** | — | — | **reserved; the descriptor is refused** |

`opacity` is a **unit8** under the frozen unit8 law: its value is `raw/256`, so
the complement of `opacity` is `256 − opacity` and needs nine bits.

BACKDROP is sky and background only. Its blend **must** be REPLACE — an
alpha-blended backdrop has nothing beneath it to blend with, so a descriptor
asking for one is malformed rather than merely odd.

### Composition order between the slots

**Same role in both slots: slot 0 composites first.**

**Different roles:** BACKDROP is composited in world setup and ATMOSPHERE in the
post stream, so the ordering question does not arise — they are not in the same
place in the pipeline at all.

All existing restrictions remain: CLUT8/RGB565 only, **nearest only**,
affine/line scroll, repeat/clamp, view mask, **one engine**, and **no private
general TMU**.

### Required features — and this list is a ceiling, not a floor

* **CLUT8 and RGB565** only;
* **nearest sampling** only — no bilinear. This is the single most important
  exclusion: bilinear is what would make this a second TMU;
* affine transform;
* line scroll;
* repeat and clamp;
* view masks/parameters where Duo requires them.

Typical uses: sky, distant landscape, water/lava, fog or cloud sheet, a bounded
world-space depth plane.

**Anything needing ordinary textured geometry uses the main renderer.** If a
request cannot be met by the list above, the answer is a triangle, not a wider
plane engine.

## Backpressure rules
Ready/valid to the compositor. The engine is time-multiplexed across the two
slots **within a scanline**, so a stall holds both slots together — they are one
engine and cannot make independent progress.

## Memory ownership
Reads plane texels and the line-scroll table through `MEM.GUARD`. Writes nothing.

Owns a small on-chip line cache sized for one scanline of both slots. **Not a
general texture cache** — the access pattern is a scanline walk with an affine
step, which is predictable and prefetchable, and that predictability is exactly
what a restricted plane engine buys over a TMU.

## Q formats and rounding
Affine coefficients fx16; texel coordinates fx16 stepped per pixel.

**Nearest sampling means truncation toward zero on the texel index**, declared
here so the reference and RTL cannot disagree by a half-texel — the class of
error that shows as a one-pixel shimmer along a horizon and is very hard to see
in a still.

The affine step is a **recurrence**: `u += du_dx` per pixel, reseeded per
scanline from the exact affine evaluation. Same structure as `RASTER.ATTRSTEP`,
and for the same reason — it removes a multiply per pixel without changing a
rendered bit, provided the reseed is exact.

## Latency (fixed or variable)
Variable, dominated by the texel fetch. The engine itself is a fixed short
pipeline per pixel.

## Target throughput
One plane pixel per clock, **shared between the two slots**. Two enabled planes
therefore cost two clocks per screen pixel.

At Z60 (92,160 pixels) two full-screen planes are ~184,000 clocks, **~14 % of a
1,333,333-clock frame.** That is affordable but not free, and it is the number
that justifies the two-slot limit rather than a general plane count.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| slot index > 1 | refuse the descriptor, count |
| `role` 2 or 3 | refuse the descriptor, count (R4: reserved) |
| BACKDROP with a blend other than REPLACE | refuse the descriptor, count |
| any request for a depth test or depth write | refuse the descriptor, count — v1 has neither |
| unknown format | refuse — never fall back to RGB565 |
| stride/width inconsistent with the base allocation | refuse |
| line-scroll table shorter than the view | refuse rather than reading past it |

A refused plane **draws nothing** and the frame completes. A plane is a
background; losing it is visible, but drawing it from a misread descriptor is
worse and unbounded.

## Counters and traces
* `plane_pixels[2]`, `plane_texel_bytes[2]`
* `descriptors_refused_by_reason[4]`
* `line_cache_misses`
* `slots_enabled_histogram[3]` — how often 0, 1 or 2 planes are actually used

## Scalar reference function
`zref::TwoPlanes` (ledger `reference_model`). Owns the affine walk, the nearest-sampling rounding, the wrap
modes, the line-scroll application and the refusal taxonomy.

## Directed tests
`tests/twod/twod_plane_directed.cpp`.

* identity affine: the plane appears unrotated and unscaled, texel for pixel;
* **the affine recurrence equals exact evaluation** at every pixel of a
  scanline, for several rotations — this is the `ATTRSTEP` property and the
  reason the recurrence is legal;
* wrap and clamp at both edges, in both axes, at the exact boundary texel;
* CLUT8 and RGB565 produce identical geometry, differing only in colour;
* line scroll: a per-line offset table applied exactly, including a line with
  zero offset;
* both slots enabled, overlapping: the compositor order is respected and the
  engine alternates without state leaking between slots — **the
  time-multiplexing correctness case**;
* every refusal condition.

## Randomized differential tests
`tests/twod/twod_plane_random.cpp`, RTL against `zref::TwoPlanes`.

Random affines biased toward **near-degenerate** ones — very large scales, near
90-degree rotations, and steps close to a texel boundary — since those are where
truncation and the recurrence disagree if they are going to.

## Formal properties
`tests/formal/twod_plane_slots.sby`:

* **no state leaks between slots**: slot 0's output is a function of slot 0's
  descriptor alone. This is the property that makes one time-multiplexed engine
  equivalent to two, and it is the whole risk of the ruling's implementation
  choice;
* a refused descriptor produces no pixels;
* handshake hygiene; reset clears staging.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 2,200 ALMs, 8 DSPs, ≤ 6 M10K.**

The DSP budget is for the affine reseed per scanline, not per pixel — a
per-pixel multiply would mean the recurrence was not used. **If this block's fit
approaches a TMU's size, the restriction has been lost** and the ruling's
"do not let it become a second unrestricted TMU" has been violated in
implementation rather than in spec.

## Integration capture cases
* **sky plus water, both full screen** — the two-slot case at full cost, and the
  ~14 % figure measured rather than computed.
* **a scrolling cloud sheet over a rotating landscape** — line scroll and affine
  together, judged on a contact sheet for shimmer along the horizon.
* **Duo with a view-masked plane** — one view shows it, the other does not.

## Notes

Second world-space plane mode is cut-order 4 (§26).
