# Contract — AUDIO.FIFO (PCM audio FIFO)

> Ledger: `design/blocks.yml` · owner ZH-018 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Read the PCM ring (HPS-hosted) into a 2048-pair stereo FIFO, cross it into the audio clock domain, and emit exactly one s16 L/R pair per audio tick with the deterministic underrun law (repeat last pair + count). The documented mixer→audio-clock bridge; leaf (I2S/framework audio out). Law: `spec/audio_rules.md` (D4).

Exclusions: no mixing, tones or envelopes (SW.MIXER owns those; the FIFO carries finished pairs), no audio policy (fade/silence insertion — repeat is the only underrun behaviour), no framework/I2S adapter RTL (hardware lane).

## Clock and reset semantics

FIFO and refill client in `gpu_clk`; output side in `audio_clk` (48 kHz) — the documented `async_bridge: true` CDC (occupancy gray-coded across, whole pairs cross under a credit handshake; deterministic in Verilator via the harness tick scheduler, plan R1). Reset: FIFO empty, output silent (zero pairs, NOT repeats — there is no "last pair" yet), `audio_underruns` 0, refill idle until first write.

## Input and output packet layouts

Input: PCM ring reads (descriptor + ring law per spec/memory_rules.md §4.2) — bursts of 256 pairs, each pair 4 B little-endian (L then R). Output: `{valid, l[15:0], r[15:0]}` — one pair per audio tick; the hardware-lane I2S adapter consumes it, Verilator sinks it to the harness stream file.

## Backpressure rules

`ready_valid` upstream: the refill client stalls when the FIFO cannot accept (overflow is STRUCTURALLY impossible — no accept when full, formal property). Downstream (audio tick) has no valid/ready — the 48 kHz consumer isomorphism is free-running; underrun, not stall.

## Memory ownership

Read-only client of the HPS PCM ring (via MEM.HPS.BRIDGE in RTL; the harness C++ IS the HPS in Verilator, plan D10). No VRAM.

## Q formats and rounding

None in this block: finished s16 pairs pass unmodified; occupancy arithmetic is unsigned integer. Upstream accumulator formats (fx24) belong to SW.MIXER (qformats.md, plan Q5).

## Latency (fixed or variable)

Variable, bounded by design: target steady-state occupancy ≈ low watermark + one burst (768 pairs ⇒ ~16 ms at 48 kHz); the exact value is recorded per test, not asserted globally.

## Target throughput

1 pair per audio tick (48,000 pairs/s = exactly 800 pairs per displayed frame, spec/audio_rules.md §1); refill bursts of 256 pairs when occupancy ≤ 512.

## Overflow and malformed-input behaviour

Underrun (occupancy 0 at tick): repeat the last emitted pair bit-exactly, `audio_underruns`++ once per continuous event; the stream never gaps, never inserts silence. Overflow: impossible (backpressure). A torn ring read cannot happen (whole-pair credit law); a harness ring overrun beyond credits stalls refill — visible as occupancy 0, i.e. an underrun event, never corruption.

## Counters and traces

`audio_underruns` (shadow-latched at frame_tick). Trace: the complete output pair stream to the harness for bit-exact differential compare.

## Scalar reference function

`zref::AudioFifo` — depth/watermark/underrun oracle: given refill timelines and the tick, the exact output stream, underrun events, and 800-pair-per-frame accounting.

## Directed tests

`tests/audio/audio_fifo_directed.cpp` — steady fill; deliberate underrun ⇒ repeat + counter + continuous stream (byte compare across the event); full backpressure (ring stall ⇒ no accept when full, no loss).

## Randomized differential tests

`tests/audio/audio_fifo_random.cpp` — PCG-jittered refill timelines, 1k fast / 100k nightly, output stream bit-exact vs `zref::AudioFifo`.

## Formal properties

`tests/formal/audio_fifo_bounds.sby` — occupancy ∈ [0, 2048] invariantly; no accept when full; the underrun law (repeat + count, pairs never torn).

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). 8 KiB M10K (4 × M10K) + control.

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — audio stream bit-equal to the oracle and `audio_underruns == 0` for 600 frames.

## Notes

2048 pairs / refill 256 / watermark 512 are D4-frozen; changing any of them is a spec/audio_rules.md amendment first, RTL second.
