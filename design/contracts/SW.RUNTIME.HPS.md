# Contract — SW.RUNTIME.HPS (HPS game runtime)

> Ledger: `design/blocks.yml` · owner ZH-014 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

The game-facing runtime on the ARM side: gameplay loop, frame-packet
issuance, streaming requests, mixer drive, pad consumption.

Wave-3 scope (plan W3.8/D9/D10): the deterministic core as the same code
the desktop runs — generated `form_game.hpp` (sim_tick/present_frame/
sim_hash) + ZRef render, cross-compiled clean with
`aarch64-none-elf-g++ -std=c++17 -fno-exceptions -fno-rtti -ffreestanding
-c` + archive (CTest `arm-cross` label, SKIP-if-absent machine probe; the
devkitA64 15.2.0 toolchain is a verified local machine fact). The real HPS
*execution* (frame ring, scanout, pads on hardware) stays the hardware
lane — nothing unexecuted is claimed (plan D12).

## Input and output packet layouts

Inputs (deterministic core, identical to SW.ZEMU): `.zpak` cartridge
(cartridge law), `PadFrame[4]` snapshots (input_rules), tick counter.

Outputs: sealed frame packets (capture_format §3) built by
`zref::FrameBuilder` from `present_frame`; the D5 sim-hash chain; on the
hardware lane later: frame-slot submission (ARM_WRITING→READY seal
protocol, capture_format §3), rumble via DebugRumble 0xF004, stream
requests, mixer drive via EmitAudioEvent 0x0400 → MixerTone.

## Backpressure rules

Backpressure: `none` on truth. Truth is never degraded because the
renderer is busy (FORM §1); a missed deadline repeats the previous
completed frame (video_rules repeat law) — a presentation decision, never
a truth one. The frame-slot ring applies hardware-side backpressure to
*presentation* only.

## Memory ownership

The runtime owns: cartridge image (read-only, CRC/manifest-verified),
`FormState` (fixed capacity — pool capacity literals size it at compile
time; no per-tick allocation, FORM §21-5), frame slot buffers (≤
FRAME_SLOT_BYTES each). Freestanding core (D9): the deterministic core is
I/O-free by construction — no libc beyond freestanding headers, no heap,
no host clocks (grep-audited with the compiler's emitter audit).

## Q formats and rounding

Generated-code law (SW.COMPILER.FORM contract): qformats single-rounding
discipline, fx16/fx24/angle16/unit8; no float/double token anywhere in the
deterministic core (charter §29-7; the freestanding cross build makes any
libm dependency a link error, which is the point of compiling it).

## Latency (fixed or variable)

Latency: `variable` per tick in wall time; the tick's internal order is
fixed (deterministic-scheduling §2). 60 Hz semantics by construction.

## Target throughput

Target throughput: 1 frame per frame deadline. Wave-3 evidence bar is the
desktop/ARM cross **hash equality**, not on-target 60 Hz (that lands with
the hardware lane); the W3.7 runtime budget test (< ~60 s for 600 frames
desktop, software renderer) is the engineering headroom proxy.

## Overflow and malformed-input behaviour

Corrupt cartridge: refuse before execution (same law as SW.ZEMU — the
loader is shared code). Generated-code pool overflow: deterministic
FORM-E-821 abort — the runtime reports and halts the scenario, never
continues with silently dropped state. Pad gaps are the frozen zero
snapshot (input_rules §2.2/2.3 — sequence gaps are countable but never
fabricated).

## Directed tests

`tests/` (W3.8, `arm-cross` label): freestanding compile + archive of the
deterministic core (probe-gated, SKIP-with-notice when devkitA64 absent);
CI ARM64 native run of the sim-hash golden (optional job, plan D9). The
600-tick hash chain equality vs the committed desktop golden is the
directed cross-target case (D5: cross-target equality by construction).

## Randomized differential tests

Nightly: seeded random pad streams replayed desktop vs ARM64-native —
hash-chain equality per tick (first-divergence reporting through TRACE
records, capture_format §4.2 0x000A, when a lane ever disagrees).

## Integration capture cases

W3.7/W3.8: the Wound Lab cartridge's committed goldens are the cross
target — identical `.zpak`, identical hashes; the wave-2 capture corpus
still replays (ABI v2 content decision). Hardware-lane captures (real HPS
run) are future evidence, tracked blocked_on: hardware.

## Notes

Phase-1-active software block. Game content itself is excluded from the
ledger. Maturity target REFERENCE_COMPLETE at wave-3 gate (deterministic
core + cross-compile/cross-run evidence, commit-pinned); HARDWARE_PROVEN
stays blocked_on: hardware.
