# RECON 4 — The Spring Machinery Map

**Run:** RUN-20260902-1816-zixxtrixx-slow-readable-s-spring
**Scope:** read-only. Nothing in either repo was modified.
**Purpose:** a reference to keep open while re-authoring the spring for
Direction 23 (become the S slowly, compress it slightly-backward and down,
compress, jump). Every number below is the CURRENT committed value in the lane
at `zixxtrixx-wholebody-s-spring-20260901`.

Primary file: `zhaozhou/tools/reel/zixxtrixx.h` (8138 lines).
Everything spring-related lives in three bands: **700–1030** (pose tables,
lifts, deform, declared allowances), **1900–2260** (route + life layer),
**2260–2800** (sampler, release, arming clock, midpoints).

**Angle unit throughout is angle16: 65536 = 360°, so 182.04 units = 1°.**
Amounts (`entry`, `squash`, `arm`, `authority`) are 1/1000. Lengths are mm.

---

## 0. The one-paragraph summary

One monotone parameter, **`arm` (0..1000)**, drives everything. `arm` is a
weighted sum of two legacy inputs `entry` and `squash`. A five-knot
Catmull-Rom spline (`spring_route_heading`) maps `arm` to a complete 19-segment
absolute heading pose. A separate authored route maps `(entry, squash)` to the
height of the planted support (station 14), from which root X/Y are *derived*
by walking the real quantised quaternion chain. A deform sidecar flattens and
spreads the cross-section as a function of `squash` alone. A "life layer" adds
two travelling waves and a per-station lag on top of the route. Presentation
half-keys are placed exactly halfway in `arm` so they sit ON the route.

---

## 1. Every knob that shapes the spring

### 1.1 The five pose tables — `kSpringGroundedHeading` … `kSpringCollapsedHeading`

These are **absolute headings of each of the 19 fixed-length spine segments**,
head (seg 0) to tail tip (seg 18), in angle16. They are NOT local joint angles
and NOT gains: joints are adjacent heading differences. Positive = nose-up /
counter-clockwise in the side view.

`zixxtrixx.h:743` `constexpr const auto& kSpringGroundedHeading = kStanceSlope;`
— **grounded is an alias for the idle stance table** (`kStanceSlope`, line 714).
Editing it changes the idle. Do not.

| seg | grounded (`kStanceSlope`) | absorb `:780` | assembled `:796` | seating `:808` | collapsed `:819` |
|----:|-----:|-----:|-----:|-----:|-----:|
| 0  |  900 / 4.9°   |  -728 / -4.0°  | -2731 / -15.0° |  5461 / 30.0°  | -1820 / -10.0° |
| 1  | 1265 / 7.0°   |   364 /  2.0°  |  -910 /  -5.0° |  1820 / 10.0°  |  -728 /  -4.0° |
| 2  | 2375 / 13.1°  |  1820 / 10.0°  |  1092 /   6.0° |   728 /  4.0°  |     0 /   0.0° |
| 3  | 4215 / 23.2°  |  4005 / 22.0°  |  3823 /  21.0° |  1456 /  8.0°  |  3277 /  18.0° |
| 4  | 6800 / 37.4°  |  6918 / 38.0°  |  7282 /  40.0° |  4369 / 24.0°  |  7646 /  42.0° |
| 5  | 14600 / 80.2° | 14563 / 80.0°  | 14382 /  79.0° | 11287 / 62.0°  | 14199 /  78.0° |
| 6  | 21400 / 117.6°| 21299 / 117.0° | 20753 / 114.0° | 15656 / 86.0°  | 20025 / 110.0° |
| 7  | 25200 / 138.4°| 25122 / 138.0° | 24576 / 135.0° | 19296 / 106.0° | 23848 / 131.0° |
| 8  | 20000 / 109.9°| 21845 / 120.0° | 28217 / 155.0° | 32040 / 176.0° | 32040 / 176.0° |
| 9  | 11600 / 63.7° | 14928 /  82.0° | 24576 / 135.0° | 37319 / 205.0° | 37319 / 205.0° |
| 10 |     0 /  0.0° | -1092 /  -6.0° |  -728 /  -4.0° | 51063 / 280.5° | 51063 / 280.5° |
| 11 |   221 /  1.2° | -1820 / -10.0° | -1456 /  -8.0° | 38865 / 213.5° | 38865 / 213.5° |
| 12 |   187 /  1.0° | -2549 / -14.0° | -2184 / -12.0° | 33132 / 182.0° | 33132 / 182.0° |
| 13 |   306 /  1.7° | -3641 / -20.0° | -2913 / -16.0° | 32768 / 180.0° | 32768 / 180.0° |
| 14 |  1173 /  6.4° | -5461 / -30.0° | -8192 / -45.0° | 24576 / 135.0° | 25848 / 142.0° |
| 15 |  1691 /  9.3° | -7282 / -40.0° |-10922 / -60.0° | 21481 / 118.0° | 22572 / 124.0° |
| 16 |  1496 /  8.2° |-10012 / -55.0° |-12743 / -70.0° | 19296 / 106.0° | 19296 / 106.0° |
| 17 | -5600 /-30.8° | -6918 / -38.0° | -8192 / -45.0° |  2184 /  12.0° |  2184 /  12.0° |
| 18 |-11400 /-62.6° | -8738 / -48.0° | -4551 / -25.0° |  1092 /   6.0° |  1092 /   6.0° |

Segment 14 is `kSpringPlantSegment` — the authored support.

**Facts an implementer needs from this table:**

* The grounded row is **generated**, not hand-typed. Segs 0–3 come from
  `front_slope(k)` (`:650`), a quadratic ramp from `kFrontSnoutSlopeA16 = 900`
  to `kFrontAnchorSlopeA16 = 6800` over `kFrontSegs = 4`, eased by
  `kFrontEaseQ = 1000`. Segs 11–16 are `kGroundSlope*A16 * ZIXX_GIRTH / 1000`
  with `ZIXX_GIRTH = 850` (`:195`). Changing girth moves the grounded run.
* **Seating and collapsed are equal on segments 8,9,10,11,12,13,16,17,18** and
  differ on 0–7 (the front's own fold) **and on 14 and 15** (25848 vs 24576,
  22572 vs 21481). The in-file comment claims the rear is "identical values to
  collapsed"; that is true of the tail but NOT of 14/15. Any change to that
  claim must be made in both the comment and the table.
* The collapsed rear carries the coil through the angle16 seam (280.5° at seg 10),
  which is exactly why `spring_unwrap` exists. **Never diff two pose tables with
  raw integer subtraction.**

### 1.2 The arming knots — where the tables sit on `arm`

