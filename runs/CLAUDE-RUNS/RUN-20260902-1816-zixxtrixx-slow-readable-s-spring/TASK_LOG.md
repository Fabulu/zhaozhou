# Task Log: RUN-20260902-1816 - slow, readable, whole-body S spring (Direction 23)

**Created:** 2026-09-02 18:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260902-1816-zixxtrixx-slow-readable-s-spring/

---

## Objective

Rebuild the Zixxtrixx jump/salto spring per Owner Direction 23: four slow readable beats, smooth motion first. Process: 5 recons -> architect plan -> implementation -> reviewer -> QA.

---

## Progress Timeline

### 2026-09-02 18:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260902-1816
- Created working directory
- Initial context: Direction 23 rejects the D22 coil pass; five recon agents dispatched

### 2026-09-02 ~23:30 UTC+02:00 - Architect pass complete

- Read Direction 23 (+22/21/20 for what stands), all five recon documents, CLAUDE.md art law
- Verified landmines against the lane code: probe:1355 phase band, seam/slice literals, prime-number 23-stride, JumpPlan release_keys literal, kSpringCoilFormation* absolute ticks, camera welded to raw root in zixx_jump_track
- Verified: every commit touching tools/reel since a2f601ef is a D22 coil commit -> surgical three-file revert is exact
- Ruled Recon 1 vs Recon 3 path/net conflict in favour of Recon 3 (probe-validated instrument)
- Wrote PLAN.md: revert to Gen Thirteen, name-the-literals no-op first, retime to 64-key arming (144 frames ground time), per-beat smoothstep schedule at milli-key resolution as the central even-motion fix, camera fix, beat-2 art pass by eye, Recon 5 criteria adopted + Recon 2 jerk gate as A0

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

- recon/RECON-1..5 (five recon agents, committed 441160ed)
- PLAN.md (architect deliverable)

---

## Decisions Made

- Starting point: surgical revert of zixxtrixx.h / zixx_probe.cpp / zixx_springpose.cpp to a2f601ef (Generation Thirteen), stage-1 literal-naming refactor done first against live
- Retime is SHARED across all salto/jump consumers; kSaltoCompressEndKey 16 -> 64, hold 8 keys, downstream attack shifts +54, kAttackKeys 294
- Central fix: route evenly spaced in shape (knot re-spacing by measured arc length, per-beat smoothstep clock evaluated at milli-keys, midpoints on the eased curve, monotone support route)
- Coil-formation press allowance deleted, not inherited (owner ruling on a withdrawn motion)
- Criteria: Recon 5 A/B/C adopted; Recon 2 jerk thresholds added as lead gate A0; A5 reversals from pose tables never pixels

---

## Next Steps

- Implementer executes PLAN.md stages 0-8
- Reviewer: eye pass beside the balance clip + diff review of the provable no-op stages
- QA: criteria table on the encoded webms, archive/CSS bump, production serving check

### 2026-09-02 (implementer) — Stage 0: scaffold and baseline

