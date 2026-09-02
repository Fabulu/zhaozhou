# QA — Direction 23 + Direction 24, the slow readable whole-body S spring

**QA pass, 2026-09-03.** Lane
`zixxtrixx-wholebody-s-spring-20260901/zhaozhou`, branch `main`, HEAD
`48310d18`, working tree clean apart from this run folder. Built with
`tools/reel/build-direct.sh` (cel, then probe, one target per call) into a
QA-owned output tree. Rendered with explicit `ZIXX_EXP=celmain`
`ZIXX_LIGHT=diagonal-cool-cross`. `sacengine` never run. **Nothing authored
was changed by this pass.**

## VERDICT: FAIL. NOT PUBLISHED.

Two independent failures, either of which alone stops the gate:

* **Direction 23 acceptance 3** — the whole body still does not become the S.
  The review-response fix is real in the pose tables and **sub-pixel on
  screen**; the implementer's headline number for it does not reproduce.
* **Direction 24 acceptance 1, 2 and 3** — the tail is still by a wide margin
  the dominant mover, it reverses direction and swings 14 % in apparent size,
  and the S does not grow. This pass made all three **worse** than the build
  the reviewer measured.

Everything Direction 23 asked for *first* — smoothness — is genuinely fixed and
should be kept. The machinery, the retime, the schedule and the life layer are
sound. What is wrong is one authoring decision, repeated for five directions:
**the travel budget lives in the tail.**

---

## How I judged it

I built the cel reel and the probe at `48310d18` myself and rendered
`zixxtrixx-spring-side` (fixed true-side camera, 191 frames),
`zixxtrixx-spring-top`, `zixxtrixx-jump-one` (293 frames) and
`zixxtrixx-balance`. I looked at every-frame contact sheets and key-pose plates
**before** reading any number.

Because the reviewer's silhouette instrument was not committed, I rebuilt it
from the method recorded in `evidence/review/reviewer-measurements.txt` and
**committed it** (`evidence/qa/qa_region.py` and the other `qa_*.py` probes, per
the "commit the probe" law). I then **calibrated it** by rendering the reviewed
build (`ecf0e3ab`, header checked out into a QA-only scratch build, working tree
restored) and re-deriving the reviewer's own published figures:

| reviewer published (ecf0e3ab) | my instrument on ecf0e3ab |
|---|---|
| beat-1 share 82.5 / 6.7 / 10.8 % | **81.5 / 7.3 / 11.1 %** |
| beat-2 share 45.3 / 23.9 / 30.8 % | **44.3 / 24.0 / 31.7 %** |
| descent f72→f160 −0.25 / +2.62 / +7.12 px | **−0.29 / +2.63 / +7.16 px** |
| nose tip −7.0 px back, +10.3 px down | **−7 px back, +10.5 px down** |
| f0–f10 byte-identical | **10 byte-identical frames** |

Agreement to within 1 %. Every prev-vs-HEAD comparison below is therefore
like-for-like: **one instrument, one camera, one window, two builds differing
only in `zixxtrixx.h`.**

Playback is 60 fps. Ground time is now 72-key arming + 12-key hold = 168 frames
= **2.80 s**.

---

## Verdict per acceptance point — DIRECTION 23

### 1. The movement is smooth — **PASS**

The primary fault that generated Direction 23 is solved, and by a wide margin.
Fixed side camera, ground phase f0–168:

| per frame | spring arming (HEAD) | balance clip, same instrument |
|---|---|---|
| centroid x \|v\| med / max | 0.143 / 0.927 px | 0.358 / 3.146 px |
| centroid x jerk max | 1.135 px | 1.495 px |
| centroid y \|v\| med / max | 0.051 / 0.305 px | 0.109 px med |
| centroid y jerk max | 0.373 px | 0.427 px |
| silhouette XOR med / max | **1.71 / 4.36 %/f** | **3.67 / 18.52 %/f** |

