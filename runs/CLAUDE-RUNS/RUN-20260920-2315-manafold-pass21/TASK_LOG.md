# Task Log: RUN-20260920-2315 - [Describe objective here]

**Created:** 2026-09-20 23:15 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260920-2315-manafold-pass21/

---

## Objective


Manafold pass 21 from Owner Direction 22: the antenna reads as having too many joints — articulation must sit ON the carriers (balls), with each run between carriers reading as one smooth piece; the END part still spazzes; the pass-20 middle-ball knead is acceptable and must be preserved; the whole antenna must read smooth in motion. Expected to be an architecture pass, not a tuning pass.

---

## Progress Timeline

### 2026-09-20 23:15 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260920-2315
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

### 2026-09-20 23:15 - Started
- Branches `manafold-pass21` created in both repos from the production-verified pass-20 mains (Zhaozhou `4bcf83db`, Upheaval `343c4ed2`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-22-2026-09-20.md`.
- Sequence: architect (read-only) -> implementer -> independent review, one agent at a time. No Qwen (reserved for another agent); GPT/Codex quota exhausted until 2026-09-25.
- Coordinator note: pass 20 found FIVE wrong-operand instruments. Treat every green gate in this pass as a claim to be fired, not as evidence.

### 2026-09-20 23:15-23:55 - ARCHITECT packet (diagnosis + design, read-only on production source)
- Built the cel renderer, mrear and mspan direct into `.tmp/p21-arch` (md5s in `P21-RECEIPTS/binaries-md5.txt`); `mhinge` does not compile on this tree (`manafold_clips.h` uses `std::memcmp`/`strcmp` without `<cstring>`) -- recorded in P21-ARCHITECTURE §2.6.
- Rendered inspect/channel/taunt with production ink; every-frame sheets; frames chosen by the probe's worst rings, not by index. 12 images looked at, notes after each in `P21-LOOK-NOTES.md`.
- Wrote and committed a per-ring curvature probe (`P21-PROBES/manafold_p21_curvature.cpp`): turn, shear and turn-rate per ring on the POSED skin, 60 Hz, five clips. It LOCATES: the corners sit 1-3 rings BEFORE each ball (rings 17-19, 24-26, 32-34) with an S-jog; the front rod bends mid-run at ring 12 (the kBNeck blend); the rear is a five-corner polygon (36/37, 45/46, 49/50, 52, 55) with rings sheared 50-90 deg.
- Attribution experiment: REAR_BOW=legacy drops the worst rear turn rate 28.5 -> 1.2 deg/sample and erases rings 45-50 to 0.0 deg; muting End+C ambient makes it WORSE. The End spazz IS the pass-20 bow (sqrt onset at the taut crossings + tangent swing past 90 deg in deep slack), rendered as a polygon.
- Owner addition received via the coordinator mid-packet ("don't let actual antennae parts bend, just stretch. the bending is at the ball joints"); the design was written to it: RODS AND BALLS on the existing 27 bones, rigid spheres at the joints, rods bound parent-joint -> pure-translation helper, rods end inside the balls (buried cones), rear rod = the chord (no bow), roll-stable rod aims, selector `ZHAO_U02_RIG=rods|pass20` for exact-off, gates R6-R10 on the posed surface.
- Deliverable: `P21-ARCHITECTURE.md`. Nothing in `tools/reel` was edited.
