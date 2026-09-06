# Task Log: RUN-20260906-2132 - Manafold pass 11, STAGE L: the particle experiment lane

**Created:** 2026-09-06 21:32 UTC+02:00
**Status:** In Progress
**Lane:** C:\programmieren\zencrifice\manafold-p11-L (own clone; NEVER touches
manafold-p11-impl or the main working trees)
**Branch:** manafold-p11-particle-lab on BOTH repos. Never merged to main --
the pass-11 implementer owns manafold_fx.h / manafold_art.h on main this pass.

---

## Objective

Owner Direction 8 section 5: "Particles need to be better too. Experiment with
them. Different shapes, different quantities, constellations, sizes of the
particular particles themselves. Folding still needs to fit into that too so
that's an unknown."

Four axes -- particle SHAPE, QUANTITY, CONSTELLATION, SIZE. Produce a LABELLED
VARIANT SHEET per axis for the owner to pick from, plus PARTICLE-LAB-FINDINGS.md
with a proposed-constants block. SHIP NOTHING. Do not rank by measurement.

HARD CONSTRAINT: every variant is rendered FOLDING (inside the knead window),
never as a still at rest. MIST OFF -- judging particles through the outgoing
mist measures the mist.

---

## Progress Timeline

### 21:32 - lane created
- Cloned zhaozhou + Upheaval from manafold-p11-impl, remotes repointed to
  origin, branch manafold-p11-particle-lab off origin/main on both.
- zhaozhou head 5b316231, Upheaval head 42b21f7.

### 21:40 - read the direction
- OWNER-DIRECTION-8 section 5 and PASS-11-ARCHITECTURE "STAGE L".
- Read the constants: kFoldEdge* family (manafold_art.h ~1250), kStencilPts and
  the six stencils (manafold_fx.h ~876), the mote knobs (manafold_art.h ~1115).

### 21:45 - CONFIRMED THE DEAD KNOB
- `grep -rn kFoldEdgeCoreGainPm tools/` returns EXACTLY ONE line: its own
  definition at manafold_art.h:1252. No reader anywhere in the tree. The edge
  core stamp in manafold_fx.h pushes a hard-coded 1000. Confirmed.

### 21:50 - built the instrument (lane-only, NOT FOR MERGE)
- Twenty builds of the reel is not a lab. So every lab knob became a runtime
  override read once from the environment, defaulting EXACTLY to the shipped
  constant, so an unset binary is the shipped look. Plus two mode enums that are
  not single numbers (PL_PSHAPE, PL_PCON) and PL_FRAMES=lo:hi, which limits only
  the DISK WRITE -- every frame is still simulated and rendered in order, so the
  written frames are bit-identical to the same frames of a full render and the
  fold timeline / drag ring buffer / area EMA are untouched.

---

## Subagent Spawns

*none -- this lane does its own work*

### 22:00 - the first plates, and the axis nobody asked for
- Baseline render (400 frames, U02_FOLD_DEBUG=1) fixes the knead window from the
  log rather than assumption: seg=1 gather f52, seg=2 hold f124, **seg=3 knead
  f239-371**, seg=4 release f372. Peak agitation in the window is f287 (740
  against a 58 mean); morph runs 3->0 (crescent to ring) across it.
- Plate frames chosen from that: f250 / f287 / f320 / f368.
- MIST OFF confirms the shapes ARE drawn (Stage S reproduced). What they read as
  is a scatter of fuzzy balls with a WHITE SMEAR through the middle.
- The white is the EDGE HALO'S RADIUS. Ablated it five ways; only the radius
  moved it. Q12 and Q90 put the same white on screen, so it is not the motes.

### 22:20 - two traps, both in CLAUDE.md, both hit anyway
- A patch script asserted on an INDENTATION mismatch (6 spaces, not 8), printed
  a traceback, and the build in the same command rebuilt unchanged source and
  exited 0. Two variants rendered against a binary that had never heard of their
  knobs and came back looking like the baseline -- an honest-looking negative
  result. Caught by the tell, deleted, re-shot.
- The retry's LINK failed ("cannot open output file ... Permission denied", a
  render still held the exe), printed BUILD_RC=1, and the shell exited 0 on the
  tail. Read the BUILD's exit code, not the pipeline's.
- Fix: kill every zhao-reel-cel.exe, rebuild (BUILD_RC=0), re-run, and verify
  each variant against the binary's own `PARTICLE-LAB <KNOB>=<v>` stderr echo
  rather than trusting the env line.

### 22:45 - delivered
- 7 plates x 2 (native + 3x zoom) in Upheaval/creature/Manafold/particle-lab-plates/
- PARTICLE-LAB-FINDINGS.md: axis-by-axis, an explicit "what folds and what does
  not", a ranked recommendation (K7) with the plate that justifies it, a
  proposed-constants block, and what the lane could not do.
- MANAFOLD-INDEX.md points at it.
- Pushed to manafold-p11-particle-lab on BOTH repos, each verified from outside
  the lane with `git fetch` + `git branch -r --contains`. NOT merged to main.

**Status: complete.**
