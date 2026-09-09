# Task Log: RUN-20260909-0316 - [Describe objective here]

**Created:** 2026-09-09 03:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-0316-manafold-p14-impl-face/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 03:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-0316
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

## IMPL-FACE — pass 14 wave 1 (R2(a) ablation first, then R1, R3)

Lane: `C:\programmieren\zencrifice\manafold-p14-face\{zhaozhou,Upheaval}` @ origin/main
(zhaozhou d171a608, Upheaval 97daa95).

### Progress timeline

* 03:16 Run opened. Read: art law (lane `Upheaval/CLAUDE.md`), PASS-14-PLAN §0-§2,
  R1/R2/R3, §5 wave design, §6 protected list; PASS-13-REVIEW §2.1/§2.4;
  PASS-13-QA §0; pass13-expressiveness/FINDINGS.md; 09-ENGINE-GOTCHAS §19/§20/§21;
  10-GATE-CHECKLIST §0/§A.
* 03:17 Baseline `--clean` build (`build-direct.sh --output build-face --clean cel`),
  BUILD_RC=0, md5 `9e994246e6d9cc3853db77fa18e4b5eb`.
* 03:2x Baseline render started: `manafold-hover manafold-channel` under
  `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.
* 03:2x `kBodySegments`/`kBodyPoleSegments` 16 -> 32 (ABLATION ONLY, not shipped),
  second `--clean` build into `build-face32`.

### Source reading done BEFORE the ablation (mechanism, gotcha §18)

The shipping env `ZIXX_EXP=celmain` sets **`g_smooth_toon_bands = 3`**, NOT
`g_cel_bands` (`zhao_reel.cpp:7570-7573`). So the shipping terminator is the
*smooth* toon branch (`creature_sim.cpp:1003-1007`): per-corner Gouraud light,
linearly interpolated across the triangle, thresholded per fragment by
`kSmoothCel3Ramp` in `rast.cpp:apply_toon_ramp`.

Two structural reasons that boundary staircases, both mesh-density-dependent:
1. the interpolated scalar is only C0 across an edge, so the iso-contour is a
   POLYLINE with a kink at every mesh edge; and
2. `kSmoothMixNum = 819` (`creature_sim.cpp:593`) mixes **20% FLAT FACE light**
   into every corner, which is discontinuous across an edge — a genuine STEP,
   not a kink.

Prediction recorded before looking: 32 segments halves both, so the staircase
shrinks but does not become a curve. The plan's outcome-2 leg ("analytic
ellipsoid normals in compile_creature") is therefore NOT obviously the answer —
the normals are already smooth/position-keyed and the deform applies the inverse
transpose. **The look decides, not this note.**

### Where I am (written down before results arrive, per CLAUDE.md)

Next step: crop plates of `channel` f157/f175/f180/f185 and `hover` f37 at 16 vs
32, at 4x and at native; report the ablation answer to the coordinator; then R1.
