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
