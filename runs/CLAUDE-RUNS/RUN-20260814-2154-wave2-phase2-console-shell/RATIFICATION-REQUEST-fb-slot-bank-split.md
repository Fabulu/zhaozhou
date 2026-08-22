# RATIFICATION REQUEST — FB slot 1 moves to DRAM bank 1 (W2.7 composition finding)

*W2.7 implementation agent, 2026-08-16. STATUS: implemented + fully re-verified,
awaiting architect sign-off (the `zhao_pkg.sv` freeze law requires it; the
precedent is the arbiter-liveness-bound edit, which carried its ratification
in-place). CORRECTED SCOPE after full measurement: the split removes the
row-thrash pathology (necessary for any future 60 Hz path and true of the
real hardware), but it is NOT sufficient for 60 Hz full-canvas cadence —
the composed blit still costs ~338k gpu cycles against a 318,592-cycle Duo
frame (fetch ~93k + starvation-free paced commit ~245k), so the gate demo
runs at the machine's sustainable half-rate cadence either way. The full
dossier is in reports/status/phase2_wave2.md. If ratification is refused,
revert the single constant and the mechanical edits listed below; the
demo's blit pacer then absorbs the thrash at a slightly worse margin.*

## The finding

The frozen Phase-2 region map places both framebuffer slots in ONE DRAM bank:

- `FB_SLOT0 = 0x0000_0000`, `FB_SLOT1 = 0x0003_C000` (memory_rules §5)
- the SDRAM controller's frozen mapping takes the bank from word-address
  bits [25:24] = byte-address bits [26:25] — every Phase-2 VRAM address is
  far below bit 25, so **all reads and all writes hit bank 0**.

No pre-W2.7 lane ever composed a sustained read stream (scanout) with a
sustained write stream (DebugFrameBlit): the W2.2 video lanes used a harness
responder, the W2.5 `mem_bandwidth_budget` drove per-line worst cases with
free address choice, and the W2.6 blit tests used a permissive guard stub.
Composed on the real chain, the two streams sit in different ROWS of the
same bank, and every read↔write handover pays a PRE+ACT row conflict
(read 18 / write 16 cycles instead of 12 / 10). Measured on the shell
(tests/shell/shell_probe.cpp):

- unpaced: **~82 of 192 Duo lines starved per frame** (41,984 starved vid
  cycles/frame) — the display shreds;
- with a shell-side "blit pacer" (writes only in scanout's idle windows) the
  starvation is exactly zero, but a full Duo blit then takes ≈ 340–358k gpu
  cycles (HPS fetch ≈ 93k under the frozen D10 16-cycle profile, paced
  commit ≈ 250k) against a 318,592-cycle frame — and CMD.SCHEDULER's D8 law
  closes every packet at its first tick, so **every full-canvas blit fences
  STATUS_DEADLINE, in every mode** (Z60 and Storm miss by similar margins).
  The Phase-2 gate demo cannot produce clean fences at any cadence.

## The fix (one constant + its mechanical shadow)

`ZHAO_FB_SLOT1_BASE: 0x0003_C000 → 0x0200_0000` (byte-address bit 25 set ⇒
DRAM bank 1). Slot spans and slot 0 are untouched; "a mode switch never
moves a slot" still holds; the 128 MB VRAM space (27-bit) contains the new
base with room to spare. Reads and writes now keep their open rows in
separate banks: the thrash vanishes **structurally**, no pacer needed.

Measured after the fix (same probe): the free-interleave starvation drops
from a hard limp to a ~2 %/line deficit — still accumulating, so the shell
keeps the blit pacer (glue 10) and starvation is exactly ZERO. The paced
blit measures ~338k cycles end to end: the split alone does NOT buy 60 Hz
(see the corrected status note above); it removes the pathology and is the
prerequisite for every future path that would.

## Files changed (all re-verified; see TASK_LOG for the lane counts)

- `fpga/rtl/common/zhao_pkg.sv` — the constant (this document is the
  sign-off reference, arbiter-bound precedent)
- `fpga/rtl/memory/zhao_mem_guard.sv` — the scanout region check becomes the
  DISJOINT two-slot form (the old `end <= SLOT1+SPAN` relied on contiguity
  and would have admitted reads from the new hole)
- `tests/formal/formal_mem_guard.sv` — harness mirrors the disjoint map;
  `mem_guard_no_escape` re-elaborated green (registry updated)
- `reference/include/zref/zref_mem.hpp`, `reference/src/zref_video.cpp` —
  oracle mirrors of the map
- `tests/memory/mem_guard_directed.cpp`, `tests/command/cmd_dma_directed.cpp`,
  `tests/video/video_harness.hpp` — literals + region expectations (reads in
  the old slot-1 window are now violations — asserted, not assumed)
- `spec/memory_rules.md` §5, `spec/video_rules.md` §3,
  `design/contracts/MEM.GUARD.md`, `design/blocks.yml` — the map text

## Alternatives considered and rejected

1. **Blit pacer only** (shell-side client pacing, no map change): fixes the
   starvation but not the deadline law — every packet still fences dirty.
   The pacer prototype is preserved in the probe history for the hardware
   lane, where a slower SDRAM clock may re-open the question.
2. **Half-rate cadence** — not an alternative to the split but the demo's
   ADOPTED operating point on top of it: the blit exceeds one frame period
   regardless of cadence, so every blit packet fences STATUS_DEADLINE and
   every second frame is a lawful (CRC-identical) repeat; the demo pins
   that pattern exactly rather than tolerating it.
3. **Bank-interleaved address hash in the SDRAM controller**: touches the
   frozen ctrl law mirrored cycle-exactly by `zref::SdramController`, the
   behavioural model and the W2.5 directed tests — a far larger re-freeze
   for the same effect.
4. **Streaming blit CRC** (overlap fetch+commit): violates the ratified
   buffer-then-release CRC-gate law (`cmd_dma_crc_gate` property b).

## Downstream notes

- ZH-004 (board SDRAM probe) must confirm the bank-address mapping of the
  real device; if the board maps banks differently, the slot-1 base is the
  single knob to retune.
- The hardware lane's MiSTer sdram controller integration inherits the same
  requirement: FB slots in distinct banks.
