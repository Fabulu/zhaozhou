# Contract — VIDEO.SCANOUT (Scanout engine)

> Ledger: `design/blocks.yml` · owner ZH-016 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Double-buffered display of the resolved framebuffer: VRAM fetch (gpu domain), 2 × 512 × RGB565 ping-pong line buffers, video-domain serializer, and the displayed-stream the whole machine CRCs. Enforces the 60 Hz law: a late frame repeats the previous complete frame, never tears. Decomposition + law: `spec/video_rules.md` §3-§4 (D7).

Exclusions: no swap/deadline decision ownership (VIDEO.FRAMECTL decides, SCANOUT executes the swap in vblank); no colour conversion or scaling (VIDEO.SCALER); no framebuffer writes (blit DMA writes, SCANOUT only reads, through MEM.GUARD).

## Clock and reset semantics

Two domains: fetch side runs in `gpu_clk` (a MEM.GUARD/arbiter client), serializer side in `vid_clk`; the line buffers are the documented gpu→video bridge (gray-coded buffer-select toggles, one full line of margin by construction). Synchronous active-low `rst_n` per domain; reset: both buffer selects to slot 0, no display until the first complete frame is fetched (the raster repeats black — repeat law with no previous frame), fetch FSM idle.

## Input and output packet layouts

Input (gpu domain): `zhao_guard_req_t` reads — 2 B per pixel, bursts per line (`h_active × 2` bytes from the slot base, row-major). Output (vid domain): `zhao_px_stream_t` — one RGB565 pixel per vid cycle during H active with x/y, plus `frame_repeated` pulse and the displayed byte stream that DEBUG.CRC consumes. Control inputs: `swap_req{slot}` / `swap_ack` from FRAMECTL (vblank only), raster position from VIDEO.MODE.

## Backpressure rules

Scanout fetch is the STRICT-PRIORITY guaranteed arbiter client (D3): it never receives backpressure at the arbiter beyond its liveness bound (34 sdram cycles refresh-free, 47 with a refresh steal — spec/memory_rules.md §2.1; tighter than the RR class's 65 because only one aging-override burst can precede it); it preempts other clients at burst boundaries only. The serializer has no backpressure (free-running raster): starvation is visible via counters and the never-torn law, never a stall.

## Memory ownership

READ-ONLY on both FB slots (region map spec/memory_rules.md §5), via MEM.GUARD. No writes, no ownership transitions. Per-client bytes counted.

## Q formats and rounding

RGB565 halfwords consumed as-is (little-endian); no arithmetic, no rounding. Bit layout `[15:11] R, [10:5] G, [4:0] B` (spec/video_rules.md §3).

## Latency (fixed or variable)

Variable but bounded: one line of prefetch margin (line N+1 fills while line N displays); swap effective at the next vblank after `swap_req`.

## Target throughput

1 pixel per vid cycle during H active (480/416/608 cycles per line by mode); fetch ≈ 1 burst-8 per 16 gpu cycles sustained.

## Overflow and malformed-input behaviour

No READY slot at vblank ⇒ previous frame repeats (`frame_repeated`, `deadline_faults++` — the 60 Hz law). Fetch starved mid-line ⇒ serializer re-emits current line-buffer content, `scanout_starvation_cycles` counts every starved vid cycle, never a torn line. A guard-rejected read (impossible in Phase 2 — scanout owns both slots read-only) would repeat the line and count starvation.

## Counters and traces

`scanout_starvation_cycles` (latched at frame_tick per spec/counters.md §3). Trace: per-frame fetch order + swap/repeat decision emitted to the harness (differential compare key).

## Scalar reference function

`zref::Scanout` — fetch-order + swap/repeat oracle: given slot-READY timelines and the raster, the exact displayed pixel stream, repeat decisions, and the fetch order.

## Directed tests

`tests/video/video_scanout_directed.cpp` — 3 clean frames; forced missed deadline ⇒ repeat + `deadline_faults++` + repeat-CRC identical; line-underrun injection ⇒ starvation counter, no tear; Duo canvas map (border rows, two 256×192 views).

## Randomized differential tests

`tests/video/video_scanout_random.cpp` — PCG slot-READY timelines (incl. jittered blit completion), 1k fast / 100k nightly, displayed stream bit-exact vs `zref::Scanout`.

## Formal properties

`tests/formal/video_scanout_linebuf.sby` — serializer never overtakes fill; swap only in vblank; the displayed stream is never torn (both line buffers and both frame buffers).

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Line buffers 2 × 512 × 16 b = 16 Kbit M10K. Per-block ALM/DSP percentage unfrozen until Phase 0.

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` (displayed CRC per frame) and `captures/golden/wave2/duo_markers.zcap` (600-frame CRC chain).

## Notes

The repeated-frame CRC-identity rule is the mechanical proof of the 60 Hz law — the directed missed-deadline case asserts it (plan §0, D7).