- SPEC_v1.md filled (objective, scope, constraints, don't-retry list)
- Committed `tools/reel/zixx_legibility.py` (comparison-side legibility/smoothness
  probe per Recon 5 §8: colour segmentation — NEVER a median plate — CSV, jerk
  table, beat segmentation, four-panel plots, contact/zoom/overlay sheets).
  Two bugs found while calibrating: reel .rgb frames carry an 8-byte w/h header,
  and int16 luminance overflow selected the whole frame as mask.
- Built cel + probe at HEAD via build-direct.sh (one target per call). Probe GREEN.
- Rendered baselines (ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross):
  jump-one 161f crc 0xF7020B2F, salto-dummy 231f crc 0xE3C23422, balance 493f crc 0x7FC8F62E
- Baseline numbers REPRODUCE the recon instruments (calibration proof):
  jump-one arming (f1-50): half-life med 5 (Recon 5: 5), shape p90 58.6 (~56),
  solidity max 0.82 (~0.85), jerk 22.7/31.3/1334 (Recon 2: 23/31/1168), closure min 0.09
  balance (f75-130): jerk 1.2/3.4/43 (Recon 2: 1.5/4.0/53), half-life med 19
- Evidence: evidence/stage0/ (CSVs, jerk, beats, panels, contact/zoom/overlay sheets)

### 2026-09-02 (implementer) — Stage 1 in progress: name the literals

- Stage-1 edits authored (currently stashed for the byte-identity proof):
  kAtkRetimeShift = kSaltoCompressHoldEndKey - 18 introduced; release/rigid/
  release-end keys, kAtkImpactKey, kAtkStickEnd derived from it; every raw
  key literal >= 22 in kAtkLift/Fwd/Aim/Curl/Auth/Spin expressed as
  base + kAtkRetimeShift; kSaltoUnrollStartKey = kSaltoCoilPoseKey +
  kSaltoUnrollStride (23, the prime contract, now structural);
  kSpringAirWobblePeriodKeys = kSaltoUnrollStride; slice/seam literals
  17/29/52/60/74/75/224/239 replaced with named constants;
  JumpPlan::release_keys = kSpringReleaseMidpointCount; static_asserts pin
  kAttackKeys == 240 + kAtkRetimeShift and the unroll keys.
- Byte-identity proof method: full 22-creature-subject render pre-change
  (stash) vs post-change, sequence CRC compare. First attempt rendered the
  entire wired reel (suns/terrain too) and was killed - process verified dead.
- Baseline zixx-springpose schedule saved (evidence/stage0/springpose-schedule-live.txt):
  live arming move_mm column peaks 528 mm at key 10, confirming Recon 1/3.

### Stage 1 COMPLETE - provably byte-identical

- Full 22-creature-subject bank rendered twice: pre-change (HEAD) and with the
  stage-1 refactor. ALL 22 sequence CRCs MATCH (evidence/stage1/crc-proof-22-subjects.txt).
- Probe green with the refactor. build-direct.sh note: tool .cpps are compiled
  fresh at every link (only zref lib objects are cached, and they do not include
  zixxtrixx.h), so the header edit cannot leave a stale reel.

### Stage 2 COMPLETE - the surgical revert, verified

- 2a: three-file checkout of a2f601ef; 2b: stage-1 naming replayed on top.
  `git diff a2f601ef HEAD -- tools/reel/` = the naming refactor + the
  legibility tool, nothing else. Probe GREEN.
- "SPRING real surface intersections full/micro: none / none" - zero
  self-intersections, and the declared coil-formation press windows no longer
  exist (deleted with the revert, per the owner ruling being D22-scoped).
- jump-one per-frame byte compare vs live (evidence/stage2/jump-one-frame-diff-vs-live.txt):
  arming f1-43 differs (the coil is gone - intended); f44, f46-f110 BYTE-IDENTICAL
  (the flight); f45 single-frame midpoint diff; f111-f152 differ.
  DEVIATION FROM PLAN TEXT: the plan predicted frames 45+ identical. The
  landing window differs because D22 commit e5402d7a ("landings ride the
  press") also touched landing behaviour and the three-file revert discards
  it with everything else - consistent with the architect's decision to take
  the full-file revert over cherry-picks, and with D23 rejecting the D22 pass
  wholesale. Gen Thirteen's landing is the owner-praised bank's landing.
- Arming bbox matches Recon 2 section 3 REF column EXACTLY on all 11
  checkpoint frames (186x61 -> 222x51 -> 186x61).
- Support/root vertical route across the arming: monotone descent with ONE
  ~5 mm reversal at the entry/squash handoff (plan criterion: <= 1). The
  whip's six reversals are gone.
- springpose schedule (gen13): arming move_mm max 80 mm/key vs live's 528.

### Stage 3 COMPLETE - jump camera off the raw root's life clock

- zixx_jump_track now derives its tracked point with kSpringNoLife (the salto
  camera's RUN-1939 lesson). Camera only; probe green, all parity gates exact.
- Measured on jump-one arming (f1-45): background churn mean 0.57 -> 0.49
  (live was 0.89; balance is 0.00 with its fixed camera). Horizon row still
  travels 12 rows with max 4 rows/frame - that is the camera TRACKING the
  16-key descent, not jitter; the plan's "< 1 px/frame" target falls out of
  the stage-4/5 retime (412 mm of descent over 144 frames instead of 33) and
  will be re-measured there.

### Stage 4 COMPLETE - the retime skeleton

- kSaltoCompressEndKey 16 -> 64, kSaltoCompressHoldEndKey 18 -> 72,
  kSaltoSpringEntryEndKey -> 36 (beat-1 end), kAttackKeys 240 -> 294.
  kAtkRetimeShift derives +54 and slides the whole downstream attack; the
  compile-enforced seam gates all pass (the shift is consistent).
- Probe phase-envelope gate (zixx_probe.cpp:1303) re-authored as DERIVED
  relationships citing Direction 23: beat 1 inside the arming and >= half of
  it, beat 2 >= 8 keys, hold 4-12 keys, release deltas unchanged (D20 #4).
- Found and fixed a probe-side raw slice literal the plan's landmine list
  missed: zixx_probe.cpp:1861 sliced the release authorship at hard-coded
  17,29 - now kAtkCompressSliceLastKey/kSaltoCoilPoseKey.
- Left a warning comment on zc::AttackPlan's stale engine-side defaults
  (zref_creature.hpp) per landmine 10.
- PROBE GREEN end to end. Jump-one now 269 frames (plan predicted ~270),
  144 frames of ground time.
- springpose schedule: arming move_mm max 19 mm/key (gen13 80, live 528) -
  the predicted ~4x scale-down.
- Flight UNCHANGED: gen13 f46-110 byte-identical to stage4 at +108 frames
  (65/65). Landing pixels differ only by the free-running 23-key life wave's
  phase in the longer clip; side-by-side pairs are indistinguishable by eye
  (evidence/stage4/landing-pairs-gen13-vs-stage4.png); final settle frames
  byte-exact.
- Legibility on the retimed arming (f1-144): centroid jerk 1.8/1.3 px and
  shape rate 4.4/9.7/16.9 already at balance level (A0 centroid + A2 PASS);
  solidity 0.59, hole 5.9, closure 0.45, spine 0.88 (B1-B4 PASS).
  STILL FAILING, as the plan expects stage 5 to fix: head-x jerk 22.9 px -
  the 30 Hz chord-midpoint shimmer is plainly visible in the head track
  (head_x flip-flops +-6 px on alternating frames through the compression);
  area jerk 230 px^2; A1 half-life dips mid-compression. A4 beats cannot
  segment yet: the single trapezoid is beat-less by construction.
- Eye check of the every-frame contact sheet: the animal is READABLE in
  every arming frame, the press is continuous and slow, no blob, no snap.
  It does not read hurried at 144 frames.

### Stage 5 COMPLETE - the central fix: per-beat schedule at milli-keys

- spring_arm_schedule_mk replaces the trapezoid: settle-in (4 keys), BEAT 1
  smoothstep to assembled, 4-key dwell, BEAT 2 smoothstep to collapsed.
  Every breakpoint a named owner knob. Dwells join C1 by construction.
- EVERY arming+hold half-key now authored from the schedule at key+0.5
  milli-keys (pose, deform, life clock, support target) - the generic chord
  bake during the arming is gone. Probe provenance gates re-authored to
  expect full ownership; the representative sample keys moved 1/4 -> 20/50
  (the settle-in made 4.5 degenerate: chord == schedule at arm 0, so the
  authored-differs-from-generic tripwire proved nothing there).
- Knot re-spacing MEASURED with the pose probe: beat-1 legs 4.9 vs 4.3
  mm/arm-unit; even-spacing absorbAt would be 231 vs current 220 - a 5%
  bias, inside noise. DECISION: keep the restored Gen-13 value 220.
- Beat split adjusted TWICE by measure-then-look: 64-key arming read fine
  but beat 2 (4714 mm of shape-arc vs beat 1's 1845, measured) ran p90 13.1
  and dug half-life to 8. Final: 72-key arming (160 frames of ground time,
  inside Recon 5's 150-180 balance-pace band), beat 1 keys 4-36, dwell 4
  keys, beat 2 keys 40-72, hold to 80. kAtkRetimeShift +62, kAttackKeys 302.
  Jump-one 285 frames. PROBE GREEN.
- Measured (evidence/stage5/jump-one-stage5c-*):
  A0: centroid jerk 3.5/0.5 px (balance 1.2/0.4; the 3.5 is at beat-2
      arrival f141-142) - centroid-y and area at balance level, centroid-x
      slightly over the ~2 px target at one event.
  A2: shape rate med 3.9 / p90 10.9 / max 15.4 PASS (limits 7/12/20).
  A3: jolts 3.8/s PASS; two pairs closer than 8 frames (f46/48, f139/141),
      both prominence-4 wiggles at the instrument's floor - borderline.
  A4: FOUR BEATS SEGMENT: settle 14f / RUN 55f / dwell 13f / RUN 62f /
      hold-quiet 13f / launch. First jump bank ever to do this.
  A1: half-life med 36; a 21-frame dip to 11-12 mid-beat-2 remains (balance
      floor on this instrument is 15). Much of beat-2's churn is the REAR
      STRAIGHTENING - a Gen-13 fault stage 6 is scoped to fix; re-measure
      after the art pass.
  30 Hz staircase: DEAD - odd/even frame speed parity 1.16 (head-y
      0.287/0.303) vs live's all-63-reversals-on-odd-frames.
  B1-B4 all PASS (0.60 / 5.8% / 0.44 / 0.88).
- Eye: beat 2 reads as a gentle continuous press; the dwell reads; the hold
  quivers. CONFIRMED Gen-13 faults for stage 6: head drifts FORWARD on
  screen during beat 2 (D23 wants slightly back), rear straightens to a rail.
- Instrument notes: head-x jerk via eye-blob is band-flicker noise (verified
  by eye at 4x: pose steady, cel highlight flickers); beat segmentation now
  merges sub-6-frame gaps per Recon 5's own settle definition.

### Stage 6 COMPLETE - the beat-2 art pass (authored by eye)

- kSpringCollapsedHeading re-authored: segments 8-9 16500/4500 -> 22000/11000
  (the mid-body arch stands taller instead of flattening forward, carrying
  the whole front BACK - the D22-section-2 mechanism, not a tighter neck
  hook: segments 5-7 untouched at Gen-13's 93.4/151.1/170.3 deg); rear
  segments 15-18 -2800/-6200/-4800/-1600 -> -4500/-9500/-13000/-14500 (the
  assembled curl KEPT verbatim into the collapse - no station loses its lobe).
- Authoring loop: numeric sweep with the pose probe to bracket (9 candidates),
  then chosen AND verified by eye on the fixed-side render.
- HEAD TRAVEL, fixed side view, in pixels: (230.8,114.1) -> (221.0,125.8) =
  9.8 px BACK and 11.7 px DOWN (~225 mm world). Slightly back, slowly down,
  never near the tail (closure min 0.444 >= 0.40). Nose ~67% of rest height
  (Gen-13's accepted 64% family).
- kZixxSpringDiagnosticKeys was a literal 30 silently cropping the diagnostic
  to a quarter of the new arming - now derived from kSaltoCoilPoseKey.
- A5 FROM THE TABLES: ZERO direction reversals on every one of the 19
  stations (balance rise: 0; live had 18). The kept rear curl is what makes
  the route monotone.
- PROBE GREEN: zero self-intersections, honest bite (-42 mm vs declared 34
  resting/60 loaded), planar (whole-body lateral span 14 mm), all parity
  gates exact.
- Full battery on the final jump-one arming (f1-144):
  A0 centroid jerk 1.0/0.5 px (balance 1.2/0.4) PASS; head-y jerk 2.5 PASS
  A1 half-life: NO frame under 16 PASS  (med 56!)
  A2 rate med 3.4 / p90 5.5 / max 15.4 PASS (the 15.4 is a single
     segmentation flicker of the thin fin at f15, verified twin frames at 3x)
  A3 jolts 1.1/s PASS (balance 4.1/s)
  A4 four beats segment: 14f settle / 58f become-S / 11f dwell / 59f
     compress / 13f hold / launch PASS
  B1-B4 0.59 / 5.7% / 0.444 / 0.89 all PASS
  C2 ground time 160 frames PASS
- Area jerk 196 px^2 and head-x jerk 14.4 px remain in the table: both
  traced to instrument noise (fin-line segmentation flicker, eye-anchored
  head-x sliding along the horizontal neck), NOT motion - each verified by
  eye on the actual frame pairs. Honest caveat for the reviewer: judge the
  head's motion from the committed zoom sheets, not from the head-x column.

### Stage 7 COMPLETE - the life layer, judged by eye (zero edits)

- Gen-13's restored amplitudes (kSpringWobble 750 / kSpringWobble2 520,
  periods 23/51 keys = 46/102 frames, both clear A6's 12-frame floor by 4x)
  were judged against the slow primary at native resolution: beat 1 carries a
  gentle visible sway, the dwell breathes, and the HOLD VISIBLY QUIVERS
  (~2 %/frame continuously, 1261 pixels changed across 8 hold frames, same
  silhouette - held-alive, not frozen, not jittery). Recon 1's crayon-grain
  diagnosis confirmed: the same amplitudes that vanished against a 71 deg/key
  primary read as life against a <=6 deg/key one. No amplitude change needed.
- Hold drift gate: 59/0 mm over the 8-key hold vs 70 allowed - the probe's
  window derives from the timeline (the earlier "2 keys" label was the old
  schedule's own count). Support drift 0: the plant never slides.
- No per-station lag added to the rise (Gen-13's existing arm-domain chain
  lag left as restored; Recon 3 J6 proved it innocent).

### IMPLEMENTER CLOSE-OUT

Done and verified: stages 0-7. NOT done, per the brief: stage 8's full
22-subject bank re-render, encode, archive bump and publish - those belong
to the reviewer/QA flow.

For the reviewer, the honest deltas from the plan:
1. The arming is 72 keys (160 frames ground time), not the plan's 64/144 -
   chosen inside the plan's own 150-180 target band after measuring beat 2's
   4714 mm shape-arc; beat split is 4/32/4/32/8. kAtkRetimeShift +62,
   kAttackKeys 302, jump-one 285 frames.
2. Landing pixels differ from live: the D22 landing-press commit died with
   the revert (plan predicted frames 45+ identical; flight IS byte-identical,
   the landing is Gen-13's own plus life-wave phase in the longer clip).
3. Probe representative sample keys moved 1/4 -> 20/50 (settle-in made the
   old ones degenerate for the authored-vs-generic tripwire).
4. Knot re-spacing measured and DECLINED (5% bias, inside noise) - the
   Gen-13 kSpringArmAbsorbAt=220 stands.
5. kZixxSpringDiagnosticKeys and a probe-side 17,29 slice literal were
   additional un-derived literals not on the plan's landmine list; both fixed.
6. A0's head-x-jerk and area-jerk columns fail their thresholds ONLY through
   instrument noise (eye-anchored head-x slides along the horizontal neck;
   thin-fin segmentation flicker) - each verified against the actual frames
   at zoom. Centroid (1.0/0.5 px) and head-y (2.5 px) are at balance level.
   The reviewer should judge smoothness from the committed sheets + webms.
7. kSaltoSpringEntryEndKey now derives from kSpringBecomeSEndKey.

What the reviewer should look at, in motion, beside the balance clip:
- do the four beats read in order (they segment cleanly on the plot);
- does beat 1 read as the whole body BECOMING the S (by the tables, every
  station moves monotonically into the assembled S; the rear gathers);
- head slightly back and slowly down in the side view (9.8 px back, 11.7 px
  down measured fixed-side; the tracking camera hides some of this in
  jump-one - the spring-side webm is the honest view);
- the hold's quiver amplitude - my eye says life, a second eye should agree;
- beat 2's pace at native res (p90 5.5 %/f, half-life never under 16).

### 2026-09-02 (reviewer) — REVIEW.md committed (3ad1b12f)

- Judged by eye first on reviewer-owned renders (spring-side, spring-top,
  jump-one, balance), then with an independent silhouette instrument, then
  against the implementer's claims. Nothing authored was changed.
- PASS: acceptance 1 (smooth — sub-pixel per frame and quieter than the
  balance clip on every column; 30 Hz staircase dead; tracking camera
  <=1 row/frame), 2 (beats legible), 6 (pace: 2.67 s ground time at 60 fps,
  deliberate not draggy), 7 (launch). PASS on the standing laws: planar
  (0/14 mm), no clips, tail-tube participation, salto byte-preserved,
  15 of 22 bank subjects byte-identical CRCs.
- FAIL: acceptance 3 and 5. Beat 1 is 82.5 % a tail action (middle station
  travel 5 mm); the compression descends head -310 / neck -281 / front -124
  mm against middle -7, taper -33, tail +1 mm.
- NEEDS AN OWNER EYE: acceptance 4's "slightly" (head-region centroid moves
  2.18 px back), and the hold's quiver (reads held-alive, not loaded).
- Excused columns adjudicated: head-x 14.4 px = instrument (confirmed);
  area 196 px^2 = NOT a motion fault but misattributed — it is one
  whole-body shading pop at frame 15, present in the raw pixels;
  f46/f48 at the instrument floor (confirmed).
- Recommendation: do not publish; one authoring pass on two pose tables.

### REVIEW-RESPONSE PASS (review at 3ad1b12f; all five faults addressed)

Timing correction absorbed: playback is 60 fps; ground time is now 2.8 s
(72-key arming + 12-key hold = 168 frames).

FAULT 1 (middle does nothing) - FIXED, authored by eye:
- The assembled S now carries a LOW MID-BODY ARCH (kSpringAssembledHeading
  stations 10-13 -> -2600/-3600/3300/4200, absorb row carries ~40% of it):
  the middle visibly RISES INTO the S in beat 1 and PRESSES FLAT in beat 2
  (stations 11-13 descend 49/107/81 mm world, probe grounded-run descent
  -37 -> -66 mm).
- The tail curl is PRESSED LOWER in the collapse (collapsed 15-18
  -4500/-9500/-13000/-14500 -> -3900/-8300/-11000/-11800): the curl keeps
  its lobe and comes down with the head (probe tail descent +1 -> -38 mm,
  taper -33 -> -46).
- NEW kSpringBladeSquashRise = +3500 (all four spring consumers, parity
  green): the fan rises with the beat-1 gather and PRESSES DOWN through the
  compression (fin top edge: rest y112 -> assembled y106 -> loaded y110).
  Sign found empirically; negative sweeps up in the rolled tail frame.
- Fixed-camera region verification (reviewer's instrument): beat-1 change
  share tail/mid/head 82.5/6.7/10.8 -> 64.6/17.1/18.3 %; beat-2 descent
  head +6.6 px, mid +2.4 px down; probe station table: middle entry travel
  5 -> 28 mm, every region's compression descent now negative except
  probe-"middle" (+5 mm, straddles the arch's standing side which carries
  the head's backward travel - its peak stations descend 107 mm).

FAULT 2 (head-back too slight) - FIXED, bracketed and chosen by eye:
- Collapsed segments 8-9 22000/11000 -> 24500/12500. NOSE TIP in the fixed
  side view: -11 px back (was -7), monotone through beat 2 with only
  +/-1 px single-pixel noise en route; head-region centroid -3.4 px (was
  -2.18), max forward excursion +0.64 px (was 0.85) - at the sub-pixel
  measurement floor, no readable en-route reversal. World route
  0 -> -12 (assembled) -> -168 mm (loaded), monotone backward.

FAULT 3 (f15 shading pop) - DIAGNOSED, no code change, evidence attached:
- The pop is the 3-band cel quantizer crossing a band boundary on a slow
  coherent surface (the reviewer's 8x pair shows the purple mid-tone band
  widening across the neck in one frame). The ACCEPTED BALANCE CLIP shows
  the same class of event at the same magnitude in its own quiet stretches:
  raw-pixel spikes of 7.7-7.8x local median at its f15 (372 vs 48) and
  f23/24 (1008 vs 129). It is a property of the shipped look exposed by
  quiet, not a fault introduced by this pass; smoothing it would mean
  changing the shipped cel shading. Recorded here with the balance numbers
  as the comparison.

FAULT 4 (frozen clip head) - FIXED:
- kSpringLifeFloor = 300 (the balance clip's own never-off floor), ramped
  open across the settle-in keys inside spring_life_wave: clip key 0 stays
  the EXACT grounded pose (seams + deform identity intact, probe green),
  and the creature is alive from the next presentation sample. Verified:
  zero byte-identical frames at the clip head (was eleven).

FAULT 5 (hold reads alive, not loaded) - EYE CALL, taken:
- Hold lengthened 8 -> 12 keys (kSaltoCompressHoldEndKey 84; 0.4 s;
  kAttackKeys 306; kAtkRetimeShift +66). kSpringHoldLivingDriftMm
  re-recorded 70 -> 90: the 70 was recorded against a TWO-key hold; the
  same wave amplitude over twelve keys sweeps more of its period. Support
  drift stays 0 (the plant never slides).
- THIS IS AN EYE CALL: with the longer hold the wave's swell makes the
  second half of the hold read as visible strain (it now segments as its
  own small activity run). If the owner wants more or less, the knobs are
  kSaltoCompressHoldEndKey (length) and kSpringWobble/kSpringWobble2
  (amplitude) - one edit each.

Final state, measured (evidence/fixes/): A0 centroid 1.0/0.5 px; A1 no
frame's half-life under 16 in f1-152; A2 3.4/5.3/10.2 %/f; A3 2.1 jolts/s;
A4 beats segment (56f/60f runs, 12-14f gaps, hold split quiet+strain);
A5 worst 1 reversal per station (the arch's own up-then-press, by design,
separated by the dwell); B solidity 0.60 / hole 5.9 / closure 0.472 /
spine 0.87; probe GREEN end to end. Jump-one now 293 frames.

### 2026-09-03 (implementer, Direction-24 fix pass) — committed f5c0110c

QA verdict absorbed (FAIL on D24 acceptance 1/2/3, one-line cause:
kSpringAssembledHeading[5..9] byte-identical to kStanceSlope[5..9], and the
whole travel budget in the tail). Direction 24 read: THE TAIL IS THE ANCHOR.

WHAT CHANGED (tools/reel/zixxtrixx.h + two probe re-records):
- Absorb/Assembled/Collapsed [15..18] now alias kStanceSlope[15..18]
  verbatim — the tail's route is a constant by construction.
- kSpringBladeSquashRise 3500 -> 0; kSpringBladeFlare 900 -> 300.
- Assembled [10..14] = -2000,-8200,2400,8800,1600: a real broad rear lobe
  (~4-5 px apex) gathered out of the flat run; [5..9] = 14700,21700,25600,
  20300,12000 (hook a few degrees deeper — visibly changes, no early
  squeeze). Absorb carries ~55%. Sine-sums balanced against grounded so
  the front holds height.
- kSpringCollapsedSupportLiftMm -26 -> -14 (planted tail rides the plant;
  -26 buried it on the upslope). kJumpLandingLoadedBiteMm 48 -> 54
  (declared: flat tail under the landing slam, ~1.3 px).
- zixx_probe.cpp: interior-knot stall gate skips authored-planted stations
  (an anchor is not a stall); kSpringWholeTailLateralSpanMaxMm 45 -> 55
  (rest tail's construction roll now carried into the deep pose).

AUTHORING LOOP: 8 drafts, each rendered on the fixed side camera and
LOOKED AT. Draft 1 (measured-first, deep hook + tall arch) read as an
early squeeze, dropped the crown, and self-intersected 45 verts — the
art law's warning made flesh; reverted by eye. Growth ended up bounded by
loop clearance (station-11 rise ~45 deg max before the under-curl kisses
the lobe at the release bridge key 85.5).

PIXEL-VERIFIED, QA's committed calibrated instruments, fixed camera:
- Fin tip x 109-110 ALL 168 ground frames (was 109->125->120); y 123-127
  (was 124->109->118). Tail bbox constant w58-62/h33-37 (was w37-60).
  NO direction change (beat1 +0.14, beat2 +1.84 px, same sign).
- Beat-1 share tail/mid/head 80.2/7.7/12.1 -> 41.3/25.1/33.6 %; residual
  tail share is life wave + body pixels leaving x<170 as the animal
  gathers, not fin motion.
- Beat-2 descent tail/mid/head +1.85/+3.34/+7.80 px — together, planted
  tail quietest.
- Outline overlay (draft8-outline-overlay.png): the QA fault picture
  INVERTED — three tail outlines trace one V; loop and mid-body separate.
- Smoothness KEPT: sil-XOR med/max 0.92/2.96 %/f (prev 1.72/4.47), worst
  frame still f15 quantizer. Nose -12 back / +11 down, monotone, gap >=
  124 px. Probe PASS end to end at f5c0110c.

HONEST FLAGS for QA:
1. Whole-frame XOR beat contrast is REDUCED (beat1 0.83 vs dwell 0.94
   %/f): the old contrast was the tail flailing; life wave ramps to full
   through beat 1, so the dwell out-rates it. Beats separate spatially
   and on region centroids; judge in motion.
2. Launch rate 11.6 %/f vs prev 25.4 (still 12x the hold) — the planted
   tail no longer whips during release.
3. Gate re-records on the record: lateral span 45->55 (construction roll),
   landing bite 48->54 (declared), stall gate skips planted stations.
4. NOT re-verified here: full 22-subject bank, Gen-13 flight byte
   identity, encode/publish — per the brief these belong to QA.
