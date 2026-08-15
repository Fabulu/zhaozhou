# Contract — INPUT.RUMBLE (Rumble bridge)

> Ledger: `design/blocks.yml` · owner ZH-017 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Carry runtime rumble requests (ABI `DebugRumble 0xF004`) out to the pads: frame-gated latch, per-pad PWM duty target, 1 kHz carrier. The documented hps→pad async bridge; a leaf (pad PHY out). Law: `spec/input_rules.md` §3.

Exclusions: no snapshot path (INPUT.SNAPSHOT), no pad protocol/PHY beyond the PWM carrier, no stop-policy (software owns timeouts — Phase 2 holds the last target).

## Clock and reset semantics

Latches in `gpu_clk` (frame_tick domain); the 1 kHz PWM free-runs in the pad clock domain (hardware lane; harness-modelled in Verilator). Documented `async_bridge: true`. Reset: all duties 0 (motors off), PWM phase 0, `rumble_frames_dropped` 0.

## Input and output packet layouts

Input: `DebugRumble` payload `{u8 pad_index; u8 enable; u8 strength}` from the executed command stream (CMD.SCHEDULER dispatch). Output: per-pad `{enable, duty[7:0]}` — duty = `strength` when enabled, else 0 — onto the 1 kHz carrier (duty/256). No byte packets.

## Backpressure rules

None. One update per frame per pad is applied; extra updates replace-and-count (see overflow behaviour) — there is no queue to press on.

## Memory ownership

None.

## Q formats and rounding

Duty is exact integer arithmetic: `duty = enable ? strength : 0` (strength 0..255 ⇒ duty 0..255/256). No rounding exists in this block.

## Latency (fixed or variable)

Fixed: latched at the NEXT frame_tick after command execution (≤ 1 frame); PWM phase never resets (no glitch on duty change).

## Target throughput

1 update per frame (per pad); PWM ticks at 1 kHz continuously.

## Overflow and malformed-input behaviour

`pad_index > 3` ⇒ request dropped entirely, `rumble_frames_dropped++` (never wraps onto another pad). A second DebugRumble for the same pad within one frame ⇒ last-writer-wins at the latch, the dropped one counts `rumble_frames_dropped++`. No command in a frame ⇒ previous target HOLDS (no auto-stop). The frame containing DebugRumble must set header flags bit0 (frame-level law, ZH_ABI_DEBUG_FLAG_REQUIRED).

## Counters and traces

`rumble_frames_dropped` (shadow-latched at frame_tick). Trace: per-frame latched duty table into the harness.

## Scalar reference function

`zref::RumbleBridge` — given command timelines and ticks, the exact latched duty table and the PWM waveform (carrier phase 0 at reset, never reset).

## Directed tests

`tests/input/input_rumble_directed.cpp` — DebugRumble ⇒ PWM duty latched at next tick; double-command ⇒ last wins + counter; bad index ⇒ drop + counter; enable=0 vs strength>0; hold-with-no-command.

## Randomized differential tests

`tests/input/input_random.cpp` — PCG command timelines (0-5 per frame, valid + out-of-range indices, duplicate-pad replacements) vs `zref::RumbleBridge` duty table + PWM carrier bit-exact every cycle; 1k fast / 100k nightly (shared timeline with the snapshot differential).

## Formal properties

Covered by the input atomicity property family (`input_snapshot_atomic` scope note): duty changes only at frame_tick; no other latch path exists. No standalone SBY planned.

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). 4 × small registers + PWM counter.

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — COUNTERS asserts `rumble_frames_dropped == 0` for all 600 frames (the demo sends at most one rumble command per pad per frame).

## Notes

leaf (pad PHY out). The carrier (1 kHz) is the hardware-lane pad clock domain; in Verilator the harness samples the duty table, not the carrier.