| constant | line | value | meaning |
|---|---|---|---|
| (implicit) | — | 0 | grounded |
| `kSpringArmAbsorbAt` | 830 | **220** | absorb knot |
| `kSpringArmAssembledAt` | 831 | **400** | assembled knot; also the entry/squash split |
| `kSpringArmSeatingAt` | 834 | **700** | seating knot (rear finished, front still folding) |
| (implicit) | — | 1000 | collapsed |

`kSpringAbsorbProfile` (`:838`) = `220*1000/400` = **550**, derived, not free.

### 1.3 Timing constants (30 Hz authored keys)

| constant | line | value | note |
|---|---|---|---|
| `kSaltoSpringEntryEndKey` | 1035 | **12** | **inert in the sampler** — the shared clock uses `kSaltoCompressEndKey`. It survives only as a *probe gate input* (see §5). |
| `kSaltoCompressEndKey` | 1036 | **16** | arming completes here |
| `kSaltoCompressHoldEndKey` | 1037 | **18** | loaded hold ends |
| `kSaltoSpringReleasePoseKey` | 1038 | **22** | exact grounded S again |
| `kSaltoRigidReleaseEndKey` | 1039 | **24** | whole S has risen, before wheel gather |
| `kSaltoReleaseEndKey` | 1040 | **28** | airborne wheel complete |
| `kAtkCompressSliceFirstKey` | 1044 | 0 | |
| `kAtkCompressSliceLastKey` | 1045 | **17** (= HoldEnd−1) | |
| `kSaltoCoilPoseKey` | 1047 | **29** (= ReleaseEnd+1) | |
| `kSaltoUnrollStartKey / EndKey` | 1048/49 | 52 / 60 | |

Keys are 30 Hz; each key is two 60 Hz presentation ticks (even tick = key,
odd = midpoint). **16 keys of arming = 0.533 s.** That is the "too fast" the
owner keeps reporting, and it is a timing constant, not a pose problem.

### 1.4 Ramp shape

| constant | line | value |
|---|---|---|
| `kSpringArmEaseInFrac` | 2532 | **300** |
| `kSpringArmEaseOutFrac` | 2533 | **260** |

`spring_arm_ease` (`:2535`) is a **trapezoidal speed** profile, not a
smoothstep: accelerate over the first 30%, one constant speed through the
middle, decelerate over the last 26%. Deliberately chosen so the middle of the
build reads as steady winding.

### 1.5 Support / plant

| constant | line | value | units |
|---|---|---|---|
| `kSpringPlantSegment` | 842 | 14 | segment index |
| `kSpringDeclaredBiteMm` | 841 | 34 | mm, resting bite |
| `kSpringDeclaredLoadedBiteMm` | 919 | 160 | mm, deepest permitted loaded press |
| `kSpringSupportCompensation` | 849 | 1000 | 1/1000. 1000 = honest plant. 0 reproduces the rejected "skull pinned, body swings" build. **Owner knob.** |
| `kSpringJumpRootLiftMm` | 991 | 0 | no clearance hop during entry |
| `kSpringCompressionDepth` | 840 | 1000 | **DEAD — defined, never referenced** |
| `kSpringBridgeEntryProfile` | 2471 | 1000 | **DEAD — defined, never referenced** |

Entry-side support route `kSpringOpenSupportLift` (`:979`), lift in mm above the
station's grounded baseline, keyed on `entry`. **Breakpoints must stay sorted
by entry** — a previous build broke by moving the absorb breakpoint out of order.

| entry | lift mm | constant |
|---:|---:|---|
| 0 | 0 | — |
| 40 | −6 | `kSpringVeryEarly*` (860/877) |
| 140 | −10 | `kSpringEarly*` (861/878) |
| 340 | −2 | `kSpringMiddle*` (862/879) |
| 550 | 52 | `kSpringAbsorbProfile` / `kSpringAbsorbSupportLiftMm` (880) |
| 850 | 35 | `kSpringKey5*` (863/881) |
| 1000 | 25 | `kSpringAssembledSupportLiftMm` (882) |

Squash-side route `kSpringSquashLiftRoute` (`:898`), keyed on `squash`:

| squash | lift mm | authored intent |
|---:|---:|---|
| 150 | −40 | belly stays down while rear rises |
| 300 | 400 | press rolls forward, body rocks onto the foot |
| 460 | 470 | whip apex — climb onto the forming coil |
| 620 | 276 | whip lands, pad takes the ground |
| 790 | 300 | head travels back over the seated coil |
| 840 | 365 | deepest transit press |
| 920 | 282 | easing toward the bow |
| 1000 | 287 | `kSpringCollapsedSupportLiftMm` (883) — the bow onto the chin rest |

### 1.6 Deform, blades, head attitude

| constant | line | value | controls |
|---|---|---|---|
| `kSpringBodyFlattenQ16` | 1001 | **31500** (~48% vertical contraction) | Q0.16 delta from identity on the contracted axis |
| `kSpringBodySpreadQ16` | 1002 | **13800** (~21%) | Q0.16 expansion on the two perpendicular lanes |
| `kSpringSkullDeformStrength` | 1003 | 64 | 0..255 local authority |
| `kSpringThroatDeformStrength` | 1004 | 188 | |
| `kSpringBodyDeformStrength` | 1005 | 255 | |
| `kSpringTailDeformStrength` | 1006 | 210 | |
| `kSpringBodyStrengthRampStations` | 1007 | 4 | |
| `kSpringTailStrengthRampStations` | 1008 | 6 | |
| `kSpringSqueezeDeformLead` | 1943 | **340** | squash is boosted by this before sampling — the section flattens AHEAD of the fold |
| `kSpringJumpHeadAttitude` | 1009 | 700 | head attitude at full entry (a16) |
| `kSpringHeadAttitude` | 1015 | **7500** (~41°) | head attitude at full squash — braces the AIM out of the dirt |
| `kSpringBladeFlare` | 1016 | **−1500** | fan CLOSES with squash |
| `kSpringBladeSquashRise` | 1021 | **9500** | fan sweeps UP with squash (rooster tail) |

### 1.7 The life layer (2104–2250)

Three named devices copied from the balance clip: two slow incommensurate
travelling waves, a per-station arrival lag, and an envelope that opens and
closes so grounded and collapsed stay exact.

