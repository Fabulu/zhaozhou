# QA-2 — Direction 24 fix pass: the tail is the anchor, the S grows from it

**Second QA cycle, 2026-09-03.** Lane
`zixxtrixx-wholebody-s-spring-20260901/zhaozhou`, branch `main`, HEAD
`d5949320` (= `origin/main` at fetch). Built with `tools/reel/build-direct.sh`
(cel, then probe, one target per call) into QA-owned trees `build-qa2`,
`build-qa2-prev`, `build-qa2-gen13`. Rendered with explicit `ZIXX_EXP=celmain`
`ZIXX_LIGHT=diagonal-cool-cross`. `sacengine` never run. **Nothing authored was
changed by this pass.**

## VERDICT: PASS.

Every acceptance point in Direction 24 and Direction 23 passes. The fault the
first cycle failed the build on — *the travel budget lives in the tail* — is
fixed at the root, not tuned around, and the fix is visible on screen at native
resolution rather than only in a table. Two gate re-records are honest but tight
and are recorded below as watch items; neither blocks.

The previous cycle's central complaint was a headline number that did not
reproduce. **Every headline number in this pass reproduces on my own build, my
own renders and the previous cycle's committed instruments**, to the digit.

---

## How I judged it, and why the comparison is trustworthy

I built and rendered three SHAs myself:

| SHA | what | spring-side CRC |
|---|---|---|
| `d5949320` | HEAD, the fix under test | `0x1B1AEAB6` |
| `48310d18` | the build the first cycle FAILED | `0xC36B9D23` |
| `a2f601ef` | archive Generation Thirteen | jump-one `0x37293039` |

My independent rebuild of `48310d18` reproduced the inherited render's CRC
**exactly** (`0xC36B9D23`), and my rebuild of `a2f601ef` reproduced the
inherited Gen-13 CRCs **exactly** (`0x37293039` / `0x86E72EE8`). The build is
deterministic and every prev-vs-HEAD figure below is like-for-like: one
instrument, one camera, one window, three builds differing only in `tools/reel/`.

Calibration: running the first cycle's committed `qa_region.py` on my own
`48310d18` render reproduces its published figures to the digit — beat-1 share
**80.2 / 7.7 / 12.1 %**, silhouette XOR **1.71 / 4.36 %/f**, nose **−11 px**,
min head-tail gap **109 px**. The instrument is the same instrument.

**I looked before I measured.** Every-frame contact sheets of the whole ground
phase, key-pose plates against the failed build, the launch, the landing and the
top view, all at native resolution, before any number was read.

Playback 60 fps. Ground time 72-key arming + 12-key hold = 168 frames = 2.80 s.

---

## Verdict per acceptance point — DIRECTION 24

### 1. The tail stays put — no position, direction or size change — **PASS**

This is not tuned to a threshold; it is **guaranteed by construction**, which is
the strongest form the answer could have taken. Stations 15–18 in all four
arming knots now alias `kStanceSlope[15..18]` verbatim:

```
kSpringAbsorbHeading[15..18]    = kStanceSlope[15], [16], [17], [18]
kSpringAssembledHeading[15..18] = kStanceSlope[15], [16], [17], [18]
kSpringCollapsedHeading[15..18] = kStanceSlope[15], [16], [17], [18]
kSpringGroundedHeading          = kStanceSlope
```

Four identical knots make the spline route through those stations a **constant**.
The tail cannot change position, direction or apparent size on the arming
parameter, at any interpolant, ever — not because a value was reduced but
because there is no longer a route.

The picture is `evidence/qa2/tail-anchor-outlines-qa2.png` — the tail at rest
(red, f12), assembled (yellow, f72) and loaded (blue, f144) at 8×, previous
build above, HEAD below. Above: three tails in three places at three angles.
Below: **the three outlines are coincident**, red almost entirely hidden under
the other two, worst separation 1–2 px.

Fixed side camera, tail third (screen x < 170):

