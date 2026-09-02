# Task Log: RUN-20260902-2307 - [Describe objective here]

**Created:** 2026-09-02 23:07 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260902-2307-texture-island-fit-and-buildability-rulings/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-02 23:07 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260902-2307
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

## Late run creation — recorded rather than hidden

This run folder was created at 23:07, hours into the work it logs. CLAUDE.md
says every session is a run and this one went without. The entries below are
reconstructed from the commits, which is why each names its commit: a
reconstructed log is weaker evidence than a live one and should read as such.

## What this run is for

Owner direction, 2026-09-02:

> "the important bit is actually fitting all the texture stuff to see if the
> 99.5 MHz renderer and full fitted console actually holds up or if it needs
> more reingeneering. Keep your eyes on the prize."

> "bro is finding many issues with the outstanding stuff, so focus on the fit
> for now, we really need to know if the renderer holds up"

Ten texture-island blocks were written and functionally verified. **None had
ever been fitted.** Functional verification says a block computes the right
numbers; it says nothing about whether it can be clocked.

## Log

**`9417988` — the buildability brief saved where it survives.**
`reports/OWNER-RULINGS-BUILDABILITY-20260902.md`. Not a run folder: every pass
creates a new one, so a file left in the current run is orphaned by the next.

**First `tmu_plan` fit returned `timeout 3385.8s`** against a 3000 s budget,
with a formal proof competing for CPU. That is not a timing result — the tool's
own header warns a timeout row "reads as 'this block does not fit', when all it
meant was 'we did not wait'". Re-run alone at 9000 s: **fitted in 2524 s**.

**`ce25658` — the first hard number: `zhao_texture_tmu_plan` at 93.55 MHz.**
1419 ALM, 1054 reg, 0 M10K, 0 DSP, 363 virtual pins. Below the shipped shell's
own 99.50 MHz and 56 MHz below the brief's 150 MHz leaf target. The gap is 12x
the measured 4.61 MHz placement noise floor, so it is not a seed draw.
Recorded in `reports/TEXTURE-ISLAND-FIT.md` with the tool's limitations
attached to the number rather than recalled later.

**`50b0f3c` — TEXJOIN v2: the free-count race (brief X4) and 8-bit generations
(X5).** `free_cnt_q` was moved by two separate nonblocking assignments in one
`always_ff`; a cycle that both accepts and retires kept only the last. **The
same fault I had found and fixed in `zhao_raster_perspuv_svc.sv` hours earlier,
reproduced in the next block written.** The six existing cases could not see it
— each accepts a batch then drains it, so the two never coincide. Case 7
streams 4000 clocks with a randomly stalled output. Proved by reverting:

    reverted:  worst outstanding 1694 against DEPTH 16, 1882 fragments out of order
    restored:  worst outstanding 16, 0 order errors, 2325 same-clock cycles

**`1831e10` — TMU PLAN narrowed.** The setup report named the path rather than
leaving it to be guessed: `t2_iv0[0] -> t3_row0[13]`, data delay 10.121 ns. A
32-bit add, a 32-bit wrap fold and a 32-bit barrel shift, on quantities bounded
by the texture. `MAXLOG2 = 11` (2048, eight doublings above the largest asset
that exists) as a **parameter**, coordinates to 12 bits, texel indices to 24,
addresses left at 32. The narrowing is exact, not approximate — REPEAT and
MIRROR are masks below 2^CW so truncation cannot change them, and CLAMP carries
the dropped bits as `neg`/`ovf` flags. 357 of 357 addresses bit-identical
against the shipped oracle. **Not yet refitted** — the Fmax claim is open.

**`f8c9ebb` — the three questions that were already answered.**
`OPEN-SPEC-DEPTH-QUANTISATION.md` marked superseded;
`OWNER-SPEC-QUESTIONS.html` carries every answer beside its question;
`sample_budget.cpp` headings renamed from "known" to "pre-Z no-rejection
envelope". I got the depth profile table wrong on the first write of that
banner — invented values for profiles 0 and 1 — and corrected it against the
brief before committing. Recorded because the whole point of the banner is that
a reader trusts it over their memory.

**`f5c33be` — SW.STREAM filled, GEOM.PARAMBUF registered.** The two blockers
the brief named. SW.STREAM was the last real stub, five TODO sections, while an
audit reported "zero contracts are unwritten by accident". GEOM.PARAMBUF had no
block, no contract and no ledger entry at all. Both list what is NOT ruled
rather than inventing it. The ledger caught four real things: four counters
missing from `counter_catalog`, three asymmetric graph edges, and five
pre-existing "by construction" claims with no machine-resolvable enforcer.

**`990de0d` — qformats C2: particle128 v1 numeric law, QFMT_VERSION 2 -> 3.**
Section 10 was "provisional" and agreed with the ruling on nothing except the
total. Replaced whole. Gates green: fixgen 12 files byte-identical, abi-gen 26
outputs match, ledger 93 blocks.

## Open

* Fit queue running the remaining eight texture-island blocks serially
  (`fitq.ps1`, 9000 s each). `cache_pipe` first.
* `tmu_plan` needs a re-fit to know whether the narrowing bought anything.
* PERSPUV two-lane rebuild (X2), TEXJOIN restructure (X3), cache synchronous
  seam (X7), palette protocol — all held until their rows land, so the
  reconnaissance measures what is committed.
