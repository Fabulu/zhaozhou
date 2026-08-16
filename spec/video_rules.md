# Zhaozhou Video Rules — Phase 2 (wave 2)

**Status:** ratified 2026-08-14 (plan W2.1, decisions D1/D6/D7). This file is
the single law for the video subsystem (VIDEO.MODE / VIDEO.SCANOUT /
VIDEO.SCALER / VIDEO.FRAMECTL). The frozen RTL interface types live in
`fpga/rtl/common/zhao_pkg.sv`; the mode enum and the command surface live in
`spec/commands.zidl` (ABI v2). Where this file and any other text disagree,
this file wins for video.

All numbers are integers. "Cycle" means a `gpu_clk` cycle unless the text
says `vid_clk`. `vid_clk = gpu_clk / 2` (one vid cycle = 2 gpu cycles) in
simulation and on the frozen sim profile; PLL ratios are re-derived from
board truth post-ZH-016 WITHOUT changing any contract surface here.

---

## 1. Modes (D6)

`enum video_mode : u8` (commands.zidl, ABI v2) — one mode selector
machine-wide:

| Value | Name | Active raster | Slot **allocation** | Bytes a frame **occupies** |
|---|---|---|---|---|
| 0 | `VIDEO_Z60` | 384×240 | 0x03C000 (245,760 B) | 184,320 B = 0x02D000 (384×240×2) |
| 1 | `VIDEO_STORM` | 320×240 | 0x03C000 (245,760 B) | 153,600 B = 0x025800 (320×240×2) |
| 2 | `VIDEO_DUO` | 512×240 (2 × 256×192) | 0x03C000 (245,760 B) | 196,608 B = 0x030000 (2 × 256×192×2) |

**Allocation is not occupancy.** Every slot is always allocated 0x3C000 bytes
— the size of the LARGEST canvas — so a mode switch never moves or resizes a
slot (§3). What a frame actually *stores* is the third column, and it differs
per mode: Z60 and Storm fill a full 384×240 / 320×240 raster, while Duo stores
only its two 256×192 view canvases (§3.1) and stores **no** border rows — the
48 black border lines are generated at scanout, so Duo occupancy is 0x30000,
not 0x3C000. Earlier revisions of this table listed 0x3C000 in the canvas
column for Duo, which read as a contradiction of §3.1; it never was one — it
was the allocation.

The same value is carried by `SetPresentationContract.mode`, by
`DebugFrameBlit.mode`, and by the .zcap FRAMEBUFFER_EXPECTED `mode` byte.

### 1.1 Mode latch law

- The mode register is written by CMD.SCHEDULER when a
  `SetPresentationContract` record executes.
- VIDEO.MODE latches the mode **only at frame start** (the first vid cycle
  after vblank end; equivalently, when the raster counter leaves the
  vertical blanking interval). Effective the NEXT frame, never mid-frame.
- A frame displayed under mode M was fetched entirely under mode M: changing
  the mode register mid-frame has no effect until the next frame start.
- Reset value: `VIDEO_Z60` (mode 0).

## 2. Timing (D1) — free-running internal raster

Zhaozhou owns its raster. The MiSTer sys/video_mixer/ascal path is an adapter
integrated at the hardware lane; it consumes the native pixel stream (§6) and
never changes these tables.

**Vertical (identical for all modes):**

| Parameter | Value (vid lines) |
|---|---|
| V active | 240 |
| V front porch | 4 |
| V sync | 4 (negative polarity) |
| V back porch | 14 |
| **V total** | **262** |

**Horizontal per mode (vid cycles per line):**

| Mode | H active | H front | H sync | H back | **H total** |
|---|---|---|---|---|---|
| Z60 | 384 | 8 | 48 (neg) | 40 | **480** |
| Storm | 320 | 8 | 48 (neg) | 40 | **416** |
| Duo | 512 | 8 | 48 (neg) | 40 | **608** |

Both syncs are NEGATIVE polarity in all modes. Pixel (0,0) is the first
active pixel of the first active line.

**Derived frame periods** (vid total × 262 × 2 gpu cycles/vid cycle):

| Mode | Vid cycles/frame | gpu cycles/frame |
|---|---|---|
| Z60 | 125,760 | **251,520** |
| Storm | 108,992 | **217,984** |
| Duo | 159,296 | **318,592** |

