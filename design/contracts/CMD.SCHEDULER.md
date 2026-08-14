# Contract — CMD.SCHEDULER (Frame-slot scheduler)

> Ledger: `design/blocks.yml` · owner ZH-009 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Own the frame-slot FSM (FREE → CLAIMED → EXECUTING → FENCED → DONE); enforce deadlines with a fault counter, issue the completion fence, hand engine dispatch and region ownership to the engines and the memory guard.

Exclusions: no decoding, no data-path work. Slot FSM: FREE → CLAIMED → EXECUTING → FENCED → DONE with deadline enforcement and a fault counter; the completion fence is the only signal that releases a slot. Overflow/malformed: a slot that misses its deadline faults, repeats or drops per policy, and NEVER crosses the frame boundary into the next slot.

## Clock and reset semantics

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Input and output packet layouts

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Backpressure rules

Backpressure: `credit`.

## Memory ownership

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Q formats and rounding

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: 1 slot transition per clock.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Counters and traces

Counters: `frame_cycles`, `deadline_faults`, `commands`. Source IDs: propagated.

## Scalar reference function

Reference: `zref::CmdScheduler` (SW.ZREF).

## Directed tests

Planned: `tests/command/cmd_scheduler_directed.cpp`.

## Randomized differential tests

Planned: `tests/command/cmd_scheduler_random.cpp`.

## Formal properties

Planned: `tests/formal/cmd_scheduler_slot_fsm.sby`.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Contract filled (Phase-1-active). The dispatch fan-out mirrors charter §5's CMD → engines edges.
