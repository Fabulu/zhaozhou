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

## 7. SNAC adapter law (INPUT.SNAC)

**Status:** added 2026-09-20 under owner ruling R7
(`reports/OWNER-RULINGS-20260919-EVENING.md`): *"INPUT.SNAC, GEOM.WARP,
POST.ECHO — Build all three (owner, explicit). They stay mandatory."*

The 2026-08-31 §6.6 ruling's constraint survives R7 and is the whole shape of
this section: *"It must emit the same canonical PadFrame and may not create a
second input semantics."* Everything in §1–§4 stays exactly as written; §7
adds a second ROUTE to the state §2 latches, and nothing else. There is no
SNAC PadFrame, no SNAC button table and no SNAC sequence law.

### 7.1 The bus

The console is the HOST. It drives `/ATT` (one per port, active low), `CLK`
and `CMD`; the pad drives `DAT` and `/ACK`. Bytes are 8 bits **LSB first**.
`CLK` idles high; each side presents its bit on the FALLING edge and samples
the other side's on the RISING edge. `DAT` and `/ACK` are asynchronous to the
fabric clock and cross through the documented 2-flop synchronizer
(`async_bridge: true` in the ledger, as for the pad→HPS handoff in §5).

The bus rate, the `/ACK` timeout and the inter-poll gap are NAMED, EDITABLE
PARAMETERS of the block, not constants of this law. Their defaults give the
PS1's ~250 kHz at a 100 MHz fabric clock. **No decoded value depends on any of
them** — that is what lets a bench run the same block at a bench rate.

### 7.2 One poll

| byte | host sends | pad returns |
|---|---|---|
| 0 | `0x01` address | (idle) |
| 1 | `0x42` read | mode byte |
| 2 | `0x00` | `0x5A`, the ready byte |
| 3 | `0x00` | buttons low, **ACTIVE LOW** |
| 4 | `0x00` | buttons high, **ACTIVE LOW** |
| 5–8 | `0x00` | right X, right Y, left X, left Y (mode `0x73` only) |

Mode `0x41` is a digital pad (no sticks); `0x73` is an analog pad. `0xFF` is
the idle bus and means NO PAD — it is **not** an error and is not counted as a
bad header, because an unpopulated port is the normal case and a counter that
fires on it carries no information. Any OTHER mode byte, and any byte 2 that
is not `0x5A`, makes the poll absent AND is counted: a pad that answered with
something unimplemented is a real event somebody needs to see.

A `/ACK` that never arrives abandons the poll and the slot reads absent. That
is how an empty port is detected; it is counted separately from a bad header.

### 7.3 Normalisation — the two transforms, and why neither is policy

§1's determinism law says hardware applies ZERO policy. Both transforms below
are pure re-encodings of the same information, and that is the test each has
to pass.

**Buttons.** The PS1's two bytes are active low. They map onto §4's frozen
32-bit table (`0x41`'s two bytes and `0x73`'s first two are the same two):

| PS1 byte 3 bit | §4 bit | PS1 byte 4 bit | §4 bit |
|---|---|---|---|
| 0 select | 14 | 0 L2 | 8 |
| 1 L3 | 12 | 1 R2 | 9 |
| 2 R3 | 13 | 2 L1 | 10 |
| 3 start | 15 | 3 R1 | 11 |
| 4 up | 0 | 4 triangle | 7 |
| 5 right | 3 | 5 circle | 5 |
| 6 down | 1 | 6 cross | 4 |
| 7 left | 2 | 7 square | 6 |

§4 bits 16–31 stay reserved zero. A PS1 pad carries nothing that belongs
there, so they are zero because §4 says so and not because the data ran out.

**Axes.** `canonical_i16 = {ps1_u8, 8'h00} ^ 16'h8000`, read as signed.
`0x80 → 0`, `0x00 → −32768`, `0xFF → +32512`. It is strictly monotonic and
**invertible**: `ps1_u8 == (canonical_i16 >>> 8) + 128` for all 256 inputs. No
information is added, removed or rounded, which is what makes it a re-encoding
rather than calibration. The asymmetry of the rails is the PS1's own (128
codes below centre, 127 above) and is DECLARED here rather than re-centred;
re-centring would be calibration, which §1 forbids in hardware.

A digital pad's four axes read 0 — the same value §2.2 gives an absent axis,
so no consumer needs a third case.

### 7.4 The merge

Per slot: a slot the adapter has a live pad on is driven by the adapter; every
other slot carries the incoming route through UNCHANGED. With no SNAC hardware
attached every slot reads absent, so the adapter is the IDENTITY on the whole
pad bus and the console behaves exactly as it does without it. That property
is asserted, not argued (`tests/input/input_snac_directed.cpp` case 1).

The merged bus is INPUT.SNAPSHOT's input. §2's atomic latch, §2.2's absent-pad
law and §2.3's sequence law all apply to it unchanged, because it is the same
bus.

### 7.5 The merge-path gap (§2.3's counter, given its meaning)

§2.3 has said since 2026-08-14 that `input_sequence_gaps` counts a gap "in the
INPUT.SNAC merge path". This is that gap, stated executably:

> Each slot carries a poll sequence that advances once per COMPLETED poll. At
> each `frame_tick`, a slot that was present at the previous tick, is present
> now, and whose poll sequence did not advance, is a frame that REUSED the
> previous frame's sample — the bus ran slower than the display did and a
> snapshot's worth of input was never taken.

The counter's two operands are loaded by DIFFERENT events (the serial engine's
completion and the frame tick), so it measures TIMING and not merely values —
the distinction `CLAUDE.md` records after a detector whose two sides moved
together. It fires on legal stimulus and therefore owes no mutant
(`input_snac_directed` case 6).

### 7.6 Test obligations

- Directed: transparency when idle (the identity on all four slots, with the
  `/ACK` timeout seen to fire); a digital pad reaching the canonical bus while
  the other slots pass through in the same cycle; **every one of §4's sixteen
  buttons walked individually**; the axis law over all 256 codes and swept
  through the RTL; an unimplemented mode counted and falling back rather than
  going dark; the merge-path gap counter fired.
- The decoded values are compared against `zref::SnacAdapter`. The button
  table and the axis law are NOT transcribed into the test — a transcription
  is a copy, and a copy of a table is how two implementations of one law come
  to disagree.