These are the DEFAULT deadline values (`BeginFrame.deadline_cycles = 0` ⇒
scheduler uses the current mode's frame period). The timing constants exist
exactly once, in `zhao_pkg.sv` (`zhao_timing_t` table `ZHAO_TIMING`).

The raster is FREE-RUNNING: it never stalls, never stretches, never waits
for fetch. If the framebuffer is not ready the previous complete frame is
repeated (§4), and the raster keeps ticking.

## 3. Framebuffer layout

- Two slots (`dst_slot` 0/1), each `canvas_bytes(mode)` contiguous bytes in
  VRAM, RGB565, little-endian halfwords (byte 0 = G[2:0]|B[4:0] low bits per
  RGB565 packing below), row-major, top-left origin, no padding between rows.
- RGB565 bit layout in the 16-bit halfword: `[15:11] R[4:0], [10:5] G[5:0],
  [4:0] B[4:0]`. Stored little-endian (low byte first).
- Slot base addresses (guard region map, spec/memory_rules.md §5):
  `FB_SLOT0 = 0x0000_0000`, `FB_SLOT1 = 0x0003_C000` (both slots are always
  sized for the LARGEST canvas, 0x3C000 = 245,760 B, so a mode switch never
  moves a slot).
- Phase 2 writes to a slot come exclusively from `DebugFrameBlit` DMA; the
  resolved-tile path (RASTER.RESOLVE) lands in later phases and must respect
  the same layout.

### 3.1 Duo canvas map (D-mode only)

Duo displays TWO independent 256×192 view canvases on the 512×240 raster:

| View | Source region | Displayed at |
|---|---|---|
| View 0 (P1) | slot bytes [0, 0x18000) — 256×192×2 | x ∈ [0,255], y ∈ [24,215] |
| View 1 (P2) | slot bytes [0x18000, 0x30000) — 256×192×2 | x ∈ [256,511], y ∈ [24,215] |

Both view canvases are vertically CENTERED in the 240 active lines: rows
0..23 and 216..239 display the border colour `16'h0000` (black). No scaling,
no mirroring — a 1:1 copy (VIDEO.SCALER is a pass-through formatter, §6).
The 48 border rows are part of the displayed stream and therefore part of
the displayed-frame CRC.

Each view is one CONTIGUOUS block: view 1 begins at slot byte `0x18000`, not
at column 256 of a 512-wide image. There is no interleaving — a displayed row
`y ∈ [24,215]` is assembled at scanout by concatenating view-0 row `y − 24`
(512 B) with view-1 row `y − 24` (512 B). This is what `zhao_scanout_fetch`
reads and what `zref::render` writes; a Duo frame therefore occupies
`0x30000` bytes of its `0x3C000` slot (§1), and the last `0xC000` bytes are
untouched and outside both CRCs (ratified 2026-08-15, review MAJOR-3: the
scanout fetcher wants one linear burst stream per view, which the interleaved
layout would break every 256 pixels mid-line).

## 4. Scanout law (D7) — repeat, never tear

VIDEO.SCANOUT decomposes into:

1. `zhao_scanout_fetch` — a gpu-domain VRAM read client (through
   MEM.GUARD/arbiter, strict priority) that fills line buffers;
2. `zhao_scanout_linebuf` — 2 × 512 × RGB565 ping-pong line buffers
   (prefetch line N+1 while line N displays; buffers sized for the widest
   mode, 512 px);
3. `zhao_scanout_serializer` — the video-domain pixel emitter (one RGB565
   pixel per vid cycle during H active);
4. a double-buffer swap controlled by VIDEO.FRAMECTL.

**The 60 Hz law:** a late frame is NEVER partially displayed. Exactly one of
the following happens at each vblank:

- A slot was signed off READY before the deadline and fully fetched into the
  display buffer ⇒ it becomes the displayed frame (`frame_ticks` normal).
- No READY slot ⇒ the PREVIOUS COMPLETE frame repeats: the same base slot is
  displayed again, `frame_repeated` pulses, `deadline_faults++` (counter
  catalog index per spec/counters.md), and nothing else changes.

**Swap timing:** the double-buffer swap happens ONLY in vblank, driven by
the FRAMECTL handshake. Fetch for the next frame may start as soon as the
slot is READY; the swap decision is made at the vblank start line
(V active + front porch = line 244, i.e. the first cycle of vertical sync).

**Displayed-CRC law:** the frame CRC-32C (DEBUG.CRC, .zcap
FRAMEBUFFER_EXPECTED) is computed ON THE DISPLAYED STREAM, AFTER the repeat
decision, over exactly `2 × active_width × 240` bytes per frame in raster
order (border rows included in Duo). "On the displayed stream" is literal in
Duo: it is 240 rows of 512 pixels **as scanned out** — `16'h0000` for rows
0..23 and 216..239, and for each row 24..215 the concatenation of view-0's row
and view-1's row assembled at scanout (§3.1), NOT a linear walk of the stored
bytes, which hold the two views as separate contiguous blocks. A repeated
frame must CRC IDENTICAL to
its first display — this mechanically proves the 60 Hz law; the directed
test `forced_missed_deadline` asserts it.

**Line underrun** (fetch starved while a line is displaying): the serializer
never stalls; it re-emits the last valid pixel data of the current line
buffer (the ping-pong law makes starvation visible, not torn). Each such
event increments `scanout_starvation_cycles` for every starved vid cycle.
Line-buffer fill must complete before the line's display window (formal
property `video_scanout_linebuf`: serializer never overtakes fill).

