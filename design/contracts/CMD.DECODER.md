# Contract — CMD.DECODER (Command decoder)

> Ledger: `design/blocks.yml` · owner ZH-008 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Validate and dispatch semantic commands from sealed packets (generated ABI package); any malformed record becomes a safe error code, never a partial write.

Exclusions: no frame-slot ownership (CMD.SCHEDULER), no memory access. Packet layouts come EXCLUSIVELY from the generated ABI package (spec/commands.zidl → zhao_abi_pkg.sv); hand-written layouts are a review blocker. Malformed input: per-record length/opcode checks; a malformed record stops the packet with a safe error and no register/memory side effects (Dalvik model — reject before any write).

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

Target throughput: 1 record per clock.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `commands`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::CmdDecoder` (SW.ZREF).

## Directed tests

Planned: `tests/command/cmd_decoder_directed.cpp`.

## Randomized differential tests

Planned: `tests/command/cmd_decoder_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Contract filled (Phase-1-active). Byte layout comes from the generated zhao_abi_pkg.sv (W4) — never hand-written.
