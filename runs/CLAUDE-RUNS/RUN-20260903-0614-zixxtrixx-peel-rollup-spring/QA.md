# QA — Direction 25: THE PEEL. Roll up off the ground, stand on the tail, compress hard.

**Cycle:** the peel pass's QA, carrying the reviewer's adversarial duties.
2026-09-03. Lane `zixxtrixx-wholebody-s-spring-20260901/zhaozhou`, branch
`main`, HEAD `e78f28e1` (= `origin/main` at fetch); `Upheaval` at `d923fb7`
(= `origin/main`). Built with `tools/reel/build-direct.sh` (cel, then probe,
one target per call) into QA-owned trees `build-qa3`, `build-qa3-prev`,
`build-qa3-gen13`. Rendered with explicit `ZIXX_EXP=celmain`
`ZIXX_LIGHT=diagonal-cool-cross`. `sacengine` never run. **No authored value
was changed by this pass.** One committed *diagnostic* was repaired — see
Fault 1; that file is comparison-side and chooses nothing.

## VERDICT: PASS — all six Direction 25 points, Direction 24's tail-anchor law from sole contact, and Direction 23's smoothness.

**But the pass's own central proof was void, and had to be rebuilt before the
verdict could be reached.** The conclusion survives. The evidence did not.

---

## FAULT 1 — the peel's central diagnostic was measuring the bounding box

`evidence/stage4/qa25_contactfront.py` is named in TASK_LOG as *"THE
CONTACT-FRONT TRACKER — the peel pass's central diagnostic"* and its output is
the pixel proof offered for acceptance points 1 and 2. Its terrain edge was

```python
ground = (fi[:,:,0] < 190) & (fi[:,:,1] < 150) & (fi[:,:,2] < 130)
```

**This reel's sky satisfies that predicate.** The top of frame is RGB
`(148, 89, 115)`; R<190, G<150, B<130 all hold. So the first "ground" row in
every column is row 0, `terrain_top` returned **0 for all 384 columns**, and
the touch test `bottom >= top[x] - 4` became `bottom >= -4` — true for every
column containing any creature pixel.

The tracker therefore reported, in every frame:

| it printed | what it actually measured |
|---|---|
| `contact_front_x` | the **rightmost creature pixel** — the nose |
| `contact_x_min` | the leftmost creature pixel — the tail |
| `contact_cols` | the creature's on-screen **width** |

Verified on my own render at HEAD, frame by frame: `contact_front_x` and the
plain silhouette bounding box agree **exactly** at every sampled frame
(242/242, 243/243, 241/241, 238/238, 227/227, 210/210, 196/196, 192/192 …).

This is why TASK_LOG reports the same number twice for two different
quantities — *"the head-most touching column recedes 242 → 192 px"* and
*"nose: rightmost body pixel 242 → 192 px"*. They were the same number. The
"final patch width 79 columns" was the whole animal's width, and the
"documented perspective artefact" explaining that width away was a
rationalisation of an instrument that had never looked at the ground.

**This is the project's signature failure, in the tool written specifically to
prevent it.** The pass would have been failed on evidence alone had the finding
not reproduced independently.

**Repaired.** `terrain_top` now calibrates the ground line the only sound way
available on this camera: the model is planar (probe: body lateral span
**0 mm**), so every point of the creature lies in ONE depth plane, and under a
fixed perspective camera that plane maps world height monotonically to screen
y. At rest the body demonstrably lies on the dirt, so **its own bottom edge
over the grounded stretch IS the ground line at its depth**. Columns outside
the rest footprint inherit the nearest calibrated value. The far sky/ground
boundary would have been wrong too — it sits at y 94 while the creature's
contact line is y 150, 55 px below, because the animal stands in *front* of the
crest. The fault, the reasoning and the fix are written into the file so the
next reader cannot repeat it.

**The repaired tool, and my own independent instrument
`evidence/qa/qa3_contact.py`, both reproduce the peel.** With a control:

| fixed tracker | contact front | worst re-advance | final patch |
|---|---|---|---|
| published `d5949320` (control) | 219 → **215 px** | 1 px | **78 columns** |
| HEAD `e78f28e1` (the peel) | 219 → **146 px** | 1 px | **14 columns** |

