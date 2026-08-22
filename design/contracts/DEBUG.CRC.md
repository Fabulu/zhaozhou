# Contract — DEBUG.CRC (Tile/frame CRC)

> Ledger: `design/blocks.yml` · owner ZH-073 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

CRC-32C over the DISPLAYED pixel stream (and, from Phase 4, resolved tiles) — the hardware-proven maturity evidence primitive (§20/§29-17) and the differential compare key. Wave 2 lands the displayed-frame CRC that enforces the 60 Hz law mechanically (repeat ⇒ identical CRC; spec/video_rules.md §4).

Exclusions: not a general checksum engine — CRC-32C ONLY, identical polynomial/table/step to the frame packet and .zcap CRC (plan A3d, capture_format.md §2); no trace (DEBUG.TRACE), no counters aggregation (DEBUG.COUNTERS).

## Clock and reset semantics

`vid_clk` domain for the displayed-stream lane (the CRC follows the serializer); gpu-domain lanes (later phases) cross via the standard toggles. Synchronous active-low `rst_n`; reset: CRC register 0x…seed, no frame in progress, `frame_crc` invalid.

**This sentence and `design/blocks.yml` disagreed until 2026-08-22, and the code followed the ledger.** The ledger said `clock_domain: gpu`, so `zhao_debug_crc` ran on `gpu_clk` and `zhao_shell_top` re-timed the video pixel register — sixteen data bits plus x, y and validity — across `vid_clk -> gpu_clk` on every displayed pixel, correct only under the frozen simulation phase (`vid_clk = gpu_clk/2`, coincident posedges). Both hold violations measured in the HIGH PERFORMANCE fitter experiment were on that seam. Fabian ruled this contract's reading (`docs/OWNER_DOCKET.md`, "RULED 2026-08-22 — BALANCED stays authoritative"): the block moved into `vid_clk`, and the ledger was corrected to `clock_domain: video`.

**What crosses to `gpu_clk` now**, and it is the whole crossing: the finalized 32-bit CRC, once per displayed frame, published on a toggle with the value registered beside it and held until the next frame. The gpu side synchronizes the toggle through three flops and edge-detects it into a one-cycle pulse. No per-pixel state leaves the video domain.

## Input and output packet layouts

Input: the displayed PIXEL stream from VIDEO.SCANOUT's serializer — one RGB565 pixel per `vid_clk`, `active_width` pixels per line, border rows included in Duo. Each pixel is two stream bytes, LITTLE-ENDIAN (`video_rules.md` §3): `in_px_i[7:0]` is the first byte on the wire and `in_px_i[15:8]` the second, so the covered bytes are exactly the bytes FRAMEBUFFER_EXPECTED covers. `in_sof_i` accompanies the first pixel of the frame (restart + latch `expect_bytes_i`), `in_eof_i` the last (finalize).

Because the stream is pixel-granular its length is always EVEN. An ODD `expect_bytes_i` therefore cannot be satisfied by any frame; the device refuses it as mis-sized rather than rounding to the nearest pixel. That is a statement about the expectation, not about the raster. Output: `{frame_crc_valid, frame_crc32c}` once per displayed frame, AFTER the repeat decision; consumed by the harness/captures and compared to EndFrame.expected_framebuffer_crc / FRAMEBUFFER_EXPECTED.

## Backpressure rules

`ready_valid` on the output register (single consumer, always ready in Phase 2); the input side has none — the displayed stream cannot stall (free-running raster), and the CRC is wide enough (2 bytes/clock) by construction.

## Memory ownership

None — streams only; never reads VRAM itself.

## Q formats and rounding

None (byte stream, CRC arithmetic is the spec's own law: poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF).

**One polynomial machine-wide still holds, by a different route.** A byte-serial step is eight dependent XOR levels, so two bytes per clock would be sixteen; this lane instead folds both bytes in ONE tree about seven levels deep, via `zhao_crc32c_fold` (`fpga/rtl/common/zhao_crc32c_fold.sv`). That module derives its columns at elaboration from the CRC-32C definition and is held to the SHIPPED `zhao_crc32c_step` by `tests/differential/crc32c_fold_directed.cpp`, so it is the same polynomial machine as the frame packet and .zcap CRC (plan A3d), not a variant.

## Latency (fixed or variable)

Fixed: the finalized CRC is valid `variable_bounded:4` cycles after the last displayed pixel (this lane uses 1 `vid_clk`). The shell's gpu-facing pulse arrives a further three `gpu_clk` cycles later — the synchronizer depth on the once-per-frame crossing.

## Target throughput

1 displayed pixel — 2 bytes — per `vid_clk`.

## Overflow and malformed-input behaviour

None possible: the byte stream is raster-lawful by construction; the CRC register's evolution is total. A mis-sized stream (wrong byte count for the mode) is a raster-side protocol violation, asserted in sim — the CRC never "adapts".

## Counters and traces

`tile_references` activates with the tile path (Phase 4); the frame-CRC value itself is the trace. Source IDs: n/a (evidence primitive).

## Scalar reference function

`zref::Crc32c` (reference/include/zref/zref_cmd2.hpp) — the displayed-stream device oracle: sof/eof framing, publish-only-on-exact-size, size_err on a mis-sized stream or a stray byte. Delegates the CRC arithmetic to the generated `zhao_abi::zhao_crc32c` (one CRC machine repo-wide, charter 19/29-6); the repeat-law test asserts repeated frames CRC-equal.

## Directed tests

`tests/debug/debug_crc_directed.cpp` — known-vector frames (the 32-byte CRC-32C test vectors from capture_format.md §2.1 reused over pixel payloads); repeat ⇒ identical CRC; mode-dependent byte counts (184,320 / 153,600 / 245,760); the smallest lawful frame (one pixel, two bytes); mis-sized and ODD expectations; a stray pixel outside any frame; and byte order stated directly, since that is the one law the byte→pixel port change could have silently inverted. The canonical nine-byte `"123456789"` vector is not here — an odd byte count is not a displayable stream; it guards the polynomial itself in `tests/unit/test_crc.cpp` and `tests/fuzz/test_abi_fuzz_parity.cpp`.

## Randomized differential tests

`tests/debug/debug_crc_directed.cpp --random N` (CTest `debug_crc_random` / `debug_crc_random_nightly`) — PCG displayed streams vs `zref::Crc32c` (which delegates to the generated `zhao_abi::zhao_crc32c`), bit-exact. **The differential is CROSS-GRANULARITY on purpose:** the device is driven one PIXEL per clock while the shipped oracle is driven the same stream one BYTE at a time, and neither the oracle nor the generated CRC was touched for the move. A byte-order slip inside the pixel lane cannot agree with an oracle folding the two bytes in the other order. Each frame is also replayed against an unsatisfiable expectation, so the publish gate is exercised as often as the arithmetic.

## Formal properties

No standalone SBY (the CRC step function is already golden-locked tri-language; the repeat-identity law is asserted in the VIDEO.SCANOUT properties and the directed tests).

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). One two-byte `zhao_crc32c_fold` tree per stream.

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` (per-frame FRAMEBUFFER_EXPECTED) and `duo_markers.zcap` (600-frame CRC chain hashed into the trajectory evidence — plan W2.7).

## Notes

CRC-32C identical to the ABI/frame CRC (A3d) — one polynomial machine-wide; the SV lane reuses the generated `zhao_crc32c_step`, never a hand-copy.
