# Task Log: RUN-20260903-0614 - THE PEEL: roll-up spring per Owner Direction 25

**Created:** 2026-09-03 06:14 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-0614-zixxtrixx-peel-rollup-spring/

---

## Objective

Rebuild the Zixxtrixx jump/salto spring arming per Owner Direction 25 (THE
PEEL): contact leaves the ground at the front of the grounded stretch and
travels progressively backward until the animal stands on the end of its tail,
then a strong, quick compression (head further back than the published pass,
never past the tail), faster loading overall, smoothness kept, clipping
relaxed-but-declared for this pass.

---

## Progress Timeline

### 2026-09-03 06:14 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-0614
- Created working directory
- Initial context: Direction 25 replaces the six-pass "turn the whole body
  into the S" instruction with a mechanical peel/roll-up description

### 2026-09-03 (architect) - PLAN.md committed

- Read Direction 25 (binding), 24 and 23 (still standing as qualified),
  previous run RUN-20260902-1816's PLAN/TASK_LOG/REVIEW/QA/QA-2 and all five
  recon documents, CLAUDE.md; verified cited line numbers against the lane
  at dbc0deec.
- CENTRAL DECISION: the peel is a TRAVELLING SUPPORT - generalize
  spring_support_origin_raw / spring_root_from_quats_raw from the fixed
  kSpringPlantSegment prefix to an authored milli-station route
  spring_support_station_mk(arm) walking station 14 -> tail tip across the
  peel. Root compensation then holds each successive contact point at its own
  grounded-baseline position: contact never slides, the contact patch recedes
  along the resting footprint, and the tail-tip stand is structural (the D24
  alias trick, one level up). Pose-tables-only and lift-route-only variants
  rejected: a fixed station-14 plant makes station 14 un-liftable by
  construction and turns the tail's grounding into emergent arithmetic.
- Knots: grounded / peel-mid (220) / tail-stand (400, the unmoved
  entry-squash split) / collapsed (1000); tail rows frozen tail-stand ->
  collapsed = D24 law from sole contact, by construction. Entry = peel,
  squash = compression, so the deform stays correctly keyed with zero
  re-plumbing.
- Timing: settle 4 / peel 28 / gather 2 / compression 16 / hold 8 keys ->
  ground time 168 -> ~116 frames (~31% faster); RECON-5 floors re-derived
  for a two-beat motion (~92-frame floor).
- Staging: support generalization as a CRC-proof no-op first, instruments and
  still-image pose brackets before any clip wiring, retime on the old motion
  as its own checkable stage, then peel, then compression, then life/polish,
  then battery+encode+publish.
- Landmines: 18 entries with file:line, incl. the three probe gates fitted to
  the superseded motion (phase 1314, support 1759, stall 2502-2567), both
  zero-headroom bite declarations (spring -60/60, landing -53/54), the
  derived-not-absolute press-window rule, and the life wave's tail share
  wobbling the stand.
- Committed and pushed PLAN.md.

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

### 2026-09-03 (implementer) - Stage 0: baseline verified

