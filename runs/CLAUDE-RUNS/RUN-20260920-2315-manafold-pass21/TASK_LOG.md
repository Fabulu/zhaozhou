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

### 2026-09-21 00:45-01:15 - REVIEWER + QA packet (independent, sole agent)
- Rebuilt all 13 gate/reel binaries from the pass-21 tree myself; every figure in
  `P21-REVIEW-QA.md` is re-measured, none quoted from the implementer's receipts.
- VERDICT **FIXED**. Three real defects found and repaired:
  * **A raw NUL byte committed in `manafold_clips.h:3121`** (a literal 0x00 inside
    a char literal where `'\0'` belongs). Value correct, tooling poisoned: git
    called the file BINARY so the pass's largest diff was unreviewable, git
    stopped normalising its line endings, grep/ripgrep silently skipped it, and
    g++ warned 12x unread. Fixed; diff is now one line, rebuild has 0 warnings.
  * **`mspan::ring_station_map()` held a SECOND COPY of the uniform station law** --
    the exact duplicate the implementer removed from mrear this pass, left in the
    sibling. Under rods it keyed 64 rings the mesh does not have, so
    `mutate_rigid_span` rebound nothing and `minimum_free_ring_step_y` returned
    **+inf** from an empty set. The gate printed `min ring dy inf mm` on all eight
    G4 legs and reported PASS. **EIGHT controls were silently dead**, not the two
    reported -- including the only control for the G5 compaction leg that R4's
    re-based floor cites as the thing holding the line. `--fail-rigid-span C-E`
    was reported as firing while its own committed receipt says UNATTRIBUTED.
  * **The overcompact mutant was sized to the RETIRED gradient length**, shorter
    than the rod on three of four spans, so it compacted without inverting.
  Fixes are gate-side only; identity proves they moved no rendered byte.
  **29 of 31 mspan controls now fire attributed under rods (was 21).**
- Claim 1 VERIFIED and strengthened: mrod's gate hard-codes 6 slots (mrear
  enumerates the bank), so the headline was 8 of 24 clips. Re-run over **all 24
  slots / 9,700 samples**: rod turn 0.02, sag 0.02 mm, joint-on-ball 999.7 pm all
  hold; R9 947->938 pm and R10 0.059->0.107 % on the unsampled slots, both fine.
  The 64-ring deviation is sound -- no run or ball is short of rings.
- Claim 3: the stage-4 skip is SAFE, but the reported comparison was invalid --
  **mrod cannot select the bow at all** (`ZHAO_U02_REAR_BOW` is parsed per-main,
  not in `apply_knead_dip_env`), so the "pass-19" and "pass-20" rows are ONE
  configuration measured twice; proved by byte-identical output with the env set
  to legacy, arc and garbage. Like-for-like on 24 slots the comparison REVERSES:
  rods **6.98** deg/sample vs pass20 **7.50** against an 8 bound. Rods beats the
  shipped rig; margin is 1.02 deg, a watch item, not a defect.
- Claim 5: **22/22 identity byte-exact** on the NUL-fixed build, and I ran the
  positive control too -- **22/22 CHANGE** under rods. Knead 19/19, margin +29 mm.
- Claim 6: plant -25 mm exactly, B owns 140/140, clearance floor and contact
  window untouched in source. B's 1.23x is required: at its 100.2 deg worst the
  crotch floor is 69 mm against R 85 -- B is the only ball whose rods stay inside.
- R4-FLOOR JUDGEMENT: the retirement is justified (worst rail is at ring 51,
  INSIDE rod C-End, where R9 applies; no bound moved; G5 clean) but **0.12 is not
  what its own argument yields** -- kSpanCompactionMinPm[3]=-700 implies ~0.300,
  so the floor is 2.5x looser than its derivation and cannot fire. Floor NOT
  moved; instead the guard behind it was made able to fire. Recommendation for
  pass 22 recorded.
- 17 controls fired by me across four gates, including `--fail-rear-strain`
  (FAIL R4 STRAIN) and R8's invented `--fail-joint-step` (fires exactly one leg).
  Caveat recorded: mrod has 6 control NAMES for 4 configurations.
- VISUAL QA (production ink, 9 clips on BOTH rigs, frames chosen by badness from
  mrod's CSV via a committed probe): (a) straight rods YES, (b) End calm YES --
  speed -44 % and jerk -41 % on inspect, and pass20's trace is jagged spikes where
  rods is a clean oscillation; (c) **the character change is a large improvement**
  -- pass20 has NO legible structure at all in half the orbit while rods reads as
  a jointed limb; it is more geometric and that is what makes it legible; not a
  faceted chain, not a bent wire; (d) no visible crotch even at A 150.4 / C 152.1
  deg (worse than declared) -- reads as a folded hinge; (e) knead preserved, both
  rigs dip at the identical key; (f) no new faults across eight clips.
- Full matrix re-run in ONE invocation on my build: 12 gates + both exact-off
  legs, all RC=0. Receipts in `P21-QA-RECEIPTS/`, looks in `P21-QA-LOOKS/`.
- 2.0 GB of `.rgb` render intermediates purged after the looks (CLAUDE.md).
- NOT done, by instruction: no bank render, no encode, no merge, no deploy.
