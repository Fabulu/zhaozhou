# Contract — POST.ECHO (Frame echo)

> Ledger: `design/blocks.yml` · owner ZH-071 · phase 11 · maturity RTL_VERIFIED

> **REVIVED 2026-09-19 by owner ruling R7** (`reports/OWNER-RULINGS-20260919-EVENING.md`):
> *"INPUT.SNAC, GEOM.WARP, POST.ECHO — Build all three (owner, explicit). They
> stay mandatory; the 2026-09-18 revocation stands."* The 2026-08-31 §4 deferral
> this file used to carry, and its "deliberately unwritten" sections, are
> withdrawn by that ruling. Every section below is written for the block that
> now exists: `fpga/rtl/compositor/zhao_post_echo.sv`.

## Purpose and exclusions

Echo the composited frame — **post-ink, pre-HUD** — into a capture buffer in
local VRAM, once per post pass, so the world as the player saw it, without the
interface, is available after the frame is gone (a pause/photo backdrop, a
capture for the host, a debug readback).

Exclusions: no colour arithmetic (the pixel is POST.COMPOSITE's stage-9 value,
bit for bit); no HUD; no scaling; no second read of the framebuffer (the echo
is fed from the compositor's tap, never from memory); no influence on the
displayed frame, ever.

## Clock and reset semantics

Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons a pass in
flight; the capture buffer is then simply incomplete, which the pass counters
say. Nothing durable lives in the block.

## Input and output packet layouts

**In — the tap.** One beat per composited pixel ACCEPTED downstream:

| field | width | meaning |
|---|---|---|
| `tap_valid_i` | 1 | `echo_valid_o && o_valid_o && o_ready_i` of POST.COMPOSITE |
| `tap_rgb_i` | 16 | `echo_rgb_o`: RGB565, post-ink, pre-HUD |
| `tap_x_i`, `tap_y_i` | 9, 8 | the pixel's VIEW-local coordinate (`o_x_o`, `o_y_o`) |

The qualification by the output handshake is load-bearing: POST.COMPOSITE holds
`echo_valid_o` high through a downstream stall, so the raw tap would repeat a
stalled pixel once per stall cycle. The accepted beat happens exactly once.

**Pass control:** `pass_start_i` (the compositor's own `frame_start_i` pulse),
`view_i`, `w_i`, `h_i` — sampled at the pulse, held for the pass.

**Out — the capture.** 32-byte bursts through MEM.GUARD as client ENGINE0 into
the capture window (`zhao_pkg` `ZHAO_POST_ECHO_BASE` = `0x05C0_0000`, span
`0x0003_C000`, `spec/memory_rules.md` 5g). The capture is a row-major RGB565
image of width `w` and height `views × h`, **views stacked vertically**:

    addr(view, x, y) = ZHAO_POST_ECHO_BASE + ((view · h + y) · w + x) · 2

Stacking keeps the stride equal to the view width and needs no multiplier: the
second view is simply `h` rows further down. Duo (2 × 256 × 192) is 196,608
bytes; Z60 is 184,320; both inside the span.

## Backpressure rules

**The echo NEVER backpressures the compositor.** It has no ready. The contract
this block replaces already said the one thing that matters: *"drop the echo
rather than stall or fault the frame. An echo is observational; it must never
be able to affect what is displayed."*

Drops are **whole 16-pixel chunks**, decided at the chunk's first pixel: a chunk
is admitted only if the skid queue has room for all sixteen of its pixels, and
is otherwise dropped in full. Chunks are row-aligned (`w` is a multiple of 16),
so every admitted chunk is exactly one RASTER.FBWRITE row burst and a dropped
chunk leaves a clean 32-byte hole — never a torn burst, never a pixel written
to a neighbour's address. A pass with any dropped chunk is **TORN**.

## Memory ownership

Owns the capture window, WRITE-only, as client ENGINE0 under the render lease
(`fb_writer == 1`); the guard refuses a capture write from any other client or
without the lease, and refuses any read of the window
(`tests/formal/mem_guard_no_escape.sby`: `a1_echo_wo`, `a1_echo_owner`,
`a1_echo_lease`, `a1_echo_not_fb`). ENGINE0 is shared with the post source
read and the post write-back through one `zhao_mem_share_n` in
`zhao_post_lease`; the echo is one of its three logical requesters.

## Q formats and rounding

None. RGB565 in, RGB565 out, unchanged.

## Latency (fixed or variable)

Variable: a pixel waits in the skid until its chunk's burst is accepted. A pass
is COMPLETE when every pixel of the pass has been written and every issued word
has RETIRED at the arbiter — the same "accepted is not retired" law as
RASTER.FBWRITE, because it is RASTER.FBWRITE doing the writing.

## Target throughput

One tap pixel per clock accepted into the skid at peak. SUSTAINED, the echo
keeps up with RASTER.FBWRITE's rate -- 16 pixels per ~22 clocks on a fast
memory -- which is the rate the compositor's own write-back (the same engine)
paces the tap to; the 256-entry skid absorbs sixteen chunks of difference in
when the two writers win ENGINE0. A tap that outruns that for longer is
starved, and drops whole chunks. The echo adds 2 bytes per composited pixel of ENGINE0 write traffic
(184,320 B per Z60 frame); it shares ENGINE0 round-robin with the post source
read and write-back, so its cost is bandwidth and time, never content.

## Overflow and malformed-input behaviour

| condition | behaviour |
|---|---|
| skid has no room for a whole chunk at its first pixel | drop the chunk (16 px), count, mark the pass TORN |
| `w` not a multiple of 16, or `w`/`h` zero | refuse the pass at its start: every pixel dropped, pass TORN |
| `views·h·w·2` > span | the burst that leaves the window is REFUSED by MEM.GUARD (the region check lives there, once) -> `fault_o`, pass TORN. `zref::post::echo::geometry_ok` states the same bound as the law. |
| the guard refuses a capture burst | `fault_o` (sticky until reset -- it is RASTER.FBWRITE's own fatal latch), every later pass TORN |
| a tap pixel arrives with no pass open | dropped and counted |

## Counters and traces

`echo_pixels_written`, `echo_pixels_dropped`, `echo_passes_complete`,
`echo_passes_torn`, `fault_o`. A complete pass also pulses `pass_complete_o`,
which is what a consumer of the capture keys on: an incomplete capture is never
reported as a capture.

## Scalar reference function

`zref::post::echo` in `reference/include/zref/zref_post.hpp`:
`echo::geometry_ok(w, h, views)`, `echo::capture_addr(view, x, y, w, h)`,
`echo::chunk_of(x)`. It owns the address law, the chunk law and the geometry
refusal; the directed test differences the RTL's every written word against it.

## Directed tests

`tests/compositor/post_echo_directed.cpp` (RTL against `zref::post::echo`):

* a Z60 pass with the tap paced as the composition paces it (16 on, 6 off --
  one FBWRITE row burst) on a fast memory: every one of 92,160 pixels lands at
  `capture_addr`, byte-identical; `passes_complete == 1`, `dropped == 0`;
* Duo: two passes, views stacked — view 1 lands `h` rows below view 0, and no
  word of either view is written outside its own rows;
* a memory that stalls hard: chunks drop WHOLE — every written word is at its
  law address with its tap value, the dropped count is exactly 16 × the chunks
  absent from memory, the pass is TORN and not reported complete;
* the tap is qualified: a stalled compositor beat (tap held, not accepted) is
  written once -- a property of the COMPOSITION (`zhao_post_lease` ANDs the
  tap with the output handshake), proven in the console smoke, where the
  capture must equal the framebuffer word for word;
* a width that is not a multiple of 16 is refused;
* a guard refusal latches `fault_o` and tears the pass;
* the echo never has a ready: the test drives the tap at one pixel per clock
  regardless and asserts nothing upstream could have waited.

## Randomized differential tests

The directed bench's third case is itself randomised (PCG-driven memory stalls
over two seeds). A separate random bench is not written: the block has no data
path of its own beyond the address law, which the directed cases exhaust.

## Formal properties

The window is proven by `mem_guard_no_escape` (above). No block-level formal
lane.

## Synthesis / resource ceiling

`zhao_raster_fbwrite` (one more instance — the same row-burst engine the
renderer uses, not a second design) + a 256 × 34-bit skid written to infer one
M10K + chunk/pass counters. Estimate ~400 ALM, 0–1 DSP (fbwrite's 12×16 row
multiply), 1 M10K. **Unmeasured; no fit has been run.**

## Integration capture cases

`tests/prod/tb_zhao_console_core_smoke.sv`: after the smoke's render frame
drains, the post pass runs and the echo captures it; the bench compares the
capture window against the framebuffer word for word (with no HUD and effects
off the two must be identical) and requires `echo_passes_complete == 1`.

## Notes

cut_order 1 — the first thing to go if synthesis fails (§26). Cutting it costs
exactly this file and one ENGINE0 requester; POST.COMPOSITE's tap stays.
