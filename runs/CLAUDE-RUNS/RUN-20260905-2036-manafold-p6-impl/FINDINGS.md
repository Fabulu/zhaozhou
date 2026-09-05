# Manafold pass 6 - IMPLEMENTER findings

**Run:** RUN-20260905-2036-manafold-p6-impl
**Lane:** manafold-p6-impl/{zhaozhou,Upheaval} - own clones. The hardware lane
and every other pass-6 lane untouched.
**Published:** upheaval.pages.dev, branch main, verified live.

---

## 1. WHAT SHIPPED

Stages 0, A, B.1, B.2, C, D.1, E and F.1, plus owner sections 5c and 5d which
arrived mid-pass. Stages D.2, E.4, C.5, 0.3 and F.2-F.6 are NOT done and are
itemised in section 5.

* **0.2 diagnostics, committed permanently** - manafold-still (exposes clip slot
  7, a pose build_still() has always produced and nothing could render), the
  manafold-fogprobe-{mana,off} ablation pair, and the knead guard's bound
  DERIVED from its array instead of hand-written.
* **A.1** the many-colour rig on every clip; **A.2** kU02CamK = 360000.
* **B.1** symmetric lens pointed at both ends; one star unit whose white is
  GENERATED from its cyan; blink cut from a 43-degree shutter to a pinch.
* **B.2** five ball parts deleted, four gentle knuckles moved into the antenna's
  own skin; the free-floating dongle resolved structurally.
* **C** the out-of-plane axis the hinges never had, per-hinge lagged envelopes,
  raised amplitude, and a 3D closure aim.
* **D.1** the pink, as a light fix at both ends of a swing.
* **E** channel's mana on every clip, more motes as breadth, two strands, and a
  new SHORT/TORN smear rung chosen by motion class.
* **F.1** hasty stops walking; traverse kept; own camera.
* **5c** the containment leash; **5d** the eye roll, shipped at 10 degrees.

## 2. VISUAL DECISIONS AND THE RUNGS I REJECTED

**The camera at 360k is the cheapest win in the pass and it compounds
everything.** The eyes became legible at native for the first time and the loop
window went from a distant sparkle to a pocket with visible strands.

**Rejected: flattening the antenna.** My first swell half-width (250 mm) turned
it into a uniform band - as wrong as beads, in the other direction. The side
sheet draws four rounded swellings with concentric joint detail; they are absent
from the front view only because they are edge-on there.

**Rejected: a scaled-copy white star.** It thickens at the tips. The shipped
white is a true outward offset, in the picture plane only.

**Rejected: the third lightning strand, on the record.** My first whitening
metric read ~16,000 px and was measuring the violet planet bloom. Restricted to
the loop pocket and reported as a ratio, white:cyan is 1.33 / 1.35 / 1.40 for
one, two and three strands. Two costs almost nothing; three is where the pocket
turns hue-neutral.

## 3. WHERE THE ARCHITECTURE WAS WRONG

**D.1's mechanism.** The plan said pull the moving-rig gain until the clipped
fraction is single digits. Measured under the pass-6 rig the fault is not pass
5's constant 49-57% clip - it is a SWING: clip 0.1% to 36.3% and dark 22% to 54%
within one clip. Rendering the ambient ladder proved ambient TRANSLATES the swing
rather than narrowing it (A32: away phase 54.3 to 47.9% dark while the bright
phase went 36.3 to 43.5% clipped). Pulling gain alone would have fixed the
bright phase and blacked out the dark one. Shipped: a raised ambient floor AND
kU02MlSourceGainPm, one knob at each end. The plan's acceptance band is also
wrong - red still clips 33% and ships, because it clips on red alone while green
and blue carry form.

**C.4's remedy.** The plan offered two fixes and BOTH were insufficient: the 3D
aim took 1539 to 1450 pm, bounding the out-of-plane amplitude took it to 1444.
An isolation build with pass-5 amplitudes still failed at 1141, so the baseline
was already marginal. My own third hypothesis was wrong too - LENGTHENING the
arm gave 1977, much worse, which is what proved it was overshooting rather than
falling short. The working levers were a deeper aim anchor and a SHORTER arm.

**B.1's "one mesh".** A RingPart carries one material and one page. Shipped as
two parts on ONE bone: two transforms per eye, no sliding, one containment rule,
and the white still generated from the cyan.

**A.1 collapsed manafold-inspect into manafold-hover** - both now render
0xC8987099, byte-identical. Not a bug; inspect existed only to carry the rig
that became standard. Its caption now says so rather than shipping a silent
duplicate.

## 4. BLAST RADIUS

**All 16 shipping clips moved.** antenna_knead() and loop_pose() are on every
one; kLoopArcMm[5], kLoopReentry*, kLoopRings and the deleted ball parts change
the model; stage A changes rig and camera; stage E changes mana and smear
everywhere. Key data was re-authored on exactly TWO clips - hasty and trick.
Everything else moved through shared mechanism, the class of change pass 5
under-reported as 3 when 7 had moved.