The control never leaves the ground. The peel recedes 73 px and closes to a
14-column patch. **That is the proof the pass needed and did not have.**

---

## How I judged it, and why the comparison is trustworthy

I built and rendered three SHAs myself:

| SHA | what | spring-side CRC | jump-one CRC |
|---|---|---|---|
| `e78f28e1` | HEAD, the peel under test | `0x3637C4B8` (139 f) | `0xB11B7C7B` (241 f) |
| `d5949320` | the PUBLISHED pass | `0x1B1AEAB6` (191 f) | `0x9829FF4A` (293 f) |
| `a2f601ef` | archive Generation Thirteen | — | `0x37293039` (161 f) |

My rebuild of `d5949320` reproduces the published CRCs **exactly**
(`0x1B1AEAB6` / `0x9829FF4A`), and my rebuild of `a2f601ef` reproduces the
archive's Gen-13 CRC **exactly** (`0x37293039`). Both match QA-2's table to the
digit. Running QA-2's committed `qa_region.py` on my own `d5949320` render
reproduces its published figures to the digit — silhouette XOR **0.91 / 2.88
%/f**, centroid x |v| med **0.099**, max **0.665**, jerk **1.040**, 30 Hz ratio
**1.06**, min head-tail gap **122 px**. **Same instrument, same numbers, three
builds differing only in `tools/reel/`.**

**I looked before I measured.** Every-frame contact sheets of the whole ground
phase and the launch, the plant burst frame by frame, the loaded pose against
the published loaded pose, the landing, the release — all at native resolution
and at 3× site scale — before any number was read. The numbers below came
after, and only to check what the eye had already decided.

Playback 60 fps. Ground time **116 frames = 1.93 s** (published: 168 frames =
2.80 s).

---

## Verdict per acceptance point — DIRECTION 25

### 1. The peel reads: contact leaves at the FRONT and travels progressively backward — **PASS** (one eye-call, below)

Measured on my own instrument (`evidence/qa/contact-honest.txt`, BAND 2) and
corroborated by the repaired tracker (BAND 4). The contact front — the
head-most column whose bottom edge sits on the calibrated ground line:

```
f0    217 px  (77 columns touching, x141..217 — the rest footprint)
f30   214            f43   203            f49   152
f38   208            f45   176            f54   145
f41   206            f47   166            f57   144  (11 columns, x134..144)
```

**Monotone. Zero re-advance across the whole peel**; a single +1 px at f88 as
the coil settles onto the loaded tail. The patch closes **77 → 11 columns**.
The recession begins at the front of the grounded stretch (x217, the nose end)
and ends at the rear of it (x144, the tail end) — the direction Direction 25
asks for, not the reverse.

The 3D machinery agrees and is the stronger statement: the support is an
authored milli-station route walking **14000 → 19000**
(`spring_support_station_mk`, `evidence/qa/schedule.txt`), and the root solve
holds *each successive contact point at its own grounded baseline*. The contact
cannot slide, by construction. Probe: travelling-support X/Y/Z error
**≤1 mm**, **max forward step 0 mm**.

**Eye-call — the peel is back-loaded.** The recession is not even: 14 px over
f0–44 (0.73 s), then **58 px over f45–57 (0.22 s)**. Three quarters of the
roll-up happens in one ninth of the ground time. On the every-frame sheets it
still reads as a roll rather than a snap, and the owner asked for *faster*, so
I pass it — but if he wants the roll-up spread out, it is one constant:
**`kSpringSupportPeelBeginArm` (150)**, lowered, starts the travelling support
earlier and stretches the same route over more of the arming.

### 2. The animal ends standing on the end of its tail — **PASS**

This was the pass's weakest claim and the one I was asked to settle on screen.
It settles.

The implementer's structural argument is sound but incomplete: it proves the
*support point* is the tail tip, not that everything else left the ground. So I
measured the daylight, using the planar / one-depth-plane argument above — the
only 2D contact test on this camera that is not a lie
(`evidence/qa/qa3_standprofile.py`). Per-column gap above the calibrated ground
line, at the stand:

| frame | contact patch | daylight under the coil (min over all columns forward of the patch) |
|---|---|---|
| f70 | x136–143 (8 columns) | **+7 to +12 px** (287–490 mm) |
| f85 | x136–143 | +8 to +11 px |
| f115 (deepest press) | x136–143 | **+7 px** (287 mm) |