| | `48310d18` | **`d5949320`** |
|---|---|---|
| fin-tip x route | 110 → **125** → 120 | **109 → 109 → 109** |
| fin-tip y route | 124 → **109** → 118 | **124 → 124 → 126** |
| fin-tip reversal | **YES**, 22 px round trip | **none** |
| tail-third centroid, beat 1 | **−7.26 px** | **+0.14 px** |
| direction change beat1→beat2 | **YES** | **none** |
| tail bbox w / h range | **37..60 / 35..45** | **58..62 / 33..37** |
| rearmost body pixel, whole ground phase | 109..**132** | **107..111** |
| probe: tail entry mean station travel | **258 mm** | **6 mm** |
| probe: taper entry mean travel | 105 mm | **8 mm** |

The trajectory plot `evidence/qa2/trajectories-prev-vs-head.png` is the same
finding as a shape: the previous build's fin tip draws a 22 px inverted V that
reverses at f100; **HEAD's draws two flat lines**.

The strongest single number is from `evidence/qa2/qa2-growth-uplift.txt`:
silhouette pixels lifted above the rest-pose top edge, in the tail band, at
f12/24/36/48/60/72 — `0 0 0 0 0 0`. **Not one pixel of the tail rises above its
rest outline during the entire windup.**

Residual: a 1–3 px slow drift after f100 as the loaded press takes the plant
down, monotone, no reversal. That is the tail going with the body it is attached
to, and its total is under a quarter of a pixel per ten frames. Correct — and a
dead-still tail would read worse.

### 2. The S visibly GROWS, forward from the planted tail — **PASS**

The first cycle failed this because `kSpringAssembledHeading[5..9]` was
byte-identical to `kStanceSlope[5..9]` — the front was authored not to change.
Both rows now move, and the mid-body rows `[10..14]` move a great deal
(`-2600,-3600,3300,4200,1500` → `-2000,-8200,2400,8800,1600`).

The honest measure of "does it grow" is not a ratio — a ratio can rise because
the denominator fell. It is **how much new silhouette appears above where the
body used to be.** Uplift area over the rest-pose top edge, mid band x170–209,
through beat 1:

| f12 | f24 | f36 | f48 | f60 | f72 |
|---|---|---|---|---|---|
| 0 | 1 | 26 | 66 | 130 | **195 px** |

Monotone, no reversal, steadily gathering — that *is* "the S visibly grows", and
it grows by **2.4× more than the failed build's mid band** (which reached 80 px)
while the tail contributes zero instead of 478 px.

Where it grows: top-edge rise from rest to assembled, by column band
(`evidence/qa2/qa2-topedge-profile.txt`):

| band | `48310d18` | **`d5949320`** |
|---|---|---|
| tail x105–169 | **+15.8 px mean, +33 max** | **−0.5 mean, 3.0 px max abs** |
| mid+front x170–264 | +0.89 mean, +19 max | **+2.68 mean, +22 max** |

The lobe apex sits at **x185–194, rising 12–22 px** — in the middle third,
forward of the planted tail and behind the head, exactly where Direction 24 asks
the S to grow. The first cycle's failure was a celebrated fix that was a **0.7 px
event**; this one is a 17–22 px apex over a 40 px band. It is unmissable in the
key-pose plate at 10× (`evidence/qa2/keyposes-big-0-36-72.png`) and plainly
visible at 384×240 in the contact sheets.

Corroborating, from the committed 3D probe rather than a projection — mean
station travel into the full-S entry, across three builds:

| build | head | neck | front | middle | run | taper | tail |
|---|---|---|---|---|---|---|---|
| `ecf0e3ab` | 51 | 47 | 27 | 5 | 8 | 105 | 257 |
| `48310d18` | 75 | 64 | 52 | 28 | 33 | 105 | 258 |
| **`d5949320`** | **148** | **140** | **125** | **90** | **61** | **8** | **6** |

