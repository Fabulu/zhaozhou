# SPEC v1: the composed fit's memory, the DSP ceiling, and the CMD.DMA redesign

**Run ID:** RUN-20260820-0252
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

Make the composed shell actually synthesise: find where the synthesis memory
goes, identify the real binding resource, and redesign the blocks that cannot
fit.

---

## Scope

**In Scope:**

- composed shell fit capacity and its memory lever
- TEXTURE.CACHE, CMD.DMA blit staging
- the DSP budget the ledger did not have

**Out of Scope:**

- implementing the two owner-authored redesign proposals (adopted, not yet built)

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

- DSP is the binding resource, not ALMs.
- My two-pass CMD.DMA answer was unsound; the owner's proposal was adopted instead.
- STATUS.md becomes the owner-facing channel.
