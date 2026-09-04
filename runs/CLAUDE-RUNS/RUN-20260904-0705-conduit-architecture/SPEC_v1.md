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

---

# IMPLEMENTATION ADDENDUM (implementer session, 2026-09-04)

**Scope executed so far:** spikes S1–S5 (all PASS — verdicts + evidence in
TASK_LOG), the reel wiring for species kUnnamed02, the creature-02 headers
(`unnamed02_{art,model,rig,clips,fx}.h`, `unnamed02.h`), the committed
`u02_meshcheck.cpp`, and the Upheaval package scaffold-and-merge
(`creature/Unnamed02/{CREATURE.json,SPEC.md,media,probes,texture,validation}`).

**Two deviations from PLAN.md, both stated in TASK_LOG:**
1. The `build_ring_part` bottom-cap fix is OPT-IN (`RingPart::cap_base_fix`)
   rather than global: the ungated fix moved every Zixxtrixx bank CRC (its
   tapered nose rings hit the same latent bug), and approved art does not
   change silently. The owner may opt the fleet in later.
2. Vertical form dimensions carry `kVStretchPm` (≈1.66): the console's
   projection paints X wider than Y and this creature is balls — the first
   world-spheres ever rendered here. Authored in on-screen proportions.

**Next:** form milestone (teardrop + loop + eyes) → page → motion → effects →
probe/gates → site + publish, per PLAN §3.2.
