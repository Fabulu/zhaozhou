# Contract — CMD.DMA (Command DMA)

> Ledger: `design/blocks.yml` · owner ZH-007 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Fetch sealed frame packets from the HPS-DDR ring via the framework bridge; verify CRC and epoch before any byte reaches the decoder; the documented hps→gpu async bridge for the command stream.

Exclusions: no semantic decoding (that is CMD.DECODER), no reordering of packets within an epoch. Clock/reset: gpu domain, reset returns the fetch FSM to idle with the ring pointer unadvanced (a partially fetched packet is never handed on). Malformed input: header CRC failure or epoch mismatch drops the packet, increments deadline_faults, and yields the safe error code — never partial delivery.

## Clock and reset semantics

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Input and output packet layouts

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Backpressure rules

Backpressure: `ready_valid`.

## Memory ownership

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Q formats and rounding

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: saturate one HPS-DDR burst slot per frame.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `commands`, `hps_ddr_bytes_by_client`, `deadline_faults`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::CmdDma` (SW.ZREF).

## Directed tests

Planned: `tests/command/cmd_dma_directed.cpp`.

## Randomized differential tests

Planned: `tests/command/cmd_dma_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Fail-safe order per capture_format.md §3.3 — never emits a byte of a packet whose header CRC failed.