The arming is less than half as fast per frame as the accepted balance clip and
its worst frame is a quarter of the balance clip's worst. The 4.36 % maximum is
not a pop — the top eight frames are all in f112–129, the fastest stretch of
beat 2, i.e. the peak of a smooth ramp.

30 Hz staircase: odd/even mean-step ratio **0.98** — dead. Authoring schedule
peaks at **14 mm/key** of station movement. Nose-tip x has 21 non-zero steps in
168 frames, every one of them ±1 px.

### 2. A viewer can see what the animal is doing — **PASS on legibility**

Every-frame contact sheets (`evidence/qa/jumpone-contact-f000-083.png`,
`…-f084-167.png`): the animal is readable in every frame, nothing blobs, nothing
snaps, no reversal is visible in the primary. Beat structure separates cleanly
on silhouette rate:

```
settle f  0- 12   0.27 %/f
beat1  f 12- 72   1.85 %/f
dwell  f 72- 82   0.80 %/f
beat2  f 82-144   2.27 %/f
hold   f144-168   0.96 %/f
launch f168-175  22.57 %/f
```

Legible *as a rhythm*. **What it is legible as** is the subject of Direction 24
and of point 3 below.

### 3. The whole body becomes the S, slowly, with the balance clip's organic quality — **FAIL**

Slow: yes, 1.00 s. Organic: yes. **Whole body: no — and essentially unmoved
from the version the reviewer failed.**

The implementer reported the fix as *"beat-1 change share tail/mid/head
82.5/6.7/10.8 → 64.6/17.1/18.3 %"*, attributed to "the reviewer's instrument".
**That number does not reproduce.** On the calibrated instrument:

| beat-1 share, f12–72, split 170/210 | tail | mid | head |
|---|---|---|---|
| ecf0e3ab (what the reviewer failed) | 81.5 % | 7.3 % | 11.1 % |
| **48310d18 (this pass)** | **80.2 %** | **7.7 %** | **12.1 %** |
| implementer's claim | 64.6 % | 17.1 % | 18.3 % |

I swept **25 combinations** of measurement window (f0/f4/f8/f12 → f72/f80) and
region split (160/200 through 180/220) to see whether any reasonable variant
produces the claimed figure. None does: HEAD's tail share lands between
**76.7 % and 80.7 %** in every variant, the middle's between 7.4 % and 11.5 %,
and the prev→HEAD delta is consistently about **1.4 percentage points**, never
18 (`evidence/qa/qa-fault1-sweep.txt`).

In absolute changed pixels — the share is a ratio and the tail's own increase
inflates it — beat 1's middle third went from **193 px to 211 px** of cumulative
silhouette change over 60 frames. That is 3.2 → 3.5 px per frame.

The world-millimetre probe, which is not a projection, agrees that the change is
real and agrees that it is small:

```
SPRING full-S entry mean station travel:
  ecf0e3ab   head=51 neck=47 front=27 middle=5  grounded run=8  taper=105 tail=257 mm
  48310d18   head=75 neck=64 front=52 middle=28 grounded run=33 taper=105 tail=258 mm
```

Middle 5 → 28 mm is a genuine 5.6× — **and 28 mm is 11 % of the tail's 258 mm.**
At this camera 41 mm ≈ 1 px, so the celebrated mid-body arch is a **0.7 px
event**. Measured directly on screen it is 1 px: the mid-region centroid rises
1.08 px across beat 1 (was 0.63), and the ventral run's top edge rises 1 px
(was 0). It is visible in an 8× still overlay
(`evidence/qa/midbody-outline-zoom-prev-vs-head.png`) and not at 384×240 in
motion over one second.

This is the CLAUDE.md art law's exact failure mode with the sign flipped: a
number that reads as a fix (5 → 28 mm! 5.6×!) describing something the eye
cannot see at delivery resolution.

### 4. The head moves slightly backward and slowly down, never approaching the tail — **PASS on the tail; NEEDS AN OWNER EYE on "slightly"**

*Never approaches the tail:* decisively, and better than before. Nose tip x
231–244 against a rearmost body pixel at 109–132; the gap never falls below
**109 px**.

