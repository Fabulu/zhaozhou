# SPEC v1: Unnamed02 mana conduit — architecture plan

**Run ID:** RUN-20260904-0705
**Created:** 2026-09-04 07:05 UTC+02:00
**Status:** Complete
**Previous Version:** N/A

---

## Objective

Deliver `PLAN.md` in this run folder: the full implementation plan for
creature 02 (the floating mana conduit), decisive on the choices the two
recons left open, with the hook-chaining spike ordered first.

---

## Scope

**In Scope:**

- The plan document only: spike list, build path, form, motion, effects,
  named constants, verification, cuts.

**Out of Scope:**

- Any implementation: no geometry, no pose values, no builds, no renders,
  no publishing. No helper agents (serial run).

---

## Constraints

- Binding direction: `Upheaval/creature/Unnamed02/OWNER-DIRECTION-1-2026-09-04.md`
  (the creature FLOATS; no attacks, no gait).
- Inherit the recon findings (`RECON-FX-FINDINGS.md`,
  `RECON-NEWCREATURE-FINDINGS.md`) — do not re-derive.
- Never touch `C:\programmieren\zencrifice\zhaozhou` or `...\Upheaval`
  (hardware agent live there); this lane only.
- Explicit-path staging only; never `git add -A`.

---

## Deliverable

`PLAN.md` — see it for the ten headline rulings (§0) and the five spikes (§2).
