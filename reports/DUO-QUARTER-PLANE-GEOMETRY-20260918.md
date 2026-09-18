# The Duo quarter-res plane is sized from the wrong surface

Raised by the POST.COMPOSITE worker as a contract disagreement it could not
settle. Settled here, arithmetically, against `fpga/rtl/common/zhao_pkg.sv`.

## The canonical geometry, from the package that owns it

```
Duo display     512 x 240          (h_active 512, v_active 240)
Duo RENDERED    2 x (256 x 192)    two views, x offsets 0 / 256, y offset 24
                                   "border rows are black" (spec/video_rules.md §3.1)
ZHAO_CANVAS_BYTES_DUO     196,608  = 2*256*192*2   -- what a frame STORES
ZHAO_DISPLAYED_BYTES_DUO  245,760  = 512*240*2     -- what is DISPLAYED
```

`zhao_pkg.sv` already warns, in its own comment, that confusing these two is a
silent Duo bug — it says so about `DEBUG.CRC`'s `expect_bytes_i`. **The same
confusion has happened again, one layer up, in the post planes.**

## The test that separates the two readings

Horizontally there is no ambiguity: 2 x 256 = 512, the full width. The whole
disagreement is **vertical** — 192 rendered rows inside a 240-row canvas, 24
border rows top and bottom.

| mode | displayed | rendered px | quarter(displayed) | quarter(RENDERED) | contract plane |
|---|---|---:|---:|---:|---|
| Z60 | 384x240 | 92,160 | 5,760 | **5,760** | 96 x 60 = 5,760 |
| Storm | 320x240 | 76,800 | 4,800 | **4,800** | 80 x 60 = 4,800 |
| Duo | 512x240 | 98,304 | 7,680 | **6,144** | 128 x 60 = 7,680 |

**Z60 and Storm cannot distinguish the two readings** — their displayed area IS
their rendered area, so both give the same number. **Duo is the only mode where
the surfaces differ, and it is the only mode that is wrong.** That is exactly why
it survived: the rule "quarter of the displayed canvas" is correct for two modes
out of three, and the third is the one with borders.

## The proof is inside the contract's own throughput table

`POST.COMPOSITE.md` lines 131–132:

| mode | main | glow prep | total |
|---|---:|---:|---:|
| Z60 | 92,160 | 11,520 | 103,680 |
| Duo | 98,304 | 15,360 | 113,664 |

Check each cell against both surfaces:

```
Z60  main 92,160 == rendered px           TRUE
Z60  glow 11,520 == 2 x quarter(displayed) TRUE
Z60  glow 11,520 == 2 x quarter(RENDERED)  TRUE   <- indistinguishable
Duo  main 98,304 == rendered px            TRUE
Duo  glow 15,360 == 2 x quarter(displayed) TRUE
Duo  glow 15,360 == 2 x quarter(RENDERED)  FALSE  <- the defect
```

**One row of one table uses two different surfaces.** Its main pass counts
RENDERED pixels (98,304 = 2 x 256 x 192) while its glow pass counts DISPLAYED
canvas cells (15,360 = 2 x 7,680). The x2 on glow is the separable blur's
horizontal and vertical passes, so the factor is right and the base is not.

### Corrected

```
Duo glow prep  2 x 6,144 = 12,288   (was 15,360)
Duo frame cost 98,304 + 12,288 = 110,592   (was 113,664)
```

Overstated by **3,072 work items, 2.7%**. Z60 and Storm are unaffected and their
numbers stand.

## The ledger was already right

`design/blocks.yml`'s POST.COMPOSITE purpose line, describing the radial_decay
sun-shaft mode, reads: *"frozen constants 96x60 Z60 / **2x64x48** Duo"*.

`2 x 64 x 48 = 6,144`, and `2 x 64 = 128`, so that is the same plane width with
the correct height — **128 x 48**, not 128 x 60. The ledger and the two contracts
disagree, and the ledger is correct.

## Why the 12 extra quarter-rows can never carry content

They map to the 48 display rows outside the two views. Those rows are black
border (`zhao_pkg.sv`) and in any case both contracts require **clamping inside
each Duo view before forming addresses** — so no displaced sample can ever read
or write them. They are provably dead cells, not spare ones.

