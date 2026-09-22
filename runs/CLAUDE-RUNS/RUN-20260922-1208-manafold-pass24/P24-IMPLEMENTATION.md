# Manafold pass 24: implementation

**Date:** 2026-09-22
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-25-2026-09-22.md`
**Worker:** Claude (sole Opus implementer; no sub-agents, no Qwen, no HomeAI)
**Source:** Zhaozhou `manafold-pass24`, from the production-verified pass-23 main
`1d449717`
**Verdict:** DONE for all three items. **Not rendered here:** the 22-subject
bank for publication, the encode, the merge, the deploy -- by instruction.

**Matrix: 224 / 224 PASS, 0 FAIL**, one invocation
(`P24-RECEIPTS/runmatrix_p24.sh` -> `gate-matrix.txt`, log in
`gate-matrix-run.log`).

| family | legs |
|---|---:|
| normals (every gate binary, incl. the new mbolt) | 15 |
| controls fired and attributed (mrear masks, mrod, mbolt's three) | 23 |
| mspan controls | 37 |
| msmooth controls | 16 |
| protected legs | 27 |
| selectors, RC 2 (the 71 carried forward plus 25 new) | 96 |
| identity + live history | 10 |

**The one-paragraph answer to the owner's question.** The lightning goes through
the antenna because it GEOMETRICALLY DOES: one drawn bolt segment in nine is
inside the antenna's volume, up to 99 mm deep, on 561 of Crackle's 600 frames.
3D avoidance removes every one of them on both clips it was asked for. Depth
splitting, measured and then looked at, **does not touch the fault** -- the bolt
was already stamped finer than the rod it crosses, so there was no resolution to
win, and the plate shows it still crossing. The comparison has an answer.

---

## 1. ITEM 2, FIRST: THE MEASUREMENT

The brief required this before anything was built, and it decided the shape of
both mechanisms and the verdict on one of them.

### The instrument

`tools/reel/manafold_boltgate.cpp` (mbolt), committed, built by
`build-direct.sh mbolt`. For every key AND midpoint of every live subject it
decodes the pose, takes the antenna's own posed geometry, generates the bolt
paths **through the production functions**, and measures segment-vs-capsule
distance in 3D. No render, no camera, no terrain; the whole bank answers in
about ninety seconds.

Three things make it a measurement of the creature rather than a model of it:

* **The antenna is capsules and spheres because the pass-21 rig made it so.**
  Four straight rods between five ball joints: `(joint[e], joint[e+1],
  kBoltRodRadiusMm[e])` IS the rod. The joints come from skinning the ring
  table's own bind stations (`fx_anchors_from_pose`), not from bone origins --
  those agree today and nothing says they must.
* **The radii come from the band's own taper**, not from a transcription.
  `bolt_rod_radius_mm(e)` evaluates `loop_blade_taper_mm` over each rod's
  station span; re-authoring `kLoopBladeRxMm` moves the clearance with it. A
  static_assert pins the four values the report quotes, so a taper edit breaks
  the build rather than silently invalidating this page.
* **The bolt paths are the DRAWN ones.** The fold figure's stations come from
  `mana_fold`'s own trace (post-`place()`, so pass 22's knead transform is in
  them) and the path between two stations is `bolt_path_morph` -- the production
  function, so the pass-24 avoidance inside it applies here too. A probe that
  re-implemented the path would measure one the renderer does not draw.

The screen-space half is **orthographic along the exact production view
direction**, and that is declared in the source and here. A perspective divide
would need the creature's staged world position, which needs the terrain
lattice; reproducing that risks a confident wrong number from a camera that is
subtly not the shipping one. A translation cancels in an orthographic test, so
the staging cannot matter, and the only error left is foreshortening across a
1.3 m creature at 10 m. **The gate asserts only on the 3D leg.**

### The answer: it is GEOMETRY, and it is not close

`P24-RECEIPTS/mbolt-census-before.txt`, every live subject, sorted by rate.

| subject | segments | intersecting | rate | frames with a hit | screen-cross in front | behind |
|---|---:|---:|---:|---:|---:|---:|
| death-drop | 16,344 | 2,715 | 16.6 % | 227 | 5,708 | 660 |
| drift | 19,640 | 3,008 | 15.3 % | 298 | 7,074 | 1,138 |
| **crackle** | **42,364** | **5,685** | **13.4 %** | **561 of 600** | 14,621 | 1,839 |
| death-gutter | 15,492 | 2,030 | 13.1 % | 195 | 6,082 | 642 |
| blown | 20,672 | 2,633 | 12.7 % | 266 | 5,314 | 652 |
| **hover / inspect** | 38,152 | **4,705** | 12.3 % | **530 of 600** | 10,011 | 6,039 |
| damage | 32,848 | 3,882 | 11.8 % | 440 | 13,793 | 1,698 |
| pirouette | 17,104 | 2,007 | 11.7 % | 211 | 5,614 | 1,670 |
| fall | 23,120 | 2,698 | 11.7 % | 299 | 6,942 | 2,355 |
| trick | 24,168 | 2,819 | 11.7 % | 342 | 7,386 | 1,231 |
| channel | 28,560 | 3,316 | 11.6 % | 374 | 9,414 | 1,203 |
| hasty | 16,928 | 1,845 | 10.9 % | 204 | 6,155 | 1,464 |
| taunt | 19,368 | 2,025 | 10.5 % | 250 | 6,495 | 1,066 |
| rest | 28,392 | 2,938 | 10.3 % | 274 | 10,159 | 2,071 |
| lasso | 23,448 | 2,221 | 9.5 % | 170 | 3,229 | 223 |
| startle | 11,344 | 880 | 7.8 % | 119 | 4,264 | 766 |
| hit | 9,024 | 616 | 6.8 % | 77 | 2,791 | 529 |
| flight | 23,680 | 1,492 | 6.3 % | 165 | 7,051 | 2,020 |
| taunt III | 25,024 | 1,502 | 6.0 % | 229 | 8,221 | 1,819 |
| taunt2 | 17,240 | 668 | 3.9 % | 112 | 2,419 | 514 |
| curious | 12,240 | 205 | 1.7 % | 42 | 4,500 | 1,151 |
| **TOTAL** | **503,304** | **54,595** | **10.8 %** | | | |

**The worst clips, named.** By absolute count and by reach, **Crackle** --
5,685 intersecting segments on **561 of its 600 frames**. The owner named it
without a probe. By RATE, **death-drop** (16.6 %) and **drift** (15.3 %) are
higher, and neither is in this pass's experiment. **Hover** and **Inspect** read
identically at 4,705 / 12.3 %, which is itself a useful confirmation: they play
the same clip.

### What the split of the number says

1. **Geometry, overwhelmingly.** One bolt segment in nine is literally inside
   the antenna's volume, up to 99 mm deep. Nothing about drawing order can help
   a line that occupies the same millimetres as the rod.
2. **There is no drawing-ORDER fault at all.** `glow_splat` depth-tests PER
   PIXEL against each sprite's own depth, so a bolt honestly behind a rod is
   honestly hidden (the "behind" column) and one honestly in front is honestly
   over it (the "front" column). Both are correct. What is left is footprint and
   resolution -- which is what item 2b was built to test.
3. **Every intersection belongs to the FOLD FIGURE's edge links. The free
   lightning strands contribute ZERO, on every clip.** So "the Lightning shape
   goes through the antennae" is, precisely, the fold figure's outline; the
   free strand never reaches a rod. That is in
   `P24-RECEIPTS/mbolt-census-before.txt`'s BREAKDOWN table, which also names
   whether each intersection cuts a rod or a ball and how long the segment was.
   Without it the next section's repair would not have been found.

---

## 2. ITEM 2a -- 3D AVOIDANCE (Crackle, Hover)

### The mechanism

`bolt_avoid_rods()` in `manafold_fx.h`, applied at the two places a DRAWN bolt
path exists: the end of `bolt_path_morph` (the fold figure's links) and the end
of `free_lightning_path` (the free strands). Both are **after** the morph lerp,
deliberately: avoiding the two endpoint paths and then interpolating between
them would put the interpolant back inside the rod on every frame between two
phases -- clear at the keys, cutting everywhere else, which is this creature's
signature failure shape.

It may move a bolt's PATH and it can reach nothing else. Radius, colour, gain,
stamp density, morph clock, station identity and topology are all untouched --
Direction 23's *"It is good now as it is"* is structural here, not a promise.

### PUSHING THE VERTICES WAS NOT ENOUGH, AND THE PROBE'S BREAKDOWN SAID SO

The first version pushed each path POINT out of every capsule and sphere, three
sweeps. Crackle went 5,685 -> 352 and Hover 4,705 -> 157. A 94 % cut looks like
success, and the instinct is to call the remainder noise. The breakdown table
refused that reading: every survivor was a fold-figure link, none was a free
strand, and **the worst was 282 mm long**. A segment whose two ends are both
outside a 46 mm rod can still run straight through it, and no push applied to
the ends alone can know that.

So the sweep also walks `kBoltSegSamples` = 8 interior SAMPLES of each segment
and distributes each sample's push back onto the two ends by lever arm.

Three further things were needed, each found by re-measuring rather than by
reasoning, each recorded at its site:

* **The sample stage may not move a STATION.** A link's first and last points
  are the fold figure's shared stations, reached from both sides; the point
  stage may move them because it is pure in the position, but a sample-driven
  push depends on the whole SEGMENT, and two links meeting at a station see
  different segments. Letting it move a station would tear the figure open
  there -- silently, and only on frames where a station is near a rod.
  `lo`/`hi` bound the sample stage; free strands share nothing and pass their
  whole range.
* **The lever must be compensated when one end is pinned.** With both ends free,
  (1-u) and u move the sample by exactly the deficit. With one pinned, moving
  the free end by the deficit moves the sample by only u of it, so samples near
  the pin converge eight times too slowly. Capped at `kBoltLeverMaxNum/Den`.
* **A chord pinned at one end needs a DIRECTED push.** Unbiased, the samples on
  one side of a chord push one way and those on the other push back, and they
  cancel; twelve sweeps and three different lever caps only traded which clip
  kept a graze (0 and 1, then 1 and 0). The push is now aimed at the PINNED
  end's side of that rod, which is the move a chord actually needs, and is
  continuous because the pinned vertex is strictly outside and always has a
  side. Biasing BOTH-free segments the same way was tried and is worse (8 and
  15 against 0 and 1), so it is applied only where it belongs.

Result: **Crackle 5,685 -> 0 and Hover 4,705 -> 0**, every key and midpoint.

### The chosen value, and the visual reason

`kBoltRodClearanceMm = 46`. Ladder BEFORE / 25 / 46 / 60 / 80 mm rendered
complete and read at 5x on Crackle f0028 -- the worst before-frame by the
probe's own count, not by index (`P24-LOOKS/02`).

| rung | what was seen |
|---|---|
| BEFORE | the blue figure runs over the right-hand band; on the left it sits on the rod |
| 25 | the crossing is mostly gone; the figure still grazes the left rod |
| **46 -- SHIPPED** | the figure sits inside the pocket, clear of both rods, at its own size and its own shape |
| 60 | clear, and beginning to look pulled-in |
| 80 | **visibly squeezed** -- narrower, more vertical: a different shape |

80 is the first rung that draws attention to itself as a restyle. 60 is the rung
below it; 46 is the one that reads best, keeping the figure's full width with the
crossing gone. **The gate did not choose it.** Below ~46 the relaxation leaves a
graze (1 segment of 42,364 at 40 mm, 4 at 25 mm); that is reported beside the
choice, and the eye picked 46 from among rungs that include two clean ones.

## 3. ITEM 2b -- DEPTH SPLITTING (Inspect)

`kBoltDepthSplitN = 4` multiplies every bolt path's stamp count. Because N times
as many ADDITIVE sprites on one path is N times the light -- a restyle by the
back door -- the additive gains and the navy backing's opacity are divided by N
(`bolt_split_gain`, with `g_u02_bolt_split_compensate` as its own knob so the
split can be looked at with the compensation off). At N = 1 every expression is
an identity and the off path is byte-exact.

Measured: 228,912 sprites unsplit, **915,648 at N = 4, exactly 4x**, and the
near-rod stamp spacing 26.59 -> 6.65 mm.

### AND THE COMPARISON'S ANSWER IS THAT IT DOES NOT HELP

Two measurements and one look agree:

* **Partial occlusion does not change.** All **155** depth-straddling segments
  are drawn with sprites on both sides of the rod's surface at N = 4 -- and
  **all 155 already were at N = 1**. The unsplit bolt stamps every 26.6 mm,
  which is finer than the 46 mm rod it crosses, so there was no resolution to
  win. (INFO B2b in `P24-RECEIPTS/mbolt-gate.txt`.)
* **The isolated A/B shows it.** `P24-LOOKS/05`, Inspect f0471, rendered with
  items 1 and 3 at their pass-23 values so only the split differs: **the bolt
  still crosses the antenna exactly as before.** The white core reads as one
  continuous filament rather than a chain of beads, and the occlusion edge is a
  little crisper. That is the whole of it.

**By eye, across the three clips: avoidance addresses the owner's complaint and
splitting does not.** The experiment did its job.

### Declared, for the owner's eye first

At the tightest loop closure (Hover f0471) the pocket is small enough that the
push carries the figure to the OUTSIDE of the rods, where it wraps the loop
rather than filling it (`P24-LOOKS/03`, top row). The figure is intact -- same
size, same jag, same colours -- but it is a placement he has not seen. Across
the rest of the clip (`P24-LOOKS/04`, six frames spanning the loop) the figure
is simply nudged off the rods and reads as the same lightning.

---

## 4. ITEM 1 -- HOVER'S BACK BALL AND FRONT

### ! SLOT 0 IS PLAYED BY TWO LIVE SUBJECTS, AND THAT IS STRUCTURAL

`manafold.h` compiles ONE CLIP PER SLOT, and `manafold-inspect` is slot 0 with a
different `cam_k` -- the pass-12 repair for a subject that was a pixel-perfect
duplicate of hover. So "lower it for Hover" lowers it for Inspect too, because
they are the same animation. Direction 25's own words are *"make that scale PER
CLIP"*, and this is what per clip means here.

The brief asked for the other 21 live subjects to be byte-identical; **20 are**,
and Inspect is the twenty-first only because it IS the Hover animation. The
alternative -- a slot 24 that is `build_hover_idle` a third time -- would give
hover its own bytes at the cost of another camera/schedule join of exactly the
kind that broke in pass 15 (seven subjects rendering an eye pose authored for a
camera they do not have). It is left as the cheap reversal if the owner wants
Inspect's rear left alone; nothing in this pass makes it harder.

### The rear ambient, per clip

`kRearAmbientClipPm[24]`, read through `rear_ambient_clip_gain_pm(slot)` -- the
ONE production read, so the solver and every gate go through the same door.
Slots 0 and 23 take **170**; the other 22 entries are pass 19's accepted 400,
untouched. The bank-wide `g_u02_rear_ambient_gain_pm` still overrides the table
when it is moved off its compiled default, because it is mrear's
`--fail-rear-joint` control and a control that a new table could silently
out-vote is a dead control.

Threading the slot cost four signatures (`hinge_play`, `loop_alive`,
`whole_wobble` and their five call sites) rather than a global set by the
first layer -- a global would have carried the previous clip's value into the
lab's forked knead, which does not call `antenna_knead` at all.

### THE HONEST SIZE OF WHAT THIS BUYS

**On Hover the rear ambient is 10-25 % of what the back ball is doing**, and
saying otherwise would be the invisible-change fault.

* `framediff` puts the worst single-frame difference between gain 400 and gain
  90 at **198 pixels**, in a 30 x 16 native box at the body entry. For scale,
  the Front gain's worst frame is **12,508**.
* mrear reads slot 0's End relative angle mean 2.2 -> 1.7 deg across that whole
  range, while its MAX barely moves (11.2 -> 10.9). The max is the authored
  swallow beat, which pass 19 deliberately placed outside this knob.
* On the ambient-dominated clips the same knob is decisive (Drift: mean
  2.6 -> 0.6), which is why it reads as the right lever in general and a partial
  one here.

! AND THE SCREEN-SPACE VERSION OF THIS MEASUREMENT WAS USELESS. Frame-to-frame
pixel energy in the rear box reads 129,794 at gain 400 and 129,774 at gain 90 --
0.015 % apart -- because HOVER'S CAMERA ORBITS and the creature sweeping through
the box swamps everything the knob does. A screen-space motion metric on an
orbiting clip measures the orbit. Recorded because it is the kind of number that
would have been quoted as "the knob does nothing".

170 is taken because it is a clear further calming of the term that is actually
jittery without reaching the rigid end pass 19 rejected at 300 bank-wide. The
honest expectation is that it helps and does not finish the job; §8 names the
remaining terms.

### Hover's Front, lifted

`kFrontFlexClipPm[24]`, slots 0 and 23 at **1350**, everything else 1000 -- and
a clip at 1000 does the same integer arithmetic it always did, so untouched
clips are byte-exact by construction.

It is a GAIN on the authored `kHover` curve, not a re-authored key table: the
shape, the timing and the loop seam (both ends are 0, so a gain cannot move
them) stay the accepted performance and only the amplitude changes. A second
Hover curve would have been a second thing to keep in step with the first.

Ladder 1000 / 1200 / 1350 / 1700 at Hover f0399, the frame the gain changes most
(`P24-LOOKS/08`):

| rung | what was seen |
|---|---|
| 1000 | today: the loop stands close to upright through the beat |
| 1200 | a touch more lean; easy to miss beside 1000 |
| **1350 -- SHIPPED** | the lean reads plainly as more motion, and it is still the accepted performance |
| 1700 | **the loop's whole attitude changes** -- a different performance, not a livelier one |

1700 is the first rung that draws attention to itself; 1350 is the rung below
it, and it is what *"could move a little more"* asks for.

## 5. ITEM 3 -- THE AMBIENT EYE LAYER

One shared layer on machinery that already exists: three channels -- gaze side,
gaze lift, and the pass-17 per-eye SCALE -- on three mutually prime INTEGER
cycle counts (2, 3, 5) so the face is never doing one readable periodic thing
and the loop seam is exact by construction rather than by arithmetic luck.

**It enters through `apply_gaze`'s own containment clamp, not after it.** A
rotation composed onto a finished pupil quat could walk the star off the lens,
which the committed extremes gate exists to catch; an ADDEND to the angle before
the clamp cannot. The route is `Rig::eye_amb_*`, the carry-field pattern
`eye_lean` established for an ordering problem of exactly this shape, written by
`antenna_knead` (the first thing every performing clip calls) and read by
`apply_gaze` / `apply_gaze_lr` and by `Rig::write`. The SIZE lands in `write()`
because most clips never call `set_eye_scale_pm` at all -- applying it there
would have reached the handful of clips that already act with their eyes and
missed exactly the "normal animations" the direction is about.

**Where it is absent, and why:**

* **The diagnostics** -- still (7), the mana lab (15), the nodule solo (16) --
  take gain 0, and two of the three do not call `antenna_knead` at all, so the
  absence is structural as well as declared.
* **Curious (3), Startle (4) and Taunt III (21)** take gain 0: the authored
  expression beats the owner named as already right. Sliding a floor under them
  is precisely the "overdone" he warned about. **Their byte-identity under
  item 3 is the proof**, and the matrix asserts it by name.
* **Trick's planted window** (keys 70-160) is muted with C2 ramps in and out, so
  nothing drifts across the headstand.

### The rung, chosen by ladder

Rest f0250-f0274, five rungs x five frames at 8x (`P24-LOOKS/10`; the one-frame
ladder `P24-LOOKS/09` could not tell "alive" from "acting" and did not decide
it).

| rung | what was seen |
|---|---|
| 0 | the stars hold one attitude; the pair reads still |
| 250 | barely separable from 0 |
| 450 | a small drift; quiet |
| **600 -- SHIPPED** | the stars move, the lenses breathe a little; it reads as life |
| 800 | **the star starts riding toward the lens rim**; the eye begins to read as acting |
| 1000 | plainly a deliberate look, in a clip whose whole subject is resting |

800 is the first rung that draws attention to itself. **600 is the rung below
it**, which is the rule the brief set.

---

## 6. GATES

### The new instrument: mbolt, three legs, three controls

| leg | asserts | shipping reading |
|---|---|---|
| **B1 CLEAR** | a subject configured for 3D avoidance has ZERO bolt segments intersecting any rod capsule or ball sphere, every key and midpoint | Hover 0 of 38,152; Crackle 0 of 42,364 |
| **B2 SPLIT** | the split reaches the drawing: the same clip measured twice in one invocation lays exactly N times the sprites at 1/N of the spacing | 915,648 against 228,912 x 4; 6.65 mm against 26.59 |
| **B3 CONTROL GROUP** | INFO: the other nineteen subjects are measured too, so "the experiment moved three clips" is a statement about twenty-two | 39,500 intersections remain, on 19 subjects |

| control | what moved |
|---|---|
| `--fail-no-avoid` | B1 red on both: 0 -> 4,705 and 0 -> 5,685. Also the pass-23 BEFORE, so the control doubles as the measurement's baseline |
| `--fail-no-split` | B2 red: the 4x ratio becomes 1x |
| `--fail-fat-rod` | B1 red on both (17,679 and 19,863) with the capsules scaled to 220 %. B1's shipping reading is ZERO and a detector reading zero is a claim -- this is the proof it reads the RODS and not a constant, and no legal stimulus can give it once the avoidance is correct |

### TWO OF THE THREE CONTROLS WERE DEAD ON THEIR FIRST FIRING

Both are recorded in `manafold_boltgate.cpp` at their own sites, because the
next person inherits the argument otherwise.

* **`--fail-no-split` could not fire**, twice over, and the second failure is
  this pass's most useful measurement.
  1. B2's first form bounded the STAMP SPACING at the thinnest rod's radius
     (46 mm). The unsplit strand already stamps every 26.6 mm, so the leg was
     green with the mechanism switched OFF.
  2. Its second form asserted that every depth-straddling segment is DRAWN
     partly occluded. Also green with the mechanism off -- **all 155 already
     are**. That is not a gate result, it is the answer to the owner's
     comparison, and it now prints as INFO B2b with both numbers beside each
     other.
  B2 is therefore the mechanism's own contract, measured by running the clip
  twice in one invocation. It encodes no art value and no threshold.
* **`--fail-thin-rod` moved the operand without firing a leg.** Shrinking the
  capsules to a tenth took the control group from 39,500 intersections to
  1,180 -- a large, satisfying, useless number, because a thinner rod is EASIER
  to clear. It is now `--fail-fat-rod` at 220 %, which turns B1 red. (The first
  attempt also had `kFatRodControlPm = 220` meaning 22 %, per MILLE against a
  name that says per cent; the control stayed dead and said so.)

### AND TWO WRONG OPERANDS OF MY OWN, FOUND THE SAME WAY

Both produced confident failure counts that were the MEASURE rather than the
drawing, and both are recorded at their sites:

* the rod's MEAN view depth instead of its depth beside the point being
  classified -- on a rod inclined to the camera the two ends differ by hundreds
  of millimetres, and B2 reported 55 failures of 241 against a reference several
  rod-widths from the place it was asking about;
* a SPHERE's surface instead of a CYLINDER's. Subtracting the full radius from
  the axis depth is right only along the sight line through the axis; at screen
  offset o the near surface is sqrt(r^2 - o^2) in front of it, going to ZERO at
  the silhouette edge -- which is exactly where crossings happen. 18 of 285
  straddles looked undrawn when the drawing was right.

### Everything else, unchanged and green

mspan, msmooth, mprobe, mjointpub, mqa, mmeshcheck, moutline, mshell, mnodule,
meyesize, mrod, mrear (both modes) all pass at their unchanged thresholds with
every control still firing its own category. **msmooth matters most here**: it
is the gate on persistent lightning/particle identities and 60 Hz continuity, so
its silence is the evidence that the avoidance moves a PATH and does not switch
a figure, and that the loop seams still close.

## 7. BYTE IDENTITY

Measured on **all 22 live subjects**, per item, in the matrix. Pass 22 learned
that a two-clip identity leg cannot separate two causes; a four-clip one cannot
separate three items.

| configuration | subjects that differ from pass 23 |
|---|---|
| everything off | **none -- 22 of 22 byte-identical** |
| item 1 alone (rear + Front) | crackle, hover, inspect -- **19 of 22 identical** |
| item 2 alone (the lightning experiment) | crackle, hover, inspect -- **19 of 22 identical** |
| item 3 alone (the eye layer) | 19 subjects; **curious, startle and taunt III are byte-identical** |
| shipping | the same 19 |

The last row is the one to read twice. **Curious, Startle and Taunt III do not
move by a single byte**, which is the proof that the authored expression beats
the owner named as already right did not have a floor slid under them.

Exact-off knobs, all strict (RC 2 on anything malformed):

```
ZHAO_U02_BOLT_AVOID=off
ZHAO_U02_BOLT_SPLIT_N=1
ZHAO_U02_REAR_AMBIENT_CLIP_PM=0:400,23:400
ZHAO_U02_FRONT_FLEX_CLIP_PM=0:1000,23:1000
ZHAO_U02_EYE_AMBIENT_PM=0
```

**No bound was relaxed anywhere.** No existing `constexpr` was lowered; every
new constant is new, and every one of them is a named editable knob with an env
ladder.

---

## 8. OPEN ISSUES (non-blocking, and the first two are for the owner)

1. **The comparison has an answer, and it is one-sided.** 3D avoidance removes
   the fault; depth splitting does not touch it. If the owner agrees on the
   plates, the rollout question is only *which clips* -- and the census says
   the two loudest ones are **not** in this experiment: **death-drop at 16.6 %
   and drift at 15.3 %**, both above Crackle's 13.4 %. 39,500 intersections
   remain on the nineteen untouched subjects.
2. **The avoidance's one visible cost, declared.** At the tightest loop closure
   (Hover f0471) the pocket is small enough that the push carries the figure
   OUTSIDE the rods, wrapping the loop rather than filling it. The figure is
   intact; the placement is new. `P24-LOOKS/03` top row. If he dislikes it the
   lever is `kBoltRodClearanceMm` downward, at the cost of the residual grazes
   the ladder records (1 segment of 42,364 at 40 mm, 4 at 25 mm).
3. **Hover's back ball is only 10-25 % this knob.** The rear ambient is the
   lever Direction 25 names and it is the right one for "finicky", but the
   measured remainder is the **authored swallow beat** (`kSwallowEndJointA16PerMm`,
   which pass 19 deliberately left outside the ambient gain and which pass 19's
   own open issue 1 already named), the socket following the breathing body, and
   the rear rod's direction change as the loop closes. If 170 does not satisfy
   him, those three are the next levers -- in that order -- and none of them is
   a damping value.
4. **`kRearCarrierCalmPm` was a knob only a gate could read**, from pass 20
   until this pass. Parsed in `manafold_rear_audit.cpp` alone, so it moved
   mrear's reading of the creature and nothing that ships; three full renders at
   1000 / 500 / 250 came back byte-identical, which is how it was found. It is
   in the shared parser now and 1000 changes nothing, so the repair is
   byte-neutral -- but **nobody has ever seen what it does**, and carrier C is
   "the back nodule the eye reads" (Direction 22). It may be a better lever for
   item 1 than the one the direction named, and it has never been laddered.
5. **The orthographic screen leg.** The crossing counts in the census are
   measured along the exact production view direction but without the
   perspective divide (see §1). The gate does not rest on them; a follow-up that
   wanted them exact would have to reproduce the reel's terrain staging.
6. **The split's brightness compensation is a division.** `bolt_split_gain`
   divides the additive gain by N, which is right to first order and not exact
   under saturation; at N = 4 on Inspect the line reads the same weight by eye
   (`P24-LOOKS/05`) but the compensation has its own knob
   (`ZHAO_U02_BOLT_SPLIT_COMPENSATE=0`) so the two effects can be separated.
7. **Not rendered here:** the 22-subject bank for publication, the encode, the
   merge and the deploy, by instruction.

## 9. EVIDENCE

| file | what it decides |
|---|---|
| `P24-RECEIPTS/mbolt-census-before.txt` | THE MEASUREMENT: the pass-23 state, all 22 live subjects, with the breakdown that found the chord fault |
| `P24-RECEIPTS/mbolt-census-shipping.txt` | the same census after both mechanisms |
| `P24-RECEIPTS/mbolt-gate.txt` | the three legs green, with INFO B2b |
| `P24-RECEIPTS/mbolt-ctl-{no-avoid,no-split,fat-rod}.txt` | the three controls, each firing |
| `P24-RECEIPTS/gate-matrix.txt` + `gatematrix_p24.sh` + `runmatrix_p24.sh` | the matrix and its script, one invocation |
| `P24-RECEIPTS/crcs-{alloff,item1,item2,item3,ship}.txt` | the per-item byte identity, 22 subjects each |
| `P24-RECEIPTS/baseline-pass23-crcs.txt` | the pass-23 bank this was measured against |
| `P24-RECEIPTS/binaries-md5.txt` | every binary |
| `P24-LOOKS/01`..`11` | the ladders and the A/B pairs, each named in P24-NOTES |
| `P24-NOTES/FINDINGS-01-looks.md` | written after each look, before the next change |
| `P24-SHEETS/*` | every frame of Crackle, Hover, Inspect, Rest and Taunt II, production ink |
| `tools/reel/manafold_boltgate.cpp` | the committed probe/gate |
| `tools/reel/framediff.py` | the committed "which frames does this knob change, and where" tool |

**Renderer:** `.tmp/p24/bin/zhao-reel-cel.exe`, MD5
`b4ed489f3ec38338d0d2211caf6ad5dc`, SHA-256
`308a3fb52a84c10be9dcda875b00868dbb11f0ca57f2bd5a37d7c3b9ec4aea0b`. Built
directly with `tools/reel/build-direct.sh` and the `zhao-env.ps1` toolchain
(g++ 16.1.0 MinGW-W64 ucrt). No CMake was used and no CMake result is claimed.

**Presentation:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, set
explicitly on every render in this report.

## 10. TWO PROCESS FINDINGS WORTH CARRYING FORWARD

1. **Do not edit a shell script that bash is executing.** Two selector legs were
   appended to `gatematrix_p24.sh` mid-run. Bash reads a script incrementally BY
   BYTE OFFSET, so the insertion shifted everything after it and execution
   resumed at the wrong place -- the identity leg `e-item2` ran and reported
   TWICE, and nothing says what the shift skipped. The run was discarded and the
   matrix re-run from a FROZEN COPY under `.tmp/`. This is the live-tree trap in
   a shell instead of in Quartus, and the only tell was a duplicated PASS line:
   a tally would have counted it as one more green.
2. **A screen-space motion metric on an ORBITING clip measures the orbit.**
   Frame-to-frame pixel energy in Hover's rear box reads 129,794 at rear gain
   400 and 129,774 at gain 90 -- 0.015 % apart -- because the creature sweeping
   through the box swamps everything the knob does. The question was answered
   instead by the posed-pose audit (mrear) and by a tight crop on eight
   consecutive frames.
