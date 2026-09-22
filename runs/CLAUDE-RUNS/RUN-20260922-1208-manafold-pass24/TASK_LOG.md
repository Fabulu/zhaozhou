# Task Log: RUN-20260922-1208 - [Describe objective here]

**Created:** 2026-09-22 12:08 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260922-1208-manafold-pass24/

---

## Objective


Manafold pass 24 from Owner Direction 25: (1) Hover only - calm the back ball, lift the front slightly, every other clip byte-identical; (2) lightning-through-antenna COMPARISON EXPERIMENT - 3D bolt avoidance on Crackle and Hover, depth-splitting on Inspect, measured first; (3) subtle ambient eye acting (gaze direction + size) on the ordinary clips, well below Startle/Curious.

---

## Progress Timeline

### 2026-09-22 12:08 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260922-1208
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

### 2026-09-22 12:08 - Started
- Branches `manafold-pass24` created in both repos from the production-verified pass-23 mains (Zhaozhou `1d449717`, Upheaval `e965dc5c`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-25-2026-09-22.md`. The owner asked to discuss before work; the lightning approach was chosen in that discussion (3D avoidance, with depth-splitting on Inspect as the comparison).
- One Opus implementer, then a bounded independent review that publishes on PASS. No Qwen (reserved); GPT/Codex quota exhausted until 2026-09-25.
- The lightning item is deliberately an experiment on three clips; the winner is rolled out in a later pass.

### 2026-09-22 ~14:00 - Item 2 measurement done, mechanisms built, gate green

**THE MEASUREMENT (item 2 first, as instructed).** `tools/reel/manafold_boltgate.cpp`
(mbolt), committed. 3D segment-vs-capsule against the pass-21 rig's own four rods
and four ball spheres, every key and midpoint of every live subject, no render.
`P24-RECEIPTS/mbolt-census-before.txt` is the pass-23 state.

**It is a GEOMETRY fault, overwhelmingly.** 12-17% of drawn bolt segments genuinely
occupy the antenna's volume. Crackle is the worst by count and by reach (5,685 of
42,364 segments, on 561 of 600 frames) - which is exactly the clip the owner named.
Every intersection in the bank belongs to the FOLD FIGURE's edge links; the free
lightning strands contribute ZERO.

**The mechanisms.** 3D avoidance takes Crackle 5,685 -> 0 and Hover 4,705 -> 0.
Depth splitting at N=4 quadruples the sprite count and quarters the spacing
(26.6 -> 6.6 mm) and changes partial occlusion NOT AT ALL: all 155 depth-straddling
segments were already drawn partly occluded at N=1.

**Three gate legs, three controls, all fired.** Two of the three controls were DEAD
on their first firing and both are recorded in the source: a split leg that was
green with the mechanism off, and a thin-rod control that moved the operand without
turning a leg red. Two wrong operands of my own were found the same way (the rod's
mean view depth instead of its depth beside the point; a sphere's surface instead
of a cylinder's).

Exact-off byte identity re-verified on 5 subjects after all fx surgery.
