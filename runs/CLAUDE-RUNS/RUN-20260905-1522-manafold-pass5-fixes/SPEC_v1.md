# SPEC v1: Manafold pass 5 — bounded fixes after the pass-4 gates

**Run ID:** RUN-20260905-1522
**Created:** 2026-09-05 15:22 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Fix QA's ranked items 2-10 from RUN-20260905-0933's QA.md (the
authoritative verdict), judged at native 384x240 on the shipping rig;
prove Zixxtrixx byte-identical from a pristine baseline; publish once with
the inspect poster present.

---

## Scope

**In Scope:**

- Items 2-10 of QA's ranked list (antenna coverage, knead coverage, build
  integrity, eye gate re-baseline, dead knob, ablation-gate retirement,
  white ring, loop seam, inspect poster).

**Out of Scope:**

- **Item 1, shape legibility at native — OWNER DECISION.** No mote
  shrinking, no stencil enlargement, no camera move, no pocket widening.
- Any change to Zixxtrixx.
- The hardware lane (C:\programmieren\zencrifice\zhaozhou) — live agent.

---

## Constraints

- QA.md wins over REVIEW.md where they disagree, unless refuted from own build.
- build-direct.sh only, one target at a time; never cmake --build.
- Frames read with tools/reel/rgbframe.py only.
- Every build's success confirmed in the same command that uses it.
- Never git add -A; never run sacengine.
- Zixxtrixx identity proven by sequence CRCs from an own pristine baseline.

---

## Don't Retry

- Feeding the smear from opaque CORES (white trail — the ramp top is
  near-white). Landed on: halo colour at core footprint.
- kFoldFeedBasePm 760 (whiter still, same reason). 520 stands.
- Raising kEyeDeepMm for stand-off (buries the star, breaks containment).
- cmp-loop byte compares under Git Bash (5+ min for 1136 files) — hash in
  python instead.
- git worktree checkout into the deep scratchpad path (MAX_PATH failure)
  — use short root paths like /c/mf5wt.

---

## Open Questions

- Item 1: owner's call between "bigger particles" and "nameable shapes".
