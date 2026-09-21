# Findings 01 — the per-clip press ladder, chosen by eye

Written after the looks, before the next change. Plates `P23-LOOKS/01`–`06`.
Production ink throughout (`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`),
native `.rgb` at 4x NEAREST, cropped on the antenna, JPEG q80.

The five faint slots are, by frame count against the bank manifest:
**1 Drift (300f), 14 Damage (464f), 17 Death-drop (450f), 18 Death-gutter (590f),
20 Blown (292f).**

## The four that answered

| slot | clip | p22 | chosen | R7 rot | what I saw at the dip bottom |
|---|---|---|---|---|---|
| 1 | drift | 730 | **900** | 7.10 -> 28.18 deg | 730 is a flat open loop lying horizontally. 850 tilts it. **900 turns it into a clear diagonal wedge with the green pocket standing upright** — at Drift's 128 px that is the difference between "a bright smear on the antenna" and "the lightning turned". 950 narrows it to a vertical sliver and the loop stops reading as a loop. |
| 14 | damage | 635 | **780** | 4.49 -> 32.99 | 635 is a broad upright double loop with a wide open right wing. 720 rolls the right wing down. **780 reads as a tilted spiral: the wing lies almost flat, the inner curl tightens, and the green pocket becomes a distinct bright lozenge.** 840 adds nothing and begins flattening the loop into a plate. |
| 17 | death-drop | 635 | **740** | 4.76 -> ~24 | 635 is a compact angular kite. 700 extends the right end. **740 is a clean long blade across the top with a sharp left hook — the figure has plainly re-formed, and it is still anchored on the antenna.** 790 steepens the blade until it reads as detached from the arm. |
| 18 | death-gutter | 590 | **720** | 1.70 -> ~33 | This is the one where the PRESS itself is the visible half. 590 leaves the antenna arm a broad flat plate and the strand a flat hook. 680 curls the strand a little. **720 presses the arm down convincingly and the strand turns and kinks with it.** 760 folds the arm sharply enough that it reads as a collapsing arm rather than a kneading one — backed off, per the brief's spasm clause. |

## The fifth — slot 20, Blown — **THE LEVER IS INVERTED, and I did not force it**

The direction's premise (raise the press, get more reaction) is true on four
clips and **false on Blown**, measured two independent ways:

**By the gate's descriptor (R7 rot), sweeping the share:**

| share | 300 | 400 | 500 | 600 | 700 | 740 | **815 (shipped)** | 900 | 1000 |
|---|---|---|---|---|---|---|---|---|---|
| rot deg | 46.69 | 28.26 | 15.30 | 9.71 | 6.29 | 4.90 | **3.70** | 2.28 | 0.65 |
| R5 B-lowest mm | -189 | -189 | -132 | -63 | -3 | +17 | **+45** | +56 | +56 |

**By the thing itself** — `dip_pm` read off the reel's own `U02_FOLD_DEBUG`
trace on complete Blown, not off the gate's proxy:

| share | 400 | 500 | 700 | 815 | 1000 |
|---|---|---|---|---|---|
| peak `dip_pm` | 102 | 70 | 31 | **18** | 3 |
| frames with any dip | 42 | 30 | 15 | **10** | 5 |

Both agree: **a deeper press gives Blown a SMALLER sag**, and the reaction rides
on the sag. The mechanism is visible in the source: `sag = mid_y(A,C) - B_y`,
and the dent's own ambient duck (`kKneadDentAmbientDuckPm`) scales the ambient
A/B/C offsets down in proportion to the press. Blown's ambient pose already
carries B below the A/C line — that is what being knocked backwards looks like —
so the duck **removes a sag that was already there** faster than the dent adds
one.

So the only direction that raises Blown's reaction is DOWN, and it breaches
`kGateDipMarginMm` (20 mm, R5's B-strictly-lowest floor) almost immediately:

* 780 pm: margin +34 mm, rot **4.47 deg** — inside the bound and visually nothing.
* 740 pm: margin +17 mm — **breaches by 3 mm** — rot 4.90 deg, still nothing.
* 500 pm: margin **-132 mm** — **breaches by 152 mm** — rot 15.30 deg, the first
  setting that would actually read.

**Blown is left at its shipped 815.** Buying a readable reaction there costs
152 mm of the B-lowest bound, which is the bound Direction 21 item 2 was about
and which the brief says must not move. The honest lever for Blown is the
ambient duck, which is bank-wide and would move all 19 hosting clips — a
separate decision for the owner, not something to slip into this pass.
