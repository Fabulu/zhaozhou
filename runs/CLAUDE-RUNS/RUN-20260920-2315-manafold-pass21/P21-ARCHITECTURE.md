# P21 ARCHITECTURE — RODS AND BALLS: articulation only at the carriers, runs that only stretch

**Date:** 2026-09-20 · **Branch:** `manafold-pass21` @ `457cfe89` (pass-20 production main)
**Packet:** diagnosis + design, read-only on production source. Nothing built.
**Direction:** `OWNER-DIRECTION-22-2026-09-20.md` plus the owner's same-day
addition relayed by the coordinator, verbatim: *"don't let actual antennae
parts bend, just stretch. the bending is at the ball joints."*
**Evidence:** `P21-LOOK-NOTES.md` (11 looks, in order), `P21-LOOKS/`,
`P21-RECEIPTS/`, and the committed probe `P21-PROBES/manafold_p21_curvature.cpp`
+ `curvature_report.py`.

---

## 0. Verdict in one paragraph

Every authority that moves the antenna is already a rotation at a carrier or a
length on a span. **The extra joints are made by the SKIN, not by the pose.**
Three things put bends where no ball is: (1) each ball's fold is blended into
the skin over the 90 mm *before* the ball, so the corner sits 47–140 mm short of
the ball and the ball itself sits on a straight piece; an LBS blend of two
frames on rings that are not at the pivot also makes an S-jog (a turn the wrong
way, then back) on the two rings before the corner; (2) the front junction's
second rotation (`kBNeck`: the aim of ball A, hinge-play neck, knead neck) is
blended in over 558–790 mm — the middle of the front run — instead of at the
junction; (3) the pass-20 rear bow is realised by six translated copies of one
frame, so the "arc" is a polygon with corners at rings 36/37, 45/46, 49/50, 52
and 55, its rings sheared 50–90° off the centreline, and its shape law has a
square-root singularity at the two taut crossings per loop and a tangent that
swings past 90° in the deep-slack region. **That bow is the End spazz** —
measured, not inferred: switching it to the pass-19 law drops the worst rear
turn rate from 28.5 to 1.2 °/sample and erases the rear corners to 0.0°, while
muting every End and C ambient rotation makes the rates *worse*. The design is
a **ball-and-stick rig on the existing 27 bones**: every ball a rigid sphere on
its joint bone, every run a straight rod skinned between its parent joint and a
pure-translation stretch helper, the rear run a straight rod from C to the End
ball whose length is the chord, no bow, no rotation-carrying helpers, all rod
aims roll-stable. Pass 20's knead is untouched (it already articulates only at
A, B and the pin at C). Exact-off is one selector, `ZHAO_U02_RIG=rods|pass20`.

---

## 1. The rule, in rig terms

From Direction 22 + the addition:

* **A run between two carriers is a straight rod.** Its direction is constant
  along its length on every frame. It may change length (the signed-span
  helpers already own that). Per-ring centreline turn inside a run: **0**, to
  quantisation.
* **All angular articulation happens at the balls** (Front/JunctionF, A, B, C,
  End). A ball is a joint. The visual transition between two rod directions is
  carried by the ball's own body, not by bending either rod.
* The "smooth" read comes from joint angles being smooth *in time* and the
  balls carrying the corners, not from curvature in space.
* The middle-ball knead (pass 20) is preserved. It is already a joint-only
  gesture (two aims at A and B, a pin at C), so it survives unchanged.

The concept sheet (`Concept/Side.png`, look 2) is exactly this: three balls at
the corners, straight thick bands between them, the rear band straight into the
body. No band curves between two balls anywhere in the drawing.

---

## 2. Diagnosis, with evidence

### 2.1 Instrument

`P21-PROBES/manafold_p21_curvature.cpp` reads the posed skin through
`zc::decode_pose` / `zc::skin_vertex` on every key and midpoint and reports per
ring: **turn** (centreline turning angle at that ring), **shear** (ring plane vs
centreline tangent) and **rate** (|Δturn| per 60 Hz sample). Ring x sample heat
maps are in `P21-LOOKS/turn-heat-0.png`, `rate-heat-0.png`, `shear-heat-0.png`
(Inspect) and `-2` (Channel). Full per-ring tables for slots 0, 2, 11, 5, 21:
`P21-RECEIPTS/curvature-report-shipping.txt`. It locates; it decides nothing.

Ring stations are 46.5 mm apart; the balls are rings JF=7 (325), A=20 (930),
B=27 (1255), C=35 (1627), End=57 (2650).

### 2.2 Where the extra hinges are (Inspect, mean turn per ring, degrees)

