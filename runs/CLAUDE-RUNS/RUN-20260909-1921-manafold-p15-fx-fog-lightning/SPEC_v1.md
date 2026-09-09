# SPEC v1: [Describe objective here]

**Run ID:** RUN-20260909-1921
**Created:** 2026-09-09 19:21 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

[What success looks like]

---

## Scope

**In Scope:**

- [Item 1]

**Out of Scope:**

- [Item 1]

---

## Constraints

- [Constraint 1]

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

---

## Open Questions

- [Question 1]

## LANE-FX (pass 15) — what this run was for

**Brief:** OWNER-DIRECTION-11 §2.3 (the fog) and §4 (the lightning).

**Acceptance, stated up front and not moved:** a named frame of a shipped clip,
at native, that a person describes in the owner's own words. Gates are
regression protection underneath that.

* **THE FOG** — *"the outer body part is made of a thick fog that gets less thick
  the nearer to the outside it goes… it no longer looks like clipping, just
  going into fog."* Acceptance: `hit` f28 at zoom, the lens sinking into a
  visible haze layer with no hard cut; and `hover` at native wearing a gas rim
  thicker than any outline, with the ink intact.
* **THE LIGHTNING** — *"connected by white lines of light with blue shimmer
  surrounding them… they need to make the shapes."* Acceptance: a person
  watching says unprompted that it is drawing a shape out of lightning; white
  lines, a live blue edge, darker air around them, more sparks; and the green
  fold still there as its own separate thing.

**Deliverables:** the mechanism, its knobs, a ladder per experiment axis from ONE
binary on both backdrops with a deliberately-too-far rung, plates with
provenance, and `PASS-15-FINDINGS-FX.md`.

**Constraints honoured:** `--clean` builds only; the build's own exit code, never
a pipeline's; frames read only through `rgbframe.py`; contact sheet to find, 2x
crop to confirm; presence metrics calibrated on a known-negative; processes
identified by command line and killed by PID (`taskkill /IM` never used); one
build and one renderer at a time; **no encode** — the publish wave owns that.
