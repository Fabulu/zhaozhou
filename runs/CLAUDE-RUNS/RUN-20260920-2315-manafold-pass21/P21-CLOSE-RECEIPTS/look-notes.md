# Pass-21 close: frame review notes

Written after each image, before the next was requested. Bank root
`C:\programmieren\zencrifice\manafold-p16\p21-final-reel-22`, 22 subjects,
7,992 frames, manifest SHA-256
`639c370cf32cb777e63896af064fc56a0d48f01edf354f2ee15d68e1ea395633`.
Sheets are `P21-BANK-SHEETS/` (every frame, 96x60 tiles, index on each; the
viewing copies are <=1600 px JPEG q80).

## 1. Inspect, all 600 frames (tile ~78 px)

Complete and contiguous: 0..599, no missing, blank, black, torn or half-drawn
frame, and no discontinuity at any row break. The orbit reads as one continuous
motion through the four-light showcase -- pink key through to the dark violet
back half and out again -- and the loop seam f599 -> f0 is continuous.

The antenna reads as a JOINTED shape at this scale on every frame: a compact
angular loop, not a soft lump. Through f060..f140 the forward run extends to the
upper right and the form is plainly made of straight sections; through f300..f340
the loop swings behind and the same construction reads from the other side.

At 78 px I can judge presence, continuity, framing, gross silhouette and the
overall read. I CANNOT judge from this image whether a bend sits mid-run or on a
ball; that is what the close looks below are for.

## 2. Inspect, twelve frames across the whole orbit, 2x whole frame
`P21-FINAL-LOOKS/L02-inspect-orbit-2x.jpg` -- f0, 60, 120, 180, 240, 300, 360,
420, 480, 540, 580, 599.

**THE OWNER'S QUESTION, ANSWERED: the runs read STRAIGHT and every bend sits on
a ball.** In all twelve frames the antenna is four straight segments meeting at
rounded knuckles. Not one frame bends in the middle of a run. The knuckles are
visible as knuckles at 2x -- rounded, wider than the rod, and they are where the
direction changes. f0, f300 and f599 show it most plainly: a straight rise, a
round corner, a straight top run, a round corner, a straight descent.

The character is more CONSTRUCTED than pass 20 -- at f0 and f300 the loop is
close to rectangular and reads a little like a wire frame with beads on it. That
is Direction 22 followed literally and the independent review's judgement was to
accept it; I agree, and I am naming it here so the owner can disagree with a
specific thing rather than a feeling.

Also correct in every tile: the outline is enclosed, both eye plates are present
and correctly placed on the body, the mana lightning sits in the loop window
with its motes, and the ground line and framing hold.