| constant | line | value | meaning |
|---|---|---|---|
| `kSpringNoLife` | 2104 | −1000000 | sentinel: "this caller has no clock" |
| `kSpringChainLag` / `…LagPeak` | 2107/2110 | 165 / 500 | 1/1000 of arm; station 0 on time, tail this far behind; peak at arm 500 |
| `kSpringWobble` / `…PeriodKeys` / `…StationPhase` | 2113-2115 | 380 / 23 / −4200 | wave 1 |
| `kSpringWobble2` / `…PeriodKeys` / `…StationPhase` | 2116-2118 | 280 / 51 / 2600 | wave 2, period incommensurate with 23 on purpose |
| `kSpringLifeRiseAt` / `…FullAt` | 2122/23 | 120 / 380 | envelope opens across these arm values |
| `kSpringLifeSupport0` / `…TailShare` | 2127/28 | 15 / 260 | stations ≥15 get the tail share only |
| `kSpringAirCoilTaper` / `kSpringAirWobble` | 2207/2210 | **0 / 0** | airborne layer disabled |
| `kSpringHoldLivingDriftMm` | 978 | **88** | how far the hold may quiver (probe gate) |

Setting the wobble amplitudes to zero returns the plain route exactly. **This is
the "organic like the balance clip" lever the owner keeps asking for**, and its
amplitudes were *damped* in the rejected Direction-22 pass (750/520 → 500/360,
now 380/280).

### 1.8 Declared allowances (owner-ruled 2026-09-02)

| constant | line | value |
|---|---|---|
| `kSpringCoilFormationWindBeginTick` | 942 | 13 (key 6.5) |
| `kSpringCoilFormationWindEndTick` | 943 | 21 (key 10.5) |
| `kSpringCoilFormationUnwindBeginTick` | 944 | 38 (key 19) |
| `kSpringCoilFormationUnwindEndTick` | 945 | 40 (key 20) |
| `kSpringCoilFormationPressFullMm` | 954 | 52 |
| `kSpringCoilFormationPressMicroMm` | 955 | 96 |
| `kSpringCoilFormationEchoBeginTick` | 964 | 13 (key 6.5) |
| `kSpringCoilFormationEchoEndTick` | 965 | 27 (key 13.5) |
| `kSpringCoilFormationHoverEchoMm` | 966 | 8 |

These declare a permitted self-press while the coil forms, and a permitted
few-mm ground hover on the retimed grid. **The windows are absolute ticks.**
See §5 for why that is a trap.

---

## 2. The route machinery — how a presentation frame becomes a pose

```
presentation tick (60 Hz)
  even tick = authored key k          odd tick = midpoint at k + 0.5
        |                                     |
        v                                     v
 spring_shared_arm_amount(k)          spring_owned_entry_midpoint(k)  [entry side]
   key 0            -> 0                or kSpringReleaseMidpointControl [release side]
   key < 16         -> spring_arm_ease(k*1000/16)        both take the arm exactly
   key 16..18       -> 1000                              HALFWAY between the two
   key 19..22       -> kSpringReleaseIntegerControl[k-18] integer keys' arms
   key > 22         -> 0
        |
        +--> spring_shared_entry_amount(k)  = min(arm*1000/400, 1000)
        +--> spring_shared_squash_amount(k) = max(0,(arm-400)*1000/600)
        |
        v
 spring_arm_amount(entry, squash) = entry*400/1000 + squash*600/1000   [round trip]
        |
        v  (per segment k)
 spring_station_arm(k, arm)      <- kSpringChainLag / kSpringChainLagPeak
        |
        v
 spring_route_heading(k, station_arm)                                  <- THE SPLINE
   knots t = {0, 220, 400, 700, 1000}
   p    = {grounded, absorb, assembled, seating, collapsed}, each unwrapped
          against the previous with spring_unwrap (shortest physical arc)
   non-uniform Catmull-Rom; centred tangents inside, one-sided at the ends
   => arm 0 and arm 1000 are BIT-EXACT; velocity is continuous at 220/400/700
        |
        + spring_life_wave(k, arm, life_key_mk)      <- two waves + envelope
        |
        v
 * authority / 1000                     (curl/wheel fades the S out)
        |
        v
 quat_z(heading[k] - heading[k-1])  -> g.q[kBSpine0 + k]
        |
        v
 spring_anchor_offset(): walk the REAL quantised chain to segment 14,
   compare against the same walk on the grounded baseline,
   add spring_support_target_y(entry, squash)
     = spring_root_drop(squash)            (-34 mm * smoothed squash)
     + spring_support_surface_lift(entry, squash)   (the two lift routes)
     + kSpringJumpRootLiftMm * smooth(entry)         (currently 0)
   convert fx16-metres -> mm  ( *1000 >> 16 ), scale by kSpringSupportCompensation
        |
        v
 root x/y for the frame.  Head motion is a CONSEQUENCE of articulating around
 the plant — it is never authored directly.
```

**Where a discontinuity can be introduced:**

1. **Off-route half keys.** Any midpoint whose `(entry, squash)` is not exactly
   halfway in `arm` between its neighbours produces a 30 Hz judder. This was the
   published fault. `spring_owned_entry_midpoint` and
   `kSpringReleaseMidpointControl` both compute the halfway arm explicitly —
   keep it that way. The `heading_override` field of
   `SpringReleaseMidpointControl` (`:2444`) still exists and is the loaded gun:
   using it re-imposes an off-route pose.
2. **The life layer sampled at the wrong clock.** `author_spring_midpoint_pose`
   passes `key*1000 + 500`. Sampling a midpoint at `key*1000` makes it disagree
   with both neighbours.
3. **Raw-integer interpolation through the angle16 seam.** Guarded by
   `spring_unwrap` and `spring_lerp_heading`; any new code that lerps two pose
   tables directly reintroduces it.
4. **A big per-key `arm` step.** Currently key 18→19 drops `arm` by **341** in
   one key (see the schedule below). That is by far the largest step in the
   action and it is the release.

**Per-key arming schedule (computed from the committed constants):**

| key | arm | Δarm | entry | squash | support lift mm | phase |
|---:|---:|---:|---:|---:|---:|---|
| 0 | 0 | 0 | 0 | 0 | 0 | entry |
| 1 | 8 | 8 | 20 | 0 | −3 | entry |
| 2 | 36 | 28 | 90 | 0 | −8 | entry |
| 3 | 80 | 44 | 200 | 0 | −8 | entry |
| 4 | 144 | 64 | 360 | 0 | 3 | entry |
| 5 | 225 | 81 | 562 | 0 | 51 | entry (absorb knot ≈ here) |
| 6 | 312 | 87 | 780 | 0 | 39 | entry |
| 7 | 398 | 86 | 995 | 0 | 25 | entry (assembled knot falls 7→8) |
| 8 | 486 | 88 | 1000 | 143 | −37 | squeeze |
| 9 | 572 | 86 | 1000 | 286 | 359 | squeeze |
| 10 | 659 | 87 | 1000 | 431 | 457 | squeeze |
| 11 | 745 | 86 | 1000 | 575 | 331 | squeeze (seating knot 10→11) |
| 12 | 834 | 89 | 1000 | 723 | 291 | squeeze |
| 13 | 906 | 72 | 1000 | 843 | 362 | squeeze (ease-out begins) |
| 14 | 959 | 53 | 1000 | 931 | 283 | squeeze |
| 15 | 990 | 31 | 1000 | 983 | 286 | squeeze |
| 16 | 1000 | 10 | 1000 | 1000 | 287 | LOADED |
| 17 | 1000 | 0 | 1000 | 1000 | 287 | LOADED |
| 18 | 1000 | 0 | 1000 | 1000 | 287 | LOADED |
| 19 | 659 | **−341** | 1000 | 433 | 458 | release |
| 20 | 380 | −279 | 950 | 0 | 28 | release |
| 21 | 160 | −220 | 400 | 0 | 13 | release |
| 22 | 0 | −160 | 0 | 0 | 0 | grounded S |

