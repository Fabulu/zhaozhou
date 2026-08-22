# RATIFICATION — FB slot 1 moves to DRAM bank 1 (GRANTED)

*Orchestrator decision, 2026-08-16. Request: `RATIFICATION-REQUEST-fb-slot-bank-split.md` (W2.7 implementation agent). Precedent: the arbiter-liveness-bound edit.*

## Decision: RATIFIED

`ZHAO_FB_SLOT1_BASE: 0x0003_C000 → 0x0200_0000` stands, with the mechanical shadow listed in the request.

## Why

1. **The finding is real, measured, and structural.** Both framebuffer slots sat in DRAM bank 0 because bank selection takes byte-address bits [26:25] and every Phase-2 VRAM address is far below bit 25. Composed on the real chain, the sustained scanout READ and the sustained blit WRITE occupy different rows of the same bank and pay PRE+ACT on every handover — read 18 / write 16 instead of 12 / 10. **~82 of 192 Duo lines starved per frame.** The fix removes the pathology structurally rather than papering over it with pacing.

2. **That 18 is the arbiter's worst case, and it was being hit constantly.** This morning's re-derivation identified the bank-conflict full read (PRE, tRP, ACT, tRCD, READ, CAS, 8 beats = 18) as the worst grant-to-grant span. The composed system was paying it on *every* handover. The corrected bound was right; what was wrong was everyone's mental model of how often the worst case occurs. Worth recording as its own lesson: **a correctly derived worst case tells you nothing about its frequency.**

3. **No lane before the shell could have found it**, and the request says so precisely: W2.2 used a harness responder, W2.5 drove per-line worst cases with free address choice, W2.6 used a permissive guard stub. Every block was correct in isolation. This is the composition class of defect the shell exists to expose, and it is the second such finding this wave after the DMA beat-stream seam.

4. **The MEM.GUARD change is a genuine safety fix, not a mechanical follow-on.** The old scanout region check (`end <= SLOT1+SPAN`) relied on the two slots being contiguous. With slot 1 relocated, that form would have **admitted reads from the new hole between them**. The disjoint two-slot form is now asserted, the formal harness mirrors the disjoint map, and `mem_guard_no_escape` was re-elaborated green rather than re-cited. Correct handling.

5. **The alternatives were considered on the merits**, and rejected for stated reasons — pacer-only does not fix the deadline law; a bank-interleaved address hash would re-freeze the cycle-exact SDRAM controller law and its oracle for the same effect; a streaming blit CRC would violate the ratified buffer-then-release gate. I agree with each.

## What I especially credit: the corrected scope

The request **withdraws its own headline**. The split does **not** buy 60 Hz: the composed blit costs ~338k gpu cycles (HPS fetch ~93k + starvation-free paced commit ~245k) against a 318,592-cycle Duo frame. It removes the pathology and is a prerequisite for any future 60 Hz path — nothing more. Reporting a fix as smaller than first believed, after measuring, is the behaviour this project has had to learn the hard way. Ratified partly *because* the claim shrank under measurement.

## The uncomfortable consequence, recorded plainly

**Every full-canvas blit fences `STATUS_DEADLINE`, in every mode.** Z60 and Storm miss by similar margins. The blit exceeds one frame period regardless of cadence, so the Phase-2 gate demo cannot produce clean fences at any cadence, and adopts the machine's sustainable half-rate: every second frame a lawful, CRC-identical repeat, with the demo **pinning that pattern exactly rather than tolerating it**.

That is the right call — a demo that pins its own degradation is honest; one that tolerates it silently is the disease this project spent a day curing. But the underlying fact is a real architectural constraint discovered by composition, and it must not be quietly forgotten:

- **A full-canvas debug blit is not a one-frame operation on this machine.** Any future design that assumes it is, is wrong.
- This is a **`DebugFrameBlit`** cost, not a rendering cost — it does not bound the renderer's own frame rate. Do not let it be misread as "the console runs at 30 Hz".
- Revisit when the hardware lane lands: the request notes a slower real SDRAM clock may re-open the pacer question.

## Conditions

1. **ZH-004 (board SDRAM probe) must confirm the real device's bank-address mapping.** If the board maps banks differently, the slot-1 base is the single retuning knob. This ratification is conditional on that confirmation; if the mapping differs, the constant changes again under the same precedent, without needing a fresh argument.
2. The MiSTer SDRAM controller integration inherits the requirement: **FB slots in distinct banks.** Record it in the hardware lane's contract so it cannot be lost.
3. The pacer prototype stays preserved in the probe history, as the request states.
4. `spec/memory_rules.md` §5 keeps the superseded map **struck through, not rewritten** — charter honesty discipline, as with the arbiter bound.

## Lesson to generalise

Three defects this wave came from composition, not from any block: the DMA blit write-data seam, the counters presentation-cycle mismatch, and this bank collision. Every one was invisible to correct per-block verification. **Add "compose the streams that will actually run together" to the standing verification expectations** — a block proven against a harness responder has not been proven against its real neighbour.
