# SPEC v1: the Form compiler (W3), typed HIR through exact C++ lowering

**Run ID:** RUN-20260817-0752
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

Build the Form language compiler's W3 lane: a typed HIR, a deterministic ZIR
scheduler, and lowering to deterministic C++17 cartridges, with exactness
enforced rather than assumed.

---

## Scope

**In Scope:**

- compiler/ typed HIR, ZIR scheduler, C++17 cartridge emission
- naming, scoping and shadowing rules
- artifact identity, source maps, cost reports

**Out of Scope:**

- concurrent capture specification, deliberately kept out of W3.3 (249e3eb)

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

- Cost reports must be truthful or absent.
- Generated cartridges may not collide with authored C++ identifiers.
