# SPEC v1: Slow, readable, whole-body S spring (Direction 23 implementation)

**Run ID:** RUN-20260902-1816
**Created:** 2026-09-02 18:16 UTC+02:00 (implementer fill 2026-09-02)
**Status:** Active
**Previous Version:** N/A

---

## Objective

Execute PLAN.md stages 0-7 (implementer stops before stage 8's encode/publish):
the jump/salto spring arming becomes four slow, readable beats per Owner
Direction 23, starting from the Generation Thirteen revert, retimed to a 64-key
arming (144 frames of ground time, ~270-frame jump clip), driven by a per-beat
smoothstep schedule at milli-key resolution so the motion is smooth per FRAME,
with a beat-2 art pass authored by eye (head slightly back and slowly down in
the fixed side view, rear keeps its curl).

Success = Direction 23's acceptance list, led by motion smoothness:
- A0 jerk gate: <= ~2 px silhouette centroid / ~4 px head / ~60 px^2 area per frame over the arming
- A1 half-life >= 16 frames; A2 shape rate med <= 7, p90 <= 12, max < 20 %/frame
- A3 jolts <= 5/s, none closer than 8 frames; A4 four beats segment
- A5 <= 2 reversals per station FROM THE POSE TABLES; A6 secondary periods >= 12 frames
- B1-B4 silhouette floors (solidity <= 0.70, hole <= 6%, closure >= 0.40, spine >= 0.65x median)
- C2 ground time >= 112 frames (designed 144)
- Verified by eye beside the balance clip, per the art law.

## Scope

**In Scope:**

- tools/reel/zixxtrixx.h, zixx_probe.cpp, zixx_springpose.cpp (revert + retime + schedule + beat-2 art pass + life-layer eye pass)
- zixx_jump_track camera fix (track the plan's smooth trajectory, not raw root)
- New committed diagnostic tools/reel/zixx_legibility.py (comparison side only)
- Diagnostic evidence committed in this run folder

**Out of Scope (implementer):**

- Full 22-subject bank re-render, encode, publish (stage 8 = reviewer/QA territory)
- The release, flight, salto wheel, landing, idle, balance clips
- Geometry, topology, pigments, eyes, fins, normals, light rig
- The life layer's structure (amplitude eye pass only)
- Engine interpolation machinery (c.interpolate etc.) - the midpoint fix is scoped to the spring arm schedule

## Constraints

- build-direct.sh ONLY, one target per call; never cmake --build; never sacengine
- ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross for every render
- Never git add -A; commit and push per stage
- Stage 1 must be provably byte-identical (sequence CRCs); stage 2 must leave
  `git diff a2f601ef HEAD -- tools/reel/` = stage-1 naming only
- Head backward travel verified in the fixed side view IN PIXELS
- Measurement never on the generation side (art law)

## Don't Retry

- Background subtraction / median plate for segmentation (moving camera poisons it) - colour segmentation only
- Pixel-domain reversal counting as a gate (noise; rejects the balance clip)
- World-space metre counts as evidence of screen-space head travel

## Open Questions

- Do re-spaced knots + per-beat clock alone meet A2, or do absorb/assembled tables need shape re-spacing? (probe with springpose schedule before rendering)
- Does 144 frames of ground time read hurried? (judge stage 4's render by eye)
- Wobble amplitudes on the slow primary: life or noise? (stage 7 eye pass)