Owned entry-side midpoints: **key 1.5** (arm 22) and **key 4.5** (arm 184),
`kSpringEarlyEntryOwnedMidpointKey = 1`, `kSpringEntryOwnedMidpointKey = 4`
(`:2510-2511`). Release midpoint arms: 830, 520, 270, 80.

**The old jerks and how they are handled now.**

* *idle → compression.* Fixed by `spring_arm_ease`'s soft in-ramp (300/1000)
  and by making grounded (`arm = 0`) an exact one-sided spline endpoint. Δarm at
  key 1 is 8. This seam is currently clean.
* *interior dead stops.* The rejected build used three independent smoothstep
  legs, so every station's velocity hit zero at absorb and at assembled.
  Replaced by the single non-uniform Catmull-Rom. **If you add or move a knot,
  you must keep the tangent computation right — the spline is what buys C1.**
* *landing → idle.* The release decelerates through four arms (660/380/160/0)
  and lands bit-exact on the grounded table, and the probe checks exactness
  to ≤1 mm.
* *stall-then-slam.* `spring_arm_entry` / `spring_arm_squash` (`:2461/2466`)
  were introduced because release arm 660 sits INSIDE the squeeze half; a
  clamped conversion made key 19 slam 400 mm.

**`kSpringMiddlePoseKey` (= 2) is live-but-inert.** It is still read at
`:3217`, `:3303`, `:3633`, `:6900` to set `use_authored_middle_pose`, but both
`spring_entry_heading` and `spring_profile_slope` take that flag as an
**unnamed, unused parameter**. It is a no-op path kept for call-site
compatibility. Do not reactivate it — that override is precisely the off-route
key that caused the 30 Hz judder.

---

## 3. The consumers

### 3.1 Slots that play the spring

| slot | name | line | built by |
|---:|---|---|---|
| 3 | the primary/golden attack (monolithic) | — | `build_attack(false, …)` |
| 10 | `kSlotAtkCompress` | 5680 | slice of `build_attack(true,…)` keys 0..17 |
| 11 | `kSlotAtkRelease` | 5681 | slice keys 17..29 |
| 12 | `kSlotAtkCoil` | 5682 | **duplicate of key 29**, looping |
| 13 | `kSlotAtkUnroll` | 5683 | slice 52..60 |
| 15 | `kSlotAtkStick` | 5685 | slice 74..75 |
| 17 | `kSlotAtkRecover` | 5687 | slice 224..239 |
| 33 | `kSlotAtkDummy` | 6729 | `build_attack_variant` + `zixx_plan_attack(4600,350,0,0)` |
| 34 | `kSlotAtkFly` | 6730 | variant, `zixx_plan_attack(3800,3200,0,0)` |
| 35 | `kSlotAtkSix` | 6731 | variant, six saltos |
| 46 | `kSlotJumpOne` | 6732 | `build_jump(zixx_jump_plan(46,1))` |
| 47 | `kSlotJumpMulti` | 6733 | `build_jump(zixx_jump_plan(47,3))` |
| 48 | `kSlotAtkNine` | 6734 | variant, nine saltos |

Frame counts: slot 3 = `kAttackKeys` = **240**. Slots 46/47 = **81** keys
(22 arm + 38 flight + 6 landing + 14 settle + 1). The four planned variants have
no fixed count — `zixx_attack_variant_phases` (`:6777`) sums
`compress_keys(16) + compress_hold_keys(2) + release_keys(4)` plus
target-distance-dependent `coil_keys / unroll_keys / plunge_keys` plus the fixed
outcome tail (`kAtkTargetHoldKeys=14`, `kAtkGroundHoldKeys=8`,
`kAtkExtractKeys=8`, `kAtkRecoilKeys=12`, `kAtkOutcomeDropKeys=16`,
`kAtkOutcomeSettleKeys=10`, `:6739-6744`). Showcase targets are hardcoded in
`zixx_variant_plan` (`:7073-7090`): dummy `(4600,350)`, fly `(3800,3200)`,
six `(5200,0)` spin 6000, nine `(8500,350)` spin 9000 apex 24000.

### 3.2 The code paths — there are THREE, not two

* **`build_attack(bool choreo, …)`** (`:3158`) bakes the whole monolithic
  240-key clip. Per key it evaluates the curve tables
  (`kAtkCurl/kAtkAuth/kAtkSpin/kAtkLift/kAtkFwd`) plus
  `entry = spring_shared_entry_amount(f)`, `pre = spring_shared_squash_amount(f)`
  into `apply_spring_stance` (`:3218`). Root: for `f <= kSaltoSpringReleasePoseKey`
  it derives from the **actual baked quaternions** via
  `spring_root_from_quats_raw` + `spring_support_target_y` (contact-accurate);
  after that it uses the formula path `spring_root_anchor_x` + `pre_drop`.
  * `choreo = false` (`:7995`) → slot 3, full root baked.
  * `choreo = true` (`:8010`, and `zixx_choreo.cpp:58`) → **local body only**:
    every root channel and the bone-0 spin are zeroed (`:3274`). This is what
    gets sliced into the phase vocabulary (slots 10–17) and what
    `build_attack_variant` uses as its `local` source.
* **`attack_choreo_sample(int key)`** (`:3291`) is **root-only** — no
  quaternions, no `Rig`. It re-derives `x_mm / y_mm / theta` from the same
  curves plus the same `spring_shared_*` schedule. **It is not the variant
  bake path.** It is used live only by `zixx_plan_sample` when
  `p.preset_golden` (the golden trajectory), and by the two proofs:
  `zixx_choreo.cpp:79` (recompose local pose × instance transform and diff
  against the baked world stations, tolerance `kTolMm = 12`, `:108`) and
  `zixx_planner.cpp:52`.
