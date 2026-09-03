# Contract — POST.GATHER (Glow/distortion gather)

> Ledger: `design/blocks.yml` · owner ZH-046 · phase 11 · maturity SPECIFIED

## Purpose and exclusions

Accumulate low-resolution glow, distortion-XY and outline buffers from the resolved frame.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons work in flight
and clears the effect buffers; they are rebuilt every frame, so nothing durable
is lost.

## Input and output packet layouts
### The rule that shapes the whole block — owner ruling 2026-08-31 §4

> POST.GATHER collects glow, displacement and mask information **during tile
> resolve**. It **must not reread the completed framebuffer merely to rediscover
> tags.**

That prohibition is the design. Re-reading the framebuffer to recover which
pixels were emissive means inferring intent from colour — which is unreliable
(a bright diffuse pixel and a dim emissive one can match), and it costs a full
framebuffer read. **Gathering during resolve is both cheaper and correct**,
because the tag is still present at that moment.

This is the same principle already recorded for the creature ink plane: *do not
infer the mask from final RGB in hardware; write it explicitly.*

### In
Resolved tile pixels with their material tags, from `RASTER.RESOLVE`.

### Out — three quarter-resolution planes

| mode | effect buffer |
|---|---|
| Z60 | 96 × 60 |
| Storm | 80 × 60 |
| Duo | 128 × 60 |

**Quarter linear resolution**, frozen by the ruling.

* **glow** — RGB, from emissive-tagged fragments;
* **displacement XY** — signed, from refraction/shockwave/heat-haze tags;
* **exterior-ink mask** — 1 bit, the creature outline plane.

Accumulation into a quarter-resolution cell is a **saturating add**, not an
average: a single very bright fragment should light the cell, and averaging
would dilute it by however many neighbours happen to be dark.

## Backpressure rules
Ready/valid from resolve. **POST.GATHER must never backpressure
RASTER.RESOLVE** — otherwise it becomes part of the renderer's critical
throughput rather than a side channel. Its accumulation is a register-file
update, one cell per fragment, and keeps up at resolve rate by construction.

**If a fit cannot meet that with the double bank, add a small tile-summary
FIFO — do not stall resolve** (R5). The escape hatch is named here so that a
fit which comes back tight does not get resolved by quietly asserting
backpressure.

## Memory ownership

**REPLACED 2026-09-02 by ruling R5.** The storage arithmetic this section used
to carry was internally contradictory: it named one accumulation format, one
storage format and one ceiling that could not all be true together. The ruling
separates the two levels that were being conflated.

### Level 1 — tile-local accumulation, in registers

A **16 × 16 pixel tile maps to exactly 4 × 4 effect cells.** Two ping-pong
banks of **16 register cells**. Per cell:

| field | format |
|---|---|
| `glow_r` / `glow_g` / `glow_b` | u16, **saturating** |
| `displacement_x` / `displacement_y` | signed 8.8 in a **wide saturating s16** |
| `ink` | 1 bit, OR |

A resolved fragment updates **at most one cell per plane**.

**No global M10K read-modify-write on the resolve path.** That is the clause
that keeps this block a side channel: an M10K RMW at resolve rate would put the
gather inside the renderer's throughput, which the Backpressure section already
forbids and this now makes structurally impossible.

### Level 2 — the global effect cell, 33 bits

| field | width |
|---|---|
| glow | RGB565, 16 b |
| displacement X | signed i8 |
| displacement Y | signed i8 |
| exterior ink | 1 b |

**At tile flush:** glow rounds and clamps **once** into RGB565; displacement
rounds **once** to integer pixels; **X clamps to [−8, +8]**, **Y clamps to
[−4, +4]**; ink is copied.

Those two clamps are not arbitrary — they are the bound POST.COMPOSITE's line
ring is built against (R6: nine complete source lines, horizontal ±8).

### The count

Duo is **128 × 60 = 7,680 cells**. The compact total is **31,680 bytes**, but
the physical count is set by shape, not by bytes: at the natural 256 × 40 M10K
shape that is **thirty M10Ks**.

**Every tile writes all sixteen cells including zeros**, so the frame overwrites
the active plane and there is no giant reset loop.