*Slightly back, slowly down:* nose tip **−11 px back, +12.0 px down** over
2.80 s (was −7 / +10.5) — the implementer's claim, confirmed. The route is
monotone backward with four single-pixel forward steps in 168 frames. The
head-region centroid moves **−3.53 px** (was −2.18), with a **+0.89 px** forward
excursion at f52 en route.

One implementer claim I could not confirm: the forward excursion was reported as
having dropped from 0.85 to 0.64 px "at the measurement floor". I measure
**0.89 px** — unchanged. Sub-pixel either way, so it changes nothing, but it is
the second number in this pass that moved in the report and not in the render.

### 5. Everything descends together during the compression — **PASS**

This one is genuinely fixed. Region centroid y, f72 → f144, fixed side camera:

| region | ecf0e3ab | **48310d18** |
|---|---|---|
| head / front | +6.65 px | **+6.52 px** |
| mid body | +1.76 px | **+2.76 px** |
| tail / fins | **+0.05 px** (holds) | **+3.23 px** (descends) |

And in world millimetres: tail compression descent **+1 → −38 mm**, taper
−33 → −46, grounded run −37 → −66. The tail V that used to hold its rest rise
through the whole squash now comes down with the head, and it is visible in the
beat-2 plate (`evidence/qa/beat2-prev-vs-head.png`). Nothing rises during the
compression except the probe's "middle" group's +5 mm, which straddles the
arch's standing side; on screen that same band descends 2.76 px.

**Note for the record:** the mechanism that earns this PASS —
`kSpringBladeSquashRise`, which presses the fan down through the compression —
is the same mechanism that fails Direction 24 acceptance 1 below. Passing 5 and
failing D24 is not a contradiction: the direction asked for the *head's*
descent to be shared, and the answer given was to make the *tail* move more.

### 6. The four beats read in order and none is hurried — **PASS**

Settle 0.20 s, become-S 1.00 s, dwell 0.17 s, compress 1.03 s, hold 0.40 s,
then the launch. Ground time 2.80 s. The rate contrast between beats and joints
is 2.3–2.8×, and between hold and launch 23×. Deliberate, not draggy.

### 7. The launch follows the compression — **PASS**

Silhouette rate goes 0.96 %/f in the hold to **22.6 %/f** across f168–175, and
the flight it releases into is byte-exact Generation Thirteen (below).

---

## Verdict per acceptance point — DIRECTION 24

Direction 24 landed while this verification was running. It is binding, it
overrides the Directions 19–22 reading that every pass has worked from, and this
build fails it. My Direction-23 measurements answer it directly, so no new
instrument was needed — only a different question asked of the same numbers.

### 1. The tail stays put — no position, direction or size change — **FAIL, and this pass made it worse**

The single picture is `evidence/qa/tail-anchor-outlines.png`: the tail at rest
(red), assembled (yellow) and loaded (blue), 7×. The three outlines are in three
different places, at three different angles, at three different apparent
lengths. The body and hook behind them are nearly coincident.

Fixed side camera, tail third (screen x < 170):

| | ecf0e3ab | **48310d18** |
|---|---|---|
| beat-1 centroid rise (f12→f72) | −7.14 px (rises) | **−7.26 px (rises)** |
| beat-2 centroid fall (f82→end) | −0.29 px (holds) | **+2.48 px (falls)** |
| **direction change** | no | **YES** |
| fin-tip travel, beat 1 | x 110→125, y 124→110 | **x 109→125, y 124→109** |
| fin-tip reversal, beat 2 | x →128, y →110 | **x →120, y →118** |
| apparent-size swing (tail area) | **7 %** | **14 %** |
| beat-2 share of change | 44.3 % | **63.3 %** |

Read plainly: the fin tip travels about **21 px diagonally up-and-forward**
during the windup and then **reverses on both axes** during the compression,
while the tail's apparent area swings **14 %** — double the previous build's
7 %. The tail is the loudest thing in the clip, it changes direction mid-clip,
and it changes size. That is, in order, every fault the owner named.

