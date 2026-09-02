# RECON 1 — the tail-balance clip's recipe, and why the spring does not have it

**Agent:** RECON 1 of 5 · read-only · 2026-09-02
**Lane:** `zixxtrixx-wholebody-s-spring-20260901`
**Question:** what does `build_balance()` actually do that makes the owner keep
pointing at it, and what of it transfers to the spring?

---

## Summary in one paragraph

The balance clip and the shipped spring move the head almost exactly the same
DISTANCE — 1408 mm vs 1523 mm. The balance takes **49 keys** to do it and never
exceeds **48 mm of station travel in a key**. The spring takes **16 keys** and
peaks at **642 mm in a key**. The balance's route is a single monotone
interpolation between exactly **two** pose tables, so its path length equals its
net displacement (ratio 1.01). The spring's route runs through **five** pose
tables with **18 per-station direction reversals**, so the head walks 2771 mm of
path to achieve 1523 mm of net displacement (ratio 1.82) — it literally goes out
past where it ends up and comes back, which is the owner's *"the snake head
shoots way past the tail"* stated as a number. The two travelling waves, the
per-station lag and the faded authority a previous pass ported over are all real
devices in `build_balance()`, but they are worth **≤3.7 deg/key of secondary
motion** and they were bolted onto a primary running at **71 deg/key**. That is
the CLAUDE.md crayon-grain failure exactly: mathematically present, visually
invisible. **The cause is pace and amplitude, not the secondary devices.**

---

## 1. THE CODE — what `build_balance()` is made of

`zhaozhou/tools/reel/zixxtrixx.h`, clip builder at **line 4336**, constants at
**lines 4027–4092**. Subject `zixxtrixx-balance` at `zhao_reel.cpp:4513`.

### 1.1 Frame budget

| | keys | 60 Hz frames | seconds |
|---|---|---|---|
| `kBalKeys` (whole clip) | 247 | 493 | 8.22 |
| gather `k0..28` | 28 | 56 | 0.93 |
| **rise `k28..77`** | **49** | **98** | **1.63** |
| balance/fight `k77..119` | 42 | 84 | 1.40 |
| lose `k119..140` | 21 | 42 | 0.70 |
| topple `k140..157` | 17 | 34 | 0.57 |
| aftermath `k157..188` | 31 | 62 | 1.03 |
| rise2 `k188..219` | 31 | 62 | 1.03 |
| settle `k219..246` | 27 | 54 | 0.90 |

Keys are 30 Hz; presentation is 60 Hz with authored midpoints. Verified against
the extracted webm: 493 frames.

**The single beat the owner is pointing at — the rise — is 49 keys long. The
spring's ENTIRE arming is 16 (`kSaltoCompressEndKey`).**

### 1.2 The primary: TWO pose tables, one monotone clock

```
const int32_t up = ss1000(f, 28, 77);            // line 4385
d  = kStanceSlope[k];
d += ((kBalRaisedSlope[k] - d) * up) / 1000;     // line 4411-4412
```

That is the whole primary. `kStanceSlope` (line 668) → `kBalRaisedSlope`
(line 4041), one smoothstep, **no per-station lag, no intermediate knot, no
reversal anywhere**. `up` has no `k` dependence — every station arrives on the
same clock.

`kBalRaisedSlope` is an authored **L, not a spear**: eleven upper segments
struggle toward vertical (values 13600–16800 a16 ≈ 75–92 deg), stations 12–13
make the weight-bearing elbow (9500, 4200), stations 14–18 lie near flat
(1350…900) as a five-segment supporting tail. `kBalSupport0 = 14`.

### 1.3 The life layer: two slow counter-travelling waves

Lines 4429–4450. Window `f >= 12 && f < 165`, stations `k < kBalSupport0` (0–13).

```
ph1 = f*(65536/47)  - k*5600  + 0       // kBalWobble  = 3900 a16 = 21.4 deg
ph2 = f*(65536/103) + k*3100  + 17000   // kBalWobble2 = 2400 a16 = 13.2 deg
```