* **`build_attack_variant`** (`:6842`) and **`build_jump`** (`:7533`) are the
  third path. They recompute entry/squash from the plan
  (`zixx_plan_spring_entry_amount` / `zixx_plan_spring_amount`, `:3581/:3598`)
  and derive root the same contact-accurate way (`:7024-7031` attacks,
  `:7587-7592` jumps). For **default timing** these short-circuit straight to
  `spring_shared_entry_amount` / `spring_shared_squash_amount`, which is what
  makes the bit-exact parity gate pass.

### 3.3 The plan defaults — the drift that actually shipped

`zc::AttackPlan` (`reference/include/zref/zref_creature.hpp:962`) declares
**engine-side defaults `compress_keys = 12, compress_hold_keys = 6,
release_keys = 4`** — the schedule Direction 20 rejected. `zixx_plan_attack`
(`:3409`) therefore *overwrites all three* immediately:

```
p.compress_keys      = kSaltoCompressEndKey;                        // 16
p.compress_hold_keys = kSaltoCompressHoldEndKey - kSaltoCompressEndKey;  // 2
p.release_keys       = kSpringReleaseMidpointCount;                 // 4
```

`zixx::JumpPlan` (`:7184`) does the same in its member initialisers — except
that `release_keys = 4` is a **raw literal**, not `kSpringReleaseMidpointCount`.

**`uses_default_shared_spring_timing`** exists in TWO versions —
`:3485` for `zc::AttackPlan` and `:7324` for `zixx::JumpPlan` — with identical
bodies. Both must be kept in step.

**The release-curve algorithm is also hand-duplicated**:
`zixx_plan_spring_release_assembled_key / _absorb_key / _entry / _squash`
(`:3509-3579`, AttackPlan) and `zixx_jump_release_assembled_key / _absorb_key /
_entry / _squash` (`:7330-7391`, JumpPlan) are twins of the same algorithm with
the same `kSpringAbsorbProfile` and `spring_smooth_amount` calls. They fire only
for **non-default** timing. Changing the retiming *algorithm* (as opposed to the
timing constants) requires editing both copies.

### 3.4 What must change together

| if you change… | you must also change… |
|---|---|
| `kSaltoCompressEndKey` / `kSaltoCompressHoldEndKey` | nothing in `zixx_plan_attack` or `JumpPlan` (both derive) — **but** the probe's phase-envelope gate (§5), the coil-formation tick windows (§1.8), and the hard-coded `17` / `29` in the slice + seam table (§4) |
| `kSaltoSpringReleasePoseKey` / `kSaltoRigidReleaseEndKey` / `kSaltoReleaseEndKey` | `kSaltoCoilPoseKey` follows automatically; the literal `29` in `duplicate_pose_clip(atk_local, kSlotAtkCoil, 29)` and `slice_clip(…, kSlotAtkRelease, 17, 29)` does **not** |
| `kSaltoCoilPoseKey` or `kSaltoUnrollStartKey` (moving the 29↔52 stride off 23) | `kSpringAirWobblePeriodKeys` — see the prime-number contract in §4 |
| `kSpringReleaseMidpointCount` | `JumpPlan::release_keys` (`:7191`) is a **bare literal 4**, not the constant. `zixx_plan_attack` uses the constant; JumpPlan does not |
| the spring key schedule at all | `zc::AttackPlan`'s engine-side defaults in `reference/include/zref/zref_creature.hpp:962` still read the rejected **12 / 6 / 4**. Harmless only because `zixx_plan_attack` overwrites all three; a future engine-side caller that default-constructs an `AttackPlan` gets the rejected schedule |
| the release *algorithm* (not its constants) | both `zixx_plan_spring_release_*` (`:3509`) and `zixx_jump_release_*` (`:7330`), and both `uses_default_shared_spring_timing` overloads |
| any of the five pose tables | re-run the probe's intersection, bite, lateral-span and cross-section gates; all consumers rebuild from the same tables so parity is automatic |
| `kSpringReleaseArm1/2/3` | `kSpringReleaseIntegerControl` and `kSpringReleaseMidpointControl` derive; `kSpringReleaseMidpointCount` must stay equal to `p.release_keys` in **both** plan types |
| `kSpringBodyFlattenQ16` / `kSpringBodySpreadQ16` | the probe's radial-ratio band (490..570 body, 850..925 head) and the declared press/bite numbers, since more flatten changes what "one flattened tube radius" means |
| the deform strength table | the shared full/micro metadata gate — micro-LOD vertices sharing a bind source must carry byte-identical deform metadata |
| `kSpringArmAssembledAt` | it is BOTH the assembled spline knot AND the entry/squash split. Moving it reparameterises the whole clock and shifts which keys are "entry" vs "squeeze". `kSpringAbsorbProfile` and `kSpringBridgeEntryProfile` derive from it. |

---

## 4. The seam contracts

The mechanism is `zc::SeamPair{slot_a, key_a, slot_b, key_b}`
(`zref_creature.hpp:300`) stored on `ClipBank::seams`, **enforced at compile
time in `compile_creature`** (`creature_core.cpp:987-1024`), which byte-compares
quats, root **and the deform sample** and fails the whole creature compile with

```
phase seam mismatch (C2): slot %u key %u != slot %u key %u (bone %u)
```

The declared table lives at **`zixxtrixx.h:8026-8039`**.

| must equal | to | why |
|---|---|---|
| `kSlotAtkCompress` key 17 | `kSlotAtkRelease` key 0 | both are attack key 17 |
| `kSlotAtkRelease` key 12 | `kSlotAtkCoil` key 0 | both are attack key 29 |
| `kSlotAtkCoil` key 0 | `kSlotAtkCoil` key 1 | **the loop**: coil is a two-frame hold duplicated from one pose |
| `kSlotAtkCoil` key 1 | `kSlotAtkUnroll` key 0 | attack key 52 must equal key 29's pose |
| `kSlotAtkUnroll` key 8 | `kSlotAtkSpearFlex` key 0 | |
| `kSlotAtkSpearFlex` key 0 | `kSlotAtkSpearFlex` key 9 | loop |
| `kSlotAtkSpearFlex` key 0 | `kSlotAtkStick` key 0 | |
| `kSlotAtkStick` key 0 | `kSlotAtkStick` key 1 | loop |
| `kSlotAtkStick` key 1 | `kSlotAtkAirHit` key 0 | |
| `kSlotAtkAirHit` key 0 | `kSlotAtkAirHit` key 11 | loop |
| `kSlotAtkAirHit` key 11 | `kSlotAtkRecover` key 0 | |
| `kSlotAtkRecover` key 15 | `kSlotAtkCompress` key 0 | **closes the cycle back onto the grounded S** |