The travel budget has been inverted, which is precisely what the first cycle's
fix list asked for.

### 3. The animal reads as one connected body — **PASS**

Beat-1 share of on-screen change, the instrument Direction 24 names:

| | tail | mid | head |
|---|---|---|---|
| `48310d18` | **80.2 %** | 7.7 % | 12.1 % |
| **`d5949320`** | **41.3 %** | **25.1 %** | **33.6 %** |

I swept the same **25 window/split combinations** the first cycle used, because
that sweep is what exposed the last claim as a phantom
(`evidence/qa2/qa2-beat1-sweep.txt`). This one holds everywhere: HEAD's tail
share stays between **40.3 % and 47.9 %** in every variant (previous build:
76.7–80.7 %), the middle between 20.8 % and 35.3 % (was 7.4–11.5 %), the head
between 19.9 % and 36.2 % (was 8.4–13.9 %). No window or split produces the old
picture, and none produces a flattering outlier either.

Connectedness is better seen than shared, though. In
`evidence/qa2/trajectories-prev-vs-head.png`, HEAD's mid and head region
centroids **rise together through beat 1 and descend together through beat 2**,
same curve shape, different amplitude, while the tail holds flat. The previous
build's plot shows the tail rising alone while the front two thirds sit still,
then the tail reversing — parts taking turns. No region in HEAD reverses
direction against the others.

### 4. Direction 23's smoothness, pace, beats, head-back, descend-together still hold — **PASS**

All seven verified below; smoothness improved rather than merely held.

---

## Verdict per acceptance point — DIRECTION 23

### 1. The movement is smooth — **PASS, and improved on the build that already passed**

| ground phase f0–168 | `48310d18` | **`d5949320`** | balance clip (accepted) |
|---|---|---|---|
| silhouette XOR med / max | 1.71 / 4.36 %/f | **0.92 / 2.88 %/f** | 3.37 / 18.52 %/f |
| centroid x \|v\| med / max | 0.143 / 0.927 px | **0.099 / 0.665 px** | 0.307 / 3.065 px |
| centroid x jerk max | 1.135 px | **1.040 px** | 1.549 px |
| centroid y \|v\| med | 0.051 px | **0.029 px** | 0.092 px |
| 30 Hz staircase odd/even | 0.98 | **1.06** | 0.97 |

The arming is now roughly **3.5× smoother per frame than the accepted balance
clip** and its worst frame is a sixth of the balance clip's worst. No regression;
a clear gain.

The f15 quantizer pop, accepted last cycle, is unchanged: local spike ratio
**2.84×** (1243 px vs a 438 local median) against the previous build's 2.65×
(1216 px) — the same event — while the accepted balance clip carries **3.11×** at
its own f15, **3.28×** at f23 and **2.92×** at f24. Still smaller than what
already ships. `evidence/qa2/qa2-pop.txt`.

### 2. A viewer can see what the animal is doing — **PASS**

Every-frame contact sheets `evidence/qa2/springside-contact-f000-083.png` and
`…-f084-175.png`. The animal is readable in every one of the 176 frames, nothing
blobs, nothing snaps, no reversal is visible. The four beats separate by eye: a
low flat S, a rising lobe, a press, a launch.

### 3. The whole body becomes the S, slowly, with the balance clip's organic quality — **PASS**

**This is the point the first cycle failed, and it is genuinely fixed.** See
Direction 24 #2 above: the probe's front-half station travel roughly doubles to
triples (head 75→148, neck 64→140, front 52→125, middle 28→90 mm) while the
tail's collapses 258→6, and on screen the mid band gains 195 px of new
silhouette above its rest line in a smooth monotone climb over 1.00 s.

Slow: 1.00 s for the become-S. Organic: the climb has no step, no plateau and no
reversal, and the per-frame rates sit below the balance clip's throughout.

### 4. The head moves slightly backward and slowly down; it never approaches the tail — **PASS**

