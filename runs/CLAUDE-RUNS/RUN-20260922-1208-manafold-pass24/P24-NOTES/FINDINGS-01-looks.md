# P24 look notes -- written after each look, before the next change

Every plate here was read in the production presentation (`zhao-reel-cel.exe`,
`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`). Frames were chosen by
BADNESS, never by index: the lightning frames come from the probe's own per-frame
intersection count, and the ladder frames from `tools/reel/framediff.py`, which
is committed for the same reason `rgbframe.py` is -- three passes in a row have
needed "which frames does this knob actually change, and where on screen".

---

## Look 1 -- where the creature is, and where the fault is

`P24-LOOKS/01-hover-f0-2x.jpg`. The loop's pocket holds the blue/white lightning
figure. **The fault is visible in the first frame anyone looks at**: the figure's
left run lies across the near rod and its right run crosses the vertical band.
The owner's sentence is a description of this picture.

## Look 2 -- the clearance ladder, on the worst crossing frame

`P24-LOOKS/02-clearance-ladder-crackle-f28-5x.jpg`, rungs BEFORE / 25 / 46 / 60 /
80 mm, Crackle f0028 (the worst before-frame by the probe's own count).

| rung | what was seen |
|---|---|
| BEFORE | the blue figure runs over the right-hand band; on the left it sits on the rod |
| 25 | the crossing is mostly gone; the figure still grazes the left rod |
| **46 -- CHOSEN** | the figure sits inside the pocket, clear of both rods, at its own size and with its own shape |
| 60 | clear, and beginning to look pulled-in |
| 80 | **the figure is visibly squeezed** -- narrower, more vertical, a different shape |

80 is the first rung that draws attention to itself as a RESTYLE, which Direction
23 forbids. 60 is the rung below it and 46 is the one that reads best: it keeps
the figure's full width while the crossing is gone. The gate is separately clean
at 46 and 60 and leaves a residual graze below ~46 (1 segment of 42,364 at 40 mm,
4 at 25 mm); that is reported, not used to choose -- the eye picked 46 and the
measurement agrees with it rather than deciding it.

## Look 3 -- the two mechanisms, isolated

`P24-LOOKS/03-avoid-isolated-ab-5x.jpg` and
`P24-LOOKS/05-split-isolated-ab-6x.jpg`.

! THE FIRST A/B WAS WORTHLESS AND I NEARLY WROTE IT UP. The shipping render
carries items 1 and 3 as well, so "before vs after" on Hover showed a different
POSE, and the lightning question was unanswerable underneath it. Both plates here
are rendered with the rear, Front and eye layers at their pass-23 values, so the
only difference in the frame is the mechanism being judged.

* **3D avoidance (Crackle, Hover).** The figure no longer lies on the band. On
  Crackle f0028 the right-hand crossing is simply gone and the figure's size,
  colour, density and jag are unchanged. On the strip across Hover
  (`P24-LOOKS/04-hover-avoid-strip-3x.jpg`, f60/180/300/420/471/540) the figure
  is nudged off the rods at every frame and reads as the same lightning.
  **Declared:** at the tightest closure (Hover f471) the pocket is small enough
  that the push carries the figure to the OUTSIDE of the rods, where it wraps the
  loop instead of filling it. It is not a restyle -- the figure is intact -- but
  it is a placement the owner has not seen before and it is the one thing on
  these plates I would put in front of him first.
* **Depth splitting (Inspect).** The bolt **still crosses the antenna, exactly as
  before**. The line is marginally smoother -- the white core reads as one
  continuous filament rather than a chain of beads -- and the occlusion edge
  where it passes the band is a little crisper. That is all it does, and it is
  what the measurement predicted: the unsplit bolt was already stamped every
  26.6 mm, finer than the 46 mm rod, so there was no resolution to win.

**The comparison's answer, by eye: avoidance addresses the owner's complaint and
splitting does not.**

## Look 4 -- Hover's back ball

`P24-LOOKS/06-rear-ladder-hover-f485-6x.jpg` (rungs 400/280/200/140/90 at the
frame the ladder changes most) and `P24-LOOKS/07-rear-motion-strip-6x.jpg` (eight
consecutive frames at 400 and at 140).

**The stills are nearly indistinguishable and that is the finding.** The rear
ambient's whole authority over Hover is small: `framediff` puts the worst
single-frame difference between gain 400 and gain 90 at **198 pixels**, in a
30 x 16 native box at the body entry, and mrear reads the End's mean relative
angle on slot 0 moving only 2.2 -> 1.7 deg across that range while its MAX barely
moves at all (11.2 -> 10.9). The max is the authored swallow beat, which pass 19
deliberately left outside this knob.

! AND THE SCREEN-SPACE VERSION OF THIS MEASUREMENT WAS USELESS, which is worth
recording: frame-to-frame pixel energy in the rear box reads 129,794 at gain 400
and 129,774 at gain 90 -- a difference of 0.015 % -- because HOVER'S CAMERA
ORBITS and the whole creature sweeping through the box swamps everything the knob
does. A screen-space motion metric on an orbiting clip measures the orbit.

So: the knob the direction names is the right one for "finicky" -- it is the
high-frequency pair -- but on Hover it is **10-25 % of what the back ball is
doing**, and the rest is the authored swallow, the socket following the breathing
body, and the rear rod's own direction change as the loop closes. 170 is taken
because it is a clear further calming of the term that is actually jittery
without reaching the rigid end; the honest expectation is that it helps and does
not finish the job. The implementation report's open issues name the next levers.

## Look 5 -- Hover's front

`P24-LOOKS/08-front-ladder-hover-f399-4x.jpg`, rungs 1000 / 1200 / 1350 / 1700 at
the frame the gain changes most (12,508 changed pixels, against the rear knob's
198 -- the Front carries the whole antenna and the back ball carries itself).

| rung | what was seen |
|---|---|
| 1000 | today: the loop stands close to upright through the beat |
| 1200 | a touch more lean; easy to miss beside 1000 |
| **1350 -- CHOSEN** | the loop's lean reads plainly as more motion, and the performance is still the accepted one |
| 1700 | **the loop's whole attitude changes** -- it tips far enough to read as a different performance, not a livelier one |

1700 is the first rung that draws attention to itself. 1350 is the rung below it
and it is what "could move a little more" asks for.

## Look 6 -- the eye ambience ladder

`P24-LOOKS/09-eye-ladder-rest-5x.jpg` (one frame, five rungs) and
`P24-LOOKS/10-eye-motion-rest-8x.jpg` (five rungs x five frames, which is the one
that decided it -- a still cannot tell "alive" from "acting").

| rung | what was seen on Rest f250-f274 |
|---|---|
| 0 | the stars hold one attitude; the pair reads still |
| 250 | barely separable from 0 |
| 450 | a small drift; quiet |
| **600 -- CHOSEN** | the stars move, the lenses breathe a little; it reads as life |
| 800 | **the star starts riding toward the lens rim** and the eye begins to read as acting |
| 1000 | plainly a deliberate look, in a clip whose whole subject is resting |

800 is the first rung that draws attention to itself. 600 is the rung below it.

## Look 7 -- the complete clips

Every-frame production-ink sheets at `P24-SHEETS/` for Crackle (600), Hover
(600), Inspect (600), Rest (400) and Taunt II (240). Every frame is populated and
continuous; no pop, no dropped frame, and the loop seams close. Detail at this
scale is thumbnail-only and the verdicts above rest on the magnified plates.