## 5. VIDEO.FRAMECTL — the frame handshake

- Monitors slot READY/ARM registers (set by CMD.SCHEDULER / DebugFrameBlit
  completion), the raster position (from VIDEO.MODE), and the deadline
  counter.
- Emits exactly ONE `frame_tick` per displayed frame at vblank start (after
  the swap/repeat decision). `frame_tick` is the machine-wide frame
  boundary: INPUT.SNAPSHOT latches pads on it, DEBUG.COUNTERS latches
  shadow registers on it (spec/counters.md §3), CMD.SCHEDULER closes the
  slot FSM on it.
- Emits `frame_complete {slot, repeated}` to CMD.SCHEDULER with the tick.
  Exactly one completion fence per FPGA_RUNNING→DONE transition (formal
  property `video_framectl_one_fence`).
- Deadline monitor: if the owning slot misses `deadline_cycles` (default =
  mode frame period), the repeat path fires and `deadline_faults` counts
  once per missed frame (not per line).

## 6. VIDEO.SCALER — pass-through formatter

Phase 2: a pass-through. It reformats the native stream into the pixel
stream type (`zhao_px_stream_t`, zhao_pkg.sv): `valid`, `rgb565`, `x`, `y`
(0-based within active video), `hsync`, `vsync`, `vblank`, `hblank`. The
MiSTer sys/adapter (hardware lane) consumes it; the aiscal seam is recorded
in the SCALER contract but has no Phase-2 behaviour. No colour conversion,
no scaling, latency 2 vid cycles.

## 7. Interface summary (frozen in zhao_pkg.sv)

- `zhao_video_mode_e` — 3 values, matches `video_mode`.
- `zhao_timing_t` — per-mode `{h_active, h_front, h_sync, h_back, h_total,
  v_active, v_front, v_sync, v_back, v_total, frame_gpu_cycles}`;
  `ZHAO_TIMING[3]` localparam table.
- `zhao_px_stream_t` — the serializer/scaler output (§6).
- `frame_tick` convention: one-cycle pulse in the video domain, gray-coded
  into the gpu domain by FRAMECTL (2-flop synchronizer + toggle handshake;
  deterministic in Verilator because the harness tick scheduler fixes the
  domain phase — plan R1).

## 8. Test obligations (directed at W2.2)

- 3-frame cycle-for-cycle walk per mode vs `zref::VideoMode` (per-cycle
  timing trace: x, y, syncs, blanks, frame_start).
- Forced missed deadline ⇒ repeat + `deadline_faults++` + repeat-CRC
  identical to first display.
- Mode switch mid-frame ⇒ old mode completes the frame, new mode effective
  at next frame start; timing constants change atomically at frame start.
- Line-underrun injection ⇒ starvation counter, never a torn line.
- Random: PCG slot-READY timelines, 1k fast / 100k nightly, vs
  `zref::Scanout` (fetch order + swap/repeat oracle).
- Formal: `video_mode_timing` (bounds, one frame_start per period,
  reset-idle), `video_scanout_linebuf` (never overtake, vblank-only swap,
  never torn), `video_framectl_one_fence`.

### 8.1 Formal scope, stated plainly (2026-08-16)

- `video_mode_timing`: UNBOUNDED prove (abc pdr) over the whole raster
  ring, plus reachability covers.
- `video_framectl_one_fence`: BMC to depth 60 (~4 abstract frame periods)
  plus covers. One unbounded pdr pass was observed to complete green but
  costs ~45+ min with run-to-run variance — recorded as a note, not the
  lane contract.
- `video_scanout_linebuf`: BMC to depth 8 — up to FOUR completed fill
  sessions, which contains every counterexample class the property has
  produced (the abort-blip torn read at step 6, the credit round-trip and
  both-buffers-fresh at step 8). It does NOT cover interleavings needing a
  fifth completion; the harness carries a self-asserting scope guard
  (`a_scope_four_sessions`) that FIRES if the depth is raised past what
  was proven, and the unbounded law stays covered cycle-exactly by the
  scanout differential lanes. Cost measurement behind the bound: each BMC
  step past 7 ≈ 10 solver-minutes (boolector and yices alike).
- **CDC hazard, fenced not fixed** (found by this property once its
  stimulus was made genuinely free): aborting a FULL line buffer un-does
  its completion toggle, and an un-toggle IS a toggle — a pulse the
  vid-side 2FF chain can sample as stale freshness for ~2 vid cycles. The
  enforcer is SYSTEM timing (aborts only at the vswap_dec re-arm / mode
  flush, freshness decisions line-edge-spaced); any future integration
  that aborts outside vblank must re-establish that spacing or redesign
  the crossing. Details: zhao_scanout_linebuf.sv header.
