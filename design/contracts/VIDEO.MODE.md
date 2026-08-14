# Contract — VIDEO.MODE (Video mode generator)

> Ledger: `design/blocks.yml` · owner ZH-016 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Free-running raster/timing generator for the three frozen modes (Z60, Storm, Duo): mode register, per-mode H/V timing tables, sync polarities, pixel clock ratio. Emits the raster position (`x`, `y`, blank/sync windows, `frame_start`, `vblank`) that drives SCANOUT, FRAMECTL and SCALER. Law: `spec/video_rules.md` §1-§2.

Exclusions: no framebuffer access (VIDEO.SCANOUT), no swap/repeat decision (VIDEO.FRAMECTL), no pixel data (SCALER). The mode VALUE comes from CMD.SCHEDULER (SetPresentationContract execution); VIDEO.MODE only latches it at frame start. Timing constants exist exactly once, in `zhao_pkg.sv` (`ZHAO_TIMING`); board-derived PLL ratios change numbers post-ZH-016, never these contract surfaces.

## Clock and reset semantics

`vid_clk` domain (`vid_clk = gpu_clk/2` on the frozen sim profile). Synchronous active-low `rst_n` from SYS.RESET. Reset: raster at x=0/y=0 pre-active (first active pixel emerges after the back porch), mode register = `VIDEO_Z60` (0). Reset reaches idle (formal `video_mode_timing` reset-idle clause).

## Input and output packet layouts

Input: `mode_we` + `mode[1:0]` (one `video_mode` value; other encodings are not generated — the ABI validator already rejected them at frame level). Output: `zhao_px_stream_t` control fields — `hsync`, `vsync`, `hblank`, `vblank`, `x[9:0]`, `y[7:0]`, plus one-cycle pulses `frame_start` (leaving vblank) and `frame_end` (entering vblank), and the latched `mode_out`. Byte/packet-free: this block is pure control.

## Backpressure rules

None. The raster is free-running: it never stalls, never stretches, never waits (spec/video_rules.md §2). Consumers must keep up or observe starvation counters.

## Memory ownership

None. No memory ports.

## Q formats and rounding

None. All quantities are unsigned integers (pixel counts, line counts, cycles). The `video_mode` enum is a u8 wire value (ABI v2).

## Latency (fixed or variable)

Fixed: 1 vid cycle from register state to outputs.

## Target throughput

1 timing tick per vid cycle, continuous.

## Overflow and malformed-input behaviour

The raster wraps modulo H/V totals by construction (no overflow state). A mode write with an out-of-range value cannot arrive through the ABI (ZH_ABI_BAD_VALUE at the frame validator); if a rogue write is forced in simulation, the generator holds the previous mode (last valid wins) — documented, tested by a directed fault injection.

## Counters and traces

Owns no §25 counter directly; the `scanout_starvation_cycles` catalog entry is shared with SCANOUT (its events) — VIDEO.MODE exposes the raster position traces that make starvation attributable. Source IDs: not applicable (control path).

## Scalar reference function

`zref::VideoMode` — per-cycle timing trace: given (mode, start-cycle, mode-switch schedule), the exact (x, y, syncs, blanks, frame_start) for every cycle, incl. the mode-latch law (spec/video_rules.md §1.1).

## Directed tests

`tests/video/video_mode_directed.cpp` — 3-frame cycle-for-cycle walk per mode vs zref; mode switch mid-frame (old mode completes the frame, new mode effective next frame start); reset-idle; timing-table spot checks (frame period gpu cycles 251,520 / 217,984 / 318,592).

## Randomized differential tests

`tests/video/video_mode_random.cpp` — PCG mode-switch timelines at random cycle offsets, 1k fast / 100k nightly; the per-cycle trace must equal zref exactly.

## Formal properties

`tests/formal/video_mode_timing.sby` — raster values bounded by the mode table; exactly one `frame_start` per frame period; reset-idle; mode changes take effect only at `frame_start`.

## Synthesis / resource ceiling

Budget group `platform` (§25 ceiling 14% for the whole group). Per-block percentage unfrozen until Phase 0 (V5 gate). Expected: registers + small comparators only, no RAM/DSP.

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` — the raster's per-frame timing profile version rides ABI_INFO; FRAMEBUFFER_EXPECTED width/height must match the mode table for every section.

## Notes

Frozen interface: `zhao_video_mode_e`, `zhao_timing_t`, `ZHAO_TIMING` in `fpga/rtl/common/zhao_pkg.sv` — changes after freeze require architect sign-off (plan §3).
