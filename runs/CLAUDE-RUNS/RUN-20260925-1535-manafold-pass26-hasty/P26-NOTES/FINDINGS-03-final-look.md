# Look 03 — the shipping clip, every frame, production ink

`.tmp/p26/bank26/manafold-hasty`, 240 frames, the 22-subject shipping render.
Sheets committed as `P26-SHEETS/P26-{BEFORE,AFTER}-MANAFOLD_HASTY-ALLFRAMES.jpg`.

## Does it read as a creature in a hurry?

**Yes.** Against the BEFORE sheet the difference is not subtle:

* **Before:** the same pose in all 240 tiles. The creature holds one lean, bobs
  gently once every 48 frames, and slides left as the camera pans. Nothing about
  it is urgent; it reads as drifting.
* **After:** a driving pulse roughly every 18 frames that is visible tile to
  tile, the body digging into the travel and easing out of it, the antenna
  trailing a beat behind and recovering, and the creature genuinely crossing the
  frame left to right while the horizon streams the other way.

**Hurried, not frantic.** The 17-cycle rung was frantic -- the antenna changed
shape every other frame and the read tipped into panic. At 13 the body motion is
coherent and the antenna reads as a thing being dragged along behind something
that is in a hurry, which is the intent. The line between the two is between
those rungs and it is recorded in FINDINGS-02 so the next pass does not have to
find it again.

**The face acts and it is legible.** The eyes are narrowed slivers on the drive
and snap wide on each of the three checks, with the pupil star appearing. At the
pass-25 framing this was not available at all: the contour ink was 1-2 px.

## The bob amplitude came down and nothing was lost

mqa's root-continuity ceiling sent 165 mm back (143.3 mm root step against a
135 mm bound). Re-looked at 140 mm: **indistinguishable in the read.** The
pulse's legibility comes from its RATE and from the surge riding it, not from
the last 25 mm of travel. The bound cost nothing here, which is worth writing
down because the reflex is to treat a gate as an obstacle to the picture.

## The seam

The creature's on-screen loop displacement is **0.0 px**, down from the 38.8 px
the owner accepted. Looked at across f236->f001: the creature holds position and
attitude through the wrap; what moves is the TERRAIN, whose horizon line shifts
slightly. On this near-featureless staging that is close to invisible, and it is
the right thing to spend: a ground that snaps reads far better than a creature
that jumps a tenth of the frame.

---

## Addendum — a check that cut instead of leaving

Found by reading the check curve against its own comment rather than by looking
at a frame, and confirmed by printing the curve (no render needed):

```
window 18..27 span 9  -> 0 250 500 750 1000 1000 750 500 250   THEN CUTS TO 0
window 57..65 span 8  -> 0 -250 -500 -750 -1000 -750 -500 -250 THEN CUTS TO 0
window 88..95 span 7  -> 0 233 466 700 700 466 233             THEN CUTS TO 0
```

`tri = (t <= half) ? t : span - t` over `t` in `[0, span)` never reaches zero on
the way down. Every check therefore ended at 25-33% of full and **cut**, which
is a one-key step in the eye size, the gaze, the lid AND the brow simultaneously
-- they all ride this single curve. About 5.7 degrees of gaze in one key.

The comment above it read *"a triangular in/out so a check ARRIVES and LEAVES
rather than cutting -- at 2 rendered frames per key a cut is a pop."* **The code
did not do what its comment claimed**, and the comment is the kind a reviewer
would have taken as evidence the arrival was handled.

Replaced with a symmetric triangle in permille of the window: exactly 0 at both
ends, full in the middle. All three windows are now odd-span (the middle one
moved 57-65 to 57-66) so the peak is actually sampled -- on an even span the
check reaches only ~86% of `dir` and `kHastyEyeCheckPm` stops meaning what it
says.

New curve, verified:

```
18..27 span 9 -> 0 250 500 750 1000 750 500 250 0
57..66 span 9 -> 0 -250 -500 -750 -1000 -750 -500 -250 0
88..95 span 7 -> 0 233 466 700 466 233 0
```
