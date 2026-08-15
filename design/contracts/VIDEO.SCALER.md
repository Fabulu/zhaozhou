# Contract — VIDEO.SCALER (Scaler feed)

> Ledger: `design/blocks.yml` · owner ZH-016 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Adapt the native pixel stream to the framework/MiSTer scaler output path. Phase 2: a PASS-THROUGH formatter — it repackages SCANOUT's stream into the video-mixer-facing port (`zhao_px_stream_t` incl. syncs/blanks) with no colour conversion, no scaling, no policy. Law: `spec/video_rules.md` §6.

Exclusions: no timing generation (VIDEO.MODE), no memory, no scaling arithmetic (the aiscal/MiSTer adapter is the hardware-lane seam recorded here as a seam, not Phase-2 behaviour).

## Clock and reset semantics

`vid_clk` domain, synchronous active-low `rst_n`. Reset: outputs invalidated (`valid=0`) until the first active pixel of the first frame; pipeline registers flushed.

## Input and output packet layouts

Input: `zhao_px_stream_t` from SCANOUT (RGB565 + x/y + syncs). Output: the same type, 2-cycle pipeline delay, retimed onto the scaler-facing port; the hardware-lane adapter (sys/video_mixer/ascal, never modified — charter §29-3) consumes it. No packets, no byte streams.

## Backpressure rules

`ready_valid` on the output port: if the (hardware-lane) consumer deasserts ready, SCALER holds the last pixel and the stall propagates to SCANOUT's serializer as a starvation condition (counted there). In Phase 2 Verilator the harness sink is always ready.

## Memory ownership

None. No memory ports.

## Q formats and rounding

None — RGB565 passes unmodified; no arithmetic exists in this block by law.

## Latency (fixed or variable)

Fixed: 2 vid cycles.

## Target throughput

1 pixel per vid cycle.

## Overflow and malformed-input behaviour

None possible: the input stream is well-formed by construction (VIDEO.MODE raster). A protocol violation in simulation (e.g. valid outside active window) trips an assertion; the block has no silent fallback.

## Counters and traces

No owned counter; `scanout_starvation_cycles` (owned by SCANOUT) covers output-stall events propagated from here. No traces.

## Scalar reference function

`zref::ScalerFeed` — identity with a 2-cycle delay: out[i] = in[i-2]; the differential test is the contract.

## Directed tests

`tests/video/video_scaler_directed.cpp` — 3 frames per mode: output stream == input stream delayed 2 cycles, sync mapping identical; reset flush; backpressure hold (ready deasserted ⇒ pixel held, no loss).

## Randomized differential tests

`tests/video/video_scaler_random.cpp` — PCG stream gaps and ready toggles; the delayed-identity law must hold bit-exactly over 100k pixels.

## Formal properties

None planned for this block beyond the identity (the random differential covers it exhaustively enough for a wire-through; revisit if the aiscal seam adds arithmetic).

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Registers only.

## Integration capture cases

`captures/golden/wave2/{z60,storm,duo}_10frame.zcap` — the displayed CRC is computed upstream of SCALER, so these capture the composite stream including SCALER's pass-through.

## Notes

The aiscal/MiSTer adapter seam: when the hardware lane integrates the core, this port is the connection point; any format change is a zhao_pkg.sv freeze request (architect sign-off).
