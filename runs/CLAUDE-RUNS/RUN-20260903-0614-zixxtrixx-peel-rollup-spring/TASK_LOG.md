# Task Log: RUN-20260903-0614 - THE PEEL: roll-up spring per Owner Direction 25

**Created:** 2026-09-03 06:14 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-0614-zixxtrixx-peel-rollup-spring/

---

## Objective

Rebuild the Zixxtrixx jump/salto spring arming per Owner Direction 25 (THE
PEEL): contact leaves the ground at the front of the grounded stretch and
travels progressively backward until the animal stands on the end of its tail,
then a strong, quick compression (head further back than the published pass,
never past the tail), faster loading overall, smoothness kept, clipping
relaxed-but-declared for this pass.

---

## Progress Timeline

### 2026-09-03 06:14 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-0614
- Created working directory
- Initial context: Direction 25 replaces the six-pass "turn the whole body
  into the S" instruction with a mechanical peel/roll-up description

### 2026-09-03 (architect) - PLAN.md committed

- Read Direction 25 (binding), 24 and 23 (still standing as qualified),
  previous run RUN-20260902-1816's PLAN/TASK_LOG/REVIEW/QA/QA-2 and all five
  recon documents, CLAUDE.md; verified cited line numbers against the lane
  at dbc0deec.
- CENTRAL DECISION: the peel is a TRAVELLING SUPPORT - generalize
  spring_support_origin_raw / spring_root_from_quats_raw from the fixed
  kSpringPlantSegment prefix to an authored milli-station route
  spring_support_station_mk(arm) walking station 14 -> tail tip across the
  peel. Root compensation then holds each successive contact point at its own
  grounded-baseline position: contact never slides, the contact patch recedes
  along the resting footprint, and the tail-tip stand is structural (the D24
  alias trick, one level up). Pose-tables-only and lift-route-only variants
  rejected: a fixed station-14 plant makes station 14 un-liftable by
  construction and turns the tail's grounding into emergent arithmetic.
- Knots: grounded / peel-mid (220) / tail-stand (400, the unmoved
  entry-squash split) / collapsed (1000); tail rows frozen tail-stand ->
  collapsed = D24 law from sole contact, by construction. Entry = peel,
  squash = compression, so the deform stays correctly keyed with zero
  re-plumbing.
- Timing: settle 4 / peel 28 / gather 2 / compression 16 / hold 8 keys ->
  ground time 168 -> ~116 frames (~31% faster); RECON-5 floors re-derived
  for a two-beat motion (~92-frame floor).
- Staging: support generalization as a CRC-proof no-op first, instruments and
  still-image pose brackets before any clip wiring, retime on the old motion
  as its own checkable stage, then peel, then compression, then life/polish,
  then battery+encode+publish.
- Landmines: 18 entries with file:line, incl. the three probe gates fitted to
  the superseded motion (phase 1314, support 1759, stall 2502-2567), both
  zero-headroom bite declarations (spring -60/60, landing -53/54), the
  derived-not-absolute press-window rule, and the life wave's tail share
  wobbling the stand.
- Committed and pushed PLAN.md.

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

### 2026-09-03 (implementer) - Stage 0: baseline verified

- Fetched origin main both repos; zhaozhou at 49672fba (= origin/main).
- Built cel + probe into `build-peel/` (one target per call, fresh tree).
- Probe at HEAD: `ZIXX PROBE: PASS` (evidence/stage0 . . . probe-head.txt).
- Rendered zixxtrixx-spring-side + zixxtrixx-jump-one with explicit
  ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross:
  spring-side sequence CRC **0x1B1AEAB6** (== QA-2's published number),
  jump-one **0x9829FF4A** (== QA-2's table). The build IS the published one.
- Stage-1 bank reference: previous run's committed
  `evidence/qa2/qa2-bank-crc.txt` (22 subjects, rendered at d5949320; the
  only commits since are documentation, proven by the two CRCs above).