`kSlotAtkCoil` is created by `duplicate_pose_clip(atk_local, kSlotAtkCoil, 29)`
(`:8020`) — both its frames are attack key **29** (`kSaltoCoilPoseKey`), so the
loop identity is structural, not asserted. The *cross-clip* identity is not:
because coil key 1 must equal unroll key 0, **attack key 29 and attack key 52
must be the same pose**.

> **THE PRIME-NUMBER CONTRACT.** `zixxtrixx.h:2211-2220` records this
> explicitly. 52 − 29 = **23**, so any free-running clock layered on the
> airborne body must be phase-locked to a 23-key stride or keys 29 and 52
> diverge and the creature fails to compile with
> `"phase seam mismatch (C2): slot 12 key 1 != slot 13 key 0"`.
> `kSpringAirWobblePeriodKeys = 23` for exactly this reason, and the comment
> notes it **may only change to another divisor of 23 — which, 23 being prime,
> means 23 or 1.** If a re-author moves `kSaltoUnrollStartKey` or
> `kSaltoCoilPoseKey`, that stride changes and this constant must change with it.

What is otherwise fragile is that **29, 17, 12, 52, 60, 74, 75, 224 and 239 are
typed as raw literals** in `:8012-8025` and again in the seam table, while
`kAtkCompressSliceLastKey` on the same lines is *derived* from
`kSaltoCompressHoldEndKey`.

> **THE LANDMINE.** Slow the arming by raising `kSaltoCompressHoldEndKey` and
> `kAtkCompressSliceLastKey` moves with it, but `slice_clip(atk_local,
> kSlotAtkRelease, 17, 29)` and `{kSlotAtkCompress, 17, kSlotAtkRelease, 0}`
> stay at 17. The compress slice then ends somewhere else and the release slice
> starts mid-compression. **Retiming the anticipation means editing lines
> 8012–8039 by hand.** Convert those literals to the named constants in the same
> edit.

Beyond the compile-enforced seams, the probe additionally requires **bit-exact
parity** of the spring across consumers: all five integer + four midpoint
release keys, and all 45 pre-lift samples (ticks 0..2×22), must be
byte-identical between the monolithic slot 3 and the release slice, dummy
attack, flying attack, six-salto, one-turn jump, multi-turn jump and nine-salto.
Failure strings: `"spring release timing/silhouette drifted across consumers"`
and `"whole pre-lift spring drifted across complete consumers"`.

Clip-loop endpoint equality is also asserted for balance, slow taunt, jump
(key 0 vs `ph.last_key`), standalone air hit, generic hit and all four
directional hits.

Three further compiler-enforced seams live outside the attack family
(`:8106-8108` and `:8055`): knockdown last key → get-up key 0; hit-floor last
key → get-up key 0; death last key → corpse key 0.

**Not seam-checked, by design:** slot 3, the four planned variants and both
jump slots are self-contained monolithic bakes with no cross-slot key pair. Their
only equivalent obligation is the tolerance-based proof in `zixx_choreo.cpp` /
`zixx_planner.cpp` (12 mm), plus the probe's bit-exact parity gates above.

---

## 5. The probe

`zhaozhou/tools/reel/zixx_probe.cpp` (3590 lines). Failure = `require(...)`
with a printed string.

### 5.1 Gates that encode REAL LAWS — keep them, fix the motion

| gate | line | contract |
|---|---:|---|
| `"spring squash begins before full-tail entry is complete"` | 1296 | ordering only: `squash>0 ⇒ entry==1000`; squash reaches 1000; both zero at key 0 |
| `"spring body runs intersect outside the declared coil-formation windows"` | 1671 | real triangle–triangle test, station gap ≥7, every tick to `2×kSaltoRigidReleaseEndKey`, both LOD rungs. Any hit outside the two declared windows fails unconditionally |
| `"spring left its accepted authored full/micro ground-bite envelope"` | 1719 | bite ∈ [−160, −34] mm through the hold, both rungs |
| `"spring station-14 support left its authored per-sample path"` | 1860 | support X/Z drift ≤1 mm, target error ≤1 mm vs the same authored formula |
| `"spring no longer releases and rises as one intact S before coiling"` (shape half) | 1496 | release + rigid-air shape error ≤1 mm — bit-near-exact return to grounded |
| `"deformation sidecar lost exact identity outside spring frames"` | 1525 | deform must be `{0,0}` outside the authorised window |
| `"spring route jumps, or a station stalls at an interior pose"` | 2634 | walks all 1000 arm values: no station may stall (`route_min_move > 0`) and no seam step > 256 a16 per 1/1000 arm |
| `"jump turn count/wrap is not exact and monotonic"` | 3238 | theta never decreases and lands on an exact multiple of 65536 — the no-rollover law |
| `"jump full/micro surface … touches terrain in the undeclared flight core"` | 3290 | strictly above ground between `launch_key+2` and `landing_key−2` |
| all seam / parity / provenance gates | 2298, 2845, 2987, 3094, 3193 | byte equality where bytes are meant to be shared |
| `"…saturated fixed-point arithmetic"` | 3544 | fixed-point must never clip at the nine-salto extreme |
| landing phase bites (impact/handoff/settle) | 3290 | `kJumpLandingLoadedBiteMm=125`, `kJumpImpactMinBiteMm=15` |

### 5.2 Gates DERIVED FROM THE CURRENT MOTION — these will fail legitimately

**Read this list before touching a constant. Each of these is a recorded
regression band, not a physical law. Failing one means "the motion changed";
re-record it after the owner has accepted the new motion by eye.**

