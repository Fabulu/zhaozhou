# Look 01 — why Hasty does not read hasty (BEFORE, production ink)

Renderer `.tmp/p26/bin/zhao-reel-cel.exe` MD5 `8a6c027b57c56766da5ba9e7c14c5ea2`,
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, 240 frames (120 keys x 2).

## What the pictures show

Native 5-up and the complete 240-frame sheet. The clip reads as a slow, even
DRIFT. Nothing in it is urgent.

## Four causes, in order of how much they cost the read

**1. The creature does not move relative to anything. There is no traverse.**
`build_hasty` ends with `c.root[f*3+0] = 0;` — Direction 12 deleted the net
traverse when it deleted the frame-history smear, because the smear was the only
thing the traverse was for. **The camera's traverse compensation was never
removed with it.** `zhao_reel.cpp` still gives slot 8 `cam_bias_x` +28000 lerped
to -28000, which `cam_pitch` folds in as `x' = k*X + bias_x*w`, i.e. NDC
`x = k*X/w + bias_x` — a CONSTANT NDC offset at every depth. So it shifts the
whole picture uniformly: the creature and the ground together. The owner saw
motion ("it moves across screen") and there is motion; it is a camera pan over a
stationary creature, and pan is not speed.

**2. The ground has nothing to stream past.** Slot 8 is `flat_staged_slot`, so
it gets `bump_ext = 18` — flat staging, authored so a traverse would read. With
the traverse gone what is left is a smooth brown gradient and a horizon line.
Even a restored traverse would buy a weak cue here.

**3. The pose is a posture, not an effort.** `kHastyPitchA16` (2400) and
`kHastyBankA16` (1900) are CONSTANT offsets applied identically on every key —
the creature is leaning, and has been leaning for the whole clip, which reads as
an attitude rather than as pushing. `kHastyFishtailA16` is 0 (pass 6, correctly:
it was a walk cycle on a floating creature). What replaced it is
`kHastyBobCycles = 5` over 120 keys = 48 rendered frames per bob. That is a
LEISURELY float period, not a hurried cadence.

**4. The face cannot act at this framing.** Slot 8 inherits
`kU02CamKTraverse = 148000` against the house 360000 — a camera pulled back for
an 8.4 m journey the clip no longer takes. The creature is a small teardrop and
its contour ink is **1-2 pixels per frame**. Owner Direction 27 asks for facial
expression; at this size there is no face to author on.

Causes 1, 2 and 4 are all the same leftover: **the staging still compensates for
a traverse that was deleted, and nobody took the compensation out.**

## Instrument notes (both faults were in the tool/fixture, not the render)

* `tools/reel/screenmotion.py` is new and committed. 5 selftest legs green,
  including the PAN-ONLY leg that is the whole point (creature and ground moved
  together -> `relative_dx` +0.00).
* Its first pan-only fixture built the ground as `gx = 400 + i*step`, which
  slides the sampling window right and moves the CONTENT LEFT — so "pan only"
  was really pan-and-travel and the leg failed against a correct tool. Fixture
  wrong, instrument right.
* On the real clip it reports **VACUOUS ink on 155 of 240 frames** and 1-2 ink
  pixels on the rest. That is not a render fault; it is finding 4 above. The
  centroid column is therefore NOT quotable for this framing, and the loud
  vacuous warning is what kept it from being quoted.
* `bg_dx` reads exactly 0.000 on every frame. The pan is ~0.68 px/frame —
  SUB-PIXEL — so an integer argmin correctly rounds it to 0 every time while
  accumulating 163 px over the clip. A per-frame integer shift cannot see a
  sub-pixel rate: needs a lag. Recorded as a known bound of the tool.