| ring | station | mean turn | max | what it is |
|---:|---:|---:|---:|---|
| 12 | 558 | **13.4** | 25.6 | start of the `kBNeck` rotation blend — a bend in the middle of the front run |
| 13–16 | 604–744 | 3–4 | 8 | the rest of that blend, a parabola across the run |
| 17 | 790 | **41.1** | 60 | S-jog before A (turns the wrong way) |
| 18 | 837 | 26.4 | 34 | S-jog, turning back |
| 19 | 883 | **80.1** | 150 | **the real corner at A — 47 mm before the ball** |
| 20–23 | 930–1069 | 0.0 | 0 | ball A and the A→B run: straight |
| 24 / 25 | 1116 / 1162 | 25.6 / 11.1 | 31 / 13 | S-jog before B |
| 26 | 1209 | **76.2** | 108 | **corner at B — 46 mm before the ball** |
| 27–31 | 1255–1441 | 10 / 0 | 14 | ball B and the B→C run: straight |
| 32 / 33 | 1488 / 1534 | 18.2 / 3.0 | 32 / 5 | S-jog before C |
| 34 | 1581 | **94.9** | 148 | **corner at C — 46 mm before the ball** |
| 35 | 1627 | 24.9 | 34 | ball C |
| 36 / 37 | 1674 / 1720 | 56.3 / 49.9 | 165 / 77 | C's EXIT corner: HingeC → the bowed HingeD frame |
| 38 / 39 | 1767 / 1813 | 18.9 / 32.5 | 28 / 48 | first bow polygon corner |
| 40–44 | 1860–2046 | 0.6 | 1.1 | straight (a polygon edge), rings sheared 51° |
| 45 / 46 | 2092 / 2139 | 15.8 / 32.8 | 26 / 59 | polygon corner at the EMid helper |
| 47–48 | 2185–2232 | 0.4 | 0.7 | straight edge, sheared 33° |
| 49 / 50 | 2278 / 2325 | 32.0 / 41.0 | 49 / 65 | polygon corner at EPreSocket / PreRoot |
| 52 | 2418 | 45.9 | 62 | polygon corner at TurnMid, sheared 90° |
| 55 | 2557 | **67.2** | 110 | the hand-off corner into the rigid End stub |
| 56–62 | 2604–2883 | 0.0 | 0 | End ball + buried tail: rigid |

Window sums (Inspect): the "A" window carries 147° of turn for a joint whose
true angle is 80°; "B" 123° for 72°; "C" 247° for 96°. The excess is the S-jogs.
The run windows A–B and B–C carry **0.0°** — those rods are already straight.
The rear run carries 230° of turn spread over five corners for a joint pair
(C, End) whose true angles are 96° and 4°.

Channel, Taunt, Rest and Taunt III show the same rings within a few degrees
(receipt). It is structural, not a clip.

### 2.3 Why: the skin ladder, line by line

`manafold_model.h:270–329` (`make_loop`, integrated root authority):

* Ball X's core `[stX−50, stX+50]` is rigid in `kBHingeX` (`kLoopCarrierCoreHalfMm`
  = 50, `manafold_art.h:781`). The fold INTO the ball is `SpanDeltaX ↔ HingeX`
  over `[stX−140, stX−50]` (`kFoldBlendMm` = 90, `art.h:426`). So the whole
  rotation of joint X — fold + tilt + yaw + aim + grip + oop + dent — is
  handed over on rings 17–19 (A), 24–26 (B), 32–34 (C): **before the ball**.
* An LBS blend between frames that share a pivot, applied to rings that are
  not at the pivot, pulls the blended ring toward the bisector: with the 52 %
  ring at 93 mm before the pivot and a 40° fold, the centreline goes 0° → −27°
  → 0° → +40°. That is the S-jog, and it is arithmetic, not a defect in a value.
* `kBNeck` shares JunctionF's pivot (`kLoopArcMm[0] = 0`, pass 9) but its
  rotation reaches the skin only through `kBSpanDeltaA`, blended from
  `kBFrontRootDelta` over `[558, 790]` (`model.h:286–288`; the `s <
  front_delta1` branch at `:281` is unreachable because `front_delta1` (372) <
  `front_delta0` (410)). Pass 9 moved the bone; it did not move the skin.
