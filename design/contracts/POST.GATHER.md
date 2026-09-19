# Contract — POST.GATHER (Glow/distortion gather)

> ## CORRECTED 2026-09-18 — the Duo quarter-res plane is 128 x 48, not 128 x 60
>
> The plane table below sizes every mode as the quarter of the DISPLAYED canvas.
> That is correct for Z60 and Storm, whose displayed area IS their rendered area.
> **Duo is the only mode where the two differ, and it is wrong there.**
>
> `zhao_pkg.sv`: Duo displays 512 x 240 but RENDERS two 256 x 192 views at y
> offset 24; the 48 remaining rows carry no rendered content, and both contracts
> already forbid a displaced sample from leaving its view — so those cells are
> provably dead, not spare.
>
> | | stated below | correct |
> |---|---|---|
> | Duo plane | 128 x 60 = 7,680 | **128 x 48 = 6,144** (two 64 x 48 views) |
> | Duo glow prep | 15,360 | **12,288** |
> | Duo frame cost | 113,664 | **110,592** |
>
> The proof is inside the throughput table itself: its Duo main pass counts
> RENDERED pixels (98,304 = 2 x 256 x 192) while its glow pass counts DISPLAYED
> cells. One row, two surfaces. Z60 and Storm are unaffected.
>
> `design/blocks.yml` was already right — its POST.COMPOSITE purpose line says
> "96x60 Z60 / **2x64x48** Duo".
>
> Full working: `reports/DUO-QUARTER-PLANE-GEOMETRY-20260918.md`.

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

## The tag-to-gather law — PROPOSED, owner ruling R37 (2026-09-19), NOT RATIFIED

This block's per-fragment input is glow RGB, a signed 8.8 displacement pair and
an ink bit. What RASTER.RESOLVE actually latches is ONE BYTE:
`spec/stars_and_flares.md` §1, frozen — `tag = (channel << 6) | strength`, with
`GLOW = 0b01` and strength the source texel's CLUT intensity (0..63). The step
between those two is an ART law, and ruling R37 asks this packet to PROPOSE it
with every coefficient in a named constant, a zref model, and a before/after for
the owner to judge by eye. Here it is; nothing in `fpga/rtl` implements it yet
and this section is a proposal until the owner rules.

**The proposal** (`zref::post::gather`, `reference/include/zref/zref_post.hpp`):

| constant | proposed | what it decides |
|---|---|---|
| `kGlowKnee` | 24 | below this strength a texel is LIT, not a LIGHT: no glow at all. The picture is most sensitive to this one. |
| `kGlowSlope` | 0x1C (Q4.4) | how fast a texel becomes a light above the knee; saturates near strength 55. |
| `kGlowTint[3]` | 255, 236, 224 | a per-channel weight on the borrowed colour — the warm bias a halo has. |
| `kGlowMaster` | 255 | the law's master gain. `SetPost.bloom_gain` scales the RESULT again, per frame. |

and three decisions that are not numbers:

1. **The glow's colour is the fragment's own colour.** A star's halo is the
   colour of the star. §1's thesis is that intensity is drawn and a palette
   colourises it; the colourising has already happened by resolve, so the bloom
   BORROWS it rather than inventing a second palette.
2. **Strength is a knee, not a scale.** Every CLUT texel carries some intensity.
   Without a knee the whole image hazes — which is exactly what the contact
   sheet's `knee 16` row shows (907 of 5,760 cells contributing against 74).
3. **Displacement and ink are NOT invented.** Channels `0b10` and `0b11` are
   unallocated in the spec, so a fragment carrying one contributes nothing and is
   COUNTED (`reserved_channel`). Under this law `c_disp_x_o`, `c_disp_y_o` and
   `c_ink_o` are ZERO — a statement about what v1 does, not an omission. Ink
   arrives as a look value on `SetPost` instead (R36), not from a tag.

**The evidence to judge it by**: `reports/post-gather-law/gather_law_contact.png`,
rendered by `tools/post/gather_law_render.cpp` (the scene, authored by eye) and
`tools/post/gather_law_sheet.py`. Rows: the resolved frame with no gather, then
the law at knee 16 / 24 / 32 at two bloom gains, then the blur comparison.

**The second question in that sheet is a COST, not a look.** The halo is
cell-quantised because the plane is quarter-resolution and POST.COMPOSITE samples
it per cell; Part A's separable blur is what rounds it. One pass is two sweeps of
the 96 x 60 plane = 11,520 cell-steps, exactly the glow prep this contract
budgets for Z60. Two passes still read blocky; FIVE read round, at 57,600
cell-steps (3.5% of a 1,666,666-clock frame). The renderer's default is TWO —
the budgeted number — and the sheet shows what the fifth pass buys so the owner
can spend it deliberately or not at all.

**What is still owed after a ruling**: the RTL adapter (resolve tag + colour ->
this block's `f_glow_*`), the HUD plane store R37 names next, and composing
`zhao_post_gather` itself. Core entry I17 (c) stays open until then.

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