Eight columns of contact — about 330 mm — at the **rear end of the rest
footprint**, i.e. the tail. Everything forward of it clears the dirt by 7 px or
more, all the way through the strongest compression.

The picture is `evidence/qa/contact-closeup.png` (16×, calibrated ground line
in cyan, touching columns in red): **three thin blades converge to a point
planted in the dirt**, and the body sits above and behind it. At 3× site scale
(`stand-native-3x.png`) it reads as a coil perched on a spike. The hill crest
is not hiding anything — it is 55 px *above* the contact line, behind the
animal.

**Eye-call — the daylight is modest.** 7 px at 240p is real and visible but not
generous. **`kSpringTailStandHeading[15..18]`** (9102, 11287, 13107, 14200 —
the steep tail column, already steepened once this pass from 36/47/58/68° to
50/62/72/78°) is the one edit that buys more.

### 3. The compression is visibly stronger than the published pass — **PASS, unmistakably**

Silhouette, rest → last ground frame, same camera, same instrument
(`evidence/qa/qa3_compress.py`):

| | published `d5949320` | **HEAD `e78f28e1`** |
|---|---|---|
| bbox width, rest → loaded | 133 → 123 px (**−8 %**) | 133 → **79 px** (**−41 %**) |
| bbox height, rest → loaded | 44 → 37 px | 44 → **36 px** |
| silhouette area, rest → loaded | 2499 → 2107 (**−16 %**) | 2499 → **1662** (**−33 %**) |
| minimum area over the ground phase | 2087 | **1662** |

Five times the width collapse and twice the area collapse. It is not a
threshold argument — `evidence/qa/loaded-prev-vs-head.png` puts the two loaded
poses side by side and the difference is a flat lying S against a pressed coil
standing on a spike.

### 4. The head travels further back and never reaches or passes the tail — **PASS**

The reported **−50 px** is real but it is the *silhouette's* rightmost pixel,
which stops moving at f80 because the coil's outer loop — not the head —
becomes the rightmost thing. Tracking the eye (the gold blob, an unambiguous
head landmark) gives the honest figure, and it is **larger** than claimed:

| | published | **HEAD** |
|---|---|---|
| eye x, f0 → last ground frame | 228.4 → 219.8 = **−8.6 px** | 228.4 → 165.3 = **−63.1 px** |
| rightmost body pixel | −11 px | −50 px |
| max forward step en route | +1 px | **+1 px** (the same life-breath) |

**7.3× further back on the head itself**, monotone but for the single +1 px
breath the published pass also has.

Never reaches the tail, on three independent measures:

* **3D** (`evidence/qa/schedule.txt`): head root x travels 0 → **−1368 mm**;
  the tail tip sits at **−1926 mm**; minimum margin **558 mm** at the deepest
  key.
* **Probe law gate**: min nose-tip margin **563 mm** against the declared
  `kSpringNosePastTailMarginMm` = 50. The 563 mm is confirmed, not inherited —
  it is the same quantity my own schedule dump computes at 558.
* **On screen**: eye at x165, tail tip at x144 — the head stays **21 px in
  front of the tip**, and the coil's rightmost pixel 48 px in front. At no
  frame does any head or neck pixel sit behind the tip.

### 5. The loading is faster than the published pass, and still smooth — **PASS**

**Faster, structurally and verifiably**: the ground phase is **168 → 116
frames**, 2.80 s → 1.93 s, **31 % faster**. Every spring consumer lost exactly
52 frames, which is the retime and nothing else.

**Still smooth.** Compared like with like against the balance clip — the
accepted art the owner named as the pace and organic reference — rendered from
my own bank at HEAD (`evidence/qa/out-*.txt`, `qa3_rate.py`):

| ground phase | published | **HEAD PEEL** | balance clip (ACCEPTED) |
|---|---|---|---|
| silhouette XOR med | 0.91 %/f | **1.53** | 3.55 |
| silhouette XOR **p90** | 1.79 | **9.88** | **16.02** |
| silhouette XOR max | 2.88 | **16.12** | 55.37 |
| centroid x \|v\| med / max | 0.099 / 0.665 px | **0.148 / 1.618** | 0.387 / 11.938 |
| centroid x jerk max | 1.040 | **1.395** | 2.483 |
| centroid y \|v\| med | 0.028 | **0.059** | 0.066 |
| byte-identical consecutive frames | 0 | **0** | 54 |

