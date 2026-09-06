# SPEC v1: RCP24 V3 — queues instead of scans, an exact 32x32 instead of a 32x64

**Run ID:** RUN-20260906-1901
**Created:** 2026-09-06 19:01 UTC+02:00
**Status:** Active
**Previous Version:** N/A

**Authority:** `reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt`, owner-supplied.
Sections 0 point I, 10, 21, 22, Appendix B, Appendix C.
**Proven arithmetic:** `tools/rtl/architecture_numeric_checks.py`
(sha256 `1b7c0b69…3b6cbb`, matching Appendix C). The script is right until proven
otherwise; the RTL implements ITS identity.

---

## Objective

A STANDALONE tile that computes the same reciprocal as `zhao_raster_rcp24`, bit
for bit, with

1. the free/ready/done scans replaced by queues and bounded execution contexts
   (S10.2), and
2. the 32-by-64 multiply replaced by an exact 32x32 product plus a signed-wrap
   high-word correction (S10.5), the NEGATIVE case handled and tested.

Success is physical evidence, not a green suite: DSP, ALM and register counts
before and after from the exact tool, with the multiplier sites counted in the
source first.

---

## Scope

**In Scope:**

- `zhao_raster_rcp24_v3` + `zhao_raster_rcp24_mul` + `zhao_raster_ticketq`
- A differential against BOTH the shipped serial RTL and `zref::rcp_u24`
- A direct drive of the arithmetic core over the script's own (x, w) families
- `run_block_fit.ps1 -MapOnly` before and after; one full fit for ALM and Fmax
- Mutation testing: every check shown to fail, with the exact text

**Out of Scope:**

- **Wiring the tile into the island.** S26.1: "Do not merge those changes into
  the composition until the shared record and credit contracts are fixed."
- Owner14 / RCP_RESULT / sample-owner tickets — same reason.
- `design/blocks.yml`, `design/fit_targets.yml` — report the entry, do not edit.
- Any claim about the composed island's fmax.
- The parallel lane's `fpga/rtl/texture/*v3*` and `tests/texture/*v3*`.

---

## Constraints

- Bit-exact includes truncation and wraparound, not an error bound (S10.1).
- The seed ROM is the existing generated table; no hand-edited replacement.
- Zero input is a SCHEDULED phase through the same final pipeline, never an
  unqueued bypass writer (S10.8).
- Never edit a file inside a running fit's closure.
- Do not git commit.

---

## Don't Retry

- **`cmake --preset windows-native` into `build/`.** Another session owns that
  tree; it dies on `Error copying file … Permission denied` in two verilate
  copy steps. A full fresh configure verilates every target in the project
  before reaching this one. Drive verilator at the single testbench instead —
  `runs/.../rebuild.ps1`, about 80 s.
- **A verilator objdir under `%TEMP%`.** The path contains a space ("Fabian
  Trunz") and GNU Make refuses: *"Unsupported: GNU Make cannot build in
  directories containing spaces"*. Use `build-rcp24v3-quick/`.
- **`2>&1` on a native exe in a PowerShell script with `ErrorActionPreference
  = 'Stop'`.** PS 5.1 wraps the redirected stderr in a NativeCommandError and
  `Stop` kills the build on a WARNING. It scored all six mutants BUILD-FAILED.
- **Writing C++ or Python escapes through a shell heredoc.** `\b` became a
  literal 0x08 byte and `\n` became a real newline, twice, in this run. Use the
  Edit tool for anything containing a backslash escape.
- **Judging the correction from denominators.** All 16,777,215 of them top out
  at w = 0x401FEF88, so the negative branch is unreachable that way and the
  paired test passes with the correction deleted. Measured, and fire-tested.

---

## Open Questions

- S10.7's mapping target was TWO DSP blocks via the two-18-bit mode. Measured
  THREE. Is a two-block packing reachable by registering or signedness changes,
  or is three the honest floor for four 16x16 unsigned products here?
- Quartus turned the correction operand's five-stage delay into an
  `altshift_taps` memory (150 flops of `*_corr_q[0..29]`). Moving the subtract
  to a stage where its operand is already present would remove it, but S10.8
  names stage C as the one that does the correction. Which wins?
- The seed plane inferred TWICE (`p_x0_q_rtl_0` and `_rtl_1`, 512 bits each)
  because MW0's writeback reads it a second time to initialise scratch. Letting
  MX0 read the seed from the payload instead removes the duplicate, but S10.2
  says MW0's writeback is what initialises the scratch row. Which wins?
- The tile keeps a local 16-entry result array and DONE queue. S10.2 wants the
  final result written to the owner's RCP_RESULT with a sample-owner ticket.
  That is composition work; its blocks are counted here so they are not lost.
