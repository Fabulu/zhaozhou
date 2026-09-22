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

### 2026-09-22 ~15:30 - values chosen by eye, and two process findings

**Chosen by ladder, each at the frame the knob changes most (framediff, committed):**
bolt clearance 46 mm (80 restyles, 60 is the rung below, 46 reads best and is
gate-clean); Hover Front gain 1350 (1700 changes the performance); eye ambient
600 (800 is the first rung that draws attention); Hover rear ambient 170.

**FINDING 1 -- an existing knob that only a gate could read.**
`ZHAO_U02_REAR_CARRIER_CALM_PM` has existed since pass 20 as the lever for
carrier C's always-on rotation and was parsed in `manafold_rear_audit.cpp`
ALONE. Three complete renders at 1000 / 500 / 250 came back byte-identical on
all 600 frames -- the signature of a knob the reel cannot see. It moved mrear's
reading of the creature and nothing that ships. Moved into the shared
`apply_knead_dip_env`; 1000 is the authored value so the repair is byte-neutral,
proven by a 22-subject re-render after the rebuild.

**FINDING 2 -- DO NOT EDIT A SHELL SCRIPT THAT BASH IS EXECUTING.**
I appended two selector legs to `gatematrix_p24.sh` while the matrix was running
it. Bash reads a script incrementally BY BYTE OFFSET, so the insertion shifted
everything after it and bash resumed at the wrong place: the identity leg
`e-item2` ran and reported TWICE, and there is no way to know what else the
shift skipped. The run was discarded, every process killed after being
identified by command line, and the matrix re-run from a FROZEN COPY of the
script in `.tmp/` so no later edit can reach the file being executed. This is
the live-tree trap (`QUARTUS_GOTCHAS` SS11) in a shell rather than in Quartus,
and the tell was a duplicated PASS line -- something a tally alone would have
counted as one more green.

**Rebuild verified byte-neutral:** all 14 binaries rebuilt after the last source
edit, and the 22-subject shipping bank re-rendered to 22/22 identical CRCs.

### 2026-09-22 ~17:20 - CLOSED

**Matrix 224 / 224 PASS, 0 FAIL**, one invocation, from a FROZEN copy of the
script so no edit can reach the file bash is executing.

It took three runs and each failure was the instrument rather than the creature:
1. the script edited mid-run (byte-offset shift; a leg ran twice);
2. the expected CRC files carrying a `unique colours` column the comparator
   stripped from only one side;
3. e-item3 red on nothing but SORT ORDER -- the shell's collation put
   manafold-taunt2 before manafold-taunt in one invocation and after it in
   another. Every sort in the matrix is `LC_ALL=C` now. A gate whose verdict
   depends on the locale goes red on someone else's machine for a reason nobody
   can see in the creature.

**Byte identity, all 22 live subjects, per item:** everything off = 22/22
identical to pass 23; item 1 alone and item 2 alone each move exactly
crackle/hover/inspect (19/22 identical); item 3 alone moves 19 and leaves
**curious, startle and taunt III byte-identical** -- the proof that the authored
expression beats were not given a floor.

**Purged** 10,992 stale `.rgb` files, 2.83 GB, from a sibling creature working
directory via `tools/maintenance/purge_render_intermediates.py` (dry run
recorded first).

**Not done, by instruction:** the 22-subject bank for publication, the encode,
the merge, the deploy. The coordinator sends the review/publish packet.