HEAD is rougher than the published pass — it must be; it does far more in 31 %
less time — and is **below the accepted balance clip on every single measure**,
by 2.3× in the median and 3.4× at its worst. Its worst frame (16.12) sits at
the balance clip's *ninetieth percentile* (16.02). No jitter, no stutter, no
repeated frames, and the only frame-to-frame reversal anywhere is the +1 px
life breath the published pass shares.

### 6. Any overlap or clipping is bounded, declared, and not what the eye lands on — **PASS**

One declared self-press, in named constants with owner-visible comments:

| declaration | value | observed | headroom |
|---|---|---|---|
| `kSpringPeelPressFullMm` | 60 mm | **54 mm** @ tick 45, stations 0/33 | 6 mm |
| `kSpringPeelPressMicroMm` | 110 mm | **104 mm** @ tick 45 | 6 mm |
| `kSpringDeclaredLoadedBiteMm` (terrain) | 95 mm | **−59 mm** @ tick 23 | 36 mm |

Outside the derived window (`spring_tick_in_declared_press_window`, keys 6–61)
intersections remain a hard probe fault, and the probe reports
`outside_count == 0`.

**Not what the eye lands on**: 54 mm at this camera's 41 mm/px is **1.3 px** of
interpenetration; the micro rung's 104 mm is 2.5 px. What is visible at the
compression is the head lying *over* the coil — ordinary occlusion for a coiled
snake, not clipping — and the gold eye stays legible through it
(`evidence/qa/loaded-zoom.png`, 10×).

---

## Verdict — DIRECTION 24, tail-anchor law from sole contact onward

**PASS, and again guaranteed by construction.**
`kSpringCollapsedHeading[15..18]` are written as
`kSpringTailStandHeading[15], [16], [17], [18]` — verbatim aliases. From the
tail-stand knot to the collapsed knot the spline through those stations is a
**constant**: the tail cannot move, invert or change size on the compression
parameter, at any interpolant, because there is no route.

In pixels, tail band x112–165 (`evidence/qa/tail-anchor-outlines.png`, 10×,
f57 red / f86 yellow / f115 blue — the three outlines coincident):

| from sole contact (f55) to release (f120) | |
|---|---|
| rearmost tail pixel x | **114–116** (±1 px) |
| lowest tail pixel y | **150 in every frame** |
| contact patch left edge | **x136, identical in every frame** |
| contact patch | x136–140 → x136–143 (grows 3 px as the load arrives) |
| tail-band centroid | x 145.16–146.05, y 130.51–131.91 |

No slide, no inversion, no direction change. The 3 px growth of the patch is
the fan taking weight, not the tail travelling; its left edge never moves by a
pixel. Probe: tip XZ drift **1 mm** from arm 350 onward.

## Verdict — DIRECTION 23, smoothness

**PASS.** See acceptance 5. Legibility: every frame of the ground phase and the
launch is readable at native resolution (`sheet-f000-059.png`,
`sheet-f060-119.png`, `sheet-f120-138.png`) — nothing blobs, nothing snaps, no
reversal is visible, and the beats separate by eye: a low flat S, a roll-up
onto the tail, a press, a launch.

---

## The implementer's five self-flagged watch items — all probed, none blocking

**1. Thin press headroom (54/60 full, 104/110 micro) — HONEST, and 6 mm each,
not 2/5.** These are *new* declarations created for this pass under Direction
25's explicit relaxation, carrying an owner-visible comment that says to ratchet
them down once the motion is accepted. They admit 1.3 px and 2.5 px of
interpenetration. The one actually worth watching is not these: it is
**`kSpringDeclaredLoadedBiteMm`, widened 60 → 95 to admit a plant transient
that the same pass then improved to −59 mm.** The declaration is now 36 mm
looser than the art needs, which is a weaker gate than it should be. Not a
fault; a note for the next pass — the honest value today is about 70.

