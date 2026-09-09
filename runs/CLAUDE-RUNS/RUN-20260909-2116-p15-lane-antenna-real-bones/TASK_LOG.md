# Task Log: RUN-20260909-2116 - [Describe objective here]

**Created:** 2026-09-09 21:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2116-p15-lane-antenna-real-bones/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 21:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-2116
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

## 2026-09-09 — LANE-ANTENNA, the elbow: the plan's fix is refuted, a better one measured

**Baselines before touching anything** (build-antenna, --clean, all three green):
* `mprobe`  closure sweep@700 **1090**, bank **982** (rim gate 1120). Surface
  crossing (entering) at **arc 2647 mm** — the re-entry ball really is at the
  skin, measured not assumed.
* `mspan`   PASS, 0 failures. G3 asserts buried arm tip motion at ceiling **0.00 mm**.
* `mnodule` PASS, solo table reproduces (A alone 198/273/253).

**PASS-15-PLAN §3's proposed fix is REFUTED**, by a sweep whose control row
reproduces the shipped 1090/982 exactly (`probes/arcsweep.sh`, committed):
moving hinge D onto the re-entry ball at 2660 fails at EVERY arm length —
best point 2241 sweep / 1718 bank against a 1120 gate, ~60% over, two-sided
with no feasible value. The stretchy spans cannot pay for it for three
independent reasons (lanes map to spans neck->A,A->B,B->C only; the effect is
a VERTEX one and mspan G3 asserts it moves the arm tip 0.00 mm BY DESIGN; and
at its 300 pm ceiling it is ~10x short of the missing reach).

**The fix that DOES work, and it uses the rig's own existing precedent:**
give hinge D **ball C's pivot**, exactly as kBJunctionF and kBNeck already
share one (kLoopArcMm[0] == 0). `kLoopArcMm[4] 380 -> 0`, `[5] 1160 -> 1280`.
* the bend is then AT a ball, and the WHOLE return limb from ball C into the
  body is ONE straight rigid run — both halves of what the owner asked for;
* closure gets BETTER: worst leg **955** vs the shipped **1090** (gate 1120),
  a 5.5x larger margin.

**IN PROGRESS when this was written / next steps:** the geometry is proved,
the skinning is not built yet. Four edits owed:
1. art.h: kLoopArcMm[4]/[5]  (done first, it is the load-bearing one)
2. art.h: NEW kLoopTaperStationMm[7] frozen at the SHIPPED positions —
   the band's taper currently rides the BONE stations, so moving a bone
   silently re-profiles the band (checklist item 24). Freeze the shape.
3. model.h: drop the (B,C) ladder rung; rings past C go (B, D) so ball C
   becomes a pure parent exactly as kBJunctionF is.
4. probe.cpp: F.1 continuity must accept the SECOND by-design coincident
   pair, and still fail on a third.

## 2026-09-09 — LANE CLOSED

All four owed edits landed, plus the ball work and the re-aimed gate.
Findings: `Upheaval/creature/Manafold/PASS-15-FINDINGS-ANTENNA.md`.

**Commits, all on origin/main and verified from outside the lane with
`git branch -r --contains`:**
* zhaozhou `9a76edf8` the elbow onto ball C + the taper freeze
* zhaozhou `5fafdeb5` the Swallow + span-gate G6 + nodule-gate demotion
* Upheaval `9084d71`  findings, 7 plates, 2 committed probes

**Final gate state:** mprobe OK (closure 955/923 against a 1120 gate, was
1090/982 — the margin went 30 -> 165), mspan PASS including the new G6,
mnodule PASS. G6's failable leg witnessed: `--fail-nolanes` takes the gated
clips 83.5 -> 13.3 mm and fails all three.

**The thing worth carrying forward:** I rebuilt the exact fault this lane was
sent to fix, twice, inside the new gate — first by measuring the inter-ball
VECTOR (which scored taunt3, the one clip that reads, LOWEST in the bank),
then by keeping a pair that crosses a hinge. Both were caught by the failable
leg and by nothing else. And the obvious known-negative — a clip that does not
move — is the one that certified the broken version, because a still clip
cannot tell "measures independence" from "measures anything".

**Housekeeping:** no processes left running (checked by command line, not by
name). Both trees clean. 3,056 `.rgb` frames / ~814 MB of render intermediates
in this lane; `git clean -fdX` is required as well as `-fd` to reclaim them.
**The lane can be deleted** — nothing exists only here.
