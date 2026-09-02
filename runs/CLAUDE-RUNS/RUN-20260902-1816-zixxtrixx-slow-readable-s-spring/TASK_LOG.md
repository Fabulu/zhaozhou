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
