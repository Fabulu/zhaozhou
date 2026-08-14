# Zhaozhou Input Rules — Phase 2 (wave 2)

**Status:** ratified 2026-08-14 (plan W2.1, decision D5). Single law for
INPUT.SNAPSHOT / INPUT.RUMBLE. The PadFrame wire layout is ABI
(`spec/commands.zidl`, ABI v2); this file defines its SEMANTICS and the
timing laws. Where this file and any other text disagree, this file wins for
input.

---

## 1. PadFrame (D5) — the canonical snapshot

`struct PadFrame` in commands.zidl, 20 bytes, 4-aligned, array stride 20:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `pad_index` (u8) | 0..3. Any other value in a producer is a tool bug; consumers MUST reject a section whose entries carry `pad_index > 3` (treat the whole CONTROLLER_SNAPSHOT section as malformed — skip, do not guess) |
| 1 | 1 | `flags` (u8) | bit0 `pad_present`; bits 1-7 reserved, MUST be 0 |
| 2 | 2 | `sequence` (u16) | per-pad monotonic snapshot sequence (§2.3) |
| 4 | 4 | `buttons` (u32) | 32 digital bits, active high, bit i = button i (bit assignment table §4) |
| 8 | 2 | `lx` (i16) | left stick X, center 0, negative = left |
| 10 | 2 | `ly` (i16) | left stick Y, center 0, positive = down (screen convention) |
| 12 | 2 | `rx` (i16) | right stick X, center 0, negative = left |
| 14 | 2 | `ry` (i16) | right stick Y, center 0, positive = down |
| 16 | 4 | `rsv` (u32) | reserved, MUST be 0 |

Stick convention: raw signed axis, no deadzone, no calibration, no
remapping in hardware — the exact raw sample travels to software unchanged
(determinism law: hardware applies zero policy). Full scale is the pad's
native ±32767; captures record raw values.

The .zcap CONTROLLER_SNAPSHOT section (0x0004) body is
`u32 count; PadFrame pads[count]` — `count ≤ 4`; entries sorted by
`pad_index`; one entry per present-or-watched pad (see §2.2 for absent pads).

## 2. Snapshot law (INPUT.SNAPSHOT)

### 2.1 Atomic latch at frame_tick

All four pad slots are latched ATOMICALLY on the broadcast `frame_tick`
(spec/video_rules.md §5): the snapshot reflects the pad state at a single
instant. A stick change that arrives mid-frame is visible only in the next
snapshot — never partially (formal property `input_snapshot_atomic`:
no output bit of the latched frame changes between two consecutive
frame_ticks without a tick in between).

### 2.2 Absent pads

A pad that was never present or was removed: its entry carries
`flags.pad_present = 0`, `buttons = 0`, all sticks `0`. Its `sequence`
FREEZES at the last present value (it does not advance while absent).

### 2.3 Sequence and gaps

- `sequence` increments by exactly 1 for EVERY frame_tick while the pad is
  present, starting at 0 after reset, wrapping mod 2^16.
- A gap (consumer sees `sequence != prev + 1 (mod 2^16)` on a present pad)
  means a snapshot was lost — on the FPGA this is impossible by construction
  (the latch is synchronous); the counter `input_sequence_gaps` counts each
  observed gap on the capture/tool side and in the INPUT.SNAC merge path.
- Formal property: sequence-exactly-once — one increment per frame_tick,
  zero increments without a tick, never two.

## 3. Rumble law (INPUT.RUMBLE)

- The runtime rumble request arrives via the ABI command `DebugRumble
  0xF004` (`{pad_index, enable, strength}`) inside a sealed frame; header
  flags bit0 is required as for every 0xF00n opcode.
- **Frame-gated:** requests are latched at the NEXT `frame_tick` after the
  command executes. Mid-frame changes never reach the pad path early.
- One update per frame per pad is applied. A second DebugRumble for the SAME
  pad in one frame REPLACES the first at the latch — and
  `rumble_frames_dropped` counts the dropped one. (Last-writer-wins is
  deterministic; the counter makes the loss visible.)
- `pad_index > 3` ⇒ the request is dropped entirely and
  `rumble_frames_dropped++` (never a wrap onto another pad).
- **PWM:** 1 kHz carrier, duty = `strength / 256` (0 = off; 255 ≈ 99.6%).
  `enable = 0` forces duty 0 regardless of `strength`. The PWM runs
  continuously in the pad clock domain; only the duty target is
  frame-latched (no phase reset — a duty change never glitches the carrier).
- With no new command for a pad in a frame, the previous target HOLDS (no
  auto-timeout in Phase 2; software owns stop semantics).

## 4. Button bit assignment (frozen, 32 bits)

| Bit | Button | Bit | Button |
|---|---|---|---|
| 0 | up | 8 | L2 (analog-as-digital) |
| 1 | down | 9 | R2 (analog-as-digital) |
| 2 | left | 10 | L1 |
| 3 | right | 11 | R1 |
| 4 | A / cross | 12 | L3 (stick click) |
| 5 | B / circle | 13 | R3 (stick click) |
| 6 | X / square | 14 | select / back |
| 7 | Y / triangle | 15 | start |

Bits 16-31 are reserved 0 in Phase 2 (analog trigger digits and pad-type
extensions land with the hardware lane). Keyboard fallback (ZEmu) and the
optional SNAC adapter (INPUT.SNAC) must produce THIS table and THIS PadFrame
— one canonical form, adapters normalize.

## 5. Interface notes (frozen in zhao_pkg.sv)

- INPUT.SNAPSHOT re-exports the generated `zhao_pad_frame_t` (from
  zhao_abi_pkg) — the packed SV mirror of `struct PadFrame`, reverse field
  order per the generator's byte-identity law.
- The pad→HPS handoff crosses domains through the documented async bridge
  (`async_bridge: true` in the ledger): 2-flop synchronizer + toggle
  handshake; the 4-entry snapshot array plus `frame_id` cross as ONE
  consistent unit (the frame_tick-aligned latch makes the array stable for a
  full frame, so a gray-coded pointer swap suffices).
- INPUT.RUMBLE is a leaf (pad PHY out); its only visible fabric interface is
  the latched `{enable, strength}` pair per pad and the
  `rumble_frames_dropped` counter.

## 6. Test obligations (directed at W2.3)

- Directed: mid-line stick/button change ⇒ atomic latch (no torn snapshot);
  sequence monotonic across 10k frames incl. wrap; absent pad ⇒ frozen
  sequence, zeroed fields; DebugRumble ⇒ PWM duty latched at the next tick;
  two rumbles in one frame ⇒ last wins + counter.
- Random differential: PCG pad streams (4 pads, presence toggles,
  out-of-order pad arrival) vs `zref::PadSnapshot`; rumble command
  timelines vs `zref::RumbleBridge` (duty trace bit-exact).
- Formal: `input_snapshot_atomic` (atomicity + sequence-exactly-once).
