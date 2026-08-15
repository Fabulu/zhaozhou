# Contract — SW.ZEMU (Desktop emulator)

> Ledger: `design/blocks.yml` · owner ZH-078 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Desktop emulator product linking ZRef; the host for capture replay and
differential runs.

Wave-3 scope (plan W3.6/D10): the software console — loads a `.zpak`
cartridge (spec/cartridge.md), instantiates the generated game
(`form_game.hpp` from SW.COMPILER.FORM), and runs the deterministic loop
`pad snapshot → sim_tick → present_frame → sealed packet → zref::render →
RGB565 canvas + CRC → optional PCM window`. Writes replayable `.zcap`
captures consumable by tools/capture. Excluded: FPGA/RTL cosimulation
(that is the Verilator harness lane), MiSTer deployment (SW.RUNTIME.HPS
hardware lane), any timing-dependent truth.

## Input and output packet layouts

Inputs:

1. Cartridge `.zpak` — container law spec/cartridge.md (magic ZPAK;
  ABI_INFO first; per-section CRC-32C; CODE_MANIFEST hashes verified before
  executing generated entry points).
2. Pad stream: scenario stream from the cartridge/scenario entry, or live
  host input normalized to `PadFrame[4]` (input_rules §1-§4 — the ONE
  canonical form; keyboard fallback produces the frozen button table).
3. CLI: `--cart <file.zpak> --ticks <n> [--scenario <name>] [--realtime]
   [--capture <out.zcap>]`.

Outputs:

1. Sealed frame packets (capture_format §3 — 40+N B, dual CRC) via
   `zref::FrameBuilder`, ABI v2 content (wave-3 D7 command set implemented).
2. `.zcap` captures (capture_format §4; section set incl. FRAMEBUFFER_
   EXPECTED displayed-CRC per video_rules).
3. RGB565 canvas + per-frame CRC-32C; the sim-hash chain `H_t` (D5).
4. PCM window to the audio sink (wave-2 mixer lane) — best effort, never
   truth.

## Backpressure rules

Backpressure: `none` on truth. The tick loop is wall-clock-independent by
default (D10): fixed 60 Hz *semantics*, not wall pacing. `--realtime`
pacing only gates when the next tick starts — it never feeds timing into
truth or into the emitted packet contents (a paced run and a free run are
byte-identical; tested). The host audio/video sinks may drop frames under
load (presentation only, FORM §1 law).

## Memory ownership

ZEmu owns: the cartridge image (read-only after load), the generated
`FormState` (fixed capacity), one frame slot buffer (≤ FRAME_SLOT_BYTES),
the capture writer's file handles. The renderer's canvas is ZRef-owned.
No GC'd per-tick allocation; truth state is flat, fixed-size, hashable.

## Q formats and rounding

The loop contains no numerics of its own: truth math is generated code
(SW.COMPILER.FORM contract, qformats law), raster math is `zref::render`
(SW.ZREF contract). ZEmu's own arithmetic: frame/CRC bookkeeping (u32/u64)
and the D5 hash chain (CRC-32C over canonical state; LE words per
capture_format §2 byte-order rule).

## Latency (fixed or variable)

Latency: `variable` (host-scheduled); one tick's *semantics* is fixed and
ordered (deterministic-scheduling §2 phases — input latch, sim, terrain,
present, seal+render).

## Target throughput

Target throughput: real-time target — 60 ticks/s under `--realtime`
(W3.7 budget: 600-frame island sequence < ~60 s headless); correctness
never trades against speed (plan R4: optimize the integer path, never
loosen exactness).

## Overflow and malformed-input behaviour

Corrupt cartridge (bad magic/version/CRC/manifest hash): refuse before any
execution, clear diagnostic naming the section — never guess
(cartridge §5; capture_format §4.3-2). Frame packets that fail validation
report the generated `zhao_abi_error` code unchanged (capture_format §3.2
fail-safe order; no partial consumption). Pool overflow in generated code
surfaces as the deterministic `FORM-E-821` abort. Absent pads are the
frozen all-zero snapshot (input_rules §2.2).

## Directed tests

`tests/e2e/` (W3.6, labels fast): pack→load round-trip byte-stable;
`zemu --cart wound_lab.zpak --ticks 600` run-twice-identical hash chain +
frame CRCs; ZEmu capture replays through tools/capture identically;
`--realtime` vs free-run byte-identity; corrupt-cartridge refusal cases
(magic/CRC/manifest).

## Randomized differential tests

Random-seed scenario soak (nightly): N random pad streams over the Wound
Lab cartridge; every run's hash chain compared against a desktop re-run —
cross-run identity is the invariant (no reference oracle exists above the
generated code itself; the ARM cross target is the differential partner,
W3.8).

## Integration capture cases

W3.7 golden set: `captures/golden/wave3/` — 600-tick `.zcap` +
`.zpak`, per-frame CRCs, sim-hash chain; the wave-2 replay corpus still
replays byte-identically (ABI v2 content preserved by the version decision).

## Notes

Phase-1-active software block (empty shell → wave-3 console). Maturity
target REFERENCE_COMPLETE at wave-3 gate (plan D12) with commit-pinned
evidence; hardware execution stays in the SW.RUNTIME.HPS lane.
