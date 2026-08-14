# Contract — INPUT.SNAPSHOT (Pad snapshot bridge)

> Ledger: `design/blocks.yml` · owner ZH-017 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Latch all four pad slots atomically at the broadcast `frame_tick`, maintain per-pad monotonic sequences, and hand the canonical PadFrame array to the HPS runtime across the documented pad→hps async bridge. Law: `spec/input_rules.md` (D5).

Exclusions: no rumble (INPUT.RUMBLE), no pad PHY or SNAC protocol (INPUT.SNAC / hardware lane), no policy (deadzone, calibration, remapping — hardware applies ZERO policy; raw samples only).

## Clock and reset semantics

Pad input side in the pad polling domain (hardware lane; Verilator models it in the harness tick scheduler), snapshot registers and the HPS handoff in `gpu_clk` — the documented `async_bridge: true` crossing. `frame_tick` arrives synchronized (FRAMECTL toggle handshake). Reset: sequences 0, all pads absent (flags.pad_present=0, fields 0), no handoff until the first tick.

## Input and output packet layouts

Input: raw per-pad state `{present, buttons[31:0], lx, ly, rx, ry}` (i16 sticks, raw). Output: the GENERATED `zhao_pad_frame_t` (re-exported by zhao_pkg.sv) ×4 plus `frame_id` — the exact ABI struct PadFrame (20 B, spec/input_rules.md §1); the .zcap CONTROLLER_SNAPSHOT section body is `u32 count + PadFrame[]` verbatim.

## Backpressure rules

None (`backpressure: none`): the snapshot register file is overwritten every tick; the HPS reads a stable double-buffered copy selected by a gray-coded pointer — the latch is stable for a full frame by construction, so no HPS stall can corrupt a snapshot.

## Memory ownership

None (register file only; the HPS-side capture writer owns any .zcap bytes).

## Q formats and rounding

Sticks: raw i16 two's complement, NO deadzone/scaling (spec/input_rules.md §1). `ly`/`ry` positive = down (screen convention). No arithmetic ⇒ no rounding.

## Latency (fixed or variable)

Fixed: 1 gpu cycle from tick to stable snapshot; HPS visibility the next domain crossing (one synchronized toggle, deterministic in Verilator per plan R1).

## Target throughput

1 snapshot per frame (per frame_tick), 4 pads atomically.

## Overflow and malformed-input behaviour

Absent pad ⇒ `pad_present=0`, `buttons=0`, sticks `0`, sequence frozen (spec/input_rules.md §2.2). Sequence wraps mod 2^16 (defined, not overflow). `pad_index > 3` cannot be produced (4 register slots); consumers reject such sections whole. Mid-frame input changes never partially appear (atomic latch).

## Counters and traces

`input_sequence_gaps` — counts observed sequence gaps (merge paths / tool side; the synchronous FPGA latch cannot gap by construction). Trace: per-frame PadFrame array into the harness for differential compare and capture.

## Scalar reference function

`zref::PadSnapshot` — given per-pad raw timelines and tick schedule, the exact latched PadFrame array and sequences per frame.

## Directed tests

`tests/input/input_snapshot_directed.cpp` — mid-line stick/button change atomicity; sequence monotonic across 10k frames incl. 2^16 wrap; absent-pad law; pad_index ordering in the array.

## Randomized differential tests

`tests/input/input_snapshot_random.cpp` — PCG pad streams (presence toggles, jittered change points) vs `zref::PadSnapshot`, bit-exact arrays, 1k/100k.

## Formal properties

`tests/formal/input_snapshot_atomic.sby` — no snapshot bit changes between ticks; sequence increments exactly once per tick while present, never otherwise.

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). 4 × ~20 B registers + CDC toggles.

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — CONTROLLER_SNAPSHOT per frame; PadFrame sequences gapless for all 600 frames (asserted).

## Notes

PadFrame layout is ABI (commands.zidl), NEVER re-defined here — the SV type is the generated `zhao_pad_frame_t` (reverse field order per the generator's byte-identity law).
