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

| UNITS | clocks per divide | divides per frame | against 276,480 |
|---|---|---|---|
| 1 | 36.00 | 46,296 | short, 6.0x |
| 2 | 18.00 | 92,571 | short, 3.0x |
| 4 | 9.01 | 184,928 | short, 1.5x |
| 8 | 4.53 | 367,985 | **sufficient**, 1.33x |

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

Against the sweep above, **UNITS = 8 (367,985 a frame) does not cover any column
of that table.** It covers `invw24` alone and nothing else. The honest reading:

* the divide count needed is **2 to 5 times** what the largest measured sweep
  point delivers;
* so either UNITS goes well past 8, or the divider gets shorter than 36 clocks,
  or fewer attributes get divided per pixel.

All three are open. None of them should be chosen from this file.

---

## What this changes about the shape of the work

**The divider is the wall, and it is a wide one.** A radix-4 divider halves the
iterations for roughly double the per-step logic; a fully pipelined array reaches
one result per clock at 33 stages of area. At 36 clocks and needing 2–5x more
than eight units deliver, this is no longer a "tune it later" item — it is the
thing that decides whether the textured path fits, and it wants a fit before
more blocks are built on top of it.

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

## What is measured, and what is not

**Measured, against running RTL:** every clock count in the first table, the
whole of the UNITS sweep, and the fact that the sweep is linear.

**Not measured:** the survivor fraction s; the real covered-fragment count for
an actual 8 km map frame (276,480 is ruling 7's estimate for the terrain
component alone, and excludes sky, creatures and objects); and the fit of any of
these blocks, which nothing here has synthesised.

**Explicitly not decided here:** the divider's architecture, the UNITS for any
unit, and whether Gouraud interpolation is worth six divides a survivor. Those
are choices, and this file is the evidence for making them, not the making.
