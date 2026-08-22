# RATIFICATION — Phase-2 gate: the `deadline_faults == 0` criterion is REPHRASED, and the gate CLOSES

*Orchestrator ruling, 2026-08-16. Requested by the W2.7 agent, which reported 9 of 10 gate criteria MET and one infeasible as frozen. Evidence: `reports/status/phase2_wave2.md` @ `ecc4dec`, dossier D5.*

## The situation

The wave-2 plan required **`deadline_faults == 0`** at 60 Hz. W2.7 measured that a lawful full-canvas Duo `DebugFrameBlit` costs **~338k gpu cycles against a 318,592-cycle frame** (HPS fetch ~93k under the frozen D10 16-cycle profile + starvation-free paced commit ~245k). Z60 exceeds raw SDRAM bandwidth outright. CMD.SCHEDULER's D8 law closes every packet at its first tick, so **every full-canvas blit fences `STATUS_DEADLINE`, in every mode.**

This is not a defect in the shell, the blit, the arbiter, or the bank split. **It is arithmetic.** No software on this machine, as frozen, can satisfy the criterion.

## Ruling

**The criterion is REPHRASED, not waived, and the Phase-2 gate CLOSES.**

The honest criterion — the one that actually tests what we care about — is:

> **The deadline fault and fence pattern is exactly predicted, pinned, and reproduced.**

The demo satisfies this: it pins the exact closed-form fault/fence pattern and the half-rate cadence (every second frame a lawful, **CRC-identical** repeat, proven 600 times), with counter ents 0/1 = 1201/601 confirming the closed forms. A demo that *predicts its own degradation to the cycle* is stronger evidence of understanding than one that merely avoids degradation.

**Why rephrase rather than waive:** a waived criterion is a criterion nobody checks. A rephrased one still fails loudly if the machine misbehaves — if the fault pattern deviates from the closed form by one fence, the demo breaks. The gate keeps its teeth.

## Why the original criterion was measuring the wrong thing

`DebugFrameBlit` is a **debug transport**: it exists to lift a rendered frame off the machine for verification. It was never the rendering path. The criterion silently assumed a full-canvas debug transport is a per-frame operation, and that assumption is simply false on this hardware.

**Recorded in the strongest terms available, because it will otherwise be misread:**

> **The 30 Hz figure is a `DebugFrameBlit` transport cost, not a rendering cost. The Phase-3+ on-fabric render path inherits no 30 Hz ceiling.**

Any future document, status page, or summary that states or implies "the Zhaozhou console runs at 30 Hz" is **wrong** and must be corrected on sight.

## On the three escape paths (dossier D5)

1. **Wider / pipelined bridge bursts** — legitimate. Deferred to the hardware lane, where the real SDRAM clock and the actual bridge profile are known. Do not speculatively re-engineer against the D10 *sim* profile.
2. **Streaming blit CRC (overlap fetch and commit)** — **REFUSED.** It violates the ratified buffer-then-release CRC-gate law, which is a *proven property* (`cmd_dma_crc_gate`, assertion b, with covers). Buying a cadence number by weakening a proof is precisely the disease this project spent the day curing: four blocks carried badges resting on proofs that were never elaborated or were vacuous. We are not now going to *deliberately* weaken a proof that works, for a demo transport's frame rate. If this path is ever revisited it requires a fresh property, not a relaxed one.
3. **Claim decoupled from vblank** — open, but it is a change to the D8 packet-closure law and needs its own ratification with its own evidence. Not free, not now.

## What closes, and what it means

Phase 2 closes with the console **composed and running**: publish a sealed packet and the machine DMAs it, schedules it, blits, swaps, scans out, and DEBUG returns per-frame `displayed_crc32c` and counters, with no test scaffolding in the datapath. Every one of 1,202 displayed frames matched the independent C++ reference over the full 245,760 displayed bytes, border rows included.

Maturity: CMD.SCHEDULER, CMD.DMA, DEBUG.COUNTERS, DEBUG.CRC → `RTL_VERIFIED` on composed-machine differential evidence (pinned `4f76d2e`). No demotions.

## Credit where the process worked

- **W2.7 withdrew its own headline twice.** First the bank split ("it bought starvation-zero, not the cadence"), then R4's bandwidth budget ("never summed a full frame — the 60 Hz cadence was never affordable"). Reporting your own fix as smaller than you first claimed, after measuring, is the single behaviour that has most improved this project's reliability.
- **Layered evidence caught what byte-identity could not.** The committed capture's COUNTERS carried stale constants from the abandoned 60 Hz model. Byte-identity passed — the bytes were consistently wrong. Only an *independent decode* found it (`4f76d2e`). Generalise: byte-identity proves reproducibility, never correctness.
- **The infra-killed nightly lane was reported as a kill, not a pass** (81 green at death, zero failures). Correct. A killed lane is not a green lane.

## Carried forward — the next verification investment

W2.7's own judgement, which I adopt: the demo exercises the *lawful* path exhaustively but never composition **under abuse**. The highest-value next verification work is an **adversarial-composition demo**:

- **mode-switch storms mid-blit** — the mode CDC + shadow-latch seam has one measured transient; hammer it
- **absent-pad churn** against INPUT's sequence law
- **publishes timed exactly at the tick boundary** — the deadline correlator's edge
- **a packet-shape fuzzer** aimed at the framer/scheduler backpressure seam — the one place a real deadlock already hid

This is worth more than re-running any green lane, and it targets the class that produced every genuine defect of this wave: **composition**, not blocks. Three wave-2 defects — the DMA blit write-data seam, the DEBUG.COUNTERS presentation-cycle mismatch, and the FB bank collision — were all invisible to correct per-block verification.

## Standing consequence for future waves

**A full-canvas debug blit is not a one-frame operation on this machine.** Any design, plan, or criterion that assumes otherwise is wrong at the point of assumption. Add to the standing verification expectations: **when writing a gate criterion, state the arithmetic that makes it achievable** — this criterion was never checked against a full frame's bandwidth before it was frozen.