The mechanisms are named constants and this pass introduced or deepened all
three:

* `kSpringBladeSquashRise = 3500` — **added by this pass**, explicitly so the
  fan rises with the beat-1 gather and presses down through the compression.
  That is an authored direction change.
* `kSpringCollapsedHeading` stations 15–18 moved
  −4500/−9500/−13000/−14500 → −3900/−8300/−11000/−11800 — the rear curl is
  pressed back down in the collapse, i.e. the rear swings up and then unwinds.
* `kSpringBladeFlare = 900` — the fan splays during the compression, which is
  most of the apparent-size swing.

The windup's change share is still **80.2 % tail** on the fixed camera. Direction
24 names this instrument explicitly and says the windup must not be dominated by
the tail.

### 2. The S visibly grows, forward from the planted tail — **FAIL**

The owner's *"the S doesn't grow"* has a one-line mechanical cause, and it is
visible in the header:

```
kStanceSlope             [5..9] = 14600, 21400, 25200, 20000, 11600   (rest)
kSpringAbsorbHeading     [5..9] = 14600, 21400, 25200, 20000, 11600
kSpringAssembledHeading  [5..9] = 14600, 21400, 25200, 20000, 11600
```

**The S's own hook is byte-identical across rest, absorb and assembled.** The
front of the animal is authored not to change at all during the windup. Stations
0–4 move by 100–200 units out of ~25000, and stations 10–14 by the 1 px arch
measured above. On screen the head region contributes 12.1 % of beat 1's change
and its centroid moves 0.20 px. There is nothing for a viewer to see growing,
because nothing grows — the S is fully formed at rest and the windup only
gathers the rear into it.

### 3. The animal reads as one connected body — **FAIL**

Same evidence, stated as a whole: 80 % of the windup happens in the back third
while the front two thirds hold still to within a pixel, and then the
compression reverses the back third's direction. That is parts taking turns, not
one body. The reviewer already called this "two consecutive half-body actions"
and the fix did not change it.

### 4. Direction 23's smoothness, pace, beats, head-back and descend-together still hold — **PASS**

All five carry over unchanged from the Direction-23 verdicts above.

---

## The reviewer's five faults, checked one by one

| # | claim | QA finding |
|---|---|---|
| 1 | beat-1 share 82.5→64.6 %, middle travel 5→28 mm | **HALF TRUE.** 28 mm confirmed by the probe. The 64.6 % **does not reproduce** — 80.2 %, and 76.7–80.7 % across 25 window/split variants. Beat 2's descent fix is real and passes; beat 1 is unfixed. |
| 2 | head-back −11 px, forward excursion at the floor | **CONFIRMED** on −11 px (nose) and −3.53 px (head region). The forward-excursion improvement (0.85→0.64) **does not reproduce** — I measure 0.89 px. Sub-pixel, immaterial. |
| 3 | f15 pop is the cel quantizer, and the accepted balance clip carries the same event at the same magnitude | **CONFIRMED, and this is the justification for shipping it.** Raw-pixel local-spike ratios: spring f15 **2.65×** (1216 px vs 458 local median); **balance f15 3.11×** (1160 vs 374), **balance f23 3.28×**, **balance f24 2.92×**. The accepted clip's quiet-stretch pops are larger than the spring's. Leaving it alone is correct. |
| 4 | `kSpringLifeFloor = 300` removes all eleven byte-identical head frames | **CONFIRMED.** Zero byte-identical frames anywhere in the clip (was 10 consecutive). Clip key 0 is still the exact grounded pose; probe green. Settle raw change 57 → 321 px/f, i.e. alive, and the segmented silhouette rate there is 0.27 %/f, so it is life and not shimmer. |
| 5 | hold 8→12 keys with more drift | **CONFIRMED, with a flag.** Hold is 12 keys / 0.40 s; probe reports shape/support drift **72 / 0 mm**. The 72 exceeds the old 70 mm band, and `kSpringHoldLivingDriftMm` was widened to 90 in the same commit to admit it. The reasoning (same wave amplitude sweeps more of its period over a six-times-longer hold) is sound and support drift is still exactly 0, so the brace is still a brace — but the gate was moved to let the value through and that belongs on the record. |

