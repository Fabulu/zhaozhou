# Task Log: RUN-20260827-2140 - [Describe objective here]

**Created:** 2026-08-27 21:40 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260827-2140-zixxtrixx-round-skull-and-v2/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-27 21:40 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260827-2140
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

## 2026-08-27 21:45 — plan settled after three coordinator updates
Scope grew mid-briefing: (1) dorsal pink must cover the whole upper surface
(body + head crown, ~40-50% of circumference, judged at the ~15 deg camera);
(2) eyes more front / pupil rotation / colour AFTER round skull and AFTER
direct colour; (3) Gouraud REVERSED BACK IN: qformats.md §8 (frozen) and
blocks.yml purpose lines already ratify per-vertex lighting + gradients —
zref implementing it is the oracle doing its job. Keep the per-row
barycentric re-evaluation model in rast.cpp (do NOT switch to a setup-side
plane form).
Order: goldens -> head ball + junction -> pink -> eyes -> RGB565+bilinear+mips
-> normals+Gouraud -> poly reduction -> amendments/proposal + worklog.
Baseline binary built from clean tree at f8f6681.

## Head: the cranium is a BALL (kBallNum)
- Goldens committed to Upheaval/creature/Zixxtrixx/golden/ FIRST (clip bytes,
  per-key pose CRCs, probe, 60 Hz sheets, source commit) — Wave 0 done.
- kEyeBulgeNum 85 -> 22; new kBallNum=280 envelope swells EVERY axis,
  smoothsteps from station 1, peak at 4 (5 dug the skull rear 75 mm into the
  dive stroke), falls to ZERO at the junction ring (kHeadEnd) — the skull
  grows out of the neck; junction ring stays bit-identical with the body part.
- head_ring() is shared by mesh builder and probe: the probe now measures the
  swollen vertical radius (honesty rule kept: in-plane axis).
- Attitude RE-SWEPT after the geometry change (evidence/sweep1-headzoom.png):
  -12000 still carries the skull level. KEPT, picked off the sheet.
- Overlap allowances re-authored on worst-key RENDERS (idle 80, attack 175,
  fall 200 — the ball nests deeper by design, like the sheet). Probe exit 0.
- Frozen check: pose CRCs + clip bytes IDENTICAL to goldens; probe bands
  idle [-8..-3], walk [-13..+10], attack -426 @56, fall min 584 — all exact.
- reel --check: all sequence CRCs match.

## Dorsal pink covers the whole top
- body_tile half 4.5 -> 13.0 texels (~41% of circumference), neck thinning
  deleted (it compensated the old 26-deg camera, which is gone).
- head crown pink_half ramps 3->13 from brow (y=20) to mid-skull, holds 13 to
  the junction -- one continuous pink top across head and body.
- Judged on renders at the current ~15 deg cameras (evidence/pink1-*.png):
  tq reads as the whole top; side reads as a broad dorsal region; front reads
  blue face in a pink cap, green chest -- Front.png's layout.