**Clamp separately at Duo view boundaries.** A displacement can never sample the
other player's view — a refraction that reaches across the split is not a
graphical artefact, it is one player seeing through the other's screen.

Writes out for `POST.COMPOSITE`; reads no external memory.

## Q formats and rounding
Glow accumulates in **u16 per channel, saturating**, then is packed to RGB565
once at the end of the frame. Accumulating in the packed format would lose the
headroom that makes bloom look like light rather than like clipping.

Displacement accumulates as signed 8.8 per axis, **clamped to a declared
bound** — the ruling requires contributions to combine *before* sampling and be
clamped, which is what stops three overlapping effects from tearing the image.

One rounding, at pack time, round-half away from zero.

## Latency (fixed or variable)
Fixed and small per fragment — a coordinate shift, a buffer read, a saturating
add, a write. Frame-level completion is bounded by resolve.

## Target throughput
One fragment per clock, matching `RASTER.RESOLVE`, because it runs beside it.

Cost is therefore **zero additional clocks** in the frame budget — it consumes
fragments that are already flowing. That is the second reason the ruling's
"during resolve" is right, after correctness.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| glow accumulator saturates | clamp and count. Saturation is expected on a bright frame and is not an error |
| displacement exceeds the declared bound | clamp and count — this is the ruling's clamp, and counting it is how an over-authored effect becomes visible |
| unknown material tag | ignore for gather purposes, count. A tag this block does not recognise must not corrupt a plane |

## Counters and traces
* `glow_saturations`, `displacement_clamps`
* `glow_cells_lit`, `ink_cells_set`
* `unknown_tags`

## Scalar reference function
`zref::post::glow_pack565`, with `zref::post::glow_accumulate` and
`zref::post::disp_to_pixels` (`reference/include/zref/zref_post.hpp`).

**This section used to cite `zref::PostGather`, and no such symbol had ever
been written** — a phantom citation the ledger caught the moment the block
gained evidence to check against. The law owns the accumulation
arithmetic, the saturation and clamp rules, and the pack.

## Directed tests
`tests/compositor/post_gather_directed.cpp`.

* one emissive fragment lights exactly one cell, at the right coordinate — the
  quarter-resolution mapping, at all four corners of the frame;
* **saturating add, not average**: one bright fragment among fifteen dark ones
  in the same cell leaves the cell bright. This is the case an averaging
  implementation silently gets wrong and it looks plausible until compared;
* displacement contributions from three effects combine **before** clamping, and
  the clamp is applied once;
* the ink mask is written from the explicit tag, and **a bright non-creature
  pixel does not set it** — the anti-inference case;
* unknown tag: ignored, counted, planes unchanged.

## Randomized differential tests
`tests/compositor/post_gather_random.cpp`, RTL against `zref::post::*`.

Random tagged fragment streams biased toward **cell collisions** — many
fragments landing in the same quarter-resolution cell — since that is where
accumulation order and saturation interact.

## Formal properties
**A formal lane is PLANNED and no file exists yet**, so it is described here
without a path -- citing one that has not been written is how a contract comes
to promise evidence nobody produced. The properties it would carry:

* **gather never asserts backpressure to resolve** — the property that keeps it
  a side channel rather than a throughput term;
* accumulation saturates, never wraps;
* every fragment affects at most one cell per plane.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,200 ALMs, 0 DSPs, ≤ 30 M10K.**

**The M10K ceiling was ≤ 10 and is wrong.** R5 raises it to **≤ 30**. The
compact data is 31,680 bytes, which looks like ten M10Ks if you divide bytes by
1,280 — but M10K count is set by the width/depth **shape** a buffer needs, not
by its byte total, and at the natural 256 × 40 shape Duo's 7,680 cells take
thirty. Dividing bytes by block size is exactly the kind of arithmetic that
reads like a measurement and is not one.

Zero DSPs: accumulation is adds and clamps.

## Integration capture cases
* **a spell frame with glow, refraction and ink together** — all three planes
  populated from one resolve pass.
* **a saturating frame** — a very bright explosion; the clamp counters should be
  non-zero and the image should still read as light.
* **Duo** — 128 × 60, both views, no bleed between them.

## Notes

Buffer precision is cut-order 6 (§26).
