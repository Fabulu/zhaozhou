# Contract — DEBUG.CRC (Tile/frame CRC)

> Ledger: `design/blocks.yml` · owner ZH-073 · phase 4 · maturity SPECIFIED

## Purpose and exclusions

CRC-32C over resolved tiles and whole frames — the hardware-proven maturity evidence primitive (§20/§29-17).

Exclusions: not a general checksum engine — CRC-32C only, identical polynomial/table to the frame packet and .zcap CRC (plan A3d). Evidence role: tile/frame CRCs are the HARDWARE_PROVEN maturity evidence primitive (§29-17) and the differential compare key.

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

Latency: `variable_bounded:4`.

## Target throughput

Target throughput: 1 byte per clock per lane.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `tile_references`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::Crc32c` (SW.ZREF).

## Directed tests

Planned: `tests/debug/debug_crc_directed.cpp`.

## Randomized differential tests

Planned: `tests/debug/debug_crc_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

CRC-32C identical to the ABI/frame CRC (A3d) — one polynomial machine-wide.
