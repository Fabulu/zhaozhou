# Task Log: RUN-20260907-2308 - [Describe objective here]

**Created:** 2026-09-07 23:08 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260907-2308-manafold-p12-2b-theatrical-clips/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-07 23:08 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260907-2308
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

## 2B-0 — lane up, and a FAILING GATE ON MAIN before I touched anything

Lane `manafold-p12-2b` cloned, origin repointed at GitHub, rebased on
`origin/main` (Upheaval 0773756, zhaozhou d820b574).

Baseline `manafold-probe.exe` on unmodified main:

```
u02-probe: slot 13 (200 keys): min clearance 19 mm at key 75 sub 1 - FAIL
u02-probe: slot 13 DECLARED CONTACT keys 78..156 (+2-key apron):
           deepest vertex -87 mm (declared -25, accepted -60..-5) - FAIL
u02-probe: CLEARANCE VIOLATED (< 40 mm)
```

**This is not mine and it is not new to this lane** - it is Wave 1's ROUND BODY
(A1) moving which vertex is deepest under the headstand, exactly the way pass 6
moved it last time (`kTrickPlantRootMm`'s own comment records that event). The
ground-contact probe on main is red. I have to fix it or I have no usable gate
for the deaths, so I am fixing it: `kTrickPlantRootMm` is the declared knob and
it goes up by the measured shortfall.

Geometry I need for the deaths, read off the same run: slot 7 (the rest still,
root 1250) reports 510 mm clearance, so **the lowest vertex sits 740 mm below
the root at rest**. A corpse resting with 25 mm of declared penetration wants a
root near 715 mm.

## 2B-1 — the plan I am authoring to

New clips, all ADDITIVE (2A is in the same file; no existing builder is
restructured):

| slot | clip | direction |
|---|---|---|
| 17 | `manafold-death-drop`   | D9 §11.2 - the owner's own mechanism |
| 18 | `manafold-death-gutter` | D9 §11.2 - the second, distinct approach |
| 19 | `manafold-lasso`        | D9 §15 - thrown by the antennae, made of mana |
| 20 | `manafold-blown`        | D9 §11/D5 §7 - "blown high up in the air" |
| 21 | `manafold-taunt3`       | D9 §11 - the nodule-vocabulary taunt |

## 2B-2 — built, gated, and three things the instruments caught

Five clips in (17..21), all appended. `manafold-probe` extended with the death
contract: per-strike contact, the airborne crash bound, the eternal rest's
declared penetration, and the deform-stops check taken off the PRODUCTION
`deformation_sample` stream. Three failable legs, all witnessed failing; control
green, rc 0.

**Three faults found, none of which a passing gate would have shown:**

1. The headstand was RED on unmodified main (-87 mm vs -25 declared).
2. The corpse breathed ONCE, at the loop seam, because the production deform
   stream interpolates the last key toward key 0. Fixed with an opening hold.
3. **The first bounce returned higher than the fall** — restitution 2. Every
   gate passed through this; the TRAJECTORY PLATE is what showed it.

Plus one I introduced and the gate caught inside ten minutes: reducing the blown
clip's tumble left it nose-down at the catch, 27 mm of antenna in the dirt.

**IN PROGRESS when this line was written:** blown re-rendering; the two deaths
still need a re-render against the corrected apex tables. Next: contact sheets,
webms, findings.
