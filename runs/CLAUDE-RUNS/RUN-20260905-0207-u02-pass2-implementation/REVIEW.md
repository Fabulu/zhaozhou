# REVIEW — creature 02, pass 2

**Reviewer run:** adversarial review of RUN-20260905-0207-u02-pass2-implementation
**Date:** 2026-09-05 · **Judged at:** HEAD `3c211c85` (zhaozhou), `6fc63994` (Upheaval)
**Standard:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-2-2026-09-04.md`, the nine
acceptance items.
**How:** own `build-direct.sh cel` build; rendered and **looked at first**, on the
**shipped lit path** (`ZIXX_EXP=celmain` + the u02 many-coloured moving rig) at
**native 384x240** plus close-ups; frames read with `tools/reel/rgbframe.py`.
Numbers only afterwards, and only on the comparison side.
Evidence: `evidence/review/` (12 plates, `MEASUREMENTS.txt`, `review_probe.cpp`).

---

## VERDICT AT A GLANCE

| # | Acceptance item | Verdict |
|---|---|---|
| 1 | No part floats/clips; every hinge boned | **PASS** |
| 2 | Gas rim much thicker, very visible, see-through | **NEEDS AN OWNER EYE** (real gain, short of the ask) |
| 3 | Pink darker and stronger, matching the drawings | **PASS with a reservation** (darkness, not pigment) |
| 4 | Eyes match the front view, protrusion kept, expressive | **FAIL** |
| 5 | Old particles gone; new mana big/smeary/plasma; alternatives exist | **PASS with one dead candidate** |
| 6 | There is lightning mana | **FAIL on the read** (mechanism present) |
| 7 | More expressive than Zixxtrixx | **NEEDS AN OWNER EYE** (body yes, face no) |
| 8 | Clip list covers bobbing, hasty, falling, hits, taunts | **PASS** |
| 9 | Many-coloured lighting is the presentation | **PASS** |

Plus: **Zixxtrixx untouched — PASS, reproduced independently by the reviewer.**

---

## WHAT IS GOOD — protect all of this

This pass moved a great deal in the right direction, and the next pass should
not spend any of it.

* **The loop is genuinely closed, and the closure is honest.** The floating
  dongle is gone. I rebuilt the committed probe and it reproduces the
  implementer's numbers *exactly* (887 / 886 pm, 160 mm). I then wrote my own
  probe to attack the two gaps I found in theirs — and the departure survives
  both (see the departure section). Through the whole tumbling fall clip the
  ring never opens and the return arm stays buried.
* **It really does float.** Clearance is **339–510 mm** over every clip at both
  subs. My own first impression from the 2D frames was "it is sitting on the
  dirt" — that was me walking straight into `CLAUDE.md`'s perspective trap. The
  3D probe is right and my eye was wrong. Worth recording.
* **The motion rework is a large, real win.** Against the pass-1 benchmark of
  **4 px across a 180-frame clip**, fixed-camera clips now move **20–32 px** of
  silhouette centroid and the fall **96 px**, with **24–29%** area swing
  (117% on the fall). The squash/stretch is unmistakable and the hinge play in
  the taunts is exactly what section 4 asked for — the loop visibly waggles.
* **The many-coloured rig is the presentation, and it works.** Four sources,
  each visibly laying its own hue across the body. Item 9 is comfortably met.
* **The pulsar genuinely pulses now.** 3,217 px of area swing against the old
  strobe's 150 px of 92,160. The named old fault is actually fixed.
* **Zixxtrixx is untouched, and I proved it myself** rather than taking the
  claim — including `zixxtrixx-moving-light`, the one subject that exercises
  the changed rig pointer.
* The probe is **committed**, per `CLAUDE.md`. That is the right habit.

---

## THE DEPARTURE FROM THE PLAN — verified SOUND

The plan (Stage B.2) said bind the last rings to a second anchor bone. The
implementer says that is geometrically impossible in this skinning model and
went closed-form on hinge D instead, declaring it.

**I accept the departure, and the closure holds.** Two gaps in its probe, both
tested (`evidence/review/review_probe.cpp`):

1. **The bank closure loop tests `sub=0` only** — the 60 Hz interpolated
   midpoints are never closure-tested. Interpolating between two individually
   closed poses is exactly how a shape opens mid-motion (`CLAUDE.md`: "the fault
   was the shape changing during the rotation"). **Tested: clean.** Worst case
   is still `sub=0`. The closed-form aim survives interpolation.
2. **It tests three ring *centreline* points**, but the terminal blade radius is
   130 mm against a 450 mm body. **The real terminal-ring rim reaches 1206 pm**
   — 20.6% outside the probe's body ellipsoid at the same worst frame. This is
   **not** a visible punch-through (the body is a tapered teardrop, not that
   ellipsoid, and the render shows no breakout), but it means the 920 gate is
   measuring something considerably more forgiving than "the arm is buried".
   The gate is not wrong; it is looser than it reads. Worth tightening to real
   rim vertices next pass.

Its probe **was** caught lying twice before, so I re-verified what it now
asserts: root-local frame handling and full root translation are both correct,
and all three headline numbers reproduce to the digit.

---

## THE FAULTS, RANKED BY DAMAGE TO THE READ

### 1. THE FACE DOES NOT READ AS A FACE AT THE ANGLE IT SHIPS AT — the worst fault

Direction 2 says the eyes "matter most", are "the whole face", and are "so
important for its personality". At **native 384x240**, on the **shipped
three-quarter camera used by every showcase clip**, the face reads as **a bright
white diagonal slash with two teal specks beside it** — a zipper, a scar, a fish
hook. Not two eyes. (`01-native-384x240-the-shipped-read.png`,
`02-face-at-the-shipped-three-quarter.png`.)

Three separate causes, all fixable, and importantly **the geometry is closer than
the read suggests**:

* **The Λ traces correctly in the front view.** `03-front-the-lambda-traces.png`
  shows a real Λ converging at the top, purple almonds, white rings, teal stars.
  The V-sign flip landed. This is genuine progress and should be protected.
* **But the almonds are far too long and thin for the sheet's plump teardrops**,
  and at three-quarter they foreshorten into a splinter, so the white ring —
  which should be a compact ring *around* the star — stretches into a long bright
  arc that becomes the brightest thing on the creature and captures the whole eye.
* **The star never grew.** Stage D asked for a star *wider than the purple band*.
  Measured on the lit path, purple runs **78–90%** of lens area (target 65–70%,
  overshot) and the teal star only **5.5–9.8%**. The purple field swallowed the
  lens. On the sheet the star is a fat, dominant shape inside each almond.

Note the area of white is only 10–20% — *the fault is its shape, not its area*.
This is the art law in miniature: a small-area measurement that looks fine while
a 1–2 px bright arc 30 px long dominates the read.

### 2. THE PROTRUSION OVERSHOOTS INTO A BOLT-ON

The protrusion is **protected** — the artist likes it, and the Description sheet
even draws it in cross-section ("*abstehendes Auge schräg von hinten
betrachtet*"). It must not be flattened. But right now the outer eye does not
read as a googly eye standing proud; it reads as **a detached slab hanging
outside the body silhouette with its own black ink box around it**, breaking the
outline (`04-eye-outside-the-silhouette.png`, `07-hover-rear-eye-slab.png`, and
visible as a dark fin all through the fall clip).

The implementer flagged this as "the PROTECTED 3D protrusion reading in 2D
profile". That is a fair description of the mechanism but it is **not absolution**
— on the sheet the eyes sit wholly inside the body outline, and the protrusion is
meant to be a low dome toward the viewer, not a plate cantilevered out sideways.
The probe says 160 mm against a protected ~166 mm, so the *depth* is right; the
problem is the **lengthened, splayed almond swinging its tips out along the
radial**, which is precisely the risk the plan's Stage A.3 named and told the
implementer to pull back. It was measured, but it was not pulled back.

Fix the almond aspect and this fault and fault 1 both shrink.

### 3. LIGHTNING DOES NOT READ AS LIGHTNING

Acceptance 6 is an item the plan itself called uncuttable, and the artist's own
Description sheet makes bolts *the creature's whole identity* ("*Durch die Antenne
generiert sie dann lähmende Energieblitze*" … "*Gibt knisternde Laute von sich*").
The `FX.LIGHTNING` recurrence is real and it does strike — it has the highest
event ratio of the six (2.23x). But what it draws is **a string of discrete round
beads with visible gaps** — no continuous filament, no branch, no jag
(`08-lightning-reads-as-beads.png`, at the *hottest* frames). At native it is a
faint pale smudge inside the loop.

The plan asked for exactly the fix that is missing: "stamp along segments at
~2 px so the path is continuous, two layers". The stamping is too coarse. It is
also the smallest effect of the six by an order of magnitude (489 px mean footprint
against boil's 4,911).

**Mechanism present, hardware asks correctly filed — but the acceptance item is
about there *being* lightning, and what a viewer sees is beads.**

### 4. ONE OF THE SIX MANA CANDIDATES IS DEAD

The menu is real and the owner does get a choice — but the count of six is
overstated. **Drip shows under 20 px of effect in 579 of its 600 frames**, and at
its hottest frame it is **2–3 tiny hard-edged blue squares**, not the planned
"3–4 LARGE opaque droplets" (`09-drip-three-blue-squares.png`).

The plan's own cut order put drip first to cut. Shipping it broken is worse than
cutting it. Effective menu: **five candidates**, which still meets the plan's
stated floor.

Of the five that read: **boil is the strongest and clearly legible at native**;
pulsar reads as a soft glowing head; plasma and bullets read but are **pale
white/pink rather than the "big BLUE plasma" the direction asked for** — additive
over the bright peach sky washes them out, and the bullets show visible triangle
faceting inside each splat.

### 5. THE GAS RIM IMPROVED BUT DOES NOT RING THE SILHOUETTE

The lever was right and it moved the right way: measured on each pass's own
shipped path, the rim is **thicker at every threshold** (mean 1.4–1.8x, max
30 px → 48–64 px, rays with no rim 73–97% → 45–76%). That is real progress and
the ambient knob was the correct choice.

But the TASK_LOG says "the rim rings the whole silhouette in every showcase
clip", and that is **not supported**: the median rim width is 0–1 px and
**45–76% of silhouette rays carry no rim at all**. By eye at native I do not see
a thick see-through gassy layer — I see an ink line and an ordinary toon
terminator. R4's own named risk (a one-sided key gives a terminator, not a rim)
has partly materialised: there is rim where the light grazes and none elsewhere.

Direction says "**very visible**". It is not yet. The owner's eye should settle
it — and R4's declared fallback (the translucent shell) is the honest next lever.

### 6. THE CREATURE SPENDS TOO MUCH OF THE LOOP AS DARK MURK

Item 3 asked for **darker and stronger** pink. The *pigment* change succeeded —
in lit frames the body is a hot crimson-magenta genuinely close to the sheets
(`06-hover-closeup-lit-path.png`), and the de-blueing to crimson was the right
call. But the ambient was then dropped hard to buy the rim, and across a large
part of the orbit the creature is a **dark purple-brown lump with no readable
pink at all** (`12-hover-contact-sheet.png`, rows 2 and 4). "Darker pink" and
"unlit" are not the same thing, and the drawings are never dim.

This is a *rig* problem, not a pigment problem — which matters, because it means
the fix is the ambient/fill balance, not repainting a colour that is now right.
It also puts items 2 and 3 in direct tension: **the rim was bought with the
pink.** That trade should go to the owner explicitly.

### 7. SMALLER RESIDUALS — the implementer's own three, confirmed and ranked

* **The ring's hole reads smaller than the sheet's tall egg** — confirmed, and
  pose-dependent: in the s4 side plate it reads as a small bean and the whole
  creature says *kettlebell* (`05-side-vs-sheet.png`), but in the shipped clips
  at other angles the opening reads much closer to the sheet
  (`07-hover-rear-eye-slab.png`). Middling severity; worth one knob turn, not a
  rebuild.
* **The Λ apex sits lower than the traced 0.68 R** — confirmed, and it compounds
  fault 1: apex low + over-splayed legs turns the sheet's two steep teardrops
  into a flat moustache. Rank it *with* the eye work, not separately.
* **The front slot reads as a slit** — confirmed, lowest severity of the three.
* **Not on their list:** the buried-arm junction shows a hard-edged pale facet on
  the body through the fall clip (`11-fall-loop-stays-closed.png`). Stage B.5 was
  meant to address that seam; it is still visible, now as a pale wedge rather
  than an ink hairline.

---

## ONE PROCESS FINDING

The standing "ALWAYS LOOK" recon on the final build was done on
`PLATE-front.png` / `PLATE-side.png`, which are **`u02-s4-*` renders**.
`subject_u02_s4()` never sets `creature_moving_light`; only `subject_u02_clip()`
does. **So the by-eye recon was judged on a rig the creature does not ship
under** — and acceptance 2's "PASS by eye under the u02 moving rig" was
therefore asserted from plates that were not on that rig.

This is a milder cousin of the black-faced-publish failure this creature already
suffered, and it plausibly explains why the rim was called visible and the face
was called good: **the front view on the s4 rig genuinely does look better than
the shipped three-quarter under the moving rig.** Next pass: judge on
`subject_u02_clip` output, or give s4 the shipping rig.

---

## WHAT I WOULD DO NEXT, IN ORDER

1. **The face.** Shorten and fatten the almonds toward the sheet's teardrop;
   grow the teal star until it is wide relative to the purple band as Stage D
   asked; make the white a compact ring, never a long arc. Judge at native on
   `unnamed02-taunt`, not on s4.
2. **Pull the eye tips back along the radial** until the assembly sits inside the
   silhouette at three-quarter — keeping the 160 mm protected depth, which is
   already right.
3. **Lightning: tighten the stamp spacing** until the path is continuous. This is
   the one uncuttable acceptance item still failing.
4. **Give the owner the rim/pink trade explicitly**, with a brighter-ambient
   variant beside the current one.
5. **Cut or fix drip**; say five candidates if it is five.

---

## Reviewer's summary

The structural half of this pass is good work, honestly declared, and where I
attacked it, it held: the loop closes, the probe tells the truth, the creature
floats, Zixxtrixx is untouched, and the animation went from 4 px to a creature
that actually acts. The presentation half is not there yet, and it fails on the
one thing Direction 2 says matters most — at native, at the angle it ships at,
the creature has no face. Everything blocking that is a knob-and-page problem on
geometry that is already close, not a rebuild.

**Recommendation: another pass, scoped to the face, the lightning stamp, and the
rim/pink balance.** Nothing structural needs reopening.
