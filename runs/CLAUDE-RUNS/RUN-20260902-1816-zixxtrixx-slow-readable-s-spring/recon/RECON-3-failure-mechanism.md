# RECON 3 — why the published spring is a "spazzy fuckfest"

Read-only reconnaissance, 2026-09-02. Lane:
`zixxtrixx-wholebody-s-spring-20260901`. Nothing in either repo was modified
except this file.

**Owner's clarification received mid-recon:** *"with you can't see what it's
supposed to be I meant not shape but the movement. It's so jittery and fast
there's no smoothness. But yeah, sure, shape too"* — so this report leads with
motion quality and keeps silhouette second.

---

## Method, and what "verified" means here

Two independent instruments, and they agree:

1. **A Python replica of the shipped sampler** (`spring_route_heading`,
   `spring_arm_amount`, `spring_station_arm`, `spring_life_wave`,
   `spring_support_surface_lift`, the station-14 root compensation, the FK
   chain and the presentation-midpoint nlerp). **Validated against the
   committed probe**: it reproduces
   `RUN-20260902-0611/evidence/final-collapsed-centreline.txt` to within
   **2 mm on every one of the 20 stations** (head expected (-1346, 633), replica
   (-1345, 635); tail tip expected (-1296, -39), replica (-1294, -38)). Scratch
   only: `…/scratchpad/recon3/spring.py`, `tick.py`, `jit.py`.
   I did not build the C++ probe (read-only brief); the replica is the
   substitute and its agreement with the committed output is its warrant.
2. **The published 384x240 60 fps clips themselves** —
   `Upheaval/website/public/renders/zixxtrixx-jump-one.webm` and
   `zixxtrixx-salto-dummy.webm`, decoded to PNGs and measured
   (colour-keyed silhouette, largest connected component, frame-to-frame IoU
   and centroid), with `zixxtrixx-balance.webm` as the owner's named pace
   reference.