**2. p90 shape-rate over the strict cap at the plant burst — HONEST, and NOT a
fault.** Reproduced: the five worst frames of the whole ground phase are
**45, 48, 49, 50, 51** — exactly the plant burst. On the qa2-calibrated
silhouette-XOR instrument HEAD reads **p90 9.88 / max 16.12** against the
accepted balance clip's **p90 16.02 / max 55.37**. HEAD's *worst frame* sits at
the accepted art's *ninetieth percentile*. The right comparison is against art
the owner has accepted, and on that comparison there is room to spare.

**3. Nose travel −50 px beyond the plan's −20..−35 bracket — CONFIRMED, and it
reads as "further back", not as overshoot.** It is also understated: the head
itself goes **−63 px**. It reads correctly because the head never crosses the
tail (558 mm, 21 px of margin) and because the backward travel *is* the body
rolling up onto the tail, which is exactly what Direction 25 describes. The
owner set no ceiling. A plan bracket is not an owner requirement.

**4. 30 Hz odd/even ratio 0.87 — CONFIRMED AS NOISE. Rejected as a fault.**
Three tests, all decisive:

* **Zero byte-identical consecutive frames** in the ground phase of either
  build. A 30 Hz staircase means repeated frames. There are none.
* **Flipping the sample phase inverts the ratio** — cy reads 0.85 on phase 0
  and **1.16 on phase 1**. A real staircase is phase-locked; noise flips.
* **Windowing destroys it** — by quarter, HEAD reads 1.39 / 1.19 / 0.97 / 1.30,
  and the *published* clip's own first quarter reads **0.85**, the same
  "suspect" value it was being compared against.

The amplitude is 0.059 px. `evidence/qa/qa3_30hz.py`.

**5. The 2D patch width at the stand — NOT a perspective artefact. It was
Fault 1.** See above. The tool was reporting the creature's width. The real
patch is 8–14 columns and it is measurable.

## The six plan errors the pass found and fixed — spot-checked

| # | fix | verdict |
|---|---|---|
| 1 | monotone (Fritsch-Carlson) spline tangents | **REAL.** `spring_route_heading` returns tangent 0 when a knot's two secants disagree in sign, and the constant on equal-knot legs. The symptom it cures — the nose bouncing forward early in the compression — is gone: max forward step on the nose is **+1 px**, identical to the published pass's, and the compression beat f57→f115 has no head step above +2.4 px. |
| 2 | `kSpringChainLag` 165 → 0 | **REAL**, and structurally right: with a travelling support the peel *is* the delay. Confirmed in source. |
| 3 | support arrives early (`kSpringSupportTipArriveArm` 340 → 350) | **REAL.** The schedule shows the support reaching mk 19000 at key 28, before the tail-stand knot at arm 400 / key 32 — the tip is pinned while the curl finishes above it. Plant transient measured −59 mm. |
| 4 | rows rest at `kStanceSlope` until the support passes them | **REAL**, confirmed in the peel-mid table. |
| 5 | **the fan was 852 mm underground; world-Z counter-rotation** | **REAL, and the right form.** `spring_counter_fan_world_z` conjugates a world-Z rotation into the fan parent's frame (`W* Rz(−turned) W`) rather than counter-rotating locally, with a named authority knob `kSpringBladeStandCounter`. On screen the fan is the visible foot of the stand in every frame of the hold and is never below the ground line — `contact-closeup.png` shows the blades converging *on* the dirt, not through it. |
| 6 | plant transient declared (`kSpringDeclaredLoadedBiteMm` 60 → 95) | **REAL** but loose — see watch item 1. |

---

## Regressions — NONE. Full 22-subject sweep, not a spot check.

Rendered at HEAD from **one fresh explicit** `ZIXX_EXP=celmain`
`ZIXX_LIGHT=diagonal-cool-cross` invocation naming all 22 subjects
(`evidence/qa/qa-bank-crc.txt`), compared against QA-2's sweep at `d5949320`.

**Fifteen byte-identical** — idle `0x25EE061F`, moving-light, walk, look,
balance, taunt, slow-taunt, hit, damage, knockdown, fall, hitfloor, death,
death2, run — with identical frame counts. **The idle law holds by CRC.**

**Seven changed, and exactly the seven shared-spring consumers**, each losing
exactly 52 frames (the retime, and nothing else):

