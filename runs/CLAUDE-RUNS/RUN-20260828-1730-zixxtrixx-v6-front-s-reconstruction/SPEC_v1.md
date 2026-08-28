# SPEC v1 — Zixxtrixx FRONT-S RECONSTRUCTION (owner direction #3, 2026-08-28)

Source of law: Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-3-2026-08-28.md
(posted four times; the file wins over every restatement, including this one).

## The diagnosis being acted on
Five head passes failed the same way: preserve the descending S -> improve the
head -> rotate it locally. The head's baseline position and apparent gaze are
determined by the FINAL THIRD of the S-curve; a local head joint cannot make a
descending neck read as a proud upward-held head. The S is NOT sacred.

## FROZEN this run (owner's list, verbatim)
skull dimensions; eyes and pupils; texture and colours; atlas resolution;
GENERAL BODY GIRTH (the 2x-girth finding from RUN 0757 stays deferred —
deferred-girth-knob.patch in this folder preserves the parked scaffold);
head pivot.

## The work — the front 30-40% of the centreline only
1. Raise the entire upper arc; the neck CLIMBS toward the head.
2. Lift distributed across five-plus neck segments; no single hinge.
3. Final neck tangent slightly UP into the skull; the head CONTINUES it.
4. kHeadAttitude returns ~NEUTRAL (expressive trims only).
5. The existing S is broken deliberately.
6. Goldens re-pinned afterwards with loud provenance naming this direction.

## Architecture — spline constraint, not more angle constants
Anchor fixed in the mid-body (the dive entry, whose slopes are preserved).
Authored knobs: the snout tangent (from the concept) and the arrival curve
shape. The seg0..4 slopes are GENERATED as one smooth C1 tangent ramp from
snout tangent to anchor tangent — constant-rate turn, no hinge, no per-joint
constants to oscillate. kBodyY re-solved so the grounded run lands where it
always did (probe-verified). Every authored value stays a named constant.

## Acceptance gate (replaces the old one)
Fixed side view against Side.png ONLY, colour removed (zixxtrixx-unlit):
- overlay silhouette on Side.png (sidecmp.py; sequence continues at sidecmp-10);
- head centre height vs the S crown;
- world snout direction (zixx-headaim, measurement side only);
- neck tangent immediately behind the skull;
- angular discontinuity between those two limited;
- NO visible inward notch at the junction.
No three-quarter beauty shot. No metric-only acceptance. No head-only view.

## Gates at close
zixx-probe 0; zixx-choreo 0; zixx-planner 0; zhao-reel --check "all sequence
CRCs match" redirected to a file; goldens re-pinned with provenance; ground
allowances re-authored against worst-key renders where the raised arc moves
them. Build via run-local build-direct.sh only, never cmake --build.
