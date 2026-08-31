# The per-pixel budget, measured

Every number in the first table is a **measurement from a directed test against
running RTL**, not an estimate. Where an estimate appears it is labelled and its
source is named. The frame budget throughout is **1,666,667 clocks**, and the
comparison load is the **276,480-pixel terrain-primary component** that ruling 7
already uses.

This file exists because the renderer's cost has moved from "the triangle setup"
to "the per-pixel units", and three of them have now been built and timed. They
are all short.

---

## What each unit costs, measured

| block | clocks per item | items per frame, ONE unit | test |
|---|---|---|---|
| `RASTER.INTERP` | 1 per pixel walked | 1,666,667 | `raster_attrinterp_directed` |
| `RASTER.ATTRDIV` | 36 per divide | 46,296 | `raster_attrdiv_directed` |
| `RASTER.RCP24` | 7 per reciprocal | 238,095 | `raster_rcp24_directed` |
| `RASTER.PERSPUV` | 11 per fragment | 151,515 | `raster_perspuv_directed` |
| `TEXTURE.TMU` CLUT | 3 per sample | 555,555 | `texture_tmu_pipe` |
| `TEXTURE.TMU` direct | 4 per sample | 416,666 | `texture_tmu_pipe` |

`PERSPUV`'s 11 **contains** `RCP24`'s 7 — it instantiates one and shares it
between u and v, which the test asserts as a count (500 fragments cost 500
reciprocals, not 1,000). The two rows are not additive.

The TMU rows are at `FILT_LANES = 2`, and its own test already reports the
comparison that matters: **CLUT is the demand-critical path** — terrain is CLUT8
— at **0.65x** of the 850,000-sample demand it was specced against, which was
itself rounded up from 829,440.

`TEXTURE.AUX` at one request per six clocks — 277,778 a frame — is **ruling 7's
number, not a measurement of ours**, and is carried here only so the whole
per-pixel picture sits in one place.

## And the divide already has a sweep

`RASTER.ATTRDIV.SVC` makes the unit count a build parameter, and the sweep is
linear, which is the property its test exists to check:

The sweep is now a GRID, because the divider's RADIX is a parameter too --
radix 4 takes two quotient bits a step, against 1x, 2x and 3x the divisor,
instead of one bit against 1x. A single unit drops from **36 clocks to 20**.

| UNITS | RADIX 2, divides/frame | RADIX 4, divides/frame |
|---|---|---|
| 1 | 46,296 | 83,333 |
| 2 | 92,571 | 166,597 |
| 4 | 184,928 | 332,502 |
| 8 | 367,985 | **658,978** |

Both axes are linear and they compose. The comparison a fit has to make is the
DIAGONAL: **radix 4 at UNITS = 4 (332,502) is within 10% of radix 2 at UNITS = 8
(367,985), for half the units.** Whether four wider dividers are cheaper than
eight narrow ones is a place-and-route question, and this grid is what makes it
answerable rather than arguable.

THE ANSWERS DO NOT MOVE. Both radices run the identical case list against the
same oracle -- the exact halves, both signs, the 400-case random sweep and the
broken preconditions -- and both are bit-exact against it. Radix 4 is the same
divider going faster, not a different one that mostly agrees.

---

## How many of each does a frame actually want?

This is where it stops being a measurement, so the arithmetic is written out
rather than asserted, and the one number that is not known is left as a
parameter.

Per **covered** fragment, before early-Z:

* 1 divide, for `invw24`. Every covered pixel pays this. Nothing else.

Per **surviving** fragment, after early-Z, if it is textured:

* 2 divides, for `u_over_w` and `v_over_w`
* 1 `PERSPUV` (which is 1 reciprocal and 2 products)
* 1 TMU sample, plus an AUX sample if terrain

and if it is also Gouraud-lit, 4 more divides for r, g, b and alpha.

Let **s** be the fraction of covered fragments that survive early-Z. **We have
not measured s** — it needs the real traces that step 9 of the architecture
calls for, and guessing it here would be exactly the sort of confident wrong
number this project has already paid for twice. So the table gives three values
and says which column any given scene lands in only once s is measured.

Divides per frame, for 276,480 covered fragments:

| s | textured only (1 + 2s) | textured + Gouraud (1 + 6s) |
|---|---|---|
| 0.25 | 414,720 | 691,200 |
| 0.50 | 552,960 | 1,105,920 |
| 1.00 | 829,440 | 1,935,360 |

Against the grid above:

* **radix 2 at UNITS = 8 (367,985) covers no column of that table.** It covers
  `invw24` alone.
* **radix 4 at UNITS = 8 (658,978) covers the textured-only column up to
  s = 0.5**, and comes within 5% of textured + Gouraud at s = 0.25.
* nothing measured yet covers **textured + Gouraud at s >= 0.5**, which wants
  1.1 to 1.9 million divides a frame.

So the shorter divider closed roughly half the gap in one parameter. How much of
the rest needs closing depends entirely on s, and on whether Gouraud is
interpolated per pixel at all. Those are the two open questions and neither is
settled here.

---

## What this changes about the shape of the work

**The divider was the wall, and radix 4 moved it once.** 36 clocks to 20 is
measured, and it cost one parameter. What is left on that axis is a fully
pipelined array (one result a clock, at 33 stages of area) or radix 8/16, both
trading more per-step logic for fewer steps. None of them should be chosen
without a fit, and the grid above is what a fit compares against.

