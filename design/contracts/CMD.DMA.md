# Contract — CMD.DMA (Command DMA)

> Ledger: `design/blocks.yml` · owner ZH-007 · phase 2 · maturity UNIT_VERIFIED

## Purpose and exclusions

Fetch sealed frame packets from the HPS-DDR FRAME_RING (triple-buffered, descriptor law spec/memory_rules.md §4.1) through the functional HPS bridge, and verify BOTH CRCs and the resource epoch before any byte reaches the decoder. Wave 2 matures this from stub to real fetch with the CRC gate before the first byte (plan D8). The documented hps→gpu async bridge for the command stream.

Exclusions: no semantic decoding (CMD.DECODER), no reordering of packets within an epoch, no slot ownership decisions (CMD.SCHEDULER claims READY slots; DMA fetches them), and — since step 6 of the DEBUG.FRAMEBLIT integration, 2026-08-21 — **no blit engine and no VRAM writes at all**. See Memory ownership.

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`. Reset: fetch FSM idle, ring pointer unadvanced — a partially fetched packet is NEVER handed on; bridge port idle; epoch register 0.

## Input and output packet layouts

Input: FRAME_RING descriptors + slot bytes via MEM.HPS.BRIDGE bursts (`zhao_hps_burst_req_t`); the slot body is a sealed frame packet (capture_format.md §3) up to `FRAME_SLOT_BYTES` (1 MiB). Output: the verified packet bytes into the command-slot buffer (ready/valid), plus `{status, bytes_consumed, commands_consumed}` per packet (the generated validator's verdict).

## Backpressure rules

`ready_valid` into the decoder-facing buffer; credit-based at the bridge port (one burst in flight).

## Memory ownership

Read-only on FRAME_RING slots and descriptors EXCEPT the single `state` word transition READY→FPGA_RUNNING it performs on claim (the only word both sides ever write, never simultaneously — spec/memory_rules.md §4.1).

**THIS BLOCK IS NO LONGER A VRAM WRITER, AND NO LONGER OWNS A LARGE BUFFER.** Step 6 of the DEBUG.FRAMEBLIT integration (2026-08-21) removed the debug blit engine outright: the `M_BLIT_*` states, the MEM.GUARD client `BLIT_DMA`, the `guard_req_o` / `guard_wdata_o` / `guard_wvalid_o` ports, and **the 1,966,080-bit whole-canvas staging buffer**. The blit dispatch goes to `DEBUG.FRAMEBLIT`, which streams fetch and commit together and stages **64 bytes**.

That buffer is worth recording as a cost that was paid for two years of design and then deleted:

- it was ~1.97 Mbit of on-chip memory, roughly a third of the device's total;
- it never inferred as an M10K — the write sat in an async-reset process and the read was combinational — so Quartus reported **Error 276003** (registers that cannot convert to RAM megafunctions) and **the composed fit could not complete**;
- elaborating this module alone once peaked at **16.2 GB**, measured 2026-08-20, while `zhao_sdram_ctrl` and `zhao_video_mode` each finish in 0.26 GB.

The open design question that the previous revision of this section deferred — *"whether a 1.97 Mbit on-chip buffer should exist at all"* — is answered: it should not, and the streaming redesign is why it does not have to.

Two earlier corrections to this section are kept because they show the same shape twice: a revision once said “No VRAM writes” while the shipped RTL wrote VRAM (false, corrected 2026-08-16), and the W2.6 write-data sideband carried only the FIRST 8 bytes of each 64-byte request while the directed test compared only those bytes and passed. A contract can be wrong about its own block, and a test can agree with it.

ENFORCED-BY: tests/command/cmd_dma_directed.cpp

## Q formats and rounding

None (byte streams, lengths, CRCs).

## Latency (fixed or variable)

Variable: bounded by slot length × bridge beat profile (16 + 1/cycle on the sim profile) plus CRC passes (1 byte/clock per lane).

## Target throughput

Saturate one HPS-DDR burst slot per frame: a full 1 MiB worst-case packet fetch + verify inside the smallest frame period (217,984 gpu cycles) is the budget test.

## Overflow and malformed-input behaviour

Fail-safe order per capture_format.md §3.2 — never emits a byte of a packet whose HEADER CRC failed; header CRC fail, ABI version mismatch, reserved flag, or bounds error ⇒ the packet is dropped whole, the slot returned DONE with the safe error code, `deadline_faults`++ (the frame repeats per video law), ring pointer consistent. Payload CRC fail after the header passed ⇒ same drop semantics, no partial decode. Epoch mismatch (slot `resource_epoch` ≠ current epoch) ⇒ drop before the first payload byte. No partial delivery, ever (formal `cmd_dma_crc_gate`).

## Counters and traces

`commands` (validated records handed on), `hps_ddr_bytes_by_client` (fetch bytes), `deadline_faults` (drops). Source IDs: propagated verbatim from packet records.

## Scalar reference function

`zref::CmdDma` (reference/include/zref/zref_cmd2.hpp) — fetch/verify oracle: given the slot contents (incl. corrupted variants), the descriptor length and the current epoch, the exact verdict, bytes_consumed (structural 36-vs-40+N law) and walked-record count. Record sizes come from the generated `sizeof(zhao_abi::ZhRecord*)` layouts, not a hand table.

## Directed tests

`tests/command/cmd_dma_directed.cpp` — happy path; corrupt header CRC ⇒ ZERO bytes forwarded; corrupt payload CRC; epoch mismatch; truncated slot; 1 MiB worst-case timing budget.

## Randomized differential tests

`tests/command/cmd_dma_directed.cpp --random N` (CTest `cmd_dma_random` / `cmd_dma_random_nightly`) — PCG packet timelines, one deterministic corruption per packet, verdict/bytes/cmds vs `zref::CmdDma::verdict` and the zero-bytes-downstream gate on every error path.

## Formal properties

`tests/formal/cmd_dma_crc_gate.sby` — no byte leaves before the header CRC passes; reset leaves no partial handoff.

Property (b), “no VRAM write is offered before the blit payload CRC passed”, went with the blit engine in step 6. It is recorded here rather than dropped silently, because it is a warning: (b) was **vacuous** until the harness gained `FORMAL_BLIT_LEN`, since the smallest lawful canvas is 153,600 B and no tractable BMC depth could open the gate. A green formal run proved nothing about it for as long as that went unnoticed. The law now lives on `DEBUG.FRAMEBLIT` and `tests/formal/debug_frameblit_safety.sby`.

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). CRC lane + counters (reuses zhao_crc32c_step from zhao_abi_pkg).

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` and `duo_markers.zcap` — every FRAME_PACKET section in every capture was fetched and verified through this path in the Verilator run (run-twice-assert-identical).

## Notes

CRC law: the SAME CRC-32C machine as the ABI (capture_format.md §2; one polynomial machine-wide — A3d).
