# Task Log: RUN-20260820-0252 - the composed fit's memory, the DSP ceiling, and the CMD.DMA redesign

**Created:** 2026-08-22 21:44 UTC+02:00 (RECONSTRUCTED — see below)
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260820-0252-composed-fit-memory-and-cmd-dma-redesign/

---

## RECONSTRUCTED, and what that means

This run folder did not exist; it was rebuilt on 2026-08-22 from git history.
A reconstruction recovers what was committed and when. It does not recover the
owner's asks, the rejected approaches, or the subagents. Read every claim below
as inferred from commit subjects unless it names a file that still exists.

This day is unusual in that **the owner intervened inside it, twice, in the
commit record itself** — two commits are addressed to me by name asking me to
read a document. That is recoverable, and it is recorded below.

---

## Objective

*(inferred)* Make the composed shell actually synthesise. Phase-8
characterization the previous night had shown blocks that would not fit, and this
day is spent on capacity: where the synthesis memory goes, what the real binding
resource is, and redesigning the two blocks that could not fit.

---

## Progress Timeline

### 02:52-03:49 — what is actually binding

- `791723e` hand the composed shell fit to a machine that can hold it.
- `c992e06` planetside suns: ten worlds, built the way the donor actually builds them.
- `44c6784` **`TEXTURE.CACHE` was a cache made of flip-flops: 5,402 ALMs -> 1,087.** A four-fifths reduction from noticing what the RTL had actually asked for.
- `96c0394` **DSP is the binding constraint: 171 against 112, and it had no budget.** The device has 112 DSP blocks; the design wanted 171. The ledger had no DSP budget field to catch it.

### 05:07-06:08 — the 28.4 GB was a wildcard, not a requirement

- `c2cc117` give the composed fit a memory lever (`NUM_PARALLEL_PROCESSORS`).
- `d1a2b8a` **name every virtual pin instead of `-to *`: the 28.4 GB was the wildcard.** `set_instance_assignment -name VIRTUAL_PIN ON -to *` was matched against every internal node in the design, not the 101 top-level ports it was written for.
- `bc66758` restore the wildcard for per-block fits, **"which I just broke"** — the fix for the composed case broke the per-block case, caught and repaired immediately.
- `9c693a9` correct a false claim about where the synthesis memory goes.

### 06:31-07:30 — CMD.DMA cannot fit, and the staging buffer is why

- `c520d04` **"STATUS.md is the channel now"** — the owner-facing channel established, and CMD.DMA reported as unable to fit the device.
- `3a97980` blit staging redesigned to one 64-bit word per entry: **16.2 GB -> 2.7 GB**.
- `71be297` **CMD.DMA elaborates: 2.70 GB, 515 s, exit 0.**

### 07:30-08:24 — the owner intervenes, in the commit log

- `42fb505` **"Add CMD.DMA and DEBUG.FRAMEBLIT redesign proposal - Claude please read the file"** — the owner's own commit.
- `f3506b6` **the composed synthesis RUNS: 28.4 GB -> 6.2 GB, and Quartus names the fault.**
- `245ab58` **"Adopt the CMD.DMA redesign proposal; my two-pass answer was unsound."** My approach was replaced by the owner's document after the owner asked me to read it.
- `a3fdf53` **"Add Line-Buffer Redesign Proposal documentation - Claude, please read this file"** — the owner again.

The day ends there. `reports/CMD.DMA_Redesign_Proposal.md` has been a standing
priority in every session brief since, which traces directly to `42fb505`.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| — | — | unrecorded | — | — |

Not recoverable from git history.

---

## Files Created

- `reports/CMD.DMA_Redesign_Proposal.md` — owner-authored (`42fb505`), still a
  standing reference.
- Line-Buffer Redesign Proposal — owner-authored (`a3fdf53`).

---

## Decisions Made

- **DSP is the binding resource, not ALMs**, and it had no budget in the ledger.
- **My two-pass CMD.DMA answer was unsound**; the owner's redesign proposal was
  adopted in its place (`245ab58`).
- The virtual-pin assignment is **named per port for the composed fit** and left
  as a wildcard for per-block fits — the two lanes genuinely need different
  handling.
- `STATUS.md` becomes the owner-facing channel (`c520d04`).

---

## Next Steps

*(as of the end of this run; superseded since)*

Adopt and implement the two owner proposals. The CMD.DMA redesign was implemented
over the following days; its staging buffer is real M10K now (3,607 ALMs), and
the `blit_buf` async-read defect that `reports/composed/README.md` once named as
THE composed-fit blocker no longer exists.

**The lesson this day carries best:** three separate "impossible" resource
figures — 28.4 GB of synthesis memory, 5,402 ALMs of cache, 16.2 GB of staging —
were each a defect in what the source had asked for, not a requirement of the
design. None of them was found by reasoning about the RTL; each was found by
reading what the tool actually reported.