Frame numbers below are **0-based video frames of the published clip at 60 Hz**.
One authored key = two frames (`build_attack`: *"150 keys = 300 frames = 5.000 s
at 60 fps"*; ffprobe confirms `r_frame_rate=60/1`).

Anything I could not verify is labelled INFERRED.

---

# PART 1 — THE JITTER (the primary fault)

Ranked by contribution to the owner's read.

## J1. The arming clock is uniform; the pose route is not. 9.4x speed ratio between legs. VERIFIED

`spring_arm_ease` is a trapezoidal-speed ramp and it does its job: it delivers a
near-constant **~87 arm-units per key** from key 5 to key 12. But the four
authored heading tables are **not equally spaced in shape**, so a constant
arm-rate is a wildly varying pose-rate.

Shape path = sum over all 20 spine stations of |Δposition|; turn = sum over the
19 joints of |Δheading|. Integrated in 200 sub-steps per leg.

| leg | arm span | **frames** | shape path | joint turn | **mm/frame** | **deg/frame** |
|---|---|---|---|---|---|---|
| grounded → absorb | 0–220 | 9.9 | 3 475 mm | 213° | 352 | 21.5 |
| absorb → assembled | 220–400 | 4.2 | 3 049 mm | 221° | 731 | 53.1 |
| **assembled → seating** | **400–700** | **6.9** | **22 748 mm** | **1 321°** | **3 293** | **191.3** |
| seating → collapsed | 700–1000 | 11.0 | 9 353 mm | 584° | 847 | 52.8 |

**The third leg is 9.4x faster per frame than the first and 3.9x faster than
the last.** It carries **59 % of the whole action's shape change in 22 % of its
frames** (frames 14–21). Nothing in the clock asks for that; it is entirely a
property of where `kSpringAssembledHeading` and `kSpringSeatingHeading` sit
relative to each other.

Where the 1 321° comes from — shortest-arc deltas, assembled → seating:

| station | 11 | 12 | 13 | **14** | 15 | 16 |
|---|---|---|---|---|---|---|
| turn | −138.5° | −166.0° | −164.0° | **−180.0°** | +178.0° | +176.0° |

The whole tail (stations 11–16) flips through **half a turn each** in 6.9
frames. That is the "whip" the code comments describe, and it is the single
event the eye reads as the spasm.

**Fragility worth naming:** station 14's delta is *exactly* 32768 a16 = 180°.
The shortest-arc unwrap is degenerate there (`spring_unwrap` resolves
`0x8000` to −32768, i.e. the negative way). A ±1 edit to either table flips the
direction the entire tail whips. That is an unauthored, invisible knob.

## J2. The planted support's vertical route reverses direction SIX times, and the camera is welded to it. VERIFIED

`kSpringSquashLiftRoute` is eight hand-authored keys, linearly interpolated
(C0, not C1), driving station 14's height — which, through
`spring_root_from_quats_raw`, is **the whole animal's vertical anchor**:

```
{150,-40} {300,400} {460,470} {620,276} {790,300} {840,365} {920,282} {1000,287}
              up        up      DOWN       UP        UP       DOWN      UP
```

Station 14's x is *constant* (−1238 mm) by construction, so every one of these
is a pure vertical reversal — a 180° flip. Measured per frame:

| frames | 9–10 | 11–14 | 15–16 | **17–18** | 19–20 | **21–22** | 23–24 | **25–26** | **27–28** | 29–32 |
|---|---|---|---|---|---|---|---|---|---|---|
| Δy per frame | +25/+23 | −7/−6/−7/−6 | −32/−32 | **+195/+195** | +46/+46 | **−67/−67** | −24/−23 | **+33/+34** | **−41/−40** | +1/+2/+1/+1 |

Seven sign changes in a 32-frame arming — a **10–15 Hz vertical bounce of the
entire creature**. At frame 17–18 the "planted" support travels **390 mm in
0.033 s**, then decelerates by −149 mm/frame² two frames later.

**And the camera rides it.** `subject_zixx_jump_one()` sets `cam_track = true`
with `cam_track_num = 1000`, and `zixx_jump_track()` returns
`x = spring_root_anchor_x(...)`, `y = lift + spring_root_offset(...)` — i.e. the
camera follows the **raw root, at 100 %, including the life-wave term**
(`life_key_mk = key*1000` is passed straight through). So every jerk in J1 and
J2 is also a whole-screen jerk. Corroborated in the pixels: background churn
(mean |Δ| over non-creature pixels) peaks at frames 19–22 and 27–28, and falls
to a third of that at frames 25–26 and 31–36 — **the world pans, stops, pans
again, stops.**

This is the same fault that was already diagnosed and fixed once for the salto
camera ("RUN 1939 (owner: *Salto camera is too jittery*)") — the fix was to
track the plan's smooth trajectory instead of the baked root. The jump camera
never got that fix.

## J3. Sixty-three direction reversals in 32 frames, and they all land on ODD frames. VERIFIED

A reversal = the displacement from f−1→f and f→f+1 differ by more than 90°,
with both steps over 3 mm. Counted over all 20 stations, frames 0–32:

```
TOTAL 63 station-frame reversals
by frame:  f9:3  f11:5  f13:5  f15:5  f17:10  f19:4  f21:1  f25:7  f27:19  f29:4
```

**Every reversal is on an odd frame.** Odd frames are the presentation
midpoints. At frame 27 alone, **19 of the 20 stations reverse direction** —
the whole animal shivers in one frame.

Per station:

| station | 0 | 1–8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| reversals | 0 | 1 each | 2 | 6 | 6 | 6 | 6 | 5 | 5 | 5 | 4 | 5 | 5 |

The rear half of the animal reverses direction five or six times each inside
half a second.

## J4. The 60 Hz "interpolation" adds no smoothing — it is a linear chord, so acceleration arrives as impulses at 30 Hz. VERIFIED

`c.interpolate = true` blends the two neighbouring keys' quaternions. That puts
the odd frame at the **chord midpoint**, so the speed on frame 2k+1 equals the
speed on frame 2k+2 and the velocity profile is a **staircase**: constant for
two frames, then a step. Head speed, mm/frame:

```
f: 17   18    19    20    21    22    23   24   25   26   27   28   29   30
v: 47  101   280   271   177   127    31   25   27   29   81   82   22   22
```

The steps between adjacent key-pairs are **2.1x (f17→18), 2.8x (f19→20 vs
f17→18), 4.1x (f22→23), 2.8x (f26→27)**. Only two of the seventeen key pairs in
the arming carry the *same* speed as their neighbour; every other key boundary
is a velocity discontinuity. There is no C1 continuity at 60 Hz anywhere in the
action — the route is C1 in `arm`, but `arm` is sampled at 30 Hz and joined with
straight lines.

Consequence: the chord midpoint cuts *inside* every arc, which is exactly why
all 63 reversals in J3 land on the odd frame. The animal goes out to the key,
back toward the chord, out to the next key: a 30 Hz shimmer riding on top of the
primary motion.

## J5. The head's velocity profile has five maxima and four minima. VERIFIED

Per-key head speed through the arming (mm/frame), keys 1→16:

```
4, 12, 18, 21, 14, 45, 40, 55, 101, 271, 127, 25, 29, 82, 22, 9
       ▲          ▼   ▲   ▼           ▲PEAK       ▼   ▲(late) ▼
```

It ramps, **dips at key 5**, jumps, dips, explodes to 271, crashes to 25,
**re-accelerates to 82 at frames 27–28**, and stops. Four distinct speed regimes
in the last 12 frames. The late re-acceleration is the "front's own fold" (arm
700→1000, `kSpringSeatingHeading` → `kSpringCollapsedHeading`) arriving *after*
the animal looks settled — the head is nearly still for frames 23–26 (25–29
mm/frame), then lurches 163 mm in two frames. Acceleration peaks: **+179
mm/frame² at f19, −93 at f21, −97 at f23, +52 at f27.**

## J6. The travelling waves and the per-station lag are INNOCENT. VERIFIED by ablation

Contrary to what the "life layer" comments would suggest, none of the organic
devices cause the jitter. Ablation on the replica, arming frames 0–32:

| configuration | reversals | peak head speed | s14 vertical sign flips |
|---|---|---|---|
| **as shipped** | **63** | 280 mm/f | 6 |
| `kSpringWobble`/`Wobble2` = 0 | 63 | 276 | 6 |
| `kSpringChainLag` = 0 | 64 | 271 | 6 |
| both off | 61 | 266 | 6 |
| **`kSpringSquashLiftRoute` made monotone (only)** | **32** | 290 | **2** |
| both off **and** lift route monotone | **18** | 238 | 2 |

The waves are 380 and 280 a16 = **2.1° and 1.5°** against a route that swings
280°; they are two orders of magnitude too small to matter. **Making the support
lift route monotone alone removes half the reversals.** Removing the life layer
removes almost none.

The damped landing bounce (`kJumpLandingSettleBounce`) fires only at
`f >= land` (≈ frame 130 in this clip) and is **not** in the arming.

**So: do not blame the life layer, and do not delete it. Blame the lift route,
the leg spacing, and the linear presentation.**

## J7. Measured against the balance clip, the owner's own pace reference. VERIFIED from pixels

`zixxtrixx-balance.webm`, 493 frames, silhouette centroid:

| metric | balance median | balance p95 | **balance MAX (whole clip)** |
|---|---|---|---|
| \|ΔCx\| px/frame | 0.40 | 3.0 | 11.9 |
| \|ΔCy\| px/frame | **0.10** | 0.6 | **1.4** |
| frame-to-frame IoU | 96.5 % | — | min 44.6 % |

`zixxtrixx-jump-one.webm`, arming frames 17–22:

| frame | 17 | 18 | 19 | 20 | 21 | 22 |
|---|---|---|---|---|---|---|
| \|ΔCx\| | 3.4 | 3.8 | 7.2 | **12.4** | 10.8 | 10.4 |
| \|ΔCy\| | 1.5 | 6.3 | **10.0** | 1.1 | 5.8 | 3.6 |
| IoU % | 58.5 | **41.8** | 42.4 | 61.7 | 59.2 | 66.4 |

**The spring's vertical centroid step at frame 19 is 10.0 px — 7.1x the balance
clip's worst frame in 493, and 100x its median.** Its IoU at frames 18–19 sits
*below* the balance clip's whole-clip minimum for two consecutive frames. When
the owner says "like the little balancing trick example", this table is the gap
he is describing.

---

# PART 2 — THE SILHOUETTE (secondary, but real)

## S1. The head does literally pass the tail. VERIFIED against the committed probe

From `final-collapsed-centreline.txt` (committed evidence) and reproduced by the
replica, at the loaded pose:

* head (station 0): **x = −1346 mm**
* tail tube tip (station 19): **x = −1295 mm**
* rearmost point of the whole animal (station 6, the neck): **x = −2127 mm**

**The head ends 51 mm behind the tail tip, and stations 0–8 — the entire front
half — end up between 51 and 832 mm behind it.** The head starts at x = 0 and
finishes at −1346: **1 346 mm of backward travel and 476 mm down** on an animal
3 050 mm long. The instruction was "slightly backwards".

Head-minus-tail x through the action (mm): 1925 (f0) → 1487 (f16) → 779 (f20)
→ 196 (f22) → **4 (f26)** → **−51 (f32)**. **The crossing happens at frame ~26.**

Note the head does not *look* like it is moving on screen — I tracked the gold
eye and it holds x ≈ 252 ± 2 px for the whole arming — because the camera is
locked to it (J2). What the viewer actually sees is **the entire body rushing
forward and gathering underneath a stationary head**, which reads as a slide,
not as articulation.

## S2. The silhouette halves in five frames, then holds an illegible lump for a quarter of a second. VERIFIED from pixels

`zixxtrixx-jump-one.webm`, silhouette bounding box:

| frame | 0–16 | 17 | 18 | 19 | 20 | 21 | 22 | 23–36 | 37–40 |
|---|---|---|---|---|---|---|---|---|---|
| width px | 186→172 | 190 | 170 | 157 | 120 | 93 | 87 | 92→100 | re-expands |
| IoU % | 78–99 | 58 | 42 | 42 | 62 | 59 | 66 | 80–99 | 31–60 |

**Width 190 → 87 px in five frames (f17→f21), a 54 % collapse.** Then frames
**22–36 are near-frozen** (IoU 80–99 %, 15 frames = 0.25 s) at 92–100 px wide.
That rendered hold is **3.75x longer than the authored 4-frame hold** (keys
16–18), because leg 4 of the route (J1) barely changes the shape: the animal
*looks* stopped for a quarter second, which is exactly why the preceding whip
reads as a spasm rather than as a wind-up.

Named frames where the read dies, from the 2x nearest-neighbour strip
(`scratchpad/recon3/zoom_jump_12_39.png`):

* **f12–f16** — legible. Clear S, head with gold eye at frame right, tail fan
  two dark spikes up-left.
* **f17** — the S is gone. The animal reads as a straight diagonal rod with a
  fork on one end. This is the first unreadable frame.
* **f18** — the fan is two parallel spikes; the body is a stub. The silhouette
  has visually halved in one frame from f17.
* **f19** — fan now horizontal, body a compact green blob. Nothing in common
  with f18 (IoU 42 %).
* **f21** — the fan collapses to a stray pink line lying *through* the body.
  The silhouette is an amorphous lump.
* **f22–f36** — the loaded pose. A closed green ring with the head lying on top
  and a detached-looking pink sliver beneath. Held for 15 frames.

## S3. Three separate reasons the loaded pose is unreadable. VERIFIED (mechanism INFERRED where noted)

1. **The body closes a loop.** `kSpringCollapsedHeading[10] = 51063` a16 =
   **280.5°**, with a −67° reversal at joint 11 and a −94° kink at joint 17.
   The mid-body wraps a full turn. All body segments carry the same green at
   240p with one key light, so a closed loop with no depth cue renders as a
   solid mass with a hole. INFERRED (from the render) that this is what makes
   head-vs-tail unresolvable.
2. **The tail fan reads as debris.** `kSpringBladeSquashRise = 9500` sweeps the
   blades up and `kSpringBladeFlare = −1500` closes them, so by f21 the fan is
   thin pink slivers lying below and behind the mass, no longer legible as part
   of the animal (visible f24–f36).
3. **The declared self-press *is* the collapse.** The owner-ruled press windows
   are presentation ticks 13–21 and 38–40 — i.e. **video frames 13–21 and
   38–40**, precisely the frames where the body visibly folds through itself.
   That ruling was made under Direction 22 to keep the tight coil; Direction 23
   withdraws the coil, so the permit should be withdrawn with it.

## S4. Contrast against the ground is NOT the problem. VERIFIED

Terrain is (82, 65, 41) brown, sky (165, 98, 107); the body is saturated green
with a near-black (24, 24, 24) outline. Separation is ample at every frame.
The failure is shape and motion, not value.

---

# PART 3 — THINGS I CHECKED THAT TURNED OUT NOT TO BE FAULTS

Recorded so nobody spends a pass on them.

* **The two code paths agree.** `zixxtrixx-jump-one.webm` (planned consumer,
  `build_jump` / `zixx_jump_track`) and `zixxtrixx-salto-dummy.webm` (the golden
  monolithic `build_attack`) are **frame-for-frame identical in structure**:
  IoU drops at f17 (58.5 vs 58.0), bottoms at f18–19 (41.8/42.4 vs 46.1/45.3),
  peak centroid step at f20, static blob f22–36, re-expansion f37–40. Width
  ratios rest→loaded are 0.54 in both (they differ only in camera distance).
  Structurally, `uses_default_shared_spring_timing()` gates both onto the same
  `spring_shared_*_amount` schedule. **There is no discrepancy to chase.**
* **The angle16 seam is not a bug.** `spring_route_heading` returns −14 516 for
  station 10 at arm 999 and +51 063 at arm 1000 — a 65 536 jump in the integer,
  but the *same physical angle*, and `quat_z` only sees it mod 360. The
  `spring_unwrap` chain deliberately takes the short way (−75.5° rather than
  +284.6°) through the assembled→seating→collapsed knots. The pose is
  continuous. (My first replica averaged raw integers here and produced a
  spurious 977 mm one-frame spike at f31; the corrected per-bone shortest-arc
  nlerp removes it entirely. Flagging this because it is an easy trap.)
* **The life waves and chain lag are not the jitter** — see J6.
* **The landing bounce does not fire during the arming** — see J6.

## Where I could not reproduce a previous agent's number

Recon 1 is reported as finding the head walking **2 771 mm of path for 1 523 mm
of net displacement (ratio 1.82)**. Walking the presentation ticks I get, for
the nose (bone 0 origin):

| span | path | net | ratio |
|---|---|---|---|
| arming, frames 0–32, 60 Hz | 1 754 mm | 1 428 mm | **1.23** |
| including the hold, frames 0–36 | 1 773 mm | 1 426 mm | 1.24 |
| per authored key, 30 Hz | 1 716 mm | 1 428 mm | 1.20 |

I cannot reach 1.82 for the head over the arming. Their figure may include the
release keys 18–22, or track the skull tip rather than bone 0. **The finding
survives in a different place**: the *tail* is where the wandering lives.

| station | 0 | 10 | 13 | **14** | **15** | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|
| path/net | 1.23 | 1.55 | 2.22 | **3.85** | **4.30** | 3.06 | 2.94 | 3.12 | 3.20 |

The tail tube walks **2 292 mm of path to end 716 mm from where it started**.
Station 15 walks 1 431 mm for a net 332 mm. That, not the head, is the part of
the animal that visibly goes back and forth.

---

# Ranked summary for the architect

| # | mechanism | contribution | where it lives |
|---|---|---|---|
| 1 | Non-uniform leg spacing: assembled→seating is 9.4x the pose-rate of leg 1 (191°/frame over 6.9 frames) | **dominant** — this IS the spasm | `kSpringAssembledHeading` / `kSpringSeatingHeading` / `kSpringArmSeatingAt` |
| 2 | `kSpringSquashLiftRoute` reverses vertically 6x; drives the whole body **and** the camera | **major** — half of all 63 reversals | `kSpringSquashLiftRoute`, `zixx_jump_track` |
| 3 | Camera welded to the raw root at 100 % incl. life wave | **major** — turns creature jitter into world jitter | `subject_zixx_jump_one`, `zixx_jump_track` |
| 4 | 60 Hz presentation is a linear chord ⇒ velocity staircase, 30 Hz impulse accelerations, all 63 reversals on odd frames | **major** | `c.interpolate` + `bake_presentation_midpoints` |
| 5 | Whole arming is 32 frames (0.533 s); hold is 4 frames | **major** — "too fast" is literal | `kSaltoCompressEndKey = 16`, `kSaltoCompressHoldEndKey = 18` |
| 6 | Head travels 1 346 mm back and ends 51 mm past the tail tip | **major** (owner's named symptom) | `kSpringCollapsedHeading`, `kSpringSeatingHeading` |
| 7 | Collapsed pose closes a 280° loop; blades become debris | moderate (readability) | `kSpringCollapsedHeading[10]`, `kSpringBladeSquashRise` |
| 8 | Declared self-press window = the visible fold, frames 13–21 | moderate | `kSpringCoilFormation*` — withdraw with the coil |
| 9 | Station 14 turns *exactly* 180° between two tables; unwrap direction is a coin-flip | latent fragility | `kSpringAssembledHeading[14]` = −8192, `kSpringSeatingHeading[14]` = 24576 |
| — | life waves, chain lag, landing bounce | **none** — do not touch | — |

The one-line version: **the clock is smooth and the route is not.** Direction 23
asks for four beats that read in order; the shipped build has one beat that
carries 59 % of the motion in 22 % of the frames, a support that bounces under
it, a camera bolted to the result, and a 30 Hz linear join that turns every key
boundary into a velocity step.