| | `48310d18` | **`d5949320`** |
|---|---|---|
| nose travel f12→f168 | −12 px | **−12 px** (identical) |
| nose max FORWARD excursion | +1 px | **+0 px** |
| min gap nose ↔ rearmost body | 109 px | **122 px** |

Head travel is **unchanged** from the build that passed this point last cycle, so
nothing regressed; the route is now perfectly monotone backward with no forward
excursion at all, and the gap to the tail improved by 13 px because the tail
stopped travelling forward to meet it.

*Minor claim that does not reproduce:* the implementer reported the gap as
**≥124 px**; I measure **122 px** on the same camera. A 2 px difference on a
122 px margin — immaterial to the verdict, recorded for the file.

### 5. Everything descends together during the compression — **PASS**

Region centroid y, f72 → f168 (down is +):

| region | `48310d18` | **`d5949320`** |
|---|---|---|
| head / front | +7.64 px | **+8.65 px** |
| mid body | +3.07 px | **+3.72 px** |
| tail / fins | +2.54 px | **+1.16 px** |

Everything descends, and the amounts now form a clean gradient from a planted
rear to the most-travelled head — which is what "descend together, hinged on a
fixed end" should look like. The tail's smaller figure is the anchor doing its
job, not a region refusing to come down.

### 6. The four beats read in order and none is hurried — **PASS**

The implementer flagged that whole-frame XOR contrast *fell* here. It did, and
that measure is the wrong one now: whole-frame XOR was previously inflated by the
tail flailing, and what remains of it in the quiet beats is the life wave, which
is uniform and carries no shape information.

Measured on the **shape itself** — mean absolute top-edge movement across
x170–264, where the life wave cancels (`evidence/qa2/qa2-beat-separation.txt`):

| beat | `48310d18` | **`d5949320`** |
|---|---|---|
| settle f1–12 | 0.039 px/f | 0.039 px/f |
| **beat 1** become-S f12–72 | 0.061 | **0.082** |
| dwell f72–82 | 0.019 | **0.045** |
| **beat 2** compress f82–144 | 0.199 | **0.143** |
| hold f144–168 | 0.072 | **0.085** |
| **launch** f168–176 | 1.813 | **1.614** |

Beat 1 is **2.1× the settle and 1.8× the dwell**, beat 2 is **3.2× the dwell** and
the largest ground beat, and hold→launch is **19×**. The beats separate cleanly,
in order. Note beat 1 is now **faster than the failed build's** (0.082 vs 0.061)
— the S grows harder; it is the tail's noise that went away. Timings unchanged:
settle 0.20 s, become-S 1.00 s, dwell 0.17 s, compress 1.03 s, hold 0.40 s.

### 7. The launch follows the compression, and reads as an explosion forward — **PASS**

The implementer asked for this to be judged rather than taken from the rate,
because whole-frame launch XOR fell 25.4 → 11.6 %/f. **The launch is not weaker;
it is stronger.** Measured on what actually reads as an explosion — the head and
the nose:

| launch f168–176 | `48310d18` | **`d5949320`** |
|---|---|---|
| nose speed | 2.165 px/f | **2.421 px/f** |
| head-region centroid speed | 1.083 px/f | **1.305 px/f** |
| body-shape rate vs the hold | 25× | **19×** |
| silhouette area f168→f176 | 2065 → 2426 | 2099 → **2426** |

The nose leaves at **2.42 px/f against 0.27 px/f in the hold — a 9× jump on the
nose alone, 20× on the head centroid.** The animal clears frame in the same ~12
frames in both builds (`evidence/qa2/launch-prev-vs-head.png`). The whole-frame
figure fell because the fins no longer whip at release, which is the entire point
of Direction 24 #1.

---

## The implementer's four self-flagged items

**(a) Reduced whole-frame beat contrast — NOT A FAULT.** Answered in D23 #6: on a
shape metric that cancels the life wave, beat separation is intact and beat 1 is
faster than the build this replaces.