One detail the worker got slightly wrong, corrected here: it called those 48 rows
"HUD scanlines". `zhao_pkg.sv` calls them black border rows. The owner plan's
§11.3 also says "the 48 HUD scanlines". Whichever is true, **it does not change
this finding** — HUD is composited last and explicitly bypasses post, and border
is black, so neither needs an effect-plane cell. But the two descriptions of the
same 48 rows should be reconciled by whoever owns `spec/video_rules.md`.

## What to change, and what NOT to claim

1. **`POST.GATHER.md` and `POST.COMPOSITE.md`**: Duo plane is **128 x 48 =
   6,144 cells**, addressed per view (64 x 48 each), not 128 x 60.
2. **`POST.COMPOSITE.md` throughput table**: Duo total **110,592**.
3. **The real benefit is not the memory.** The owner has ruled that spending
   M10K is fine. Saving 1,536 cells is not the point and should not be sold as
   one. The point is that **view-local addressing makes the per-view clamp
   STRUCTURAL** — with two 64 x 48 planes, a displaced sample cannot reach the
   other view because there is no address that names it. With one 128 x 60 plane
   the clamp is a comparator that has to be right, and "no bleed between them"
   becomes a thing to test rather than a thing that cannot happen.
   That is an ALM saving and a whole bug class removed, in that order of
   certainty: the bug class definitely, the ALMs unmeasured.
4. **Do not claim a fitted number from any of this.** Nothing here has been
   through synthesis.

---

## Checked against `spec/video_rules.md` — "are the missing rows just off-screen?"

Owner asked directly. **No. They are displayed.** But there IS a nearby
ratified decision that sounds like this, and it is a different one.

### What §3.1 actually says

> "Both view canvases are vertically CENTERED in the 240 active lines: rows
> 0..23 and 216..239 display the border colour `16'h0000` (black)."
>
> "**The 48 border rows are part of the displayed stream and therefore part of
> the displayed-frame CRC.**"

And §1:

> "Duo stores only its two 256x192 view canvases and stores **no** border rows —
> **the 48 black border lines are generated at scanout**, so Duo occupancy is
> 0x30000, not 0x3C000."

So the 48 rows are: **displayed yes; inside the CRC yes; stored no; rendered no.**

### This makes the correction STRONGER, and for a cleaner reason

The write-up above argued the cells are dead because the border is black and
because a displaced sample may not leave its view. Both true, but the decisive
fact is simpler and structural:

**The border rows are generated at SCANOUT. Post runs before scanout.** Nothing
upstream of the formatter — render, resolve, or post — ever produces, consumes or
addresses those rows. A quarter-res effect plane cell covering them is not
"usually zero"; it is unaddressable by the stage that owns the plane.

`128 x 48 = 6,144` stands, and the reason is now a pipeline-order fact rather
than a content argument.

### The decision the owner was probably remembering

`spec/video_rules.md` §1 and §3.1 record a ratified resolution (2026-08-15,
review MAJOR-3) that reads very much like "those rows don't count":

> "Earlier revisions of this table listed 0x3C000 in the canvas column for Duo,
> which read as a contradiction of §3.1; **it never was one — it was the
> allocation.**"
>
> "a Duo frame therefore occupies `0x30000` bytes of its `0x3C000` slot, and
> **the last `0xC000` bytes are untouched and outside both CRCs**."

That is **allocation versus occupancy** — every slot is allocated the largest
canvas so a mode switch never moves a slot, and Duo's unused tail is ignored.
It is about the 49,152-byte tail of the SLOT, not about the border rows, and the
two have opposite CRC status: the slot tail is **outside** both CRCs, the border
rows are **inside** the displayed CRC.

Easy to conflate, and worth keeping apart, because a change that treated the
border rows as "outside the CRC" would silently break `DEBUG.CRC`.

### A third discrepancy, found on the way

The owner plan's §11.3 says *"Include the **48 HUD scanlines** and per-view
clamping in captures."* `spec/video_rules.md` **does not mention HUD at all**,
and no spec or contract file in the tree contains the phrase "48 HUD scanlines".
Those 48 rows are black border by the ratified spec.

Either the plan means a different 48 rows, or it inherited the number and
misattributed it. This does not change the plane geometry — HUD bypasses post
either way — but "the 48 rows are HUD" is now written in an authoritative-looking
document and is not supported by the video spec. Flagged, not resolved: the
owner of `spec/video_rules.md` should say which.
