# Contract — VIDEO.FRAMECTL (Frame control handshake)

> Ledger: `design/blocks.yml` · owner ZH-016 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

The frame boundary owner: decides swap-vs-repeat at each vblank, drives the double-buffer swap, emits the machine-wide `frame_tick` broadcast, and closes the loop back to CMD.SCHEDULER with `frame_complete{slot, repeated}` and the completion fence. Monitors the deadline. Law: `spec/video_rules.md` §4-§5, §7; counters per `spec/counters.md`.

Exclusions: no pixel data or fetch (SCANOUT), no raster generation (MODE), no slot ownership FSM (CMD.SCHEDULER owns slots; FRAMECTL reads READY/ARM status and returns completion).

## Clock and reset semantics

`vid_clk` domain (the vblank decision is raster-aligned); the `frame_tick` crosses to `gpu_clk` via a 2-flop synchronizer + toggle handshake (the documented video→gpu control bridge, `async_bridge: true`). Reset: no tick, no swap; `frame_complete` idle; first tick only after the first complete displayed frame.

## Input and output packet layouts

Inputs: raster position/vblank from VIDEO.MODE; per-slot READY/ARM flags from CMD.SCHEDULER (mode register mirror); `deadline_cycles` (default = mode frame period). Outputs: `swap_req{slot}`/`swap_ack` to SCANOUT (vblank only); `frame_tick` (1-cycle pulse, machine-wide); `frame_complete{slot, repeated}` + completion fence to CMD.SCHEDULER; `deadline_faults` event. No byte packets — pure control.

## Backpressure rules

None (single-cycle handshakes). `swap_ack` must arrive within the same vblank; if SCANOUT cannot ack (fault), FRAMECTL holds and repeats — the fail-safe direction is always repeat.

## Memory ownership

None.

## Q formats and rounding

Unsigned integer cycles only; the deadline compare is exact (no rounding).

## Latency (fixed or variable)

Fixed: 1 vid cycle from vblank start to tick/fence (decision combinational in the vblank window).

## Target throughput

1 handshake per displayed frame (60 Hz-equivalent: one tick per frame period).

## Overflow and malformed-input behaviour

No READY slot ⇒ repeat path: previous slot retained, `frame_repeated`, `deadline_faults++` once per missed frame (not per line). Exactly one completion fence per FPGA_RUNNING→DONE — a repeated frame fences the slot it repeats (the scheduler counts the miss). A mode change during vblank resolves before the swap decision (mode-latch law first, spec/video_rules.md §1.1).

## Counters and traces

`frame_cycles` (one per tick) and `deadline_faults`, both shadow-latched at the tick itself (spec/counters.md §3). Trace: per-frame decision record {slot, repeated, deadline margin cycles} to the harness.

## Scalar reference function

`zref::FrameCtl` — decision oracle: given slot-READY timelines and deadlines vs the raster, the exact swap/repeat/tick/fence schedule.

## Directed tests

`tests/video/video_framectl_directed.cpp` — clean handoff; missed deadline ⇒ repeat + fence + counters; late-but-inside-dedeadline acceptance boundary (1 cycle early vs exactly at deadline); mode change across vblank.

## Randomized differential tests

`tests/video/video_framectl_random.cpp` — PCG READY/deadline timelines vs `zref::FrameCtl`, 1k/100k; fence-exactly-once under adversarial READY flapping.

## Formal properties

`tests/formal/video_framectl_one_fence.sby` — exactly one fence per FPGA_RUNNING→DONE transition; tick rate exactly one per frame period; reset-idle.

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Registers + comparators.

## Integration capture cases

Every `captures/golden/wave2/*` frame: COUNTERS sections carry `frame_cycles` and `deadline_faults`; the Duo marker demo asserts `deadline_faults == 0` for 600 frames.

## Notes

`frame_tick` is THE frame boundary for INPUT.SNAPSHOT, DEBUG.COUNTERS and CMD.SCHEDULER — its law is spec/counters.md §3; changing its timing is a zhao_pkg.sv freeze request.
