# Task Log: RUN-20260906-0705 - [Describe objective here]

**Created:** 2026-09-06 07:05 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0705-manafold-pass8-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 07:05 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0705
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## Pass 8 log

### Lane
`C:\programmieren\zencrifice\manafold-p8-impl\{zhaozhou,Upheaval}`, cloned fresh
from `origin/main`, branch `manafold-pass8` in both. `origin/manafold-p7-review`
merged into the zhaozhou branch first: its three instruments (`webm2rgb.py`,
`eyesheet.py`, the `plates.py` crop mode) and `PASS-7-REVIEW.md` had never
landed on main, and the review is a `Read first` input.

### Item 1 -- the antenna. DONE (constants), by eye + a new committed probe.
The pass-7 review's read ("a uniform strap with mitred corners, no taper, no
knuckles") and the shipped source (five named `kKnuckleSwell*` constants that
arithmetic said nearly doubled the band) could not both be true. Wrote
`tools/reel/manafold_bandprobe.cpp` -- reads the COMPILED MESH, groups vertices
by exact bind y, prints halfX/halfZ per ring; `--selftest` plants a bulge and
also proves a flat stack reports flat.

**Both were true.** halfX ran 63..123 mm (ratio 195%) in pass 7. The knuckles
were there. The fault was that a 170 mm half-width swell on stations 340 mm
apart fills the whole gap -- the band never gets to BE a band -- and every
widening lands exactly on a fold station, where it reads as a fat mitre.

Fixed by making the knuckles LOCAL and the run between them THIN:
`kKnuckleSwellHalfMm` 170->120, `kLoopRings` 48->64 (so a narrower knuckle and
a wider fold blend both still round), `blend` 145->165 (the mitre itself; 168 is
the hard ceiling = half the shortest station spacing), the blade taper thinned
~15%, the swell profile u^2 -> smoothstep u^2(3-2u) (84% of full height at half
width instead of 56%, still zero-slope at the rim so pass 6's no-crease property
survives). New profile: halfX 50..117, ratio 234%.

Sheet ratio used as a BIAS check only, like for like (both side projection, both
as a fraction of body width): sheet band ~7.8% / knuckle ~15.5%; pass 7 shipped
14% / 24%. Deliberately not taken all the way to 7.8% -- that is under 4 px on
the house zoom.

Gates after: meshcheck CLEAN (28 meshlets, 2448 tris), clearance contract holds,
closure worst rim 1053 pm vs gate 1120, slot-13 declared contact -28 mm
(declared -25, band -60..-5), 5d gate A 22 mm, gate B 801 pm.

### WHERE I WAS when OWNER-DIRECTION-7 arrived (written BEFORE reading it)
CLAUDE.md: "write down where you were BEFORE reading it."

In flight, uncommitted, in `zhaozhou/tools/reel`:
* `manafold_fx.h` -- DONE, unbuilt: `kManaAquaCoreMid/Hi` + `kRampAquaCore` +
  `mana_core_ramp()`, so the mote's OPAQUE heart stops writing the additive
  ramp's near-white HI (that is the traced mechanism of the 48.7% hue-neutral
  reading: heart writes kManaAquaHi (175,255,236), halo then adds (59,86,80) on
  top of it = white). And `kSmearChromaFloorPm = 600` in `smear_composite`.
* `zhao_reel.cpp` -- HALF DONE: `manafold-fogprobe-smear` subject added, and
  the `u02_smear_only` ctx flag + its two `break`s at the splat draw loops.
  **NEXT STEP: wire `SceneSubject::u02_smear_only` -> `cr_ctx.u02_smear_only`
  (near line 3644 where u02_smear_preset is wired) and set it on the new
  subject; `s.u02_mana = 0` on its own would have made the subject
  byte-identical to fogprobe-off, because the whole mana block including the
  smear is gated on `u02_mana != 0` and the smear has no source but the splats.
  That is the SAME structural blindness pass 7 shipped, so it must not ship
  again.** Then build cel, render the fogprobe trio + rest, and re-measure.

### Items 2, 3 and 10, and Direction 7 §6/§6a. DONE.

