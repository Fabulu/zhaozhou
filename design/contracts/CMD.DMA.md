# Contract — CMD.DMA (Command DMA)

> Ledger: `design/blocks.yml` · owner ZH-007 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Fetch sealed frame packets from the HPS-DDR FRAME_RING (triple-buffered, descriptor law spec/memory_rules.md §4.1) through the functional HPS bridge, and verify BOTH CRCs and the resource epoch before any byte reaches the decoder. Wave 2 matures this from stub to real fetch with the CRC gate before the first byte (plan D8). The documented hps→gpu async bridge for the command stream.

Exclusions: no semantic decoding (CMD.DECODER), no reordering of packets within an epoch, no slot ownership decisions (CMD.SCHEDULER claims READY slots; DMA fetches them).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`. Reset: fetch FSM idle, ring pointer unadvanced — a partially fetched packet is NEVER handed on; bridge port idle; epoch register 0.

## Input and output packet layouts

Input: FRAME_RING descriptors + slot bytes via MEM.HPS.BRIDGE bursts (`zhao_hps_burst_req_t`); the slot body is a sealed frame packet (capture_format.md §3) up to `FRAME_SLOT_BYTES` (1 MiB). Output: the verified packet bytes into the command-slot buffer (ready/valid), plus `{status, bytes_consumed, commands_consumed}` per packet (the generated validator's verdict).

## Backpressure rules

`ready_valid` into the decoder-facing buffer; credit-based at the bridge port (one burst in flight).

## Memory ownership

Read-only on FRAME_RING slots and descriptors EXCEPT the single `state` word transition READY→FPGA_RUNNING it performs on claim (the only word both sides ever write, never simultaneously — spec/memory_rules.md §4.1). No VRAM writes.

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

`zref::CmdDma` — fetch/verify oracle: given ring state and slot contents (incl. corrupted variants), the exact verdict, byte counts and slot transitions.

## Directed tests

`tests/command/cmd_dma_directed.cpp` — happy path; corrupt header CRC ⇒ ZERO bytes forwarded; corrupt payload CRC; epoch mismatch; truncated slot; 1 MiB worst-case timing budget.

## Randomized differential tests

`tests/command/cmd_dma_random.cpp` — fuzz corpus replay (the committed abi_corpus.zcorpus error set) + PCG ring timelines vs `zref::CmdDma`.

## Formal properties

`tests/formal/cmd_dma_crc_gate.sby` — no byte leaves before the header CRC passes; reset leaves no partial handoff.

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). CRC lane + counters (reuses zhao_crc32c_step from zhao_abi_pkg).

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` and `duo_markers.zcap` — every FRAME_PACKET section in every capture was fetched and verified through this path in the Verilator run (run-twice-assert-identical).

## Notes

CRC law: the SAME CRC-32C machine as the ABI (capture_format.md §2; one polynomial machine-wide — A3d).
