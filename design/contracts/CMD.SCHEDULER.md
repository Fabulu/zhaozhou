# Contract — CMD.SCHEDULER (Frame-slot scheduler)

> Ledger: `design/blocks.yml` · owner ZH-009 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Own the 3-slot frame ownership FSM with the CHARTER §7.4 STATE NAMES — `FREE → ARM_WRITING → READY → FPGA_RUNNING → DONE → FREE` — exactly one owner per slot at any time. Enforce `BeginFrame.deadline_cycles` (default = current mode frame period) with `deadline_faults`; issue EXACTLY ONE completion fence per FPGA_RUNNING→DONE; claim window for the harness-as-HPS in Verilator; dispatch interface with Phase-2 sinks (debug-blit DMA, counter snapshot) and no-op engine ports. Wave-2 scope per plan D8: real = slot FSM + deadline + fence + dispatch sinks; stubbed = semantic engine fan-out (ports exist, no-op).

Exclusions: no decoding (CMD.DECODER), no data-path work, no raster (VIDEO.*), no region law (MEM.GUARD takes grants from here).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`. Reset: ALL slots FREE (reset-idle — formal), deadline counters clear, mode register `VIDEO_Z60`, fence idle, dispatch idle.

## Input and output packet layouts

Input: validated command records from CMD.DMA (BeginFrame sets epoch/deadline; SetPresentationContract sets the mode register; DebugFrameBlit/DebugRumble sink to dispatch), `frame_complete{slot, repeated}` + `frame_tick` from VIDEO.FRAMECTL, FRAME_RING descriptors (spec/memory_rules.md §4.1). Output: engine dispatch records (op + payload slice), slot-state word writes to the ring, guard region grants, the completion fence, the latched mode register to VIDEO.MODE.

## Backpressure rules

`credit` — dispatch credits per sink; a no-op sink always accepts (Phase 2).

## Memory ownership

Owns the FRAME_RING `state` transitions on the FPGA side (claim READY→FPGA_RUNNING, release DONE→FREE; the HPS owns FREE→ARM_WRITING→READY). Grants VRAM regions per frame via MEM.GUARD. Owns no pixel/audio data.

## Q formats and rounding

Unsigned cycles/counts only; the deadline compare is exact.

## Latency (fixed or variable)

Variable; slot transitions at 1 per clock (`1 slot transition per clock`).

## Target throughput

1 slot transition per clock; 3 slots in flight; one fence per displayed frame.

## Overflow and malformed-input behaviour

A slot that misses its deadline faults (`deadline_faults++`), the video side repeats the previous frame (never a partial display — the 60 Hz law), and the slot NEVER crosses its frame boundary into the next slot (one-owner law). A packet failing validation (CMD.DMA verdict) never claims FPGA_RUNNING: it returns DONE with the error, and the deadline/repeat machinery handles the miss. Adversarial double-claim (two READYs same slot) is unreachable through the descriptor law and asserted impossible by the formal property.

## Counters and traces

`frame_cycles` (from FRAMECTL ticks, mirrored), `deadline_faults`, `commands`. Source IDs: propagated to dispatch. Trace: slot-state timeline + fence events to the harness (differential key).

## Scalar reference function

`zref::CmdScheduler` — slot-FSM oracle: given ring timelines, deadlines and frame_completes, the exact state sequence, fence schedule and repeat accounting; fence-exactly-once by construction.

## Directed tests

`tests/command/cmd_scheduler_directed.cpp` — happy cycle; late seal ⇒ repeat, no fence for the dead frame, next frame clean; corrupt packet ⇒ DONE-with-error, no FPGA_RUNNING; fence-exactly-once adversarial (READY flap at deadline); harness claim window.

## Randomized differential tests

`tests/command/cmd_random.cpp` (CTest `cmd_random` / `cmd_random_soak`) — PCG ring/deadline timelines vs `zref::CmdScheduler`, 1k/100k.

## Formal properties

`tests/formal/cmd_scheduler_slot_fsm.sby` — exactly one state per slot; no two owners (charter §20.4); DONE-before-FREE; reset-idle; deadline never crosses the frame boundary.

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling).

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — 600 sealed frames (BeginFrame + SetPresentationContract + DebugFrameBlit + EndFrame per frame) through the real FSM; COUNTERS show `deadline_faults == 0` and no ownership violation.

## Notes

State names are charter §7.4 law (the Phase-1 sketch's CLAIMED/EXECUTING/FENCED aliases are retired). The dispatch fan-out mirrors charter §5's CMD → engines edges; Wave 2 sinks only blit + counters.
