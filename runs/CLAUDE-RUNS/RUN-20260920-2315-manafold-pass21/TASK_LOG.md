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

### 2026-09-21 00:00-04:00 - IMPLEMENTER packet (sole Opus, no sub-agents, no Qwen)
- Built the RODS AND BALLS rig from P21-ARCHITECTURE, in the staged order, running each stage's falsifying experiment before moving on. Deliverable: `P21-IMPLEMENTATION.md`.
- Stage 1 falsifier PASSED: `mmeshcheck` CLEAN on both rigs (34 meshlets / 2800 tris, identical counts), Still shows 0.01 deg worst rod turn and 999.7 pm of articulation at the balls.
- DEVIATION from the architecture, argued in the doc: the ring count stays 64 (the design budgeted ~70). `kLoopRings` sizes an array and keys a ring in a dozen instruments; changing it would have handed all of them a stale ring table at once. Only the station LAW changed, and every consumer reads `loop_ring_station_at()`.
- Stage 4 (roll-stable aims) NOT MADE: the blade-roll falsifier passes without it (5.10 deg/sample against an 8 deg bound; pass-19 and pass-20 read 4.84). The 8.6 deg residual the design cited was a POLYGON CORNER ring, which rods does not have. The first version of that trace read 88.9 and was the instrument folding at 90, not the rig -- both rigs agreed on the absurd number, which is the tell.
- THE LOOK CHANGED TWO SHIPPED VALUES and the gates could not have found either:
  * the balls did not READ at the architecture's default radii -- mrod said ALL LEGS OK on a frame where the antenna looked like a bent wire. A four-rung 8x ladder against the Side sheet chose 1.4x.
  * the bigger foot then broke Trick's plant (approach 32 mm against a 40 mm floor). `kTrickPlantRootMm` -- the lever passes 6 and 12 used -- moves the plant depth 1:1 but the approach only 3 mm in 16, because build_trick pivots about the planted support. Shipped: B's ball at 1.23x, sized to the contact it makes, plus a rig-dependent plant root landing on exactly the declared -25 mm. No floor lowered, no window widened.
- Gates: mrod R6-R10 NEW and committed (`tools/reel/manafold_rodgate.cpp`, `build-direct.sh` target `mrod`), all legs OK over 2858 posed samples, SIX controls fired. mrear RC=0 with R4's floor re-based and argued, and a SECOND hand-off operand added (the old one measures the intended stretch under rods). mspan RC=0 with three pass-20-ladder legs declared NOT APPLICABLE in its own output. mjointpub repaired (carrier F read 0/0 because no vertex binds to kBJunctionF under rods). mprobe/mmeshcheck/moutline/mshell/mnodule/meyesize/msmooth/mqa all pass.
- TWO INSTRUMENT FAULTS FOUND AND FIXED, both the pass-20 pattern: `mrear --fail-rear-strain` could not fire under rods (it moves a knob the rods path never calls) and mrod's R8 joint-step leg had no control that fires. Both now have controls that do.
- TWO mspan controls still cannot fire under rods and mspan says so itself; recorded as an open issue rather than left to be discovered.
- IDENTITY: `ZHAO_U02_RIG=pass20` reproduces the pass-20 bank on 22/22 subjects, frame counts and sequence_crc32c, against the committed manifest. Checked on 22 not 3 (pass 20's own lesson). `zhao-reel-cel --crc` added so it costs no disk.
- NOT done, by instruction: no 22-subject bank render, no encode, no merge, no deploy.