| | wave 1 | wave 2 |
|---|---|---|
| period | 47 keys = **1.57 s** | 103 keys = **3.43 s** |
| amplitude | 3900 a16 = 21.4 deg | 2400 a16 = 13.2 deg |
| spatial lag | 4.02 keys/station | 4.87 keys/station |
| wavelengths over stations 0–13 | 1.20 | 0.66 |
| travel direction | head → tail | tail → head |
| **peak angular rate** | **2.86 deg/key** | **0.80 deg/key** |

Periods 47 and 103 are coprime, so the pair never repeats inside the clip — the
shape never looks looped. **Combined peak secondary rate: 3.67 deg/key.**

Three multiplicative gates on the wave:
* `live = 500 + (13-k)*500/13` — authority 1000 at the head fading to 500 at
  station 13, hard zero from station 14 (the ground support does not wobble).
* `struggle = 300 + 700*ss1000(f,30,77)/1000` — **the wave is never off**: it
  runs at 30% through the whole gather and ramps to 100% at the top.
* `(1000-over_k)` and `(1000-recover)` — killed by the topple and the get-up.

Effective peak secondary by station: **34.6 deg at station 0, 29.3 at station 4,
23.9 at station 8, 17.3 at station 13.** Mid-body, the secondary is as large as
the primary (station 8's primary change is only 29.7 deg) — that is why the
raised body reads as *wobbling*, not *posed*.

### 1.4 The other devices, and which beat each belongs to

| device | constant | value | beat it serves |
|---|---|---|---|
| breath | `kBalBreath` | 700 a16 = 3.85 deg, period 112 keys, stations 1–9 | all, damped 70% while standing |
| one-way lean | `kBalLeanA16` | 3000 a16 = 16.5 deg, `ss1000(f,122,142)`, tapered `(14-k)/14` | **lose** only |
| buckle lag | `kBalBuckleLagK` | **6 keys spread over 18 stations = 0.33 keys/station** | **topple** only |
| overshoot whip | `kBalOvershoot` | 2600 a16, off the last 3 joints | **topple** only |
| impact ripple | `kBalRippleAmp` | 2200 a16 on `kImpactEnv` + `kShockLagMk` | **aftermath** only |
| aftermath wave | `kBalAftermathWave` | peak 700 at station 3 | **aftermath** only |
| fin flare | `kBalFinFlare` | 2600 | rise/fight |
| root | `kBalFork[]` 20 keys | eases fork −255 mm over k45–77, holds to k140 | rise + fight |

**Note for the architect:** the *only* per-station lag in the entire clip is
`kBalBuckleLagK`, it is 0.33 keys per station, and it belongs to the FALL. **The
rise has no per-station lag at all.** Whatever the previous pass ported as "a
per-station arrival lag from the balance clip" was taken from the wrong beat and
was sub-frame in magnitude at source.

### 1.5 Committed velocity bound

`kBalMaxStationStepMm = 320` (line 4056) — the probe's hard gate on the balance
clip's **fastest 60 Hz station step**. The equivalent gate on the jump is
`kJumpMaxStationStepMm = 1300` (line 7314). **4.06x.** These are committed
numbers in the repo, not my measurements.

---

## 2. THE NUMBERS — balance rise vs spring arming

Method: I reconstructed both centrelines from the committed heading tables in
Python (`scratchpad/recon1/vel.py`), 19 segments of 160.5 mm, pinned at station
14 (`kSpringPlantSegment`), and walked the authored clocks — `ss1000(f,28,77)`
for the balance, `spring_arm_ease` through the four-knot arm route for the
spring. This is **not** the shipped root solve, so absolute world positions will
differ; the shape-change rates and ratios below are exact and are the point.

| | balance rise | spring arming |
|---|---|---|
| keys | **49** (1.63 s) | **16** (0.53 s) |
| pose tables traversed | **2** | **5** |
| net head travel | 1408 mm | 1523 mm |
| head **path** length | ~1420 mm | **2771 mm** |
| **path / net** | **1.01** | **1.82** |
| mean head speed | 29 mm/key | 95 mm/key |
| **peak station step** | **48 mm/key** (24 mm/tick) | **642 mm/key** (321 mm/tick) |
| peak / mean | 1.65 | 6.8 |
| largest net joint change | 83.7 deg (station 1) | **280.5 deg (station 10)** |
| peak joint rate (primary) | **2.6 deg/key** | **71 deg/key** |
| per-station direction reversals | **0** | **18** |

### The spring's per-key station step, key by key

```
 k 1 arm=   8    16 mm      k 9 arm= 572   657 mm
 k 2 arm=  36    55 mm      k10 arm= 659   651 mm
 k 3 arm=  80    87 mm      k11 arm= 745   297 mm
 k 4 arm= 144   126 mm      k12 arm= 834    62 mm
 k 5 arm= 225   153 mm      k13 arm= 906    54 mm
 k 6 arm= 312   110 mm      k14 arm= 959    44 mm
 k 7 arm= 398   118 mm      k15 arm= 990    27 mm
 k 8 arm= 486   670 mm      k16 arm=1000     9 mm
```

**Keys 8, 9 and 10 carry 1840 mm of head travel — more than the whole net
displacement of the arming — in one tenth of a second.** Then keys 12–16 carry
196 mm between them. The trapezoidal `spring_arm_ease` is doing its job on the
ARM PARAMETER and is completely defeated by the pose route: `kSpringArmAssembledAt
= 400` → `kSpringArmSeatingAt = 700` is 300 arm units of an *enormous* pose gap
crossed in 3.8 keys, while `700 → 1000` is 300 arm units of a nearly-identical
pose pair (`kSpringSeatingHeading` and `kSpringCollapsedHeading` differ on 3
stations only) crossed in 5 keys. **A uniform-speed clock over a wildly
non-uniform route is not a uniform-speed motion.** This is the single most
mechanically fixable fault I found.

### Where the reversals are

Per-station heading along `stance → absorb → assembled → seating → collapsed`:

```
 st   stance  absorb  assem  seating  collapsed   signs  path  net
  0     4.9    -4.0   -15.0    30.0    -10.0      --+-  104.9  -14.9
  1     6.9     2.0    -5.0    10.0     -4.0      --+-   40.9  -10.9
  6   117.6   117.0   114.0    86.0    110.0      .--+   55.6   -7.6
  7   138.4   138.0   135.0   106.0    131.0      .--+   57.4   -7.4
 10     0.0    -6.0    -4.0   280.5    280.5      -++.  292.5 +280.5
 11     1.2   -10.0    -8.0   213.5    213.5      -++.  234.7 +212.3
 12     1.0   -14.0   -12.0   182.0    182.0      -++.  211.0 +181.0
 13     1.7   -20.0   -16.0   180.0    180.0      -++.  221.7 +178.3
 14     6.4   -30.0   -45.0   135.0    142.0      --++  238.4 +135.5
 15     9.3   -40.0   -60.0   118.0    124.0      --++  253.3 +114.7
 16     8.2   -55.0   -70.0   106.0    106.0      --+.  254.2  +97.8
```

Station 0 travels **105 degrees of path to net −15 degrees**. Stations 10–16 all
reverse: they go *negative* into the absorb/assembled gather and then swing
180–280 degrees positive into the seating. **That reversal is the visible
"weird".** In the balance's rise every station goes one way, once.

---

## 3. THE SCREEN — beat by beat

Frames extracted from `Upheaval/website/public/renders/zixxtrixx-balance.webm`
(493 frames, 384×240, 60 Hz) to `scratchpad/recon1/bal/`; contact sheets
`bal_rise2.png`, `bal_fight2.png`. Jump from `zixxtrixx-jump-one.webm` (161
frames) → `jmp_arm2.png`, `jmp_seam.png`.

### 3.1 The balance, as it plays

* **k0–28 (0.93 s) — pure life layer.** The animal is the lying S. `up` is 0,
  `kBalFork` is 0. The *only* thing moving is the 30%-amplitude wobble (from k12)
  and the breath. Nothing "starts". A full second in which the animal is alive
  and has not yet decided anything. On the contact sheet, rows 1–2 (keys 0–36)
  are essentially the same picture with a drifting head.
* **k28–45 (0.57 s) — the front lobe opens.** The S starts to unroll and the
  belly presses. The fork is still at 0; nothing has lifted yet.
* **k45–77 (1.07 s) — the rise.** `kBalFork` eases the fork down 255 mm over
  k45→77 while `up` carries the eleven upper segments toward vertical. The animal
  erects into the J. It is one continuous rotation of the whole front over more
  than a second. Row 3→4 of the sheet is where the shape actually changes, and it
  changes by a small amount per frame at every step.
* **k77–119 (1.40 s) — the fight.** The primary is *finished* and parked. For
  1.4 seconds the ONLY motion is the two waves + breath, at ≤3.7 deg/key. The
  contact sheet rows 1–3 of `bal_fight2.png` are 80 keys in which the silhouette
  is recognisably the same J and never once identical. **This is the beat the
  owner calls "wobbly and organic".**
* **k119–140 (0.70 s) — the loss.** `kBalLeanA16` adds a one-way headward bend
  the corrections stop cancelling. It reads as losing, not as a new pose.
* **k140–157 (0.57 s) — the topple**, `over = ss1000²` so it accelerates, with
  the buckle lag and overshoot whip.
* **k157 impact**, ripple, stunned hold, get-up k188–219, exact loop at k246.

### 3.2 Legibility, measured (`scratchpad/recon1/leg.py`)

Silhouette pixel count and bounding-box fill, per frame:

| balance | key 0 | 20 | 40 | 60 | 80 | 110 | 140 |
|---|---|---|---|---|---|---|---|
| silhouette px | 1191 | 1118 | 1119 | 1070 | 1048 | 1031 | 1030 |
| bbox fill | 0.24 | 0.21 | 0.20 | **0.12** | 0.14 | 0.12 | 0.14 |

| jump arming | key 0 | 4 | 7 | 8 | 9 | **10** | 12 | 16 |
|---|---|---|---|---|---|---|---|---|
| silhouette px | 2384 | 2372 | 2326 | 1798 | 1423 | **1175** | 1365 | 1401 |
| bbox | 177×55 | 183×59 | 168×73 | 160×64 | 158×76 | **114×47** | 89×44 | 94×47 |
| bbox fill | 0.24 | 0.22 | 0.19 | 0.18 | 0.12 | 0.22 | **0.35** | **0.32** |

*(the two clips use different camera framing, so absolute px is not comparable
across them; the within-clip trend is the finding.)*

**The balance holds its silhouette pixel count within ±8% for 140 keys and its
fill DROPS as it rises — it becomes a more open, more readable curve.**
**The jump loses 51% of its visible pixels between key 7 and key 10** — that is
self-occlusion, the body wound onto itself — **and its bbox fill rises from 0.19
to 0.35**: from an open curve to a filled blob, in 3 keys. Its bounding box goes
from 168×73 to 89×44, i.e. **the animal shrinks to 32% of its screen area in
0.17 s.**

`jmp_seam.png` (keys 6→12) shows it plainly: keys 6–8 are still a legible S with
the fins up; at key 9–9.5 the whole rear sweeps *through* the body; by key 10 it
is a small green shrimp. The head is not "reading as too fast" — the animal stops
being a snake.

---

## 4. HONEST ASSESSMENT of the previous pass's "three devices"

| claimed device | actually in the balance? | is it what makes it work? |
|---|---|---|
| two slow travelling waves | **Yes** — `kBalWobble` 3900 / `kBalWobble2` 2400, periods 47 & 103 keys, counter-travelling | **No, not on its own.** Peak 3.67 deg/key against a spring primary of 71 deg/key — a 5% modulation on a motion that is already illegible. |
| per-station arrival lag | **Only in the topple**, `kBalBuckleLagK = 6` = **0.33 keys/station**. The rise has none. | **No.** Ported from the wrong beat, and sub-frame at source. |
| support-faded authority | **Yes** — `live` 1000→500 over stations 0–13, hard 0 from station 14 | **No, and it is actively wrong here.** It is an amplitude taper that *excludes the support from the wobble*. Direction 20 §6 and 22 §1 demand the rear JOIN the shape. Copying this device tapers the rear out of the very thing it is supposed to be doing. |

The devices are real. They are the balance's **life layer**. They are not its
motion. Adding a 3.7 deg/key secondary to a 71 deg/key primary is the CLAUDE.md
crayon-grain error verbatim: "narrower than the light rig's own range, so it was
mathematically present and visually invisible."

**The real causes, in order of magnitude:**

1. **The collapsed pose is too far away.** 280 degrees of net rotation on station
   10. The balance's largest is 84. No easing makes a 280-degree joint rotation
   read slowly inside a spring-length window.
2. **The route doubles back.** 18 reversals; path/net 1.82. The head goes out and
   returns — the owner's "shoots way past the tail".
3. **The route is non-uniform under a uniform clock.** Arm 400→700 crosses a
   ~250-degree pose gap in 3.8 keys; arm 700→1000 crosses almost nothing in 5.
4. **The pose ends illegible.** Bbox fill 0.35, 51% of pixels self-occluded.
5. **Only then**: 16 keys is too few, and there is no life layer during the hold.

---

## 5. THE TRANSFERABLE RECIPE

What a spring would have to do to feel like the balance clip:

### 5.1 Budget (hard numbers to design to)

* **Peak station step ≤ 100 mm/key (50 mm per 60 Hz tick).** The balance's own
  bound is 48 mm/key; a spring may legitimately be twice as urgent as an idle
  stunt. That is 6.4x tighter than the shipped 642 mm/key. Express it as a new
  gate constant beside `kJumpMaxStationStepMm`, e.g. `kSpringArmMaxStationStepMm
  = 100` at 30 Hz keys, applied over keys 0..`kSaltoCompressEndKey` only — the
  release keeps 1300.
* **Peak joint rate ≤ 6 deg/key** during arming (balance primary is 2.6, plus
  3.7 of secondary).
* **Largest net joint change ≤ ~90 deg** across the whole arming. The balance's
  is 84. This is the constraint that actually sizes the collapsed pose.
* **path/net ≤ 1.10.** The balance is 1.01.
* **Bounding-box fill ≤ 0.25 at every arming frame** and **silhouette pixel count
  within ±15% of the idle value**. `scratchpad/recon1/leg.py` is 20 lines and
  computes both from rendered frames; it is worth committing as a legibility
  probe. Both are comparison-side measurements, per the art law.

Given 90 deg of net joint change at ≤6 deg/key, the arming lands at **28–36
keys (0.93–1.20 s)**, i.e. `kSaltoCompressEndKey` roughly doubles. This is
consistent from three independent directions (head travel / balance mean speed;
joint budget / joint rate; the owner asking for slower three directions running).

### 5.2 Ordering (Direction 23's four beats mapped onto balance devices)

1. **Settle beat, ~4–6 keys, life layer only.** The balance spends 28 keys doing
   nothing but breathing before the stunt. Copy the *idea*, not the length: a few
   keys where `arm` is still ~0 and only the wobble runs, so the entry seam is
   velocity-continuous by construction rather than by ease-in fraction. This
   directly serves Direction 20 §3.
2. **"Become the S", ~16–20 keys, ONE monotone interpolation between exactly TWO
   pose tables.** Grounded → a single new whole-body S pose. Delete
   `kSpringAbsorbHeading` and `kSpringSeatingHeading` from the arming route, or
   place their knots so that **every station is monotone from grounded to the
   loaded pose**. Author the loaded pose so that its per-station deltas alternate
   in sign along the body — like `kBalRaisedSlope`'s
   `+77,+84,+66,+68,+44,+12,−39,−48,−30,+26,+81,+74,+51,+21,+1,−3,−2,+36,+68` —
   which is what makes it a whole-body S with a fading support region rather than
   a lobe plus a rail.
3. **"Compress the S", ~10–14 keys.** Head slightly back and slowly down, and
   everything descends with it. Again monotone, again one pose pair.
4. **Loaded beat, 2–4 keys, life layer only, not frozen.** `kSpringHoldLivingDrift
   Mm = 88` already exists; the balance's fight is the proof that a *parked
   primary plus a living secondary* is the most alive thing in the whole bank.
5. **Release unchanged** — 4 keys, `kSpringReleaseArm1/2/3`. Direction 20 §4 is
   explicit that only the arming is slow.

### 5.3 The life layer, ported honestly

Port `kBalWobble`/`kBalWobble2` verbatim in *character* — two counter-travelling
sines at coprime, slow periods, one ~1.5 s and one ~3.4 s, spatial lag ~4–5
keys/station, amplitudes ~20 and ~13 deg before gating — but:

* **Scale the periods to the new arming length**, not to key counts copied from a
  247-key clip. Over a 30-key arming, a 47-key wave delivers less than two thirds
  of a cycle: that reads as a drift, not a wobble. Use ~18 and ~40 keys to get
  the same *number of visible undulations per second of screen time*.
* **Do NOT copy `live`'s fade to zero at station 14.** For the spring the rear is
  the subject. Fade authority toward the actual ground-contact stations of the
  loaded pose, wherever those end up, and keep it non-zero everywhere else.
* **Keep the `struggle` floor.** The balance's wave is at 30% before anything
  starts and never switches on. A device that switches on is a seam.
* Keep the breath (`kBalBreath` 700, damped 70% under effort) — it costs nothing
  and it is why the still beats are not still.

### 5.4 Specific to balancing, do NOT port

`kBalBuckleLagK`, `kBalOvershoot`, `kBalLeanA16`, `kBalRippleAmp`,
`kBalAftermathWave`, `kBalImpactSink`, `kCorpseSlope` — all topple/impact
devices. The `kBalFork` hand-keyed root height curve is worth copying as a
*method* (`kSpringSquashLiftRoute` already is that method) but not its values.
`kBalFinFlare` is a balance-specific read.

---

## 6. What I could not determine

* **Exact shipped world velocities.** I did not build or run the committed probe
  (read-only lane). My mm figures come from a station-14-pinned reconstruction of
  the authored heading tables and exclude the shipped root solve
  (`spring_root_anchor_x`, `spring_root_offset`, `kSpringSupportCompensation`)
  and the presentation midpoints. **The ratios and the reversal counts are exact
  and table-derived; the absolute mm are indicative.** The two committed
  constants `kBalMaxStationStepMm = 320` and `kJumpMaxStationStepMm = 1300` are
  the authoritative version of the same comparison and agree with my direction.
* **Which bank is Direction 23's "the reference implementation".** Out of my
  scope and I did not do the archive comparison. Candidates on disk are the
  `archive-2026-08-28-*`, `archive-2026-08-29-v8-normal-*` and
  `ARCHIVE-GENERATION-{NINE..TWELVE}` sets in
  `Upheaval/creature/Zixxtrixx/`. Note the owner separately named **Archive
  Generation Eleven** as "closer" in Direction 21 §6.
* **Whether the collapsed pose can be made legible at all at Direction 22's
  "coil at its extreme".** My measurements say a coil that self-occludes 51% of
  the silhouette cannot read at 240p. Direction 23 withdraws the extreme coil, so
  I believe this is now moot, but if the architect keeps any of the wound loop it
  needs the fill-ratio check before it ships.
* **The salto/attack consumers.** `kSaltoCompressEndKey` is shared by the attack
  and every salto variant (`uses_default_shared_spring_timing`). Doubling it
  retimes them all. I did not scope that blast radius; RECON on the clip
  inventory should.

---

## Files examined

* `zhaozhou/tools/reel/zixxtrixx.h` — lines 102–250 (geometry), 620–1060 (stance,
  spring pose tables, timing), 2440–2600 (arm ease, release control),
  3950–4000 (`curve_mk`, `kImpactEnv`, `ss1000`), 4027–4092 (balance constants),
  4336–4553 (`build_balance`), 7185–7340 (jump plan, phases, motion sample)
* `zhaozhou/tools/reel/zhao_reel.cpp` — 4512–4532 (`subject_zixx_balance`)
* `zhaozhou/tools/reel/zixx_probe.cpp` — 716–870 (the balance gates)
* `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-20/21/22/23`
* `Upheaval/website/public/renders/zixxtrixx-balance.webm`, `zixxtrixx-jump-one.webm`

Scratch (not in repo):
`scratchpad/recon1/` — `vel.py`, `vel2.py`, `leg.py`, `bal/`, `jmp/`,
`bal_rise2.png`, `bal_fight2.png`, `jmp_arm2.png`, `jmp_seam.png`
