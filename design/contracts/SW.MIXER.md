# Contract — SW.MIXER (Fixed-point audio mixer)

> Ledger: `design/blocks.yml` · owner ZH-075 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

48 kHz fixed-point mixer on the HPS (same code in ZRef/ZEmu and the hardware path — Q5 ruling): Phase-2 subset is the deterministic TEST TONE (`zref::MixerTone`) and the PCM ring writer that feeds AUDIO.FIFO. Full 32-voice/3-bus mixing lands with the software phases; the accumulator formats are already frozen (fx24, qformats.md).

Exclusions (Phase 2): no voice mixing, no reverb/delay, no ADPCM decode, no positional pan — the tone+rng subset only. `present` code never mutates deterministic game truth (charter §29-8): the mixer consumes timestamps, not game state.

## Clock and reset semantics

Software block (HPS; `clock_domain: hps`): runs to the 48 kHz tick driven by the runtime scheduler; in Verilator the harness IS the runtime. "Reset" = process start: phase accumulator 0, ring empty. No hardware reset semantics.

## Input and output packet layouts

Input (Phase 2): tone selection from the demo/runtime configuration (frozen tone table: TONE_A4/TONE_A5/TONE_C4, spec/audio_rules.md §4). Output: s16 stereo pairs, L then R little-endian, written into the PCM ring (descriptor law spec/memory_rules.md §4.2) — the exact bytes AUDIO.FIFO consumes and captures record.

## Backpressure rules

None on the mixer itself (`none`): the ring applies backpressure via its free-space law (the mixer never overwrites unread pairs — the write pointer never passes the FPGA read pointer).

## Memory ownership

Owns the PCM ring data region and `host_write_ptr`; never touches `fpga_read_ptr` (FPGA-owned). No VRAM.

## Q formats and rounding

Phase accumulator u32, increment `floor(freq × 2^32 / 48000)` (frozen table, spec/audio_rules.md §4). Sample = `fx_sin(angle16) >> 1` — the qformats.md §7.1 quarter-wave table (257 × Q1.16), arithmetic shift toward −∞, saturating; fx24 accumulators govern the future mix bus (single-rounding law, qformats.md §3).

## Latency (fixed or variable)

Fixed: 1 output pair per 48 kHz tick (`fixed:1`).

## Target throughput

1 output sample pair per 48 kHz tick; 800 pairs per displayed frame (the accounting law, spec/audio_rules.md §1).

## Overflow and malformed-input behaviour

The phase accumulator wraps mod 2^32 BY DEFINITION (exact turns arithmetic, not overflow). Amplitude arithmetic saturates (s16 bounds); saturation is deterministic and golden-locked. The tone set is closed — an unknown tone id is a configuration error caught at build time, not runtime.

## Counters and traces

No FPGA counter (software); the output stream IS the trace (bit-exact compare vs goldens and RTL-side FIFO consumption).

## Scalar reference function

`zref::MixerTone` — the exact pair stream for each tone (Phase-2 subset); the future full mixer reference extends the same fixed-point library.

## Directed tests

`tests/audio/mixer_tone_directed.cpp` (W2.4 lane) — full-frame golden streams per tone; saturation boundary; ring free-space law.

## Randomized differential tests

`tests/audio/mixer_tone_random.cpp` — tone switching at PCG frame boundaries vs the oracle, bit-exact.

## Formal properties

Not applicable to the software lane (the differential law is exhaustive byte compare; no SBY).

## Synthesis / resource ceiling

Not synthesized (software block; ledger `kind: software`; maturity ceiling V13 without runs_on_target_hardware).

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — the demo audio stream bit-equal to `zref::MixerTone` across all 600 frames (the W2.7 normative acceptance).

## Notes

Adopts the ZRef fixed-point library per plan Q5 (fx24 accumulators, single rounding). Wave-2 scope note: SW.MIXER may advance to REFERENCE_COMPLETE on the tone+ring subset once W2.4 commits the golden evidence (plan W2.4).
