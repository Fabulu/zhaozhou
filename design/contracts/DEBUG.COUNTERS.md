# Contract — DEBUG.COUNTERS (Counter aggregation)

> Ledger: `design/blocks.yml` · owner ZH-072 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Aggregate the §25 mandatory counter set (bytes by client, per-engine counts) and expose it to debug commands.

Exclusions: no trace (DEBUG.TRACE), no CRCs (DEBUG.CRC). The counter set is exactly the counter_catalog of design/blocks.yml (§25 minimum + declared extensions); adding a counter is a ledger edit, not an RTL whim. Counters are readable via debug commands (0xF00n umbrella, header flags bit0 set).

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

Target throughput: 1 counter update per clock.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `frame_cycles`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::DebugCounters` (SW.ZREF).

## Directed tests

Planned: `tests/debug/debug_counters_directed.cpp`.

## Randomized differential tests

Planned: `tests/debug/debug_counters_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Contract filled (Phase-1-active). Counter set = the counter_catalog of this file (V12).
