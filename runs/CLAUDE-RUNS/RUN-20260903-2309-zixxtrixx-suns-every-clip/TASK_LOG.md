# Task Log: RUN-20260903-2309 - Direction 29: a sun in every clip, visibly lit, distinct colours; publish tonight

**Created:** 2026-09-03 23:09 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-2309-zixxtrixx-suns-every-clip/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 23:09 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-2309
- Created working directory
- Initial context: 21 of 22 bank clips have no point light; approved additive term does nothing for them. Read OD-27/28/29, 08-LIGHTING, additive-term report, RUN-2144 log.

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

## 2026-09-03 23:55 — sun system implemented; v1-v4 tuned by eye

Design: ZixxSunSpec per clip -- one point source tracking the terrain-snapped
instance at a fixed mm offset, ABOVE (+8.6 m) and to one camera-side flank
(azimuth authored per cam_yaw family), inner radius 15 m so the whole body AND
the salto dummy sit inside attenuation 1: a pure directional lambert sun; the
never-enters-inner-radius trap impossible by construction. Installed in compose
as a 1-source array with the additive gate raised subject-scoped (gate global
still default OFF); the Cool Cross rig underneath is untouched. ZIXX_SUNS=off
kill-switch = revert demo. Moving-light subjects keep their 4-source rigs.

Proof so far: pristine HEAD (c23c6a63) and my build with ZIXX_SUNS=off give
identical CRC on death (0x4B7B3E3E). NOTE: RUN-2144's evidence table CRCs
(e.g. death 0x221453EA) do NOT reproduce on pristine HEAD either -- consistent
with that run's own bookkeeping finding; my identity claim is against pristine
HEAD of this tree, which RUN-2144 separately showed matches the published webms
to encoder noise.

Tuning by eye at native + 2x pairs (base over sun):
* v1: mult dominants 1.25-1.55 -- stripe/crown flooded magenta on warm suns.
  Learned: the 1.0 gain ceiling means big mults just clamp; the ADD carries.
* v2: mults cut to ~0.62x -- barely moved (ceiling, as predicted).
* v3: adds raised ~1.5x (dominant 0.45-0.63). THE READ ARRIVES: walk's azure
  turns the head blue and lays a cool sheen down every lit face; death's deep
  red re-hues the head hot pink-red with a gold-olive lit back (red add over
  rig-lit green pigment = gold: physics, same as RUN-2144's lesson).
* v4: idle gold raised (620/330/30), taunt rebalanced rosier (560/25/380 --
  R+B adds on green were washing grey-pink).