**(b) Launch rate 25.4 → 11.6 %/f — NOT A FAULT.** Answered in D23 #7: nose and
head-centroid launch speeds both *rose*. The lost percentage was fin flail.

**(c) Three gate re-records — CHECKED ONE BY ONE. Two honest, one honest but
tight; all recorded.**

| re-record | verdict |
|---|---|
| `kSpringCollapsedSupportLiftMm` −26 → −14 | **HONEST, and the right direction.** This is an *art* constant moved so the pose fits an **unchanged** declared limit — `kSpringDeclaredLoadedBiteMm` stays 60, `kSpringDeclaredBiteMm` stays 34, `kSpringHoldLivingDriftMm` stays 90 (widened in an earlier pass, not this one). They moved the art to satisfy the gate, not the gate to admit the art. **Watch item:** the loaded spring now bites **−60 mm against a 60 mm declaration — zero headroom.** It passes; any future change to the press moves it out. The knob is `kSpringCollapsedSupportLiftMm`. |
| `kJumpLandingLoadedBiteMm` 48 → 54 | **HONEST but fitted to the measurement.** This one *is* a declared limit widened to admit new behaviour; observed deepest is **−53 mm**, so the new value leaves **1 mm of headroom**. The cause is real and is a direct consequence of Direction 24: the planted tail lies flat behind the plant through the landing slam instead of curling up out of the way. The consequence is **1.3 px** at 41 mm/px, and in `evidence/qa2/jumpone-landing-qa2.png` the landing reads as weight with the fins resting on the surface — nothing buried, nothing hovering. Accepted. **Watch item** for the 1 mm margin. |
| probe `kSpringWholeTailLateralSpanMaxMm` 45 → 55 | **HONEST — verified independently, not taken on the rationale.** The number it admits went 8 → 48 mm, a 6× jump, which is exactly where a helix would hide. It is not one. The **trunk** gate that actually guards the S is untouched and reads **0 mm against 30**. On the top view — where the lateral axis *is* an image axis, so this measures the thing and not a projection — the creature's lateral spread is **identical between the two builds to within 0.4 px at every sampled frame** (35.2 / 35.1 / 34.4 / 34.6 / 34.8 / 34.2 / 33.2 / 32.3 / 31.9 px against the previous build's 35.2 / 35.1 / 34.7 / 35.0 / 35.2 / 34.5 / 33.1 / 32.2 / 31.4). The 48 mm is the fin assembly's own authored construction roll re-entering the measured axis now that the planted tail sits at its rest heading instead of being curled out of it — a static property of the model, worth ~1.2 px, and the probe's own pre-existing comment already described the mechanism. |

**Interior-knot stall gate now skipping authored-planted stations — HONEST and
tight.** The guard skips a station only when all four knots are equal, which is
self-limiting: it cannot exempt a station that is authored to move. I enumerated
the rows to be sure it exempts nothing else — `absorb ≠ assembled` at **every one**
of stations 0–14, so the guard skips **exactly stations 15–18** and nothing more.
Requiring a station with four identical knots to keep moving is a contradiction,
not a check.

**(d) Not re-verified downstream — I verified it.** Bank, Gen-13 and probe below.

---

## Regressions

**Generation Thirteen flight — byte-identical, and identically so.** My own build
of `a2f601ef` reproduces the archive's CRCs exactly. Against HEAD's `jump-one` at
the +132 offset, the byte-identical frames are

```
44, 46-108, 110, 152-160   (74 frames)
```

which is **character-for-character the set the first cycle recorded for
`48310d18`**. The accepted flight and the final settle are untouched by this
pass. `evidence/qa2/qa2-gen13-compare.txt`.

**Probe — `ZIXX PROBE: PASS`** end to end at `d5949320`
(`evidence/qa2/probe-head-d5949320.txt`).