Shipping CRCs: hover/inspect 0xC8987099, drift 0x519C34D1, channel 0xC7454F19,
curious 0xFC9862DA, startle 0x19ED18C8, rest 0x7AC3A5F2, pirouette 0xCE5D4FAE,
hasty 0x55A67594, fall 0xDCB633CD, hit 0xA24020B8, taunt 0xAB17AAAD, taunt2
0x673841EF, trick 0x49D90F05, damage 0x10536934, crackle 0xA86F0841. Against the
three pass-5 baselines I captured, all three moved.

**The committed probes caught four things looking would not have:** the re-entry
knuckle sitting 460 mm inside the body (I guessed its arc station instead of
reading the probe's own surface-crossing report); trick's headstand drifted to
+1 mm where -25 mm is declared; an unsound eye-shift pivot; and the 18-degree
roll digging the lens into the body.

**Closure margin is thin: bank 1064, sweep 1059, gate 1120.** The two constraints
pull opposite ways, so kLoopArcMm[5] = 1270 is near the balance point rather
than a comfortable place.

## 5. WHAT I DID NOT DO, AND WHY

1. **D.2 - the fog shell with its own knobs. NOT DONE, and it is the biggest
   gap.** It is coupled: E.5's plateau fix reduces accumulation and the fog is
   currently MADE OF that accumulation, so the fog is now thinner where
   Direction 5 section 3 asked for "thicker by a lot". The architecture binds
   D.2 and E.5 as one delivery unit and I shipped one half.
2. **E.4 - restore the saturation pass 5 spent. NOT DONE as a separate item.**
   Raising kKneadClipPm across the bank also makes re-burial a live risk on rest
   and taunt2, the two clips pass-5 QA cut for exactly that reason.
3. **F.2-F.6 - the clip inventory. NOT DONE.** Flight bob, fall re-judged as
   being thrown, the directional hit, the taunts (still 3.5-5.8 px in nine
   seconds), bouncy/stretch/inhale/exhale. Direction 5 section 6 is largely
   unmet, and "neither nose nor mouth" makes it load-bearing.
4. **C.5 - deform fill-on-bend. NOT DONE.** The joins are smooth because the
   antenna is one surface, not because the deform channel is filling them.
5. **0.3 - the hardware-ask amendment. NOT DONE.** Documentation only.
6. **kKneadClipPm[13] (trick) stays 0** - raising it failed the declared
   ground-contact gate.
7. **The ink-silhouette diff plate. NOT BUILT.** The silhouette did change and
   always would have; the balls WERE the silhouette at five points.
8. **The far eye is still foreshortened at three-quarter** (Direction 3 section
   2, already open).
9. **The purple pigment was not darkened** - the "94% value" reading measured
   kStarR/G/B, which is only the fallback; the page's cyan is already at 82%.
10. **Per-hinge trajectory plots not produced.** I judged stage C from an
    8-frame antenna contact sheet, before against after.
11. **kEyeShiftMaxPm (5c) NOT SHIPPED.** The pivot mechanism is unsound: a ring
    cx offset lives in the bone's rotated frame while a bind translation lives
    in the parent's unrotated one, and face_rest's attitude sits between them.
    The fix is to counter-rotate the offset or add a translation channel.
12. **Extremes gate A (the eyes never touch) is REPORTED, NOT ENFORCED.** It
    returns 0 mm at every amplitude including zero roll, where the lenses are
    ~130 mm apart by construction, so the instrument is wrong rather than the
    geometry. I did not tune the creature to satisfy it.
13. **Leash rule 1 reports 0 mm overhang - an honest finding, not an inert
    gauge.** Clips author gaze as FRACTIONS of the clamp, so none reaches it:
    5c grants the leash and no shipped clip spends it. That is clip authoring.
14. **The mana-lab edge-snap-held mechanism is NOT integrated.** It is published
    as experiments with the finding stated. Note the unification: the net screen
    travel that makes the smear trail work is the same property that makes a
    held shape separate, so hasty's traverse decision and the fold fix are ONE
    requirement. Clips that never travel get neither.

## 6. OWNER QUESTIONS

Proceeded on the recommended defaults; 5c and 5d superseded question 4.
Question 1's alternative (b) was NOT rendered side by side - omission recorded.

## 7. INSTRUMENTS COMMITTED THIS PASS

Because this creature's probes keep being written into run folders and orphaned:

* tools/reel/plates.py - labelled grid / pair / contact-sheet builder. Imports
  rgbframe rather than re-reading the format.
* tools/reel/bodymeter.py - pigment clip/dark/hue meter over a DIFFERENTIAL
  hide-creature mask. A colour mask cannot separate a hot-red lit flank from the
  terrain, which is the exact pixel it counts.
* manafold_probe.cpp - the 5c leash (three rules) and the 5d composed-extremes
  gate (16 corners of roll x gaze-side x gaze-lift x shift, both eyes).
* manafold-still and the two fog probes, as permanent subjects.
