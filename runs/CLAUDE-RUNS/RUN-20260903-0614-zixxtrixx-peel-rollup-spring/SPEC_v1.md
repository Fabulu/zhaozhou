# SPEC v1: THE PEEL — travelling-support roll-up spring (Owner Direction 25)

**Run ID:** RUN-20260903-0614
**Created:** 2026-09-03 06:14 UTC+02:00
**Status:** Implementation complete, handed to QA (2026-09-03)
**Previous Version:** N/A

---

## Objective

The spring arming becomes Direction 25's peel: contact leaves the ground at
the front of the grounded stretch and travels progressively backward along
the resting footprint until the animal stands on the end of its tail, then a
strong quick compression (nose visibly further back than the published
-12 px, never past the tail), ~31% faster ground time (168 -> ~116 frames),
with Direction 23's smoothness and Direction 24's tail-anchor law (from sole
contact onward) preserved. Verified in pixels on zixxtrixx-spring-side.

## Scope

**In Scope:**
- Generalise spring_support_origin_raw / spring_root_from_quats_raw to a
  milli-station support parameter + authored route spring_support_station_mk
  (stage 1: provable no-op; stage 4: the peel route 14000 -> 19000).
- Re-author the two interior pose knots (peel-mid @220, tail-stand @400) and
  collapsed (@1000); tail rows frozen tail-stand -> collapsed.
- Retime: kSpringPeelEndKey=32, settle 2, kSaltoCompressEndKey=50,
  kSaltoCompressHoldEndKey=58 (kAtkRetimeShift derives +40).
- Re-derive the three motion-fitted probe gates (phase envelope :1314,
  support path :1759, stall guard :2502) for the new motion; nose-past-tail
  law gate; declared bounded self-press window (derived ticks) if needed.
- Contact-front pixel tracker committed as the pass's central diagnostic.

**Out of Scope:**
- Full 22-subject re-render, encode, publish (QA does that after me).
- The salto wheel, flight curves, idle, balance, kStanceSlope, spline
  machinery, half-key authoring, engine midpoints.
- Physics/IK; every value stays a named owner knob.

## Constraints

- build-direct.sh only, one target per call, --clean after struct changes.
- Never run sacengine. Never git add -A. Renders always with explicit
  ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross.
- 15 non-spring bank subjects must stay byte-identical; exactly the 7 spring
  consumers may change.
- Pixel-space verification on the fixed side camera is the primary evidence.

## Don't Retry

- Pose-tables-only peel with fixed station-14 plant (rejected in PLAN 2.2a).
- Lift-route-driven peel (rejected in PLAN 2.2b).

## Open Questions

- Does the lerped travelling-support origin move the root smoothly at
  station boundaries? (PLAN 9.1 - springpose velocity check before render.)
- Does ~116 frames read deliberate? (PLAN 9.2 - stage-3 render experiment.)