| gate | line | current band | what breaks it |
|---|---:|---|---|
| `"spring phase timing left the accepted entry/hold/release envelope"` | 1355 | `kSaltoSpringEntryEndKey ∈ [10,14]`; `CompressEnd − EntryEnd ∈ [3,6]`; `HoldEnd − CompressEnd ∈ [2,4]`; `ReleasePose − HoldEnd ∈ [3,5]`; `RigidEnd − ReleasePose ∈ [2,3]`; `ReleaseEnd − RigidEnd ∈ [3,5]` | **This is THE gate Direction 23 will trip.** "Slower and more deliberate" means more keys in the arming, and this band pins the key counts to the current schedule. Note `kSaltoSpringEntryEndKey` is otherwise inert — it exists almost solely to feed this gate. |
| `"…lost its ordered assembled-to-collapsed planar S"` (lateral half) | 1460 | `kSpringTrunkLateralSpanMaxMm = 30`, `kSpringWholeTailLateralSpanMaxMm = 45` (probe.cpp:34-35) | any change to how far the S swings out of plane |
| `"spring lost its readable, genuinely held maximum brace"` (drift half) | 1489 | `kSpringHoldLivingDriftMm = 88` mm | changing the hold length or the wobble amplitudes |
| `"spring cross-sections left the accepted positive-volume, selective-squash envelope"` | 1605 | body/min radial ratio ∈ [490,570]; head ∈ [850,925] per-mille | any change to `kSpringBodyFlattenQ16` or squash depth |
| `"…releases and rises as one intact S"` (lift half) | 1496 | `rigid_air_lift ∈ [550,650]` mm | a smaller or slower launch |
| `"slot %d overlap %d mm > declared %d mm"` | 225 / 1216 | slot 3 allowance **370 mm** capsule-radius overlap | a differently proportioned coil |
| `"real tail tips stopped participating in the enlarged S"` | 1409 | `tail_follower_count ≥ 150` | mesh density, not motion |
| follower / normal count floors | 1622 | full ≥160, micro ≥70 | mesh density |
| `"jump has a discontinuous 60 Hz station step"` | 3243 | `kJumpMaxStationStepMm = 1300` (`zixxtrixx.h:7314`) | explicitly documented as "a regression band recorded from the accepted art, not a physical limit" — it moved 1146 → 1274 → 1300 when Direction 20 grew the head travel |
| six/nine wheel step | 3566 | 2200 mm | bespoke looser ceiling for the fast wheel |
| `"enlarged jump S stopped recruiting a body region"` | 1339 | every region's mean travel > 0 | *nominally* a law, but a much smaller-travel S could round a region to 0 mm in fixed point |

### 5.3 The declared coil-formation press

Declared at `zixxtrixx.h:920-966`, enforced by
`spring_tick_in_declared_press_window()` (`probe.cpp:566`) and
`spring_self_intersections()` (`probe.cpp:573`), checked at 1689-1717 and again
for the retimed fixture at 2502-2532.

* **Windows:** wind = ticks **13..21** (keys 6.5..10.5); unwind = ticks
  **38..40** (keys 19..20).
* **Depth:** ≤ **52 mm** full rung, ≤ **96 mm** micro rung, measured by
  `triangle_poke_through_mm` (the smaller of the two one-sided plane
  excursions). Both correspond to the same ~40 mm skin press on screen; the
  micro rung reads it larger because its triangles are coarser.
* **Only** station pairs ≥7 apart, **only** inside those windows. The loaded
  pose, the hold, the release pose and every airborne phase remain
  zero-intersection law.
* **Companion:** the hover echo — on the retimed (+1 hold key) grid the lowest
  micro vertex may clear the ground by ≤ **8 mm** between ticks **13 and 27**
  (keys 6.5..13.5). Outside it, grounded pre-release requires contact ≤ 0.

> **TRAP.** These windows are **absolute presentation ticks**. Retiming the
> arming (which Direction 23 requires) moves the coil-formation motion but not
> the window, so the allowance silently lands on the wrong frames: real presses
> become faults and the allowance covers frames that no longer press. **Any
> retime must re-derive all six tick constants, and the owner ruled the
> allowance on the 2026-09-02 motion — a materially different motion is a new
> ruling, not an inherited one.**

### 5.4 `zixx_springpose` — the diagnostic to actually use

`zhaozhou/tools/reel/zixx_springpose.cpp` (170 lines). **Comparison side only:
it asserts nothing, it prints.** It walks the same fixed-point spine the reel
walks — `spring_profile_slope`, the quantised quat chain,
`spring_support_origin_raw`, `spring_root_anchor_x/offset` — and prints world
millimetres.

| mode | args | prints |
|---|---|---|
| `pose <entry> <squash> [middle]` | 0..1000 each | one sample: anchor, root, base/posed support, then per spine bone `x_mm y_mm head_a16 r_mm` |
| `sweep [n]` | default 9 | `n+1` poses over entry 0→1000 at squash 0, then `n` over squash 0→1000 at entry 1000 |
| `clip <slot> <key0> <key1>` | default 5, 0, 0 | decodes the **real compiled clip** through `zc::decode_pose` — the way to check a baked key against the formula |
| `schedule` | none | **per key: arm, entry, squash, head x/y, and `move_mm` — the head's travel per key.** This is the direct read-out of "is it too fast", as an even column instead of an assertion. |

**`build-direct.sh` does NOT build it.** Compile it by hand with the same flags:

```bash
ROOT=".../zixxtrixx-wholebody-s-spring-20260901/zhaozhou"
g++ -O2 -std=c++17 \
  -I$ROOT/reference/include -I$ROOT/runtime/include -I$ROOT/tests/render \
  -I$ROOT/compiler/tests/generated -I$ROOT/reference/src \
  $ROOT/tools/reel/zixx_springpose.cpp build-lane/obj/*.o \
  -o build-lane/bin/zixx-springpose.exe
```

(reuse the `obj/*.o` a prior `build-direct.sh` run produced).

---

## 6. The deform sidecar

**Definition:** `reference/include/zref/zref_creature.hpp:202`

```cpp
struct DeformSample { uint16_t flatten = 0; uint16_t spread = 0; };  // Q0.16 DELTAS from identity
```

Carried on `zc::Clip` as `std::vector<DeformSample> deform` (one per authored
key) and `mid_deform` (one per presentation midpoint). **Empty means exact
identity**; it never enters the PoseBank, so decoded bone matrices stay rigid
and shareable.

**Authoring:** `spring_deform_sample(amount)` (`zixxtrixx.h:1949`):

```
q = spring_smooth_amount( min(1000, squash + kSpringSqueezeDeformLead) )   // lead = 340
flatten = kSpringBodyFlattenQ16 * q / 1000        // 31500 max
spread  = kSpringBodySpreadQ16  * q / 1000        // 13800 max
```

The **lead** is what makes the body fire out of the coil still pressed and
re-round as it extends. Zero stays exactly zero and full stays exactly full, so
the identity and endpoint gates are untouched by the lead.

**Per-vertex authority:** `spring_deform_strength(station)` (`:1910`), stations
0..56:

| stations | strength | source constant |
|---|---:|---|
| 0..10 | 64 | `kSpringSkullDeformStrength` (skull, barely squashes) |
| 11 | 188 | `kSpringThroatDeformStrength` |
| 12,13,14 | 204, 221, 238 | ramp over `kSpringBodyStrengthRampStations = 4` |
| 15..51 | 255 | `kSpringBodyDeformStrength` |
| 52..56 | 246, 237, 228, 219, 210 | ramp over `kSpringTailStrengthRampStations = 6` down to `kSpringTailDeformStrength` |

**Roles** (`zref_creature.hpp:406`):