| subject | `d5949320` | **`e78f28e1`** | frames |
|---|---|---|---|
| attack | `0xEF626E4A` | `0x2AED9D76` | 612 → 560 |
| jump-one | `0x9829FF4A` | `0xB11B7C7B` | 293 → 241 |
| jump-multi | `0xEC4FAB91` | `0x9E96F7F2` | 293 → 241 |
| salto-dummy | `0x30E39024` | `0x652E456A` | 363 → 311 |
| salto-fly | `0x7B577591` | `0xE4332117` | 365 → 313 |
| salto-six | `0x01B1402F` | `0x139F143B` | 385 → 333 |
| salto-nine | `0xC244F239` | `0x2E005FAD` | 487 → 435 |

Nothing extra, nothing missing.

**Generation Thirteen flight — byte-identical, and identically so.** My own
build of `a2f601ef` reproduces the archive CRC exactly (`0x37293039`). Against
HEAD's `jump-one`, an exhaustive offset search finds the best alignment at
**offset 80** — exactly 2 × the derived `kAtkRetimeShift` of 40 — with **74
byte-identical frames** in the ranges

```
44, 46-108, 110, 152-160
```

**character-for-character the set QA-2 recorded** at offset 132 for the
published build. The accepted salto and the final settle are untouched.
`evidence/qa/qa3_gen13.py`.

**Probe — `ZIXX PROBE: PASS`** at HEAD on my own build, and its output is
**byte-identical to the implementer's committed `stage4-probe-pass.txt`**. The
build is deterministic. `evidence/qa/probe-head.txt`.

**Landing** (`evidence/qa/landing2.png`): the wheel unrolls, the body extends,
lands flat and settles into the resting S. No tail-stand slam — the absorb cap
(`kJumpLandingAbsorbArm = kSpringSupportPeelBeginArm`) works.

**Launch** (`evidence/qa/launch-cmp.png`): the release structure — loaded pose
→ unroll to the flat S → airborne wheel — is **identical to the published
pass**, frame for frame in shape, differing only in how compressed the pose it
starts from is. Raw launch rate is *lower* than the published pass's (1337 vs
1639 changed px/f at the second launch frame), so nothing snaps.

## Standing laws

| law | verdict | evidence |
|---|---|---|
| planar, no helix | PASS | probe body lateral span **0 mm** against a 30 mm gate |
| ground contact authored and declared | PASS, one note | terrain −59 mm against a declared 95; the declaration is 36 mm looser than the art needs |
| tail anchored from sole contact | PASS | by construction; three coincident outlines |
| the accepted salto unchanged | PASS | Gen-13 byte-identical, the same 74-frame set |
| idle untouched | PASS | CRC `0x25EE061F`, identical |
| every value a named owner knob | PASS | every shape, time and clipping value is a named `constexpr` with an owner-facing comment |
| probe green | PASS | `ZIXX PROBE: PASS`, output reproduces byte for byte |

## What I verified vs what I inherited

**Verified on my own builds, my own renders, my own instruments:** the peel's
contact front on two independent instruments plus a published-pass control; the
daylight under the coil per column at four stand frames; the tail-anchor law in
pixels and in the pose tables; compression on bbox and area against the
published pass; head travel on the eye and on the silhouette against the
published pass; the head-to-tail margin in 3D from my own schedule dump; every
smoothness figure against the published pass *and* the accepted balance clip;
the 30 Hz hypothesis on three tests; the press peaks and every declared
constant; all six plan fixes in source; the 22-subject CRC sweep; Gen-13 build,
render and frame-by-frame identity; the probe at HEAD; every-frame legibility;
the landing and the launch by eye. **And I found and repaired the pass's
central diagnostic.**

**Taken from the committed probe without re-deriving:** the world-millimetre
station travel tables, the per-vertex self-intersection counts, the seam and
release errors, the deform-source metadata counts.

**Could not confirm:** nothing material. Two of the implementer's numbers are
imprecise *against* its own interest — press headroom is 6/6 mm not 2/5, and
head travel is −63 px not −50 — and one, the contact front, was not a
measurement at all.

## Evidence in `evidence/qa/`

