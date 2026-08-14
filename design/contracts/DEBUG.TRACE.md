# Contract — DEBUG.TRACE (Trace ring and source IDs)

> Ledger: `design/blocks.yml` · owner ZH-074 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Selectable trace ring (charter §20.6) with source-ID propagation into the HPS trace arena.

Exclusions: no counters, no CRC. Source-ID scheme per spec/capture_format.md §5: every trace event carries the propagated source ID; the ring drains into the HPS trace arena via MEM.HPS.BRIDGE. Trace selection is a debug command; the ring is bounded and drops-oldest with a drop count (never silently stops).

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

Target throughput: 1 trace event per clock.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `commands`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::DebugTrace` (SW.ZREF).

## Directed tests

Planned: `tests/debug/debug_trace_directed.cpp`.

## Randomized differential tests

Planned: `tests/debug/debug_trace_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Contract filled (Phase-1-active). Source-ID scheme per capture_format.md §5.
