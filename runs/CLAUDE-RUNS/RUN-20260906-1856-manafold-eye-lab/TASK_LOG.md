# Task Log: RUN-20260906-1856 - [Describe objective here]

**Created:** 2026-09-06 18:56 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-1856-manafold-eye-lab/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 18:56 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-1856
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

## Field trip journal — EYE LAB (Manafold creature 02)

**Lane:** `C:\programmieren\zencrifice\manafold-eye-lab\{zhaozhou,Upheaval}`, both cloned
fresh from `origin/main` 2026-09-06. Ships nothing; output is plates + findings.

**Brief:** OWNER-DIRECTION-7 §12 (+ §5a-d, §7.5, §7.7). Four questions:
travel, the near-eye bar, blink-by-squash, star placement/size.

### Reading pass — mechanism facts established before authoring anything

1. `kBEyeL/R` are children of `kBRoot`, bind at `(kEyeXMm=400, vmm(90), ±215)`.
   A bone rotates about ITS OWN origin, so `apply_eye_shift` spins the lens on
   the spot. **Travel needs a pivot at the BODY centre**, not at the lens.
2. `kEyeShiftPivotMm = 0` is not merely unshipped, the mechanism is
   **half-written**: `make_eye_lens` sets `rs.cx = fxu(kEyeShiftPivotMm)` and
   `build_skeleton` gives the pupil bone a matching `+pivot` bind, but the EYE
   bone's own bind is never pulled inward by `pivot`. Setting the constant
   non-zero today translates the whole assembly OUTWARD and relocates no pivot.
3. `DeformSample{flatten,spread}` is ONE global sample per frame; `sub` is the
   half-tick interpolation rung, NOT a second channel. Per-part opt-in is
   `deform_role/axis/strength/center` on the RingSpec.
4. **Only `make_body` opts in.** The lens and star declare `kNone`, so the
   eyes are RIGID against the breath today — they do not follow the pulsation
   at all. §7.7's "computed against the neutral shape" fault is live, not
   hypothetical.
