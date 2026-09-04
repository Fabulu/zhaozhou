# Task Log: RUN-20260904-0705 - Unnamed02 mana conduit: architecture plan

**Created:** 2026-09-04 07:05 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260904-0705-conduit-architecture/

---

## Objective

Produce the implementation PLAN for creature 02 (the floating mana conduit,
`Upheaval/creature/Unnamed02/`) — a plan, not an implementation. No geometry,
no pose values, no builds, no renders. Rule where the two recons left choices,
confront the three known obstacles (opaque particles, the single pre-resolve
hook slot, the ≤2 suns/flares cap), and hand an implementer an ordered spike
list, build path, form/motion/effects design, knob list, verification plan and
cut order.

---

## Progress Timeline

### 2026-09-04 07:05 UTC+02:00 - Task Started

- Fetched `origin main` in both lane repos; zhaozhou at `0ef1c35c`
  (= origin/main), Upheaval at `1cb1ec5` (= origin/main), local branch
  `zixxtrixx-wholebody-s-spring` in both, exactly on origin/main.
- Created run via init-run.ps1, slug `conduit-architecture`.

### 2026-09-04 - Reading pass (in the ordered sequence)

1. `OWNER-DIRECTION-1-2026-09-04.md` — binding; includes the late FLOATS
   amendment. `Unnamed02/reports/` does not exist yet.
2. `Unnamed02/README.md` + all three concept sheets LOOKED AT (Side, Front,
   Description incl. the oblique bulging-eye sketch).
3. `RECON-FX-FINDINGS.md` (469 lines) and `RECON-NEWCREATURE-FINDINGS.md`
   (617 lines) — inherited, not re-derived.
4. `00-START-HERE.md`, `07-MOTION-STYLE.md`, `08-LIGHTING.md` (incl.
   Directions 29/30), `05-BUDGETS.md`, blueprint layout.
5. `zhaozhou/CLAUDE.md` (matches root working rules) and
   `reports/DoubleHelixTornado.md`.

### 2026-09-04 - PLAN.md written

- `PLAN.md` committed in this run folder. Headline rulings in its §0;
  five-spike list in §2 with the hook-chaining spike first, as required.

---

## Subagent Spawns

None — serial run by instruction.

---

## Files Created

- `PLAN.md` — the deliverable.
- This TASK_LOG.

---

## Decisions Made

(Full list with rationale: PLAN.md §0. Summary:)

1. Additive blend IS wired into the particle path (new flag bit; off =
   CRC-identical). Spike S3.
2. Pre-resolve hooks chained sky → celestial → creature; Spike S1 first;
   fallback = explicit composition inside creature_hook for u02 subjects.
3. Sun effects are scene-level; per-conduit centre glow is a shared-ramp baked
   additive sprite, NOT a sun/flare — dissolves the ≤2-per-view cap collision.
4. Lightning = deterministic jittered bead-chain on the Population tri path.
5. Ten kinds = ten named emitter tables feeding one capped scene Population.
6. Code beside Zixxtrixx split into migration-shaped headers, plus
   `unnamed02_fx.h`; durable record in the Upheaval package via scaffold-to-
   sibling-and-merge.
7. Eyes are real faceted geometry with lens+pupil bones and a deform squint.
8. Compression = deform sidecar, one `kCompressAmpPm` knob driving sidecar and
   sympathetic hinge bob.
9. Probe asserts clearance (floats); ground-contact law declared inapplicable
   in CREATURE.json.
10. Publish at end of finished pass without asking (standing authorisation +
    explicit direction), `-Branch main` mandatory.

---

## Next Steps

For the implementer, not this run:

1. Re-read `OWNER-DIRECTION-1-2026-09-04.md` and check `Unnamed02/reports/`
   before the first edit (the file grew twice in one day).
2. Run spikes S1–S5 in PLAN.md §2 order; log verdicts.
3. Follow the build path in PLAN.md §3.2.