**22-subject bank — NONE.** Rendered at HEAD from **one fresh explicit**
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` invocation naming all 22
subjects (`evidence/qa2/qa2-bank-crc.txt`), compared against the first cycle's
sweep at `48310d18`. Frame counts unchanged on every subject.

**Fifteen byte-identical** — idle `0x25EE061F`, moving-light, walk, run, look,
balance, taunt, slow-taunt, hit, damage, knockdown, fall, hitfloor, death,
death2. **Seven changed, and exactly the seven shared-spring consumers:**

| subject | `48310d18` | **`d5949320`** |
|---|---|---|
| attack | `0x653B40FA` | `0xEF626E4A` |
| jump-one | `0x1CFCDC7C` | `0x9829FF4A` |
| jump-multi | `0xEC33913E` | `0xEC4FAB91` |
| salto-dummy | `0x070C83A8` | `0x30E39024` |
| salto-fly | `0x7F9869E2` | `0x7B577591` |
| salto-six | `0x57A82576` | `0x01B1402F` |
| salto-nine | `0x89BC54A8` | `0xC244F239` |

The changed set is **exactly** the expected seven, with nothing extra and
nothing missing — the same split the first cycle found. Idle is byte-identical,
so the idle law holds by CRC.

*Note on method:* a bare `zhao-reel-cel.exe <dir>` with no subject names renders
**every wired subject**, which is a larger and different set than the 22 named
bank subjects — it produces diagnostic subjects (`-front`, `-side`, `-still`,
`-tq`, `-spring-micro`, …) and takes far longer. The sweep above names all 22
explicitly. Worth recording: a directory count reached 22 while `zixxtrixx-hit`
was still mid-write and had no `meta.txt`, which would have silently produced a
short, wrong comparison.

## Standing laws

| law | verdict | evidence |
|---|---|---|
| whole body to the last point of the tail **tube**, not the fins | PASS | probe: 166 real tail followers, entry travel mean/max 69/163 mm. The tube still participates — as the fixed end the S is built against, which is Direction 24's explicit override of the earlier "the rear must move" reading. |
| planar, no helix | PASS | probe body lateral span **0 mm** against a 30 mm gate, untouched; top-view spread identical to the previous build within 0.4 px at every frame; top-view plate flat across the arming |
| nothing clips | PASS | probe: **real surface intersections full/micro none / none** |
| ground contact declared | PASS, two watch items | spring −60 mm against a declared 60 (zero headroom); jump landing −53 against a re-recorded 54 (1 mm headroom); both authored, both invisible at ~1.3 px |
| seam contracts | PASS | release rest-shape error **0 mm**, intact airborne-S error **1 mm**, whole-support lift 600 mm |
| the accepted salto unchanged | PASS | Gen-13 flight byte-identical, same 74-frame set as last cycle |
| idle untouched | PASS | CRC `0x25EE061F`, identical — see Bank |
| probe green | PASS | `ZIXX PROBE: PASS` |

## A correction to my own method, for the record

My first top-view planarity instrument segmented the whole frame, silently locked
onto the **static ground** rather than the creature, and printed an identical
lateral span for every frame of both builds — a confident, wrong, perfectly
stable number, which is exactly the failure this project keeps paying for. I
caught it because a raw frame-difference showed the top view *does* change
(996–2043 px between f0 and later frames) while my instrument insisted nothing
moved. The fixed version crops to the creature's own bounding box before
segmenting; the note is committed in `evidence/qa2/qa2_planar2.py` so the next
reader does not repeat it. **A constant is a claim and needs checking like any
other.**

## What I verified vs what I inherited

**Verified on my own builds, my own renders, my own committed instruments:** every
smoothness figure; the beat structure on both a whole-frame and a shape-only
metric; the beat-1 and beat-2 region shares on both builds plus the 25-variant
sweep; the uplift-area growth trajectory; the top-edge rise profile; region
descents; nose and head travel and the head-tail gap; the tail's centroid,
fin-tip, bbox and rearmost-pixel routes; the launch on nose and head speed; the
f15 pop against the balance clip; the top-view lateral spread on both builds; the
pose-row aliasing of stations 15–18 and the enumeration proving the stall guard
exempts nothing else; the declared-bite constants and which of them moved; Gen-13
build, render and frame-by-frame identity; the probe at HEAD; every-frame
legibility on the contact sheets; the landing by eye; the 22-subject CRC sweep.

**Taken from the committed probe without re-deriving:** the world-millimetre
station travel tables, self-intersection and lateral-span gates, terrain bite
samples, hold drift, and the release seam errors.

**Could not confirm:** the implementer's head-to-tail gap of **≥124 px** — I
measure **122 px**. Immaterial.

## Evidence in `evidence/qa2/`

| file | what it shows |
|---|---|
| `tail-anchor-outlines-qa2.png` | **the Direction-24 #1 picture** — tail at rest/assembled/loaded, 8×, prev above HEAD below |
| `qa2-growth-uplift.txt` | **the Direction-24 #2 number** — uplift area per band per frame; tail `0 0 0 0 0 0`, mid `0→195` |
| `trajectories-prev-vs-head.png` | region centroids and fin-tip route over all 168 frames, both builds |
| `keyposes-prev-vs-head.png`, `keyposes-big-0-36-72.png` | key poses at 5× and 10×, prev above HEAD below |
| `springside-contact-f000-083.png`, `-f084-175.png` | every frame of the ground phase and the launch |
| `launch-prev-vs-head.png`, `jumpone-landing-qa2.png` | the launch and the landing |
| `planarity-top-qa2.png`, `qa2-planarity.txt` | the top view across the arming, and the corrected lateral-spread instrument |
| `qa2-beat1-sweep.txt` | the 25-variant window/split sweep |
| `qa2-beat-separation.txt` | beat rates on the shape metric, and the launch physics |
| `qa2-topedge-profile.txt`, `qa2-midbody-profile.txt` | where the silhouette rises, by column |
| `qa2-tail-anchor.txt`, `qa2-head-travel.txt`, `qa2-pop.txt` | tail route, head travel, quantizer pops |
| `qa2-gen13-compare.txt` | Generation Thirteen frame-by-frame identity |
| `qa2-bank-crc.txt` | the 22-subject CRC sweep at HEAD |
| `probe-head-d5949320.txt` | the probe at HEAD, PASS |
| `springside-prev-48310d18-region.txt`, `springside-head-d5949320-region.txt` | the calibration run and the same instrument at HEAD |
| `qa_*.py`, `qa2_*.py` | the instruments, committed so these numbers are reproducible |

---

## For the owner — what to look at

The two clips that matter are **`zixxtrixx-spring-side`** (the fixed camera — the
honest view) and **`zixxtrixx-jump-one`**.

* **Watch the tail — and this time watch it do nothing.** It was the fault; it is
  now nailed down by construction, not by tuning. `tail-anchor-outlines-qa2.png`
  shows rest, assembled and loaded on top of each other.
* **Watch the middle of the body.** Between the planted tail and the head, a broad
  lobe rises steadily over the first second, then presses flat. That is the S
  growing, and it is the thing that was missing.
* **Watch the whole animal rather than one end.** The windup's change is now split
  41 / 25 / 34 across tail, middle and head instead of 80 / 8 / 12.
* **Watch the launch.** It fires harder at the nose than the version before it,
  even though it looks calmer — the calm is the fins no longer flailing.

**Eye-call items, each one named constant away:**

* **The head's backward travel** is 12 px of nose over 2.8 s, unchanged for two
  passes. If it should be more or less, `kSpringCollapsedHeading[8..9]`
  (24500 / 12500) moves it — one edit.
* **The rear lobe's height** is the new thing on screen and the most likely thing
  to want tuning. `kSpringAssembledHeading[11]` (−8200) and `[13]` (8800) are the
  lobe's rise and crest — one edit each.
