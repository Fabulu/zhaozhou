# SPEC v1: FIELD v3: prepared four-wide vector fabric

**Run ID:** RUN-20260827-1747
**Created:** 2026-08-27 17:47 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

v2 frozen as oracle; contracts amended to the v3 architecture; an exact
software planner whose FPLAN reference executor matches zfield::interpret
bit-for-bit on every output, every saturation lane, rcp0, boundaries, random
legal programs and all three committed Earth programs; then five fitted
probes each meeting its II / timing target with mutation-swept tests.

---

## Scope

**In Scope:**

- Phase 1 contract amendments and regenerated cost model
- Phase 2 planner (C++), generated uop translation, differential tests
- Phase 3 probes with fits and sweeps (as far as fits allow)

**Out of Scope:**

- Phase 4 (composed Earth machine) and Phase 5 (console integration)
- Any approximation of distance/reciprocal/curves
- Hardwiring the three committed Earth spells

---

## Constraints

- Canonical->uop translation GENERATED from the canonical operation table
- Preparation uses the same zref:: fixed-point primitives as the interpreter
- ctest -L fast green + ledger check green before every commit
- Only stage this run's files; git pull --rebase before push

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

---

## Open Questions

- [Question 1]