- Fetched origin main both repos; zhaozhou at 49672fba (= origin/main).
- Built cel + probe into `build-peel/` (one target per call, fresh tree).
- Probe at HEAD: `ZIXX PROBE: PASS` (evidence/stage0 . . . probe-head.txt).
- Rendered zixxtrixx-spring-side + zixxtrixx-jump-one with explicit
  ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross:
  spring-side sequence CRC **0x1B1AEAB6** (== QA-2's published number),
  jump-one **0x9829FF4A** (== QA-2's table). The build IS the published one.
- Stage-1 bank reference: previous run's committed
  `evidence/qa2/qa2-bank-crc.txt` (22 subjects, rendered at d5949320; the
  only commits since are documentation, proven by the two CRCs above).

### 2026-09-03 (implementer) - Stage 1 in progress + two plan gaps found

Machinery generalised (support_mk milli-station):
- `spring_support_origin_raw(quats, support_mk, ...)` walks the prefix to
  floor(mk/1000) and advances frac of one segment along that bone's own
  axis; mk 14000 reproduces the old walk bit-exactly; 19000 = tail tip.
- `spring_root_from_quats_raw(quats, support_mk, target_y, ...)` walks both
  chains to the same mk.
- New route `spring_support_station_mk(arm)` (stage 1: constant-returns
  14000) + ONE derivation helper `spring_support_station_mk_for(entry,
  squash)`; every consumer call site goes through it. Midpoint path gets
  mirror derivations `spring_plan_midpoint_support_mk` /
  `spring_shared_midpoint_support_mk` with the identical branch structure as
  the target-y derivations, so a half-key can never disagree with its key
  about which point is planted.
- All 8 consumer call sites threaded (attack :3281-region, variant :7047,
  jump grounded/landing/mid loops, shared release midpoints, midpoint pose,
  anchor offset); zixx_springpose.cpp updated; probe untouched (it reaches
  the walk only through these functions).
- Probe: PASS after clean rebuild. Bank render for byte-identity running.

**PLAN GAP 1 (jump landing).** `kJumpLandingAbsorbArm = 700` absorbs the
landing INTO the route's second half (its comment "never reaches the
assembled knot" is stale -- 700 > 400). Under Direction 25's re-authored
route, arm 700 will be a compressed-on-tail-tip pose and the same-derivation
support would put the landing plant at the TAIL TIP -- the landing would
slam into a tail-stand. Decision for stage 4/5: cap the landing absorb at
the grounded-contact stretch of the route (kJumpLandingAbsorbArm ->
kSpringSupportPeelBeginArm, derived), keeping ONE support derivation
everywhere. The cushion loses the deform flatten (squash stays 0); the
bite bias route still gives the weight. Eye-check at stage 5; if the
landing reads stiff, the remedy is a small dedicated landing-absorb pose,
NOT a second support law.

**PLAN GAP 2 (the stand hovers without a descent term).** The plan's
"support rides the resting footprint at near-zero lift" is only true to
station ~17: measured on the committed baseline chain (springpose pose 0 0),
stations 10-16 rest at centreline y 155..100 mm, station 17 is the LAST and
LOWEST footprint point (x -1714, y 77), and 18/19 CURL UP (159, 301 mm) --
the stance tail tip is ~300 mm in the air. Holding the posed tip at its own
baseline position would stand the animal on an invisible pedestal. Fix
(stage 4): a named descent route `spring_support_descent_mm(support_mk)`
added INSIDE spring_support_target_y (0 while the support is on the real
footprint, authored down to plant the tip on the dirt as mk walks 17000 ->
19000). No signature changes; the probe's expected-target formula follows
automatically because it calls spring_support_target_y.

Springpose extended (stage-2 instrument, comparison side only): schedule
gains support_mk / support-baseline x,y / posed tail-tip x,y / nose-to-tip
margin columns; new `peel` mode samples every half-key (the 60 Hz grid) for
the station-boundary kink check (plan experiment 1). World orientation
confirmed: +x = screen right = nose side; tail tip at x -1925.

### 2026-09-03 (implementer) - Stage 1 PROVEN no-op, committed

- Full 22-subject bank rendered from the generalised build (one explicit
  ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross invocation naming all 22):
  **every sequence CRC identical to the published bank** (qa2-bank-crc.txt),
  all frame counts identical. `evidence/stage1-bank-crc.txt`.
- Probe PASS on the same build. The travelling-support machinery is now a
  constant edit away from the peel, exactly as the plan staged it.

### 2026-09-03 (implementer) - Stage 2 (pose brackets by eye) + Stage 3 (retime)

Stage 2, on the committed centreline sketchpad (evidence/stage2/, nine
iterations cand-v1..v9, chosen BY EYE):
- v1 spiral roll = unreadable knot; v2 tall diagonal = cobra tower, weak
  compression; v3/v4 open loop but the planar chain CROSSING put station 6
  coincident with 14 (full-tube interpenetration); v5 sweep chose the
  "stand C" family; v6-v8 fixed the loop/column adjacency and peel-mid wave;
  v8 peel-mid overshot (nose swung BEHIND the tail mid-route - rejected);
  **v9 chosen**: C-loop stand (~225 deg sweep, gap on the left flank, dive
  enters above the gap, column exits below), breaking-wave peel-mid (nose
  monotone backward: 0 -> -267 -> -720 -> -1207 mm), collapsed pressed low
  and flat with nose back to ~-1207 mm (~-29 px on the 41 mm/px camera; the
  published pass's whole travel was -12 px). Every per-station spline leg
  <= 165 deg (landmine 12 margin); station 9 is routed through peel-mid
  ~195 deg because its direct stance->stand arc is exactly +182 deg - the
  unwrap degeneracy the plan warned about, found and dodged.
- Remaining adjacency: dive 5-6 brushes loop-closure/column-top 13-16 at the
  stand, and the crown presses the coil top through the compression - this
  is THE declared self-press of Direction 25's relaxed-clipping ruling,
  bounded at stage 5.

Stage 3 (retime skeleton, old knots still installed):
- kSpringBecomeSEndKey 36 -> kSpringPeelEndKey 32; settle keys 4 -> 2
  (kSpringPeelSettleKeys); kSaltoCompressEndKey 72 -> 50;
  kSaltoCompressHoldEndKey 84 -> 58; kAtkRetimeShift derives 66 -> 40;
  kAttackKeys 306 -> 280 (compile-asserted); probe representative key
  50 -> 42 (must sit mid-beat-2 of the NEW schedule; 20 stays mid-peel).
- Probe phase-envelope relationships verified against the new schedule
  (64>=50, 18>=8, 8 in [4,12], release deltas 4/2/4 unchanged) and the
  comment re-cited to Direction 25.
- Schedule instrument (evidence/stage3-schedule.txt): ground time 116
  frames; move_mm per key 5-11 mm through the peel (was 5-14 over 72 keys),
  release unchanged.
- **KNOWN RED, superseded by stage 4:** probe FAILS one gate -
  "spring body runs intersect": 10 pairs at ticks 37.5-38.5 (key ~19,
  arm ~210), stations 20/34. Cause: the retime shifts the life-wave phase
  against the arm, and the OLD absorb-region pose was within a hair of
  touching. Stage 4 deletes that pose; re-verified there.