* The rear (`model.h:305–329`): `HingeC ↔ SpanDeltaEStart` over `[1700, 1790]`
  (C's exit corner), then EStart ↔ EMid ↔ EPreSocket — three translated copies
  of the HingeD frame — then rigid `RearPreRootDelta` (ring 50), rigid
  `RearRootTurnMid` (51–54), rigid `RearSocket` (55+). A linear blend between two
  translated copies of one frame is a straight edge, and the copies' offsets
  are the bow's sample points: **the arc is rendered as a polygon** whose
  corners are the helper stations. Because the copies share one rotation, the
  ring planes stay parallel to HingeD's frame while the centreline turns
  (shear 51–90° on rings 40–53, `shear-heat-0.png`).
* The `RearRootDelta ↔ RearSocket` ramp at `model.h:317–321` is also dead code:
  `rear_support0` (2558) > `inE1` (2540), so rings jump from TurnMid straight to
  RearSocket. Rings 51–54 are one rigid block.

### 2.4 The End spazz: cause, attributed by experiment

`P21-RECEIPTS/attribution-and-joints.txt`, Inspect, worst turn rate per ring
(°/sample):

| config | r36 | r37 | r45 | r46 | r49 | r50 | r52 | r55 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| shipping (pass-20 arc bow) | **28.5** | 24.3 | 2.7 | 5.3 | 6.0 | 8.0 | 9.5 | **11.3** |
| `ZHAO_U02_REAR_BOW=legacy` | 1.2 | 8.6 | 0.1 | 0.1 | 0.1 | 0.2 | 4.0 | 3.4 |
| End ambient 0 + C calm 0 | 11.1 | 13.8 | 3.8 | 7.4 | 8.9 | 10.7 | 14.3 | 15.6 |

Mean turn at rings 45–50 goes from 16–41° to **0.0°** under the legacy law.
The front hinge rates (≤ 4.6) do not move in any row.

The worst samples are where the shape law is most sensitive to the chord:

* **The two taut crossings per loop** (rear span sign changes at f35.1, 47.0,
  150.0, 165.0): `rear_bow_alpha16` solves sinc(α) = c/L, and near c → L,
  α ≈ √(6·slack/L). At f164.1 → 166.0 the span goes +5 → −9 → −32 → −54 mm and
  the two-bone hand-off goes 20 → 41 → 78 → 103 mm, the rear centreline bend
  6 → 31° — in three samples. Pass 20 measured this ("rail step 0.071 →
  0.163"), laddered `kRearBowOnsetMm` and found every non-zero onset worse on
  the fold, and shipped the singularity.
* **The deep-slack region** (f73–75, span −518 … −603; f219; Channel f76–78):
  α passes 90°, so the arc's start tangent is perpendicular to the chord and
  swinging; the ring-37 blend between HingeC's frame and a HingeD copy displaced
  by the arc's first sample (dx ≈ 80–90 mm, dy ≈ −80 … −110 mm) flips the
  exit corner 20–28° per sample (`INSPECT-REAR-EVENT-4X.jpg`, look 6).
* **The hand-off** (ring 55): the rigid End stub is oriented along the chord
  (`qd`, pass 19) while the polygon arrives at ≈ α off the chord — up to 110°
  in one ring, moving with α.

So: **not a competing authority, not an oscillator, not a discontinuous aim.**
It is a shape law with a singular derivative, driven by the chord, rendered as
a polygon. Pass 19 damped End rotations (a term this table shows is not the
cause — pass 20 already found it at 0.7 %); pass 20 replaced the fold with this
bow. The fold is gone (rail 0.692) and the flicker is what replaced it. The
`nodule_aim` blind spot named in pass 20 is real but is not this: under the
legacy law ring 37 still reads 8.6 and ring 39 3.9, which is C's exit corner
through the z-then-x closure aim — the second-order term, addressed in §3.6.

### 2.5 Every bend authority, and whether it bends at a carrier

| authority | where | writes | at a carrier? | makes an extra hinge? |
|---|---|---|---|---|
| `loop_pose` folds/tilts/yaws JF, A, B, C, End | `clips.h:1001–1023` | joint quats | yes (JF/Neck pivot, A, B, C, socket) | no — the skin does |
| ambient nodule solve (3× `nodule_aim`) | `:1047–1085` | Neck, HingeA, HingeB rotations + span deltas | yes | **yes, indirectly**: Neck's rotation reaches the skin mid-run (§2.3); roll not stable (blade may twist) |
| closure aim (loop_pose) | `:1289–1338` | HingeD (overwritten later) | yes (C pivot) | — |
| dent: `dent_target_mm`, 2× `nodule_aim_rollstable`, pin | `:1099–1276` | HingeA, HingeB, HingeC, spans 1–2 | yes | no |
| `set_span_delta` + `span_helper_delta_fx` | `:498–516`, `:274` | child +Y, helper fraction | translation | no (straight) — but the fraction law puts the corner before the ball |
| `finalize_rear_follow` aim | `:1484–1493` | HingeD (z-then-x) | yes (C pivot) | second-order (roll) |
| rear span delta + **bow** | `:409`, `:339–407`, `:1495–1510` | 6 helpers, x AND y | **no — curves the run** | **yes: the polygon, the flicker** |
| v18 rear rotation staging (IDs 24–26) | `:1517–1539` | 1/3, 2/3, 1 of `conj(qd)·socket` | no — between C and End | yes (rings 50–54 turn) |
| `rear_socket_compose` (End = qd × Authored) | `:1518` | RearSocket | at End | the rigid stub's roll |
| `hinge_play` tilt/yaw per station | `:2196–2240` | JF/Neck/A/B/C/End | yes | no (Neck via §2.3) |
| `antenna_knead` grips, oop, wags | `:3414–3500` | JF, Neck, A, B, C, RearSocket | yes | no (Neck via §2.3) |
| `swallow_nodules` | `:2600–2624` | JF quat, nod offsets, socket quat | yes | no |
| front flex (`front_flex_play` → `tilt_front/yaw_front`) | `:2489`, `:1001` | JF | yes | no |
| per-clip beats (Lasso `end_turn`, nodule-solo, crown shuffle, Trick) | clip builders | joints / offsets | yes | no |
| **skin ladder** `make_loop` | `model.h:270–329` | weights | **no** | **yes — §2.3, all of it** |
| `kFoldBlendMm`, `kLoopCarrierCoreHalfMm`, `kSpanHelperRunMm` | `art.h:426, 781, 2388` | the ladder's geometry | — | the numbers that place the corners |

Conclusion of the audit: the pose layer is already joint-only. **The whole
repair is in the skin and in the rear translation law**, plus the roll of the
rod aims.

### 2.6 Two incidental findings

* `manafold_clips.h` uses `std::memcmp` / `std::strcmp` without `<cstring>`;
  `manafold_hinge_traj.cpp` does not include it either, so `build-direct.sh
  mhinge` fails to compile on this tree (`.tmp/p21-arch/build_mhinge.log`). The
  header relies on its consumer's includes. One-line fix for the implementer.
* The two unreachable ladder branches (§2.3) are silent dead code that the
  redesign removes anyway; recorded so nobody reads them as behaviour.

---

## 3. The design: RODS AND BALLS

### 3.1 The rig statement

Five joints on the existing joint bones; four visible rods and one buried tail;
**no new bones, no bone removed, IDs stable** (`kBoneCount` stays 27; four
bones become inert).

| element | bone(s) | owns |
|---|---|---|
| joint F | `kBJunctionF` ∘ `kBNeck` (one pivot at the body-surface exit) | rest yaw, neck fold, Front tilt/yaw, JF grip/wag/swallow, the aim of A, hinge-play neck, knead neck. The body ball IS this joint's ball; the Front swell is a thickening on the rod. |
| rod F→A | `kBNeck` → `kBSpanDeltaA` | straight, stretch |
| ball A | `kBHingeA` | a rigid sphere on the pivot; fold A + tilt + yaw + aim of B + grips + oop + dent aim |
| rod A→B | `kBHingeA` → `kBSpanDeltaB` | straight, stretch |
| ball B | `kBHingeB` | rigid sphere; fold B + … + aim of C + dent aim |
| rod B→C | `kBHingeB` → `kBSpanDeltaC` | straight, stretch |
| ball C | `kBHingeC` | rigid sphere; fold C + … + the dent pin |
| rod C→End | `kBHingeD` → `kBSpanDeltaEStart/EMid/EPreSocket` | straight, stretch/compact; direction = the closure aim (the chord to the socket) |
| ball End | `kBRearSocket` | rigid sphere at the body-following socket point |
| buried tail | `kBReturnTip` | as today |
| inert | `kBFrontRootDelta` (23), `kBRearRootDelta` (24), `kBRearPreRootDelta` (25), `kBRearRootTurnMid` (26), `kBSpanDeltaE` (19, receipt only) | skin nothing; identity; kept for ID stability and the pass-20 selector |

### 3.2 The skin: one chain, authored stations, rigid balls, buried cones

`make_loop` stays one chain `RingPart` (one meshlet family, one page, one
outline). Its ring stations become an **authored table** `kLoopRingStationMm[]`
instead of `total·i/63`, and the ladder becomes, per element:

* **Rod**: rings from the parent joint's pivot to the next ball's centre.
  `b0 = parent joint bone` at the pivot (w=64) … `b1 = SpanDelta helper` at the
  rod's far end (w=64), weights linear in station. Both carry the SAME
  rotation (the helper is a zero-rest-offset child of the parent joint), so
  the rod is straight and uniformly stretched by construction, whatever the
  delta. This is exactly the mechanism that already makes rings 21–23 and
  28–31 straight today; it is extended to the whole rod.
* **The rod ends AT the pivot, inside the ball.** The rod's last ring sits at
  station `pivot − 1 mm` with the rod's radius, in the parent frame. Its
  distance from the ball centre is ≤ the rod radius in every pose, so it is
  inside any ball whose radius exceeds the rod's. The next ring is the ball's
  entry pole (`pivot − R_ball`, radius `kBallPoleRxMm` = 2 mm like the
  terminal cap, so no degenerate triangles) in the CHILD frame: the strip
  between them is a cone folded back inside the ball. Hidden in bind and in
  every pose.
* **Ball**: rings from `pivot − R` to `pivot + R` in the joint bone's frame,
  radius profile a named per-ball ellipse (`kBallRxMm[5]`, `kBallRzMm[5]`),
  defaulted to today's profile-at-centre so the ball SIZE the owner accepted in
  version 18 does not change (A: 72/64 mm, etc.). A body of revolution about
  its own axis, rigid about its centre, is invariant under the joint's rotation
  — so the sphere is exact under any joint angle, with no LBS shrink and no
  crease ring.
* **Exit**: after the exit pole (`pivot + R`, 2 mm) comes the next rod's first
  ring at `pivot + 1 mm` (rod radius, child frame) — the mirror cone, also
  inside the ball — then the rod outward.
* **Joint F** has the body as its ball: the Root → (JF∘Neck) hand-off stays
  buried (rings inside `kLoopBuryMm`), and rings from the pivot outward are
  bound `kBNeck` → `kBSpanDeltaA`. `kBFrontRootDelta` is unbound. The Front
  swell rides the rod rigidly.
* **Ball End** is a sphere on `kBRearSocket` centred at the socket point; the
  rear rod ends at that centre (cone inside), the buried tail continues from
  the far pole along the chord as today.

Ring count: rods F→A 10, A→B 5, B→C 6, C→End 16 (dense, for the compaction
staging), tail 4; balls 3 × (2 cone rings + 5 sphere rings) + End 5 + base 3 →
≈ 70. `kLoopRings` becomes the table's length; whether 70 rings fit the
meshlet/vertex budget is the very first falsifier (§5, stage 1). If it does
not, the ball ring counts drop to 3 before rod density does.

Ring-order gates that key rings by bind y (`ring_map` in mrear, mspan G6's
posed-order leg, my probe) get the table and a "structural ring" flag for the
two cone rings per ball, which are legitimately non-monotone in station.

### 3.3 Rods only stretch: the length laws

* Front rods: `set_span_delta` unchanged in meaning; the helper carries the
  **full** child delta (fraction 1 at the rod's end) instead of
  `run/gradient`, because the rod now runs all the way to the ball centre.
  `kSpanHelperRunMm` and `kSpanGradientMm` collapse to the rod length; the
  signed bounds `kSpanStretchMaxPm` / `kSpanCompactionMinPm` keep their values
  (the rod lengths measured on the shipping bank are F–A 642–819, A–B 279–397,
  B–C 265–458 mm — inside the existing envelope).
* Rear rod: `finalize_rear_follow` keeps its solve verbatim — HingeD aims at
  the body-following socket, `full_delta = chord − kRearSocketFromCMm` — and
  writes it as **translation only**: `SpanDeltaEStart / EMid / EPreSocket`
  carry `full × (station − stC) / (stEnd − stC)` at their own stations (the
  staging survives because 6-bit weights over a 662 mm delta need it: three
  segments give ≈ 9 weight levels per ring, ±2 mm placement; one segment
  would give 3 levels, ±5 mm). No x component, no bow, no `rear_span_limit`,
  no deep bias; `write_rear_bow` is not called under `rods`. Rings 24–26 get
  identity quats and zero translation.
* **The closure question, answered under the rule.** A fixed-length band whose
  ends come closer must go somewhere. It cannot curve (the rule). It cannot
  bend at a non-ball. So the rear rod's length IS the chord: 349–1296 mm on
  a 1010 mm rest across the bank (Inspect 349–1164). That is −65 % … +28 %,
  inside the unchanged C–E bounds (−700 / +440 pm). Uniform compaction to
  0.345 is a shorter tube with denser rings, not a fold: every rail edge on
  the rod reads the same ratio, the ring planes stay perpendicular, and the
  crayon grain compresses along the band — which is the literal reading of
  "just stretch". The alternative (the rod slides through the End socket so
  the tail takes the length) is rejected in §4.
* **The attachment does not pay, by construction.** The End ball is at the
  socket point (`deform_body_point` of `kRearSocketTarget*`), the tail's
  burial is `kRearSocketBurialMm` along the chord — both untouched. The rod
  ends at the End ball's centre on every sample because its far-end helper
  carries exactly `chord − rest`. There is no hand-off (every rear ring has
  `b0 == b1` rotation), so the two-bone disagreement is 0 by construction, not
  by a 420 mm bound. C–E's bounds are not moved. The rip cannot return: the
  fold was non-uniform compaction (rings 51–54 rigid as a block while the
  gaps 49→50 and 54→55 took the share, plus the 1/3–2/3 rotation staging) and
  the new law has neither.

### 3.4 The balls absorb the whole angle: size, skinning, continuity

Measured on the shipping bank as ball-to-ball chord angles (what the joints
WILL be under rods; `attribution-and-joints.txt`):

| clip | A mean/max | B | C | End | per-sample joint step max |
|---|---|---|---|---|---|
| Inspect | 80 / **152** | 72 / 104 | 96 / **148** | 4 / 14 | A 4.4, B 2.8, C 5.3 |
| Channel | 79 / 124 | 75 / 90 | 91 / 142 | 3 / 13 | 2.9 / 1.8 / 3.6 |
| Taunt | 70 / 134 | 70 / 80 | 92 / 149 | 4 / 14 | 4.2 / 1.5 / 6.2 |
| Rest | 83 / 149 | 73 / 86 | 100 / 140 | 2 / 9 | 3.9 / 2.1 / 4.9 |
| Taunt III | 58 / 101 | 50 / 76 | 86 / 132 | 4 / 10 | 7.0 / 6.8 / 6.5 |

* **Continuity is exact at any angle**: the sphere is closed, the rods end
  inside it, the cones are inside it. There is no open edge and no LBS across
  the joint, so nothing can pinch, splay or shear. `mmeshcheck` stays CLEAN
  (2 mm poles).
* **What the size buys.** Two rods of radius r meeting at a joint with turning
  angle θ intersect each other on the inside of the bend out to
  `r / cos(θ/2)` from the centre. Inside the ball that is hidden; beyond it,
  the two rods visibly merge in a V-crotch. With r = 46 and R = 72 (A/B/C
  today) the crotch is hidden up to θ ≈ 100°. B never exceeds 104°. A exceeds
  it only during the deep knead press (152° at the bottom of the dent — the
  A→B rod folds back along F→A), C during the tightest loop closure (148° at
  the shortest chord). At those poses the crotch would need R ≈ 178 / 167 mm
  to hide, which is the "obviously big protruding balls" version 18 rejected.
  **Declared:** at joints past ~100° the two rods touch and merge outside the
  ball as a clean crotch (correct normals, union silhouette) — the folded
  hinge the pose actually is. It is not the pass-19/20 wedge, which was a
  skin fold. Levers, both art and both named: per-ball radius
  (`kBallRxMm`/`RzMm`, laddered by eye), and the dent depth / closure depth
  the owner already owns. The gate REPORTS `r/cos(θmax/2)` per ball beside
  the shipping radius; it does not gate it.
* **The knead** reads through the balls exactly as before: the press is two
  joint angles at A and B and a pin at C; nothing in the dent path touches the
  skin ladder or the span law's meaning. R5's B-lowest ranking is measured on
  the posed ball core, which is now a rigid sphere — a cleaner operand.

### 3.5 What is removed, merged, re-expressed

| today | under `rods` |
|---|---|
| 90 mm pre-ball fold blends, 50 mm rigid cores | ball = rigid sphere at the pivot; rods end inside it |
| `kBNeck` rotation blended 558–790 | composed at the F pivot; the front rod is one frame |
| `kBFrontRootDelta` (v18 front root helper) | inert |
| rear bow (`write_rear_bow`, `rear_bow_alpha16`, `rear_bow_delta_mm`, sign/onset/max-α knobs) | not called; translation-only staging |
| `rear_span_limit_fx`, `kRearSpanDeepBiasPm` (both already off) | not called |
| v18 rear rotation staging (24/25/26), `rear_socket_compose`'s staged relative | inert; End ball is a sphere on `qd` (roll invisible) |
| End ambient/authored rotations (`hinge_play` End, knead B2 wag, swallow End, Lasso `end_turn`) | **inert on a sphere.** Kept as code under the selector; declared. Direction 14's public End proof is re-based (§6). |
| `nodule_aim` (3 ambient callers) and both HingeD aims (z-then-x) | `nodule_aim_rollstable` with the loop-plane normal as the conditioning axis (§3.6) |
| `kLoopCarrierCoreHalfMm`, `kFoldBlendMm`, `kSpanHelperRunMm`, `kRootSwellSupport*`, `kRearRootRotation*` | retired from the `rods` ladder; kept for `pass20` |

### 3.6 Rod aims must be roll-stable

The blade is elliptical (rx 44–58 vs rz 20–30). Under rods, an aim's residual
ROLL about the rod is the blade's roll — visible as the flat side flipping.
Pass 20 left `nodule_aim`'s three ambient callers and both HingeD aims on the
z-then-x decomposition "because changing them moves every byte". Every byte
moves in this pass anyway, so all five rod aims use
`nodule_aim_rollstable(..., axis)` with the axis = the pre-aim loop-plane
normal (for the rear: the normal of the (B, C, socket) triangle, computed once
per sample from the ambient pose, the same construction as the dent's). The
old z-then-x residual at ring 37/39 (8.6 / 3.9 °/sample under the legacy law)
is this term. Under `pass20` the old primitives run verbatim.

### 3.7 Integer / fixed-point / determinism

Nothing new in the pose path. The rear delta is the same `chord − rest` in
fx16; the station-proportional shares are one rounded divide per helper (the
existing `span_fraction_delta_fx` with run = station offset, gradient = rod
length). The sphere profile is bind-time integer arithmetic in `make_loop`
(`isqrt` of `R² − d²` scaled by rx/rz per mille). No floats, no `getenv` in
the solver, midpoints by the existing `finalize_rear_follow_midpoints`
averaging (which now has nothing nonlinear to re-solve except HingeD's aim,
exactly as today). The 6-bit weight quantisation bounds the rod's straightness
error: a weight step of 1/64 on a ≤ 480 pm delta over a ≥ 265 mm rod moves a
ring ≤ 2.5 mm off the straight line — < 0.6° of apparent turn per ring, which
is the tolerance R6 uses (§6).

### 3.8 Exact-off compatibility with pass 20

One strict selector: `ZHAO_U02_RIG=rods|pass20` (`kRig`, compile default
`kPass20` until the final step, then `kRods`). It gates: the `make_loop`
ladder + station table, the rear translation law (bow on/off, staging quats),
the front binding, the aim primitive, and the End rotations. Under `pass20`
the bank must be **byte-identical on all 22 subjects** to the pass-20 exact
bank (`P20-FINAL-BANK-INTEGRITY.md`), with the three pass-20 switches at their
shipped values. Checked on 22, not 3 — pass 20's own lesson (§10 of its
findings). The existing selectors (`REAR_BOW`, `ROOT_AUTHORITY`,
`REAR_SOCKET_FRAME`, `KNEAD_DIP_SOLVER`, …) remain meaningful only under
`pass20`; under `rods` the banner prints them as "inert under rods" rather
than silently accepting them (the pass-20 re-review's mode-selector lesson).

---

## 4. Alternatives rejected

1. **Keep the bow, smooth it** (onset blend, α cap, more helpers). Forbidden
   by the rule (it curves a run), and pass 20 already laddered onset and cap
   and found every rung trades fold for step. More helpers make more corners.
2. **Slide the rear rod through the End socket** (fixed-length rod, the tail
   takes the length). Physically pretty, but the End ball would have to be a
   separate body part the rod passes through, the bind rod would need to be
   ≥ 1296 mm to keep the tip buried at Taunt III's longest chord, and the End
   ball would stop being a joint of the rod. Also not what the owner said:
   "just stretch". Kept as the fallback if a 3× compacted rod reads badly by
   eye — that is the one art risk of §3.3 (risk 2).
3. **Separate parts** (rigid `make_ball` spheres + rod chain segments with
   caps). Same geometry as §3.2 with cleaner ring bookkeeping, but it splits
   the one loop meshlet family every gate keys on and re-opens pass 6's "no
   separate closed spheres". It is the **fallback** if the chain builder or
   the meshlet budget refuses the non-monotone cone rings (stage 1 decides).
4. **Move the fold blend INTO the ball** (a 1–2 ring LBS blend centred on the
   pivot). LBS across the equator shrinks the sphere along the bisector by
   cos(θ/2) — 23 % at 80°, 50 % at 120°: a dented ball at every deep joint.
   A rigid sphere has no such term.
5. **Half the ball in each frame** (hemisphere split at the equator). Leaves a
   lune-shaped hole between the two tilted equators. No.
6. **Bigger balls to hide every crotch.** R ≈ 170 mm at A and C — version 18's
   rejected beads. The crotch is declared instead and the radius stays an eye
   knob.
7. **Bound the joint angles** (re-author the closure so C never exceeds ~100°,
   or slide the socket along the body toward C via `kBLoopBase2`). Changes the
   loop's accepted motion and the attachment's meaning; not asked for.
   Declared omission.
8. **Merge `kBNeck` into `kBJunctionF`.** Would change rotation composition
   order for every authored curve; binding the rod to the composed frame gets
   the same picture with no clip retimed.
9. **Iterative closure / IK.** Nothing here needs it; closure stays
   closed-form (the chord).

---

## 5. Staged build order, each with its cheapest falsifying experiment

Every stage ends in a commit; stages 1–4 ship with the selector at `pass20`.

0. **Receipts from the HEAD binary before touching a file**: the pass-20 exact
   bank CRCs for all 22 subjects; the probe's per-ring tables (this packet's
   `curvature-report-shipping.txt` is the BEFORE).
1. **The selector + the station table + the `rods` ladder in `make_loop`**
   (spheres, cones, rod bindings). No solve change yet.
   *Falsify (30 s, no render):* `mmeshcheck` CLEAN on the `rods` type; the
   probe on slot 7 (Still, 2 samples) reports **turn ≤ 1.5° on every rod
   ring** at rest and the rest silhouette's ball centres within 5 mm of
   pass 20's (the rest fold must live entirely in the ball rings). If the
   compile rejects the ring count or the builder mis-stitches the cone rings,
   fall back to §4.3 before anything else is built.
2. **Front**: bind the F→A rod to `kBNeck`/`kBSpanDeltaA`, helper fraction 1,
   `kBFrontRootDelta` inert.
   *Falsify:* probe on Inspect: rings of rod F→A turn ≤ 1.5° on all 600
   samples; joint A's total turn equals the ball-chord angle ±2°.
3. **Rear**: translation-only staging, no bow, staging quats identity, End
   sphere on `qd`, End rotations inert.
   *Falsify:* probe on Inspect: rear rod rings turn ≤ 1.5°, rate map has no
   stripe > 8 °/sample anywhere on the rear; mrear's rear span receipt is
   digit-identical to pass 20's per sample (the solve did not move); hand-off
   = 0 on every ring; mprobe burial green; R1 arm↔End = 0.
4. **Roll-stable aims** for the five rod aims.
   *Falsify:* a per-ring BLADE-ROLL trace (the ring's rx axis vs the loop
   normal) on Inspect and Taunt III shows no step > 8° per sample, and mspan
   G9's carrier step does not rise.
5. **Look** (the acceptance): Inspect at native and 4× on the same by-badness
   frames as this packet (f73–79, f123–165, f300–330), Channel f76–78, Taunt
   f7/f56, the knead window (`CLOSE-KNEAD-NATIVE` frames), all-frame sheets of
   all 22; before/after pairs against `pass20` at the same frames. Chosen by
   the probe's worst rings, not by index.
6. **Gates** (§6) with every positive control fired before its silence is
   quoted; the 22/22 identity leg; full matrix from scratch.
7. **Ball radius ladder** by eye (per ball; the crotch floor printed beside
   it), then flip the default to `kRods`, matrix again, commit.

---

## 6. Gate plan — measured on the POSED SURFACE

New binary `manafold-rodgate` (`mrod`), built from this packet's probe
(committed at `P21-PROBES/`; the implementer moves it to `tools/reel/` and
`build-direct.sh`). Every leg has a fired control.

| leg | asserts (every key + midpoint, every clip) | control |
|---|---|---|
| **R6 ROD STRAIGHT** | on every rod ring: centreline turn ≤ `kRodTurnMaxDeg` = 1.5 (6-bit quantisation ceiling, §3.7), ring-plane shear ≤ `kRodShearMaxDeg` = 5 | `--fail-rod-bend` = `pass20` rig (rings 17–19 read 41/26/80°) |
| **R7 JOINT ON BALL** | ≥ 98 % of each joint's total turn lies inside its ball's ring window; the mid-run rings 12–16 read ≤ 1.5° | same control (ring 12 reads 13°) |
| **R8 JOINT SMOOTH** | per-ball joint angle (ball-chord to ball-chord) step ≤ `kAntennaMaxAngularStepDeg` = 8 (unmoved); per-ring turn rate on the rear ≤ 8 | `--fail-rod-flicker` = `pass20` rig on Inspect (ring 36: 28.5) |
| **R9 ROD UNIFORM** | on each rod, min rail / max rail ≥ `kRodRailUniformPm` = 850; rod compaction/stretch within the existing per-span bounds (numbers unchanged) | `--fail-rod-uniform` = `pass20` (rear rail 0.129 vs 1.25 on one rod) |
| **R10 BALL RIGID** | every ball ring's rail and hoop strain against REST = 1.000 ± 1 LSB | `--fail-ball-blend`: a mutant that weights one ball ring 50 % to the parent (strain moves by cos(θ/2)) |
| **R4 re-based** | keep `kGateRailRegressFloor` 0.12 hard; retire the 0.50 *fold* target in favour of R9 (a uniform 0.345 is a compaction, bounded by C–E's −700 pm, not a fold). Print both so the retirement is visible. | `--fail-rear-strain` (existing) |
| **R1** | arm↔End rotation ≤ existing ceiling — reads 0 under rods; keep as the "hand-off exists again" detector | `--fail-rear-frame` |
| **mspan G5** | helper law = "fraction 1 at the rod's end"; rear helpers station-proportional; every bound value unchanged | existing rigid/clamp/drift/overcompact controls |
| **mspan G6/G7/G9/G10/G11, R5, mprobe, mmeshcheck, moutline, mshell, mnodule, meyesize, msmooth, mqa** | unchanged thresholds; G6's posed-order leg skips the two structural cone rings per ball (flagged in the station table) | unchanged |
| **mjointpub End** | re-based: the End's public motion is the rear rod's direction change at the End ball over the clip ≥ 20 mm-equivalent at the last visible ring (the same visible thing Direction 14 asked for); End rotation on a sphere is not a witness | `--mute-E` (existing) must still fire |
| **identity** | `ZHAO_U02_RIG=pass20` → pass-20 exact bank, **22/22** byte-identical | a 1 mm ball radius change under `pass20` must change a CRC |
| **INFO, not gated** | per ball: max joint angle and `r/cos(θmax/2)` beside the shipping radius; per rod: length min/max vs rest | — |

A gate reading zero on R6 is a claim: its control is fired first, in the same
matrix invocation, every time.

---

## 7. Declared omissions

* No change to any authored curve, beat, schedule, gain or timing: the pose
  layer is not touched except the aim primitive's roll (§3.6).
* No change to the dent (`kKneadDent*`), R5, or G10; the knead's read is
  re-judged by eye through the new balls and nothing else.
* No new bones; the four inert helpers stay allocated.
* No socket slide, no joint-angle bounding, no re-authoring of the loop's
  closure depth (§4.7).
* No ball-size decision here: defaults = today's centre radii; the ladder is
  the implementer's by-eye step 7.
* The `pass20` code paths (bow, staging, old ladder, z-then-x aims) stay in
  the tree behind the selector for this pass; deleting them is a later
  cleanup once the owner has accepted the rods rig.
* `manafold_lab.h`'s forked `lab_antenna_knead` and slot 16 nodule-solo are
  not re-examined (they never call the knead layer; the skin change reaches
  them automatically).
* The front pivot stays at the body-surface exit (250 mm), not at the Front
  swell's centre (320 mm): the body is that joint's ball and the swell is a
  thickening on the rod. Moving the pivot would retime Trick's plant and the
  closure walk for a 70 mm cosmetic; flagged for the owner's eye, not done.

---

## 8. Risks, in the order I expect them to bite

1. **The crotch at deep joints** (§3.4): at the bottom of the knead press (A
   ≈ 150°) and at the tightest closure (C ≈ 148°) the two rods merge outside
   the ball. The owner may read it as a fold. Levers: ball radius (art),
   dent depth, closure depth. The gate prints the number; the eye decides.
2. **A 3× compacted rear rod may read as an accordion** at the shortest chord
   (Inspect f79, 349 mm). At 240p it is ~15 px of tube; if the grain bunching
   shows, the slide-through-socket alternative (§4.2) is the fallback, and it
   is a rear-only change.
3. **Every clip's bytes change.** There is no way to satisfy the rule without
   re-skinning the whole antenna; the identity leg proves `pass20` is intact
   and the all-22 every-frame sheets are the breakage check.
4. **Trick's pinned plant.** Ball B becomes a sphere of the same centre radius
   but a different profile; `mprobe`'s carrier-B support ownership (140/140)
   and depth range (−33…−18 mm) will move by the profile difference. Not a
   relaxation: `kTrickPlantRootMm` is re-chosen by eye on the plant frames
   and the probe re-certifies 140/140 owned. Any depth outside the crash
   limit is a fault.
5. **The rear attachment bound must not be relaxed** — and is not needed to
   be: the rod's length is the chord, which today's bank already keeps inside
   −700/+440 pm; the End ball is pinned to the body point. If any clip's chord
   leaves the envelope after the change, that is a defect in the change (the
   solve is supposed to be byte-identical), not a reason to move the bound.
6. **Meshlet / vertex budget** for ~70 rings (stage 1's falsifier).
7. **Gate bookkeeping keyed on uniform ring stations** (mrear `ring_map`,
   `ring_of_station`, `kRootSwellSupport*`, `kRearRootRotation*`, G6's
   order leg): all move to the station table; a stale copy anywhere reads the
   wrong ring and reports a confident wrong number (the pass-19 blindness
   pattern). The table lives in `manafold_art.h` and every consumer reads it.
8. **The blade's roll** under the roll-stable aims changes the lit face of the
   rods on some frames; it is judged by eye in stage 5 and bounded in stage 4.

---

## 9. Evidence index

| file | what it decides |
|---|---|
| `P21-LOOKS/turn-heat-0.png`, `-2.png` | where the band bends: stripes 1–3 rings before each ball, zero inside the runs A–B / B–C, a five-corner forest on the rear |
| `P21-LOOKS/rate-heat-0.png`, `-2.png` | where the bend MOVES: the rear polygon flickers, the front hinges do not |
| `P21-LOOKS/shear-heat-0.png` | the bowed rings are sheared 50–90° off the centreline |
| `P21-LOOKS/INSPECT-WORST-NATIVE.png` | the by-badness frames at native |
| `P21-LOOKS/INSPECT-REAR-EVENT-4X.jpg`, `INSPECT-STRIP-070-082-2X.jpg` | the ring-36/37 event: the rear elbow changing shape frame to frame |
| `P21-LOOKS/INSPECT-FOLDED-4X.jpg` | the squeezed loop as a polygon with corners off the balls |
| `P21-LOOKS/INSPECT-FRONT-JOINTS-4X.jpg` | corners one ring before the balls; the front rod bending mid-run |
| `P21-LOOKS/CHANNEL-TAUNT-EVENTS-4X.jpg` | the same on two lively clips |
| `P21-RECEIPTS/curvature-report-shipping.txt` | per-ring tables, five clips |
| `P21-RECEIPTS/attribution-and-joints.txt` | the bow-vs-ambient attribution; ball-chord joint angles and crotch floors |
| `P21-RECEIPTS/render.log`, `binaries-md5.txt` | the build and render these came from |
| `P21-PROBES/` | the committed instrument and its report script |