## Regressions — NONE

Full 22-subject bank rendered at HEAD from **one fresh explicit**
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` invocation
(`evidence/qa/bank-crc-qa.txt`), compared against the pre-run bank in
`evidence/stage1/crc-proof-22-subjects.txt`.

**Fifteen byte-identical:** idle `0x25EE061F`, moving-light, walk, run, look,
balance, taunt, slow-taunt, hit, damage, knockdown, fall, hitfloor, death,
death2. **Seven changed, and exactly the seven shared-spring consumers:**
attack, jump-one, jump-multi, salto-dummy, salto-fly, salto-six, salto-nine.
`kAtkRetimeShift` +66 and `kAttackKeys` 306 moved nothing they should not have.

**Generation Thirteen comparison** (built from `a2f601ef` in a QA scratch tree,
working tree restored): `jump-one` flight frames **44, 46–108, 110 are
byte-identical** at the +132 offset, and the final settle 152–160 as well. The
accepted flight survives the retime and survives `kSpringBladeSquashRise`
untouched. `salto-dummy` from frame 44 onward: **137 of 187 frames
byte-identical**, worst frame 107 changed pixels (~2 % of the silhouette). The
landing differs by up to 9293 px, which is a few pixels of tracking-camera
placement plus fin phase XORing to about two silhouettes; side by side
(`evidence/qa/landing-gen13-vs-head.png`) it is the same landing.

## Standing laws

| law | verdict | evidence |
|---|---|---|
| whole body to the last point of the tail **tube**, not the fins | PASS | probe: real tail followers 166 vertices, entry travel mean/max 232/491 mm |
| planar, no helix | PASS | probe body/whole lateral span **0 / 8 mm** (was 0/14); top view flat for all 168 frames (`evidence/qa/planarity-top-keys.png`) |
| nothing clips; ground contact declared | PASS | probe: surface intersections full/micro **none / none**; terrain −42 mm at key 72 against a declared 34 resting / 60 loaded bite |
| the accepted salto unchanged | PASS | flight byte-identical; salto-dummy ≤107 px/frame |
| idle untouched | PASS | CRC `0x25EE061F`, identical |
| seam contracts, probe green | PASS | `ZIXX PROBE: PASS` end to end at `48310d18` (`evidence/qa/probe-head-48310d18.txt`) |

---

## What the fix takes

The diagnosis is small and specific, which is the good news. **The travel budget
is in the wrong stations, and it always has been.** Beat 1 spends 258 mm on the
tail and 28 mm on the middle; Direction 24 says to spend it the other way round.

1. **Plant the tail.** Bring `kSpringAssembledHeading[15..18]`
   (−4500/−9500/−13000/−14500) and `kSpringCollapsedHeading[15..18]`
   (−3900/−8300/−11000/−11800) to at or near their grounded values
   (`kStanceSlope[15..18]`, ending −5600/−11400). The rear then holds its rest
   shape through both beats and the direction change disappears with it.
2. **Stop the fan moving.** `kSpringBladeSquashRise = 3500` → 0 removes the
   authored rise-then-press outright. `kSpringBladeFlare = 900` is most of the
   remaining apparent-size swing; reducing it shrinks the 14 % area swing back
   toward the 7 % the previous build had, and below.
3. **Make the S grow.** Give `kSpringAssembledHeading[5..9]` real travel — it is
   currently byte-identical to `kStanceSlope[5..9]` and to the absorb row, which
   is precisely why nothing grows. This is where the 258 mm the tail is giving
   back should be spent: the hook deepening and the neck gathering *forward*
   from a planted rear.
4. **Carry the middle with it.** `kSpringAssembledHeading[10..14]` currently buys
   a 1 px arch. If stations 5–9 start moving, 10–14 have to move with them or
   the animal breaks in the same place, one station further forward.

Everything else in the pass — the retime, the milli-key schedule, the camera
fix, the life floor, the longer hold — is finished work and should be kept as
is. Do not touch the smoothness. Steps 1–4 are four named constant rows in
`tools/reel/zixxtrixx.h` and no new machinery.

---

## What I verified vs what I inherited

**Verified on my own build, my own renders, my own committed instrument:** every
smoothness figure; the beat structure and its rates; the beat-1 and beat-2
region shares on both builds, plus the 25-variant sweep; the region descents;
head and nose travel and the head-to-tail gap; the tail's centroid route,
fin-tip route and apparent-size swing; the mid-body arch amplitude on screen;
the f15 pop against the balance clip's own pops; the byte-identical-frame count;
the 22-subject CRC sweep; the Gen-13 flight, salto and landing comparison; the
probe at HEAD; planarity by eye on the top view; every-frame legibility on the
contact sheets; the pose-row identity of `kStanceSlope[5..9]` and
`kSpringAssembledHeading[5..9]`.

**Taken from the committed probe without re-deriving:** the world-millimetre
station tables, lateral-span and self-intersection gates, terrain bite, hold
drift, and the A5 per-station reversal count.

**Could not confirm:** the implementer's beat-1 share of 64.6/17.1/18.3 %, on
any window or split; and its report that the head's forward excursion fell from
0.85 to 0.64 px.

## Evidence in `evidence/qa/`

| file | what it shows |
|---|---|
| `qa_region.py`, `qa_cmp.py`, `qa_sweep.py`, `qa_tail.py`, `qa_beats.py`, `qa_track.py`, `qa_pop.py`, `qa_arch.py`, `qa_plates.py`, `qa_zoom.py`, `qa_poseplate.py`, `qa_contact.py` | the instruments, committed so these numbers are reproducible |
| `springside-prev-ecf0e3ab-region.txt` | the calibration run that reproduces the reviewer's published figures |
| `springside-head-region.txt`, `qa-measurements.txt` | the same instrument at HEAD |
| `qa-fault1-sweep.txt` | the 25-variant window/split sweep behind the "does not reproduce" finding |
| `qa-tail-anchor.txt` | the tail's centroid, fin-tip and apparent-size routes, both builds |
| `tail-anchor-outlines.png` | **the Direction-24 picture**: tail at rest / assembled / loaded, 7× |
| `beat-outline-overlay-prev-vs-head.png`, `midbody-outline-zoom-prev-vs-head.png` | rest / assembled / loaded outlines, whole body and mid-body at 8× |
| `beat1-prev-vs-head.png`, `beat2-prev-vs-head.png` | the two beats, rendered, prev beside HEAD |
| `jumpone-contact-f000-083.png`, `jumpone-contact-f084-167.png` | every frame of the ground phase |
| `planarity-top-keys.png` | the top view across the arming |
| `landing-gen13-vs-head.png` | the landing against Generation Thirteen |
| `bank-crc-qa.txt` | the 22-subject CRC sweep |
| `probe-head-48310d18.txt` | the probe at HEAD, PASS |

## For the owner — what to look at

Nothing was published; the live site still serves the Direction-22 bank. If you
want to see this build before it is reworked, the two clips are
`zixxtrixx-spring-side` (fixed camera — the honest view) and `zixxtrixx-jump-one`.

* **Watch the tail.** It is the fault, and `tail-anchor-outlines.png` shows it in
  one still: the fins swing up and forward through the windup, then reverse and
  drop through the compression, and their apparent size swings 14 %.
* **Watch the front third.** It does not change during the windup. That is the
  "S doesn't grow" — and it is authored that way, not a bug.
* **Watch the smoothness.** This is the part that is finished. Compare it beside
  the tail-balance clip: the arming moves at less than half the balance clip's
  frame-to-frame rate and has no stutter, no staircase and no reversals.
* **One item is still an eye call if the rest gets fixed:** the head's backward
  travel, now 11 px of nose over 2.8 s. `kSpringCollapsedHeading[8..9]`
  (24500/12500) moves it, one edit.
