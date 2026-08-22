# SPEC v1: phases 6 to 8: terrain, surface, texture, forge and measure in RTL

**Run ID:** RUN-20260819-0015
**Created:** 2026-08-22 (RECONSTRUCTED from git history)
**Status:** Complete
**Previous Version:** N/A

---

> **This SPEC is inferred.** The run was never initialized, so no
> contemporaneous specification exists. What follows is reconstructed from
> commit subjects and touched paths on 2026-08-22. The Objective and Scope
> are what the work turned out to be, which is not the same thing as what
> was asked for -- that wording is unrecoverable.

---

## Objective

Take phases 6, 7 and 8 from reference into RTL -- terrain LOD and projection,
surface sheet and stamp, texture mosaic and aux, forge cliff, terrain bake and
velocity, measure tokens and governor -- each with a differential, a mutation
sweep, and figures MEASURED rather than counted off the source.

---

## Scope

**In Scope:**

- RTL, contracts and formal runs for ten blocks across three phases
- per-block characterization for source cones outside the shell

**Out of Scope:**

- seam law E between MEASURE.TOKENS and GEOM.BINNER, deferred (994c5b9)
- two phase-7 FIELD blocks, deliberately not started, reason recorded (9971173)

---

## Constraints

- Everything in this run is simulation, synthesis or fit. Nothing ran on a board.
- One implementation subagent at a time (owner rule, 2026-08-15, tightened 2026-08-17).

---

## Don't Retry

*Not recoverable.* Failed approaches were not committed, which is precisely
what this section exists to preserve and what its absence here demonstrates.

---

## Open Questions

*Closed or superseded since; see the TASK_LOG's Next Steps.*

---

## Decisions recovered from the commit record

- Figures are measured, not counted off the source.
- Prefer a total proof to a bounded one where the blend allows it.
