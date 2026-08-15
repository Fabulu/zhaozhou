# Contract — DEBUG.CRC (Tile/frame CRC)

> Ledger: `design/blocks.yml` · owner ZH-073 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

CRC-32C over the DISPLAYED pixel stream (and, from Phase 4, resolved tiles) — the hardware-proven maturity evidence primitive (§20/§29-17) and the differential compare key. Wave 2 lands the displayed-frame CRC that enforces the 60 Hz law mechanically (repeat ⇒ identical CRC; spec/video_rules.md §4).

Exclusions: not a general checksum engine — CRC-32C ONLY, identical polynomial/table/step to the frame packet and .zcap CRC (plan A3d, capture_format.md §2); no trace (DEBUG.TRACE), no counters aggregation (DEBUG.COUNTERS).

## Clock and reset semantics

`vid_clk` domain for the displayed-stream lane (the CRC follows the serializer; reuses `zhao_crc32c_step` from the generated zhao_abi_pkg — the per-byte SV step, capture_format.md §2.2); gpu-domain lanes (later phases) cross via the standard toggles. Synchronous active-low `rst_n`; reset: CRC register 0x…seed, no frame in progress, `frame_crc` invalid.

## Input and output packet layouts

Input: the displayed byte stream from VIDEO.SCANOUT's serializer (2 × active_width bytes per line, RGB565 LE, border rows included in Duo — exactly the bytes FRAMEBUFFER_EXPECTED covers), with line/frame markers from the raster. Output: `{frame_crc_valid, frame_crc32c}` once per displayed frame, AFTER the repeat decision; consumed by the harness/captures and compared to EndFrame.expected_framebuffer_crc / FRAMEBUFFER_EXPECTED.

## Backpressure rules

`ready_valid` on the output register (single consumer, always ready in Phase 2); the input side has none — the displayed stream cannot stall (free-running raster), and the CRC is wide enough (1 byte/clock) by construction.

## Memory ownership

None — streams only; never reads VRAM itself.

## Q formats and rounding

None (byte stream, CRC arithmetic is the spec's own law: poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF).

## Latency (fixed or variable)

Fixed: the finalized CRC is valid `variable_bounded:4` cycles after the last displayed byte (pipeline flush).

## Target throughput

1 byte per clock per lane.

## Overflow and malformed-input behaviour

None possible: the byte stream is raster-lawful by construction; the CRC register's evolution is total. A mis-sized stream (wrong byte count for the mode) is a raster-side protocol violation, asserted in sim — the CRC never "adapts".

## Counters and traces

`tile_references` activates with the tile path (Phase 4); the frame-CRC value itself is the trace. Source IDs: n/a (evidence primitive).

## Scalar reference function

`zref::framePixelCrc` — byte-exact displayed-frame CRC oracle: given the canvas bytes and mode (incl. Duo border law), the identical CRC-32C value; the repeat law oracle asserts repeated frames CRC-equal.

## Directed tests

`tests/debug/debug_crc_directed.cpp` — known-vector frames (CRC-32C test vectors from capture_format.md §2.1 reused over pixel payloads); repeat ⇒ identical CRC; mode-dependent byte counts (184,320 / 153,600 / 245,760).

## Randomized differential tests

`tests/debug/debug_crc_random.cpp` — PCG canvases vs `zref::framePixelCrc`, bit-exact, 1k/100k frames.

## Formal properties

No standalone SBY (the CRC step function is already golden-locked tri-language; the repeat-identity law is asserted in the VIDEO.SCANOUT properties and the directed tests).

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). One 32-bit XOR/shift lane per stream.

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` (per-frame FRAMEBUFFER_EXPECTED) and `duo_markers.zcap` (600-frame CRC chain hashed into the trajectory evidence — plan W2.7).

## Notes

CRC-32C identical to the ABI/frame CRC (A3d) — one polynomial machine-wide; the SV lane reuses the generated `zhao_crc32c_step`, never a hand-copy.
