# Task Log: RUN-20260906-0029 - [Describe objective here]

**Created:** 2026-09-06 00:29 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0029-manafold-p6-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 00:29 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0029
- Created working directory
- Initial context: [brief description]

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

## QA log

- Lane: manafold-p6-qa/{zhaozhou,Upheaval}, cloned from manafold-p6-impl at
  zhaozhou 83be0c66 / Upheaval bd80b5c. Build tree manafold-p6-qa/build.
- Built manafold-probe from the shipped SHA with tools/reel/build-direct.sh
  (never cmake --build). Reproduced every published probe number exactly.
- FOUND: units bug in manafold_probe.cpp. fxu(mm) = mm*65536/1000, so a raw
  fx16 value is METRES*65536. The 5d gate and 5c rule 1 both convert with
  ">>16" and label the result "mm". Everything under 1000 mm truncates to 0.
  * 5d gate A's "0 mm closest approach at every amplitude" is this, not an
    unknown cause. True separation, measured with (raw*1000)>>16: 98 mm at
    roll 0, 18 mm at the shipped 10 deg cap.
  * 5c rule 1's off_mm is always 0, so over = -rim_mm <= 0 always: rule 1
    cannot exceed 0, rule 2 is pinned at 1000 pm, and rule 3's body-outline
    loop is guarded by over>0 and NEVER RUNS. All three leash rules inert.
- Also found: eye_shift_a16(1000) = 166880 a16 = 916.7 deg, because
  kEyeShiftPivotMm = 0 and the guard divides by 1. The committed 5d gate calls
  apply_eye_shift at full amplitude in all 16 corners, so it poses the eyes
  through a ~197 deg flip. kEyeShiftMaxPm is NOT shipped in the clips, so no
  clip is affected -- but the GATE is.
- Next: correct-frame overhang, roll sweep to 18 deg, then the other claims.

- Eye geometry, correct units: separation 98 mm at roll 0, falls to 0 mm at
  7.0 deg, and an ellipsoid containment test puts 24 LEFT-eye vertices INSIDE
  the RIGHT lens at the shipped 10 deg clamp. Known-bad (each eye pulled 40 mm
  inward) moves 98 -> 18 mm, so the instrument responds.
  BUT: over the whole shipped bank, 0 of 2204 clip frames intersect (closest
  1514 pm of the surface, min vertex gap 42 mm). Latent, not shipped.
- apply_eye_roll has ZERO callers in the clip builders -- 5d is built and
  never used. Same for apply_eye_shift (5c eyeball shift) and apply_gaze_lr
  (5b rule 4 asymmetry). Only the probe calls them.
- 5c leash re-measured in the right units AND the right frame (bind-space
  offset subtracted): -9 mm at rest (sane), rises with gaze, 25 mm at full
  gaze vs a 24 mm cap. Over the bank it hits 142 mm at slot 2 key 115 --
  driven by apply_twinkle spinning the ASYMMETRIC star (bottom arm 216 mm)
  up to 119 deg across an 84 mm-half-width lens.
- LOOKED at it. channel and rest both show the far eye's star hanging off the
  purple onto the body/sky, and hard near-black notches at every lens tip on
  100% of frames (rest: 400/400 frames, ~22 px). kEyeLensWidthPm ends 0 with
  caps enabled -> degenerate zero-radius cap.
- BIGGEST FIND: zhao_reel.cpp:3264 bounds kSmearPresets with "< 5" but the
  array has SIX entries. Pass 6 added rung 5 (SHORT/TORN) and assigns it to
  every clip except still/drift/hasty -- so 12 of 15 clips fall back to
  preset 0 (no smear) and ship with NO SMEAR PLANE. Same bug class the same
  pass wrote a lesson about for kKneadClipPm.
  A/B: one-byte variant (< 5 -> < 6) building now.
- Media/site agent: 949 declared files all decode; 479 manifest subset all
  decode; ten manalab variants present; 9 archive generations intact.