* `kRadial` — body/head chain rings (`:7744`, `:7861`) with `deform_axis = 2`
  (local Z = bind-space vertical). Contract the axis lane by `65536 − flatten`,
  expand the other two by `65536 + spread`, and apply the fixed-point
  inverse-transpose to the normal (`diag(s,s,t)` trick, no division).
* `kFollower` — the pupil stripe (`:7829`, strength = `spring_deform_strength(kPupilStation)`),
  the blade leaves (`:7942`) and the middle spike (`:7971`), both at
  `kSpringTailDeformStrength = 210`. A follower **translates** by its carrier
  point's contraction; its own dimensions and normals stay rigid, so fins and
  markings follow without being crushed.
* `kNone` — everything else, untouched.

**Identity guarantee** (`creature_core.cpp:448`): `deform_skin_vertex` returns
`v` unchanged, before any arithmetic, when `role == kNone`, `strength == 0`, or
`flatten == spread == 0`; and again after the per-vertex strength scaling if
both scale to zero. **No fixed-point rounding can touch an ordinary clip.** The
probe asserts this at `probe.cpp:1525`.

**Interaction with contact.** The deform is what pays for every touch:
chin-over-tail, loop-over-pad, stacked runs, the flattened radii the coil sits
on. Direction 22 raised the flatten specifically because *raising the flatten is
what buys the height* — the runs squash and spread rather than intersect. The
declared 40 mm coil-formation press is measured **at the deformed skin**, so
flatten and the press allowance are coupled: lower the flatten and the press
gets deeper for the same pose.

**Midpoints.** `author_spring_midpoint_pose` writes `mid_deform[key] =
spring_deform_sample(control.squash)` only when
`c.mid_deform.size() == frame_count`, and records
`kMidpointDeformAuthored` in the provenance mask. The engine's
`deformation_sample` prefers `mid_deform` and otherwise averages the two
neighbouring keys.

---

## 7. Build, render, encode, publish

All paths relative to the lane root
`C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901`.

```bash
# 1. BUILD -- ONE TARGET PER CALL. Passing two silently builds only the last.
bash zhaozhou/tools/reel/build-direct.sh --output build-lane cel
bash zhaozhou/tools/reel/build-direct.sh --output build-lane probe

# 2. PROBE
build-lane/bin/zixx-probe.exe

# 3. RENDER -- both env vars are mandatory for the shipped look
ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
  build-lane/bin/zhao-reel-cel.exe Upheaval/website/scratch-reel \
  zixxtrixx-spring-side zixxtrixx-jump-one zixxtrixx-salto-dummy
# omit the subject names to render every wired subject (the 22-subject bank)

# 4. ENCODE (primary format: full-colour VP9 webm at 60 fps)
python Upheaval/website/tools/tovideo.py Upheaval/website
python Upheaval/website/tools/togif.py  Upheaval/website   # archive only

# 5. ASSEMBLE (deploy.ps1 runs this itself)
python Upheaval/website/tools/assemble.py Upheaval/website

# 6. PUBLISH -- -Branch is MANDATORY
pwsh Upheaval/website/deploy.ps1 -Project upheaval -Branch main
```

**`build-direct.sh`** — `--output <dir> [--clean] [reel|cel|meshcheck|probe|all]`.
`TARGET` is a single overwritten string, default `cel`; `--output` is required.
Produces `<dir>/bin/*.exe` and `<dir>/obj/*.o`. Flags `g++ -O2 -std=c++17` plus
five `-I` roots; `cel` adds `-DZIXX_PAGE_VARIANT="<…/zixxtrixx_page_cel.h>"`. It
never invokes CMake/Ninja/Verilator — that is the entire point.

**Env vars** (`zhao_reel.cpp` `main()`, 5591-5686). `ZIXX_EXP=celmain` sets
`g_smooth_toon_bands = 3; g_cel_main = 1` — the shipped presentation, and **not**
the default. `ZIXX_LIGHT=diagonal-cool-cross` is the shipped rig; an unrecognised
value hard-fails (`return 2`), but an **unset** one silently falls back to a
non-shipped default. Export both, every time.

**Render CLI:** `zhao-reel-cel.exe <output-dir> [subject …]`. No flags for
resolution or frame count — `W=384, H=240` is a constant in `render_scene()`;
frame counts are per-subject fields. Output: `<dir>/<subject>/NNNN.rgb`,
`palette.rgb`, `meta.txt` (per-frame + sequence CRC-32C).

**Traps.**

1. `build-direct.sh probe cel` builds **only cel**; the probe stays stale and
   reprints yesterday's numbers with no error (`RUN-20260901-2121/TASK_LOG.md:72`).
2. `cmake --build` races Verilator regenerating `build.ninja` and runs the stale
   binary. **A measurement that did not move after a change that must have moved
   it is the tell.**
3. `build-direct.sh` only rebuilds a `.o` when its `.cpp` is newer, so a
   header-only **struct-layout** change leaves stale objects that look exactly
   like a rendering bug. Use `--clean`.
4. Encoding **overwrites** `public/renders/<subject>.*` in place. Archive the
   outgoing bank (bump `MAX_ARCHIVE_GENERATIONS` *and* both CSS selector
   families together) **before** re-encoding.
5. `deploy.ps1` without `-Branch` → Wrangler silently demotes to a PREVIEW.
   It also refuses an `index.html` that has lost its `noindex` meta.

---

## 8. Shortest path from Direction 23 to the code

Direction 23 asks for four beats, slower and readable, with the head moving
only **slightly** backward. In this machinery that is:

1. **"Too fast"** is `kSaltoCompressEndKey` (16) and the trapezoid
   (`kSpringArmEaseInFrac/OutFrac`) — plus every seam literal and every probe
   band listed in §3.4, §4 and §5.2 that keys off it. Use
   `zixx-springpose schedule` to read `move_mm` per key before and after.
2. **"The head shoots past the tail"** is the collapsed table's front
   (segs 0–9) plus `kSpringSupportCompensation`, not a translation knob — head
   travel is derived from articulating around segment 14.
3. **"Become the S slowly, whole body"** is the absorb/assembled knots and their
   positions (`kSpringArmAbsorbAt` 220 / `kSpringArmAssembledAt` 400), which
   currently give only 7 of 16 keys to "becoming the S".
4. **"Organic like the balance clip"** is the life layer (§1.7), whose
   amplitudes were *reduced* in the rejected pass.
5. **The extreme coil being withdrawn** most likely makes the declared
   coil-formation press (§5.3) unnecessary. If the re-authored wind no longer
   presses, delete the allowance rather than inherit it — it was an owner ruling
   on a motion that has now been rejected.