| file | what it shows |
|---|---|
| `contact-front-fixed.png` / `.csv` | **the peel** — the repaired tracker, front 219 → 146 px, patch 80 → 14 columns |
| `contact-front-prev.png` / `.csv` | the control — the published pass, front 219 → 215 px, patch 78 columns |
| `contact-honest.txt`, `qa3_contact.py` | my independent contact instrument, every frame |
| `contact-closeup.png` | **the Direction-25 #2 picture** — 16×, the tail planted, calibrated ground line in cyan |
| `groundline-stand.png`, `qa3_standprofile.py` | the ground line over the whole roll-up; per-column daylight at the stand |
| `tail-anchor-outlines.png`, `qa3_tail.py` | the Direction-24 picture — f57/f86/f115 tails coincident |
| `loaded-prev-vs-head.png`, `loaded-zoom.png`, `qa3_compress.py` | the compression, published above peel below; 10× on the coil |
| `stand-native-3x.png` | the stand at site scale, no overlay — what the owner will see |
| `sheet-f000-059.png`, `-f060-119.png`, `-f120-138.png` | every frame of the ground phase and the launch |
| `pb-a.png`, `pb-b.png`, `plantburst.png` | the plant burst, frame by frame, ground line marked |
| `launch-cmp.png`, `landing2.png` | the release against the published pass; the landing |
| `out-head.txt`, `out-prev.txt`, `out-balance.txt` | QA-2's `qa_region.py` on all three, calibration included |
| `qa3_rate.py`, `qa3_30hz.py`, `qa3_eye.py`, `qa3_gen13.py` | shape-rate percentiles; the 30 Hz tests; the eye track; Gen-13 identity |
| `schedule.txt`, `probe-head.txt` | the 3D arming schedule; the probe at HEAD |
| `qa-bank-crc.txt` | the 22-subject sweep against QA-2's |

---

## For the owner — what to look at

Two clips: **`zixxtrixx-spring-side`** (the fixed camera — the honest view) and
**`zixxtrixx-jump-one`**.

* **Watch the ground, front to back.** The body's front lifts first and the
  touching part travels backward along the animal until only the tail end is
  down. It is a roll-up now, not a shape it assembles into.
* **Watch it stand.** Around a third of the way in, three thin blades converge
  to a point in the dirt and the whole coil sits above it with daylight
  underneath. `contact-closeup.png` is that moment at 16×.
* **Watch it squeeze.** The animal goes from 133 px wide to **79 px** — five
  times the collapse the published version managed. `loaded-prev-vs-head.png`
  is the old loaded pose above the new one.
* **Watch the head.** It travels **63 px back** against the published version's
  9, and stops 21 px short of the tail. It ends lying over the top of its own
  coil.
* **It is 31 % quicker** — 1.93 s on the ground against 2.80 s — and still
  smoother, on every measure, than the tail-balance clip you accepted.

**Eye-call items, each one named constant away:**

* **How much daylight under the coil** — 7 px at the deepest press. It reads as
  perched, but it is not generous. **`kSpringTailStandHeading[15..18]`**
  (9102, 11287, 13107, 14200 — the tail column's lean) is the one edit; steeper
  lifts the coil.
* **How evenly the roll-up spreads** — three quarters of it happens in the last
  0.22 s. **`kSpringSupportPeelBeginArm`** (150), lowered, starts the travel
  earlier and spreads it over more of the arming.
* **How far the head goes back** — **`kSpringCollapsedHeading[0..4]`**
  (−3641, 0, 6372, 14563, 21845 — the crown fold).
* **How hard it squeezes** — **`kSpringCollapsedHeading[10..14]`** (the loop
  pressed to a low flat ellipse) and **`kSpringCollapsedSupportLiftMm`** (−14).
* **Overall speed** — **`kSpringPeelEndKey`** (32) for the roll-up,
  **`kSaltoCompressEndKey`** (50) for the squeeze,
  **`kSaltoCompressHoldEndKey`** (58) for the hold.
* **How much clipping is allowed** — **`kSpringPeelPressFullMm`** (60) and
  **`kSpringPeelPressMicroMm`** (110). Currently spending 54 and 104. When the
  overlap should go, ratchet these down and the probe will fail until the poses
  obey.
* **The release still unwinds to a flat S on the ground before it jumps.** That
  is inherited from the published pass, not new, and it is the salto's own seam
  contract. If it should fire straight off the coil instead, that is a new
  pose, not a knob.