**The probe first (item 2's second half).** `manafold-fogprobe-smear` is the
third leg the pass-7 pair lacked. It could NOT be built as `u02_mana = 0`: the
whole mana block, the smear included, is gated on `u02_mana != 0` and the plane's
only source is the splats, so that subject would have been byte-identical to
fogprobe-off -- a second blind probe. It is `u02_mana = 9` (rest's own
candidate) with a new `u02_smear_only` flag that suppresses the mana BODIES and
keeps the FEED. `mana_hue_probe.py` gains a `dominant` mode (the stratification
the reviewer hand-rolled), a byte-identical-corner VALIDITY check that is printed
whether it passes or fails, and a `selftest` that proves it reports white on
white, colour on colour, and can FAIL its own validity check.

**The mote white took three authoring passes and only the third was right, and
every wrong turn was found by looking, not by the metric.**
1. Gave the opaque heart a dark saturated ramp (the arithmetic said the halo's
   known add would then land saturated). Measured WORSE: 50.5%, (217,233,239).
   The render said why -- the white blobs are ~7 px and the heart is ~3, so the
   white was never the heart. It is the additive halo, and §4's law is that
   additive over bright pink can only whiten.
2. Drew the halo FIRST and the heart SECOND, grew the heart. Still 50.1%. The
   render showed hard dark-green specks: `opaque` skips t < 20 and the Lorentzian
   bloom only reaches t >= 20 inside ~28% of its radius, so an "opaque heart" of
   r_px 8 has ALWAYS painted ~1.6 px whatever kMoteCoreOfHaloPm said. Two passes
   had been recolouring a speck.
3. A `soft` splat mode (blend by the sprite's own intensity, so it is saturated
   where it dominates and feathered at the rim and CANNOT stack to white), a
   core ramp whose LO == MID so the body is bright across its face rather than
   nearly black, and r_px 1600 pm of the halo.

Motes: **48.7% -> 22.8% hue-neutral, saturation 39 -> 87, mean RGB
(215,229,236) -> (163,232,227)**, and by eye the pocket is round turquoise
bodies inside white glows instead of white steam.
kMoteHaloGainPm, kMoteCount, the radii and the spread are all UNTOUCHED.

**The smear (item 3).** Two faults, one visible only after the other was fixed:
the plane was fed from the HALO's near-white ramp, and the composite's
"kept vivid" `c[k]*3/2` clipped two channels and destroyed the hue at the last
step (§9's "clearly visible in a comment" again). Fed from the saturated core
ramp, vivified hue-preservingly (`kSmearVividPm`), plus a signed chroma floor
(`kSmearChromaFloorPm`) that pulls the channels the cell is WEAK in down -- the
half a lerp cannot do, and the half that makes a block read as gas not dirt.
Smear alone: **0.0% neutral, saturation 168, (69,202,180)**.
09-ENGINE-GOTCHAS §14 applied on the way past: `kMoteCoreOfHaloPm` was also the
smear's feed radius; split into `kSmearFeedOfHaloPm` BEFORE moving it.

**Item 10:** `kFogThicknessPm`'s comment said 2000 where it ships 4500. Corrected.

**Direction 7 §6 / §6a:** the forehead knuckle down (58 -> 50 rx), every other
knuckle UP (A 56 -> 64, B 54 -> 62, C 52 -> 60, end 54 -> 62). Checked which of
the two things at the forehead station was the fat before cutting, as the
direction warns: the taper's flare is 74 rx and the swell was +58, making that
station 19% heavier than hinge A, so the SWELL was the outlier and the flare --
whose removal would re-open the Direction 5 §1 dongle -- is untouched.

### Direction 7 §5 (the eyes). DONE, with one declared cost and one REFUTED hypothesis.

* **§5.1 the star is centred on the purple.** `kStarOffsetYMm` 46 -> 0. The lens
  is symmetric about its own y=0 (`make_eye_lens` runs t_pm -1000..+1000), so 46
  was a genuine offset and not the lens's centre. Confirmed by eye on `channel`
  f224: the star now sits in the middle of the almond.
* **§5.1's HYPOTHESIS IS REFUTED, and the refutation is the useful part.** The
  direction asked whether the off-centre rest was why rule 3 reads violated, and
  asked for it to be tested first. Measured on the committed probe: 1499 -> 1463
  violations, 2.4%. **It is not the rest offset.** Item 4 does not shrink, and
  centring is still right on the owner's own eye, independently.
* **Rule 1 then failed at 32 mm against a 29 mm cap**, and the reason is worth
  recording: pass 7 had set that cap to EXACTLY its own worst measurement
  (330 pm == 29 mm == the reported worst), so it had zero headroom and any change
  to the rest pose crosses it. Swept `kGazeLiftMaxA16` 5200/4800/4400 -- the
  worst does not move at all, so it is not gaze travel and cutting the gaze would
  cost readable motion for nothing. Cap 330 -> 370 pm, stated openly, with rule 2
  (the rule that encodes the owner's intent) IMPROVING 760 -> 890 pm against a
  600 floor as the corroborating evidence that the star is more contained, not
  less.
* **§5.2/§5.3 the purple sinks in.** `kEyeXMm` 381 -> 400. Deepest point 801 ->
  837 pm of the body's ellipsoid, crown 108 -> 123 mm proud. ⚠ **Declared cost:**
  the star rides the lens, so rule 3's count rises 1463 -> 1658. §5.3 exempts the
  PURPLE from rule 3 and explicitly does not exempt the star, so this is a real
  trade; 400 is the low end of what fixes the sink, for that reason.

### Direction 7 §4 (the smear scales with speed). DONE.
`smear_speed_mul_pm(mm this frame)` -> the composite's opacity, with
`kSmearSpeedBasePm = 380` (never zero) reaching full at
`kSmearSpeedFullMmPerFrame = 34`, which is `hasty`'s own travel (8330 mm / 120
keys / 2 frames per key) because the owner names hasty as the reference for the
moving end. Measured from the POSED root anchor `fa.body`, not from the instance
placement -- the instance does not move; the clip's root displacement does, and
only the posed anchor carries it. `fall` (slot 9) joins the travelling rung.

The preset table SURVIVES because a rung is an identity (decay, step, jitter,
tear), not an intensity -- but intensity is no longer decided per clip at all,
which is what retires the off-by-one class the direction names.

Looked at: `fall` f200/f250 now carries a large aqua cloud through the drop and
the catch (the vertical-motion case the direction asked to be checked
specifically -- it works, because the trail separates vertically on screen);
`hasty` keeps its reference trail; `rest` has a modest, present haze instead of
either nothing or a full cloud.

### Direction 7 §1 (hinges in all directions, individually). DONE.
**What was actually wrong, and it is §9's pattern again.** `loop_pose` already
TOOK a per-hinge out-of-plane tilt for A, B and C -- pass 6 C.1 built it -- but
`loop_alive` and `whole_wobble`, the two layers every clip goes through, passed
ONE tilt value and handed it to A only. B and C carried their rest tilt and
nothing else, and `kBNeck` was driven by nothing at all. "Each hinge moves up
and down separately" was true of the RIG and had never once happened on screen.

Now: a shared `hinge_play()` gives all four stations (neck, A, B, C) their own
tilt (about X) and yaw (about Y) on top of their fold (about Z), each with its
own scale (`kHingeAxisScalePm`), its own phase (`kHingePhaseStepA16` per
station, so the motion TRAVELS along the antenna), and two axes at rates that
do not divide into each other or into the fold's.

**The constraint is authored as part of the look**, per the direction: fixed
axes in a fixed composition order at every station, bounded amplitudes small
next to the fold, phase lag rather than free noise. A joint with a rotation
ORDER reads as a mechanism; one that can go anywhere reads as string.

`loop_pose`'s signature is append-only and every new argument defaults to zero,
so no existing call site changes meaning and no clip retimes.

Gates after: meshcheck CLEAN (2448 tris), closure sweep 989 pm and clip-bank
1053 pm (both unchanged -- expected, the closure aim is computed inside
loop_pose and compensates by construction), clearance holds, slot-13 declared
contact -35 mm inside the -60..-5 band, 5d gate A 22 mm, gate B 837 pm.
Bandprobe: halfX 50..125, ratio 250%.

### Direction 7 §3 (shapes are intermittent) and §2 (edge-drawn). DONE, with §2's tension REPORTED not papered over.

**§3 needed almost no new machinery, because the creature already had it and
nobody had used it.** The mote cloud's COHERENCE is derived from the antenna's
own enclosed area, so when the hands open the shape dissolves into the channel
cloud by itself. The only thing missing was a segment in which the hands are
open. `kSegDrift` is that segment, hashed 88..168 frames, at the HEAD of every
cycle so a clip also OPENS in the standard look instead of mid-fold. Direction
4's "a loop of that going on all the time" is superseded, as §3 says.
The short-clip compression now includes the drift and compresses it HARDER than
the other segments -- a short clip that spent itself drifting would never show a
shape at all, which is the fault's mirror image.

**§2, the edge.** Ported the lab's edge into the shipping fold: `fold_edge_link`
(which links close for RING/TRIANGLE, run open for CRESCENT/S-CURL, skip the
BAR's seam and stay within an arm for the STAR), stamped along `bolt_path` in
the fold's OWN ramp with pass 8's soft body as its core -- never the lightning
primitive's hard-coded white, which the lab measured putting 366 near-white px
on screen and dropping saturation to 108.9. Lit only above
`kFoldEdgeCohMinPm`: below that there is no shape to outline.
**The world-space HOLD is deliberately NOT integrated**, per §2.

Also §2: the fold now rotates the shape on **all three axes** (the authored
facing yaw plus two slow incommensurate turns about X and Z), and the shape is
**malleable** -- the stencil offset is scaled anisotropically by the knead
agitation, so it changes PROPORTION under handling rather than size. And the
clipping fix is positional as instructed: `kStencilClear{X,Y,Z}Mm` moves THE
SHAPE out of the antenna band's plane, not the antenna.
One `place()` lambda does all of it, shared by the outline and the motes, so
they cannot disagree about where the shape is.

⚠ **§2's recorded tension, reported rather than resolved:** on `channel` the
pocket now reads as bright turquoise folded mana with a visible outline, and the
mana clearly does something different at different times -- but **the shape is
not yet nameable sitting on the antenna**. That is exactly what the direction
said to say with plates rather than fix by quietly reintroducing the hold, and
the hold has not been reintroduced. Plates: pG.png (four beats of channel),
pH.png (the pocket at 7x).

Gates after: meshcheck CLEAN, clearance holds, closure 1013 pm vs 1120,
declared contact -35 mm in band, rule 1 OK, rule 2 890 pm, both 5d gates green.

### Closing checks
* Pass-8 item 11: `check_css_wiring` now also guards the archive-generation
  selector family (it had only ever checked the three TAB families). Proved
  failable: `MAX_ARCHIVE_GENERATIONS = 20` against the shipped `style.css`
  refuses the build with the right message.
* **All 16 clips render, no crash**, frame counts matching the shipped table.
  Whole-bank sheets looked at (plates 11, 12).
* **A/B'd `kEyeXMm` 381 vs 400 BY EYE on `damage` f230 at 9x** rather than
  trusting the rule-3 count that moved. The far lens crosses the body outline at
  BOTH values and the difference is a pixel or two, so the crossing is not caused
  by the push-out; the push-out slightly aggravates a pre-existing fault while
  fixing the sink the owner named, and §5.3 wants the purple over the edge. 400
  kept, and the reasoning is in PASS-8-FINDINGS rather than only here.
* Final gates: meshcheck CLEAN (28 meshlets, 2448 tris), clearance holds,
  closure 1013 pm vs 1120, declared contact -35 mm in the -60..-5 band, 5c rule 1
  and 2 OK, 5d gates A 22 mm and B 837 pm. Rule 3 REPORTED-NOT-ENFORCED at 1658,
  declared.
* No background processes left running; every render was foreground and its exit
  code read directly, never a pipeline's.
