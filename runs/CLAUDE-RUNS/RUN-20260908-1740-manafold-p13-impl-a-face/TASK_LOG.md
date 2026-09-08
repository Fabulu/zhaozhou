# Task Log: RUN-20260908-1740 - [Describe objective here]

**Created:** 2026-09-08 17:40 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-1740-manafold-p13-impl-a-face/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 17:40 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-1740
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

## IMPL-A "face" — pass 13 R1
Lane: `C:\programmieren\zencrifice\manafold-p13-a\{zhaozhou,Upheaval}`.
Build dir: `../build-a`. Plates: `../plates`.

### 17:40 baseline
`build-direct.sh --clean cel` → RC=0. Rendered `still`, `taunt3`, `hover` under
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`. Frames read only via
`tools/reel/rgbframe.py`.

### 17:55 R1(a) — the mechanism, found by looking, then ablated
`manafold-still` (true rest: `build_still` calls only `loop_rest`+`face_rest`,
no gaze, no travel) at 8x: BOTH stars sit hard against the OUTER edge of their
lenses. Decomposed along the lens axis, the displacement is ~3.6 native px
ALONG the long axis and ~12 px PERPENDICULAR to it, outward.

That is not a registration error. `kStarCentreYMm` (+16) already registers the
asymmetric drawn mass on the lens centre along the long axis, and the along-axis
residual is small. The perpendicular error is PARALLAX: the star's geometry sits
`kEyeBulgeMm`(88) + `kStarCyanProudMm`(20) = 108 mm PROUD of the lens centre
plane, and at rest each lens is seen ~30-45 deg off its own face (the eye sits at
x400/z215 on the ball, plus `kEyeYawOutA16`). 108 mm of stand-off at 40 deg
throws the star ~70 mm sideways against a lens half-width of only 84 mm.

Corroboration with no rebuild: in the `hover` sheet the stars ARE centred and
DO read as stars on exactly the tiles where the lens presents its face (f100,
f125, f200), and slide outward + collapse to a sliver on the oblique tiles.
Same star, same constants, different obliquity.

### 18:20 R1(a)+(b)+(c) landed — zhaozhou 01d2dbe4
Eye assembly flattened in depth by ~0.45 (`kEyeDeepMm` 90->40, `kEyeBulgeMm`
88->40) and the pass-11 `bar-cyan-fat` compensation reverted (16/16/6) per
D9 §12.3 — because `taunt3` f192-216 shows the white ESCAPING, not vanishing,
and a 46 mm cyan slab riding 20 mm proud of a 12 mm white plate is two shapes
at different depths that parallax slides apart. `eye_travel_life_pm` rewritten
as dwell-and-glance.

### 18:55 R1(d) landed — zhaozhou a3556f58
Rule 3 swept over the shipping camera ring (72 yaw steps at the showcase
pitch), gated on the EXCURSION not the count: 308 pm of a body radius past the
outline against a 420 cap; `--fail-outline` reads 825 and exits 1.
And E.2's near-eye-bar gate was dividing by `kEyeWideMm` (the in-plane
half-WIDTH) where the outward axis is `kEyeDeepMm`. Checklist item 13.
Corroborated against EYE-LAB §12.2's independently derived 180 mm.

### 19:10 the C5 plate — zhaozhou 9fa2a2aa
`U02_EYE_TRAVEL_PIN=<pm>` committed in `eye_travel_life_pm`. Ladder at
0/15/30/45 deg, same subject/frame/camera/pose. Star + outline survive together
at 0/15/30 on both eyes; at 45 the unit holds but the eye is at the limb.

### 19:20 gates
probe exit 0; `--fail-outline` exit 1 (witnessed); spangate 0; nodule 0.
qa-p12 exit 1 with 4 failures, all pre-existing R2/R5 work (corpse deform
lanes, root seam pops). No deform, no root, no core file touched.

### 19:30 CLOSED
`Upheaval/creature/Manafold/PASS-13-FINDINGS-A.md` + `pass13-plates-a/` pushed
(Upheaval ecd6412). Lane can be deleted — see findings §8.