## 3. The sharpest joints in the whole bank -- is there a visible crotch?
`L03-sharp-joints-4x.jpg` (Fall f80, the bank's worst joint C at **152.1 deg**),
`L04-jointC-taunt-hasty-3x.jpg` (Taunt f134, 147.7 deg; Hasty f82, 147.2 deg),
`L05-jointAB-hover-3x.jpg` (Hover f242, the worst joint B at **100.2 deg**, and
f246, the worst joint A at **150.4 deg**). Frames chosen by BADNESS from mrod's
own per-ring CSV over all 24 slots via the committed probe
`P21-CLOSE-RECEIPTS/p21_badness_frames.py`, never by index.

**NO VISIBLE CROTCH ANYWHERE. The answer is the same as the review's and I
looked at different frames to get it.** At every one of these angles the
junction reads as a FOLDED HINGE: the band doubles back on itself and the merge
is hidden by the band's own width and by the outline ink. There is no wedge, no
spike and no second silhouette edge running out of a ball.

* **Fall f80 at 4x** is the clearest. The antenna leaves the body, runs straight
  up-left, and the fold at the far end is a smooth narrow neck. The ink is a
  closed O around the whole creature.
* **Taunt f134** folds the loop right over to the left; the outer knuckle is a
  plainly rounded bead and the two runs rejoin at the top with no notch.
* **Hover f242/f246** are the deepest folds in the bank and the most
  foreshortened -- the runs nearly overlay each other. Even here the knuckle
  reads as a bead and the outline stays enclosed.
* **Hasty f82** is a DISTANCE clip; at 3x the creature is ~90 px tall. The
  distance-thinned ink is correct and the clip is continuous, but **this image
  cannot judge a joint on Hasty and I am not claiming it does** -- Hasty's joint
  evidence is mrod R6/R7 over its own frames, not this picture.

## 4. The knead, on Inspect at 3x -- f540, f558, f576, f594
`L06-knead-inspect-3x.jpg`. The press frame was chosen by BADNESS, not by index:
ball B's centroid height per sample straight out of mrod's CSV, lowest wins.
The deepest press in the bank is **slot 0 key 288 = presentation frame 576**,
the same key the independent review measured on its own build.

**THE PASS-20 KNEAD IS PRESERVED AND IT READS.** Up, press, up:
* f540 -- the loop's top run is up and away from the body, the top-left knuckle
  a distinct bead.
* f558 -- the window is open wide.
* f576 -- the middle of the top run is driven DOWN into the body and the loop
  window collapses to a wedge. It is a different, deliberate shape, not a wobble.
* f594 -- back up, the window open again, the mana figure back inside it.

**And the mana answers the press**, visible rather than inferred: at f576 the
lightning figure has ridden down with the ball and flattened along it; by f594 it
is back in the loop window as a broad ring.

⚠ THIS IS ALSO THE CLEAREST RODS-AND-BALLS EVIDENCE IN THE PACKET. f594 is a
clean quadrilateral -- four straight rods, four round knuckles, bending only at
the knuckles. Owner Direction 22, drawn. Whatever the press is doing, it is not
bending a run.

## 5. The End ball's motion trace, five clips
`L07-end-trace.jpg`, drawn by the committed probe
`P21-CLOSE-RECEIPTS/p21_endtrace.py` from the POSED skin at 60 Hz -- speed and
|jerk| per sample, one row per clip, each autoscaled so the SHAPE is what reads.
A spazz is a shape (a jagged run of tall erratic spikes), not a peak number, so a
peak alone cannot answer Direction 22 item 2.

| clip | speed peak | jerk peak | median jerk | peak/median |
|---|---:|---:|---:|---:|
| inspect/hover | 2.53 mm | 0.72 | 0.244 | **2.9** |
| channel | 4.12 | 1.06 | 0.279 | **3.8** |
| taunt3 | 8.12 | 4.60 | 0.405 | 11.4 |
| fall | 4.57 | 6.87 | 0.192 | 35.7 |
| blown | 6.18 | 8.61 | 0.184 | 46.8 |

**THE END IS CALM, and the trace shows WHY the two big ratios are not a
counter-example.** On the looping clips -- the ones where the spazz lived --
speed is a clean regular oscillation and jerk is a low, dense, UNIFORM band with
no isolated tall spike anywhere: peak/median 2.9 and 3.8. On the one-shots the
speed trace is a STEP FUNCTION (flat hold, authored burst, flat hold) and every
jerk spike sits exactly on a step edge, with jerk near zero in between: two edges
on Fall (the landing), four on Blown, three bursts on Taunt III. That is authored
motion arriving through a rigid rod. A spazz is the opposite shape -- spikes
scattered through the holds.

⚠ WHAT THIS IMAGE CANNOT DO is compare against pass 20; a trace with nothing
beside it shows a shape, not an improvement. That is section 6.

## 6. THE BEFORE/AFTER: the same probe, the same clips, the pass-20 rig
`L08-end-trace-PASS20.jpg` beside `L07-end-trace.jpg`. One probe, one binary,
one CSV format; only `ZHAO_U02_RIG` differs. This is the like-for-like the
creature's own rules demand -- same pose, same clips, same instrument.

| clip | speed peak p20 -> rods | jerk peak p20 -> rods | **median jerk p20 -> rods** |
|---|---|---|---|
| inspect/hover | 4.74 -> **2.53** (-47 %) | 5.38 -> **0.72** (-87 %) | 0.814 -> **0.244** (-70 %) |
| channel | 5.03 -> 4.12 (-18 %) | 3.42 -> **1.06** (-69 %) | 0.757 -> **0.279** (-63 %) |
| taunt3 | 9.19 -> 8.12 (-12 %) | 10.93 -> **4.60** (-58 %) | 0.908 -> **0.405** (-55 %) |
| fall | 6.86 -> 4.57 (-33 %) | 10.35 -> 6.87 (-34 %) | 0.712 -> **0.192** (-73 %) |
| blown | 5.66 -> 6.18 (**+9 %**) | 7.03 -> 8.61 (**+22 %**) | 0.626 -> **0.184** (-71 %) |

**THE SPAZZ IS VISIBLE IN THE PASS-20 PICTURE AND ABSENT FROM THE PASS-21 ONE.**
Pass 20's inspect/hover row is a JAGGED trace -- erratic tall spikes scattered
right through the oscillation, the cycle shape barely readable underneath, and a
jerk field full of isolated towers. Pass 21's same row is a clean regular
oscillation over a low uniform jerk band. That is the owner's complaint, drawn,
and then removed.

**And it explains the two numbers that go the WRONG way, which matters more than
the ones that go the right way.** Blown's peak speed and peak jerk both RISE
under rods -- and its MEDIAN jerk falls 71 %. The same pattern holds on every
clip: the median (the continuous chatter, which is the spazz) collapses by
55-73 % everywhere, while the peaks, which are authored one-shot beats, are
sometimes carried harder because a rigid rod transmits a beat a bowed band used
to absorb. The independent review saw the peak rise on Taunt III and called it
authored motion; the median column is the evidence for that reading, on five
clips rather than one.

Recorded honestly: **on peak jerk alone, Blown is 22 % worse than pass 20.** It
is inside every bound and it is an authored impact, but it is the one number in
this packet that moved the wrong way and it should not be buried.

## 7. The other 21 subjects, every frame
Individual sheets: Hover (600), Crackle (600), Death II / death-gutter (590).
Composites, with the tile scale printed on each by `compose.py`:
`C03` Channel + Trick (60 px), `C04` Taunt III + Death / death-drop (59 px),
`C05` Hits / damage + Rest (56 px), `C06` Fall + Flight + Mana lasso (47 px),
`C07` Taunt + Hasty + Lasso / taunt2 + Pirouette (49 px),
`C08` Drift + Blown + Curious + Startle + Hit (45 px).

**ALL 22 SUBJECTS, ALL 7,992 FRAMES, LOOKED AT ON COMPLETE EVERY-FRAME SHEETS.**
No missing, blank, black, torn or half-drawn frame anywhere in the bank, no
discontinuity at any row break or loop seam that sheet scale can resolve, and
framing holds on every clip. (The black block at the end of a sheet's last row is
sheet PADDING where the frame count does not fill 20 columns -- it is not a
frame. Checked against each subject's count.)

Per subject: Hover and Crackle are steady loops with the jointed antenna legible
throughout; Channel's night blaze is continuous with the mana in the loop window
every frame; Trick plants and holds through the full 360; Taunt III's crown
shuffle reads; Death / death-drop and Death II / death-gutter both sag out to a
still corpse with the mana fading; Damage and Rest, Fall, Flight and Mana lasso
(the ring leaves and returns), Taunt, Hasty, Lasso, Pirouette, Drift, Blown,
Curious, Startle and Hit are all complete and continuous.

⚠ **WHAT COMPOSITE SCALE CANNOT JUDGE, plainly.** At 45-60 px I can judge
presence, continuity, framing and gross silhouette, and nothing else. Joint
placement, line weight, eye size and the mana's fine behaviour on those subjects
rest on the gate matrix, on the exact-off scope proof, and on the six subjects
looked at closely above -- not on these pictures.

## 8. Ground contact: Trick's plant
`L09-trick-plant-4x.jpg` (f120, f180, f230, f290 at 4x) plus the committed 3D
pose probe, re-run on this bank (`P21-CLOSE-RECEIPTS/mprobe.txt`, RC 0).

**AUTHORED, DECLARED, AND IT READS.** The probe:
* `slot 13 DECLARED CONTACT keys 78..148 (+2-key apron): deepest vertex -25 mm
  (declared -25, accepted -60..-5)`
* `ANTENNA SUPPORT carrier B key+midpoints 140/140 owned, missing 0 depth-fail 0,
  range -25..-16 mm`
* `PLANTED 360 keys 100..140: carrier-B owned 82/82, depth-fail 0, range
  -25..-17 mm`

And in the picture: at f180, f230 and f290 the antenna is two STRAIGHT rods
running down from the body to a single round knuckle that rests ON the dirt, with
dust around it. The knuckle is slightly into the surface and nothing else is --
no float, no sink. f120 is the tip-over before contact, correctly clear.

This is also the most legible rods-and-balls frame in the bank for a different
reason: upside down, the support reads unmistakably as stick-ball-stick.

⚠ The penetration number is the PROBE's, walking posed vertices against terrain
height. It is not counted off the rendered image, which CLAUDE.md records as
unsound.

---

# VERDICT: **NO FAULT FOUND. The bank is good to encode.**

Owner Direction 22, item by item:
1. **Too many joints / each run should read as one piece** -- MET. Sections 2, 4
   and 8: four straight rods, four round knuckles, no mid-run bend in any frame
   looked at, at 2x, 3x and 4x, across the whole orbit and upside down.
2. **The End still spazzes** -- MET. Sections 5 and 6: the jagged spike field is
   in the pass-20 trace and gone from the pass-21 one; median jerk falls 55-73 %
   on all five clips measured. One honest exception recorded: Blown's PEAK jerk
   is 22 % higher, an authored impact now carried by a rigid rod.
3. **Keep the pass-20 knead** -- MET. Section 4: press and release read clearly
   at the deepest press in the bank, and the mana rides down with it.
4. **The whole antenna must read smooth in motion** -- MET with a named
   qualification. It reads as a JOINTED LIMB, which is more geometric than pass
   20 and is what makes it legible; pass 20 had no structure to read at all. The
   character change is the owner's to accept or reject, and section 2 names it.
