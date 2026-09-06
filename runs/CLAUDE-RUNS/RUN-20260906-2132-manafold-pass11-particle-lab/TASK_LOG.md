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
