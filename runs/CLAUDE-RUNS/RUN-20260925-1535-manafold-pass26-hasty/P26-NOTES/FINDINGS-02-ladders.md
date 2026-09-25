# Look 02 — the ladders, and what each rung looked like

All rungs rendered from ONE binary via the same-binary env controls, production
ink (`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`).

## Staging — and three faults found by rendering, not by reading

The staging took five rungs because three separate things were wrong, and each
one produced a picture that looked like a different problem.

**1. The follow must scale with `cam_k`.** `cam_pitch` folds the aim in as NDC
`x = k*X/w + bias_x`. The creature's displacement per millimetre carries the k;
the aim offset does not. The moment Hasty took a tighter camera the two sides of
the follow stopped describing the same journey and the creature out-ran its own
tracking shot, with every constant individually correct.

**2. The follow's CLOCK was the frame index, not the animation phase.** The reel
advances then renders, so the last frame shows KEY 0 -- the creature's root has
already wrapped to the start of its journey while a frame-index lerp has the aim
at the far end of the sweep. They are a whole traverse apart and **the subject is
simply GONE for the seam frames**. Pass 25 survived this only because its root
was pinned at centre. Fixed with `SceneSubject::cam_bias_x_wrap_keys`, which
clocks the aim by `((f+1)/2) % keys`. The terrain snaps at the seam instead of
the creature, and on this staging that is very nearly invisible.

**3. The traverse was running along +X, which is 45 degrees off the screen.**
The u02 camera is a fixed three-quarter (`kU02FixedCamYawA16` = 0x2000). World
+X is therefore HALF LATERAL AND HALF DEPTH, so the creature visibly GREW and
SANK as it travelled -- it was running diagonally into the lens -- and no
lateral follow could ever hold it, because the bias corrects the horizontal half
while the depth half changes scale and height. R5's standing comment claims a
linear lateral lerp "tracks them exactly"; that is true only for travel parallel
to the screen. Travelling along `(cos, 0, -sin)` makes the rotated Z constant.
Size and height then hold flat across the clip, visibly.

**`kHastyFullFollowBiasX` is NOT `kU02HastyBiasX`.** That constant's own comment
says it was picked "at f235, the frame where it used to be gone" -- it is the
sweep that was *enough to keep the creature in shot*, never a calibrated
tracker, and it is short of full follow by ~2.7x.

And the calibration itself had a trap. Deriving it from the projection
("65536 units = 1 NDC = 192 px of a 384 px frame") gives 89,170 and is WRONG:
rendered, that left the creature drifting 164 px where the arithmetic promised
40. The viewport's horizontal NDC is ~2.49 wide, not 2.0. Solved instead from
two measured rungs -- follow 0 (+2.177 px/frame) and follow 800 (+0.771) -- and
**verified with a third** at follow 1000, which came back -0.167 px/frame. That
third render is the point: two rungs give a line, a third says whether the line
was real.

| rung | what it looked like |
| --- | --- |
| cam_k 148000 (pass 25) | creature is a teardrop, contour ink 1-2 px, no face to act with |
| cam_k 200000 | face legible, 32/66 px margins, safe but small |
| cam_k 250000 | touches the left edge on 8 frames at follow 820 |
| **cam_k 280000** | **chosen.** eyes clearly readable, 45/93 px margins, 0 touching |
| cam_k 320000 | 63/116 px margins at follow 900, but the traverse has to shrink to fit |
| follow 650 | 15 px from the left edge -- one bob from clipping |
| follow 750 | 174 px of travel, 15/62 margins. Too tight at this k. |
| **follow 820** | **chosen.** 113 px of travel, 45/93 margins, ground still streams |
| follow 1000 | body pinned dead centre; throws away the screen motion the owner liked |

## Cadence (`kHastyHurryBobCycles`), looked at over 24 consecutive frames

* **9** — one long smooth arc over the whole strip. An easy lope, not a hurry.
* **13 — CHOSEN.** A firm driving pulse, ~18 rendered frames, and the antenna
  trails and recovers without smearing.
* **17** — the antenna is agitated frame to frame and the read tips from
  hurried into panicked. This is the "frantic rather than hurried" line, and it
  is between 13 and 17.

The direction-change count over the tracked centroid was tried first as a
cadence measure and is **useless** -- it returns 34, 34, 42 for 9, 13, 17,
because the antenna's own motion dominates the centroid. The strip answered it
immediately.

## Surge (`kHastyHurrySurgeA16`), same 24 frames

* **0** — the body holds ONE lean for the whole strip. The antenna rotates and
  the body does not. This is pass 25's fault in isolation: a posture, not an
  effort.
* **1400 — CHOSEN.** The body visibly digs in and eases back across the pulse.
* **2600** — it rolls and lurches rather than driving.

## Face (`kHastyEyeDrivePm` / `kHastySquintDrivePm`), 4x through a check

* **800 / 520** — the drive eye almost vanishes; risks reading as eyes shut.
* **880 / 430 — CHOSEN.** The drive eye stays a present, narrowed sliver and the
  check snaps it clearly wide with the pupil star showing.
* **960 / 300** — drive and check are too close together; the acting flattens.

Startle and Curious were rendered alongside for the "how much is too much"
comparison, but they sit at the house 360000 and Hasty at 280000, so the crops
are NOT like for like and the comparison is about KIND, not amount: Startle's
tops-apart alarm and Curious's asymmetric double-take are both EVENTS, and the
hurry is a STATE with events cut into it. Hasty's brow goes tops-together
(effort), which is Curious's sign and the opposite of Startle's.