**Early-Z is worth more than any arithmetic here.** The gap between the two
columns of that table is entirely Gouraud, and the gap between `1` and `1 + 6s`
is entirely survivors. Ruling 6's ordering — affine `invw24`, then early-Z, then
everything else — is doing more for this budget than any width change could.

**s is the missing measurement and it is worth getting early.** Every number in
the second table swings by a factor of four across the range of s, and no amount
of care about the units matters next to knowing it. It comes from the real 8 km
map / army / giant traces, which is step 9 of the architecture and is currently
scheduled after the blocks that depend on it.

**Three independent units all landed within a factor of two of the budget.**
`RCP24` at 238,095, `AUX` at 277,778 (ruling 7) and `PERSPUV` at 151,515 against
276,480. That is not a coincidence to be pleased about — it means there is no
slack anywhere on the per-pixel path, and any one of them regressing slightly
puts the frame over.

---

## SUPERSEDED: the divide crisis was an implementation artefact

**Everything below about the divide being 2-5x short is now historical.** It was
correct for one divide per attribute per pixel, and that is no longer the
architecture.

`RASTER.ATTRSTEP` replaces the per-pixel divide with an exact quotient/remainder
recurrence (owner ruling 2026-08-31 #2). Measured on a fully covered tile:

    256 pixels cost 17 divides = 0.066 a pixel, against 1.000
    = 15.1x fewer divides, and NOT ONE RENDERED BIT CHANGES

Proved twice: `tests/proofs/attribute_step_equivalence.cpp` over 640,000
pixel-attributes in arithmetic, and `raster_attrstep_directed` against the
divider RTL itself over 1,536 more, including 69 sign crossings and 256 exact
halves.

### What that does to the demand table

The worst column below -- textured + Gouraud at s = 1.0, 1,935,360 divides a
frame -- becomes roughly **128,000 seed and step divides**, which radix-4
`UNITS = 1` already covers four times over. The right question is no longer "how
many dividers" but "how few".

| | divides a frame | covered by |
|---|---|---|
| per-pixel, textured, s = 0.5 | 552,960 | radix-4 UNITS = 8, barely |
| per-pixel, +Gouraud, s = 1.0 | 1,935,360 | nothing measured |
| **stepped, +Gouraud, s = 1.0** | **~128,000** | **radix-4 UNITS = 1, 5x over** |

And the second-order effect matters as much: **Gouraud RGB and alpha stop being
four expensive divides a survivor and become four cheap accumulators.** Cutting
vertex lighting to save divides is no longer a trade anyone has to consider.

### What is still open

The recurrence is a PROTOTYPE in the ruling's sense. Before `UNITS` is frozen it
needs the composed tile test against the numerator-plane path, a resource and
Fmax comparison against the divider farm, and the context-cache experiment. The
divider path stays as oracle and fallback until those are green.

---

## The 276,480 is ambiguous, and it changes three verdicts

Every "SHORT" in this file and in three block tests is measured against ruling
7's **276,480 terrain-primary** figure. `tools/render/count_fragment_load` now
measures the fragment count of a full-screen terrain pass directly, with the
shipped oracle, and gets **92,160 — exactly one per pixel, at 1.00x overdraw.**

276,480 is exactly 3 x 92,160, and that leaves two readings:

* **276,480 is FRAGMENTS.** Then the terrain-primary component assumes 3x
  overdraw (or three layers, or a larger canvas), and the per-fragment units are
  short as this file says.
* **276,480 is DIVIDES.** A textured fragment needs three — `invw24`,
  `u_over_w`, `v_over_w` — so 92,160 pixels is 276,480 divides exactly. Then it
  is a figure about the DIVIDE and comparing per-fragment units against it is a
  category error.

**Ruling 7 uses it for AUX requests, which are per fragment, so the first
reading is the one it intends.** That is the conservative one and it is what the
tests assert against, which is the right default. But it should be confirmed
rather than inherited, because under the second reading the picture changes:

| unit | per frame | vs 92,160 fragments | vs 276,480 fragments |
|---|---|---|---|
| `RASTER.PERSPUV` | 151,515 | **1.64x, sufficient** | 0.55x, short |
| `RASTER.RCP24` | 238,095 | **2.58x, sufficient** | 0.86x, short |
| `TEXTURE.TMU` CLUT | 555,555 | 6.03x | 2.01x |
| `TEXTURE.AUX` | 277,778 | 3.01x | 1.00x |

So whether **PERSPUV and RCP24 need replicating at all** turns entirely on what
that one number means. Both are comfortable against a measured full-screen pass
and both are short against ruling 7's estimate of it.

**This is not resolved here.** What is now measured is that a full-screen
terrain pass is 92,160 fragments; what 276,480 counts is a question for whoever
wrote it. The units stay sized for the conservative reading until then, because
being 3x over-provisioned costs area and being 3x under-provisioned costs the
frame.

## What is measured, and what is not

**Measured, against running RTL:** every clock count in the first table, the
whole of the UNITS x RADIX grid, and the fact that both axes are linear.

**Not measured:** the survivor fraction s; the real covered-fragment count for
an actual 8 km map frame (276,480 is ruling 7's estimate for the terrain
component alone, and excludes sky, creatures and objects); and the fit of any of
these blocks, which nothing here has synthesised.

**Explicitly not decided here:** the divider's architecture, the UNITS for any
unit, and whether Gouraud interpolation is worth six divides a survivor. Those
are choices, and this file is the evidence for making them, not the making.
