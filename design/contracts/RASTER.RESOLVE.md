# Contract — RASTER.RESOLVE (Tile resolve)

> Ledger: `design/blocks.yml` · owner ZH-024 · phase 4 · maturity SPECIFIED

## Purpose and exclusions

Resolve one finished 16×16 tile from the 64 bpp working store to RGB565 with the charter §8 ordered dither, and hand off the tile's CRC-32C. RTL: `fpga/rtl/raster/zhao_raster_resolve.sv` plus `fpga/rtl/raster/zhao_raster_quant.sv` (one channel quantizer, a separate module so the formal property proves the shipping arithmetic and not a copy of it) and `fpga/rtl/raster/zhao_raster_div255.sv`.

Exclusions — none of these are in this block: VRAM addressing and the framebuffer write itself (it emits a pixel stream; MEM.GUARD and the write path own the address), depth/stencil resolve (charter §8: "no external full-screen depth buffer in the normal tile path"; `capture_format.md`'s `DEPTH_STENCIL_CRC` is optional and not built), scanout (VIDEO.SCANOUT), the frame-level displayed CRC (DEBUG.CRC, over a different byte stream), tile scheduling and tile-index generation, post effects, and the effect-tag *gather* (POST.GATHER — this block only carries the tag out undithered).

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release. On reset: idle, `start_ready_o` high, `tr_valid_o` and `fb_valid_o` low, the skid FIFO empty, the occupancy counter zero, `tile_crc_valid_o` low, `tile_references_o` 0. One tile is in flight at a time; `start_ready_o` stays low from acceptance until the CRC handoff, so there is no cross-tile pipelining and no reset ordering hazard. Every per-tile register (CRC seed, address cursors, occupancy, FIFO pointers) is re-armed at `start`, not only at reset — `test_back_to_back` exists to pin that.

## Input and output packet layouts

Start (`start_valid_i` / `start_ready_o`) — the `resolved_tile_trigger`:

| field | width | meaning |
|---|---|---|
| `start_tile_x_i`, `start_tile_y_i` | signed 12 | the tile's top-left PIXEL; sets the Bayer phase (only bits [1:0] are used) |
| `start_tile_index_i` | 16 | the `.zcap` TILE_CRC `tile_index` (`capture_format.md` §4.2 declares it u32; this block carries 16 bits, zero-extended by the writer) |
| `start_src_id_i` | 16 | source id, carried through untouched (`source_ids: true`) |

Tile-read master (`tr_valid_o` / `tr_ready_i`), the RASTER.TILESTORE back-bank port:

| field | width | meaning |
|---|---|---|
| `tr_addr_o` | 8 | `{row[3:0], col[3:0]}`, issued in ascending raster order |
| `tr_data_valid_i`, `tr_data_i` | 1, 64 | exactly one response per accepted request, at the TILESTORE's fixed 1-cycle latency |

`fb_tiles` out (`fb_valid_o` / `fb_ready_i`), one beat per pixel, tile raster order:

| field | width | meaning |
|---|---|---|
| `fb_rgb565_o` | 16 | `[15:11]` R, `[10:5]` G, `[4:0]` B (`video_rules.md` §3) |
| `fb_tag_o` | 8 | the effect tag — **never dithered** (`stars_and_flares.md` §1) |
| `fb_addr_o` | 8 | `{row, col}` within the tile |
| `fb_last_o` | 1 | the 256th pixel of this tile |
| `fb_src_id_o` | 16 | the tile's source id |

`tile_crc` out: `tile_crc_o` 32, `tile_crc_index_o` 16, `tile_crc_valid_o` a one-cycle pulse in the cycle after the last accepted pixel. That pulse is also the tile's completion.

## Backpressure rules

`ready_valid` on all three channels. `fb_valid_o` is `fcount != 0` — a function of registers only, never of `fb_ready_i`. A stalled beat holds `fb_rgb565_o` / `fb_tag_o` / `fb_addr_o` / `fb_last_o` / `fb_src_id_o` stable until accepted; the driver asserts that on every beat of every tile.

`tr_valid_o` does depend combinationally on `fb_ready_i` (through the occupancy credit `occ − pop < 2`) — that is a *different* channel's ready, which is the permitted direction; it never depends on `tr_ready_i`, its own. The composition is loop-free because RASTER.TILESTORE's `res_ready_o` is a constant 1.

**The credit rule.** `occ` counts reads issued but not yet emitted. A response converts an outstanding read into a FIFO entry (net zero), so `occ` moves only on issue (+1) and on emit (−1); issuing only while `occ − pop < 2` therefore keeps `occ ≤ 2` and the 2-entry skid FIFO can never overflow. That is what lets the block sustain one pixel per clock when the consumer is ready and still never drop an in-flight read when it is not.

## Memory ownership

None. No VRAM, no arena, no tile RAM — the tile lives in RASTER.TILESTORE and this block reads it through a master port. Internal state is the 2-entry skid FIFO (2 × 33 bits) plus the per-tile registers.

## Q formats and rounding

**The law is `reference/src/zrender/resolve.cpp`, quoted:**

```
r5 = min(31, (r*31 + (B*16 +  8)) / 255)
g6 = min(63, (g*63 + (B*32 + 16)) / 255)
b5 = min(31, (b*31 + (B*16 +  8)) / 255)
```

with `B` the 4×4 Bayer value and `/` a floor division on a non-negative numerator. The RTL computes exactly this: `zhao_raster_quant #(MAXQ, QW, AMP, RND)` instantiated three times as `(31,5,16,8)`, `(63,6,32,16)`, `(31,5,16,8)`.

**`AMP` and `RND` are parameters, not a formula.** The oracle's header calls green out by hand — "its dither amplitude doubles (32 vs 16) while its quantization headroom halves" — and green's 32/16 is *not* what the stated threshold `t = (B+0.5)/16` of **one** quantization step would give. One output level is 255 numerator units for all three channels, so that threshold yields 16/8 for every channel; green's 32/16 dithers across **two** quantization steps. Deriving `AMP` from `MAXQ` would therefore be inventing a law that contradicts the oracle. The oracle is the law; these are its constants, named at the instantiation.

**The dither matrix is transcribed, not generated.** `plan W3.5`, quoted in `resolve.cpp`'s own header: *"fixgen has NO dither table today (checked at W3.5 start), so the canonical 4×4 Bayer thresholds (B+0.5)/16 are defined HERE, once."* The RTL's `bayer4` function is that matrix and nothing else. There is no generated table to include and none was invented. See Notes for the stale ledger claim.

**The clamps are the 2026-08-16 white rail.** Green at `B ≥ 8` with `g ≥ 252` quantizes to 64, which wraps in a six-bit field; full white then resolves to a `0xFFFF`/`0xF81F` checkerboard. Only green can actually exceed its field (the 5-bit numerators top out at `255·31 + 248 = 8,153 < 32·255 = 8,160`), but all three are clamped exactly as the oracle clamps them, because a resolve that relies on one channel's arithmetic slack is one parameter change from wrapping.

**The Bayer phase is ABSOLUTE, not tile-local**: `B = bayer4[(tile_y + row) & 3][(tile_x + col) & 3]`. `resolve.cpp` indexes the matrix by the pixel's position in the surface. For the 16-aligned tile grid the tile-local form coincides (16 ≡ 0 mod 4), which is exactly why a tile-local phase would pass a careless test and break the moment anything resolves at an unaligned origin. Only the low two bits of the origin are used, so a negative origin needs no sign handling.

**The division.** `zhao_raster_div255` computes `floor(n/255)` as `(n + (n>>8) + 1) >> 8`. That identity is exact for `n < 255·257 = 65,535` (the RTL header carries the proof); the shipping width is 15 bits and the widest numerator the block ever forms is green's `255·63 + 15·32 + 16 = 16,561`, a factor-of-2 margin. It is proved total at the shipping width — see Formal properties.

**The CRC.** CRC-32C, `spec/capture_format.md` §2 (poly `0x82F63B78` reflected, init and xorout `0xFFFFFFFF`), via the **generated** `zhao_abi_pkg::zhao_crc32c_step` — the same function `zhao_debug_crc` and `zhao_cmd_dma` call. One polynomial machine-wide (plan A3d); this block mints no CRC variant. It covers the 512 framebuffer bytes of the tile in raster order, **little-endian halfwords** (`video_rules.md` §3): low byte then high byte, two chained steps per accepted pixel. The effect tag is *not* in the CRC — the CRC covers framebuffer bytes and the tag never reaches VRAM.

## Latency (fixed or variable)

**Variable**, as the ledger declares. At full readiness on both sides a tile costs 258 cycles (256 pixels, the two-cycle pipeline fill, and the finalize cycle that carries `tile_crc_valid_o`), plus one cycle to accept the start. Unbounded above under backpressure on either side — a stall costs beats, never pixels. Lower bound per tile: 259 cycles from `start` acceptance to the CRC pulse.

## Target throughput

One resolved pixel per clock while both sides are ready; the ledger's "1 resolved tile per pass" is 256 such pixels. The 2-deep skid FIFO plus the occupancy credit are what sustain the per-clock rate across a 1-cycle-latency read port.

## Overflow and malformed-input behaviour

- **Every input pixel is legal.** All 2^24 colour values × 16 Bayer phases are in range by construction, and the quantizer is *proved* (not argued) never to exceed its field for any of them.
- **Arithmetic overflow** is structurally impossible: the widest numerator is 16,561 in a 15-bit path, and the `/255` identity is proved exact over that whole width.
- **Depth and stencil are ignored**, not rejected: `tr_data_i[31:0]` is dropped by design, and `test_depth_stencil_ignored` pins that the unresolved half of the word cannot change a pixel.
- **An unaligned or negative tile origin** is legal and defined (only bits [1:0] matter), and is exercised deliberately — the random lane runs half its tiles at non-16-aligned origins.
- **A response with no outstanding request** is an upstream protocol violation. The block would push a spurious FIFO entry; it does not police it, and there is no error output. The tile-read contract (one response per accepted request) is RASTER.TILESTORE's, which honours it by construction with a fixed 1-cycle latency. Stated here so the gap is on the record rather than implied.
- **`tile_references_o` saturates** at `0xFFFF_FFFF` per `spec/counters.md` §4.

## Counters and traces

`tile_references_o` counts **resolved tiles**, one per completed tile, saturating at 32 bits. As with RASTER.TILESTORE, the catalog id and the `frame_tick` shadow latch (`spec/counters.md` §3/§5) are **not** wired: `tile_references` has no live owner in wave 2 and is claimed by three blocks, and reconciling that is a `counters.md` amendment for the DEBUG.COUNTERS integration wave. Trace: the full `fb_tiles` pixel stream plus the tile CRC is what the differential tests compare.

## Scalar reference function

`zref::TileResolve` (`reference/include/zref/zref_tileresolve.hpp`, `reference/src/zrender/tileresolve.cpp`).

It is deliberately **not** a second implementation of the dither: the body calls `zref::render::resolve_rgb565` (`reference/src/zrender/resolve.cpp`) — the frozen Bayer matrix, the three quantizers and the white-rail clamps — and reads the resolved halfwords back out. Its CRC likewise calls `zhao_abi::zhao_crc32c`. So "RTL == `zref::TileResolve`" is literally "RTL == the charter §8 resolve", and there is no dither arithmetic anywhere in the test tree.

`resolve_rgb565` takes its Bayer phase from the buffer origin, so the model resolves a surface padded by `(tile_x & 3, tile_y & 3)` on the top and left and reads the 16×16 sub-rect back: the pad puts tile pixel `(col,row)` at buffer position whose low two bits are exactly `(tile_x+col) & 3` and `(tile_y+row) & 3`. Same matrix, same quantizer, same clamps, right phase — the same shape of translation trick `zref::EdgeWalk` uses for tile origins. The RTL is handed the untranslated tile plus the origin, so a bug in its phase arithmetic still shows as a pixel mismatch.

## Directed tests

`tests/raster/raster_resolve_directed.cpp` (driver `tests/raster/raster_resolve_dev.hpp`) — **110 checks over 3,512 resolved tiles**. Every case diffs all 256 RGB565 pixels, all 256 tags and the tile CRC against `zref::TileResolve`; on top of that:

- **the rails.** White is `0xFFFF` at every one of the 16 Bayer phases and no pixel wraps to `0xF81F`; the near-white band `[248,255]` never wraps green to 0. Black is pinned to its *actual* law — see Notes: every pixel is `0x0000` or `0x0020`, red and blue are exactly 0, and exactly 128 of 256 pixels are lifted.
- **channel sweep** — 1,024 constant tiles (grey, and each channel alone, for every value 0…255), i.e. 262,144 pixels. A constant tile visits all 16 Bayer phases 16 times each, so every (value, phase) pair of every channel is *covered*, not sampled.
- **green amplitude** — the vector set is first *proved able to tell* amplitude 32 from 16 (2,037 of the 4,096 (g, B) pairs discriminate them; the candidate arithmetic is used only to measure that, never as an expected value) and only then used to check the RTL against the oracle. The red sweep pins the converse: a 5-bit channel keeps amplitude 16.
- **rounding edges** — 1,888 tiles in a ±2 band around every 5-bit and 6-bit quantization boundary, at four phases.
- **dither phases** — all 16 absolute phases produce a distinct tile (all 120 pairs differ), five 16-aligned origins including negative ones all reduce to phase 0, and five unaligned/negative origins take the right phase.
- **tag** — every tag value rides through undithered and unpermuted against a colour that is being dithered hard, and does not depend on the colour or the phase.
- **depth/stencil** — two tiles identical in colour+tag and maximally different in depth+stencil resolve identically.
- **CRC payloads** — an all-white tile is checked against `zhao_crc32c` over 512 `0xFF` bytes (an independent known payload); the mixed tile's CRC is checked against the little-endian halfword reconstruction *and* the big-endian one is shown to differ, so a byte-order flip cannot hide; black is used as a negative anchor (it is *not* 512 zero bytes).
- **backpressure** — 8 PCG stall patterns on both the `tile_read` and `fb_tiles` sides give bit-identical pixels and CRC.
- **back to back** — six tiles through one instance with no reset.

## Randomized differential tests

`tests/raster/raster_resolve_random.cpp` — deterministic from two fixed seeds.

**Lane A** — resolve differential over four tile populations (uniform random, near-rail `[0,7] ∪ [248,255]`, low-contrast 8-wide bands, and smooth gradients) at PCG origins deliberately *not* all 16-aligned: 744 of 1,200 fast-run tiles sit at an unaligned Bayer phase, and the lane fails if that fraction collapses. Half run with PCG-gated `tr_ready_i` and `fb_ready_i`.

**Lane B** — the two invariants. Resolving a tile at `(x, y)` and at `(x+4k, y+4m)` must give the identical picture (the phase depends on the origin mod 4 and nothing else — a tile-local phase fails this), and three different stall patterns must give the identical picture *and* CRC (backpressure costs cycles, never pixels).

Default 1,200 + 300 tiles (CTest `fast`); `--nightly` 18,000 + 4,000. Failing vectors are serialized per charter §29-17.

**Mutation evidence** (2026-08-18): seven deliberate RTL defects were injected one at a time and **each was caught by BOTH lanes**; the two arithmetic ones were caught by the formal property as well.

| mutation | directed | random | formal |
|---|---|---|---|
| green amplitude 16/8 instead of 32/16 | RED | RED | — |
| Bayer phase tile-local (tile origin dropped) | RED | RED | — |
| Bayer matrix entry pair swapped | RED | RED | — |
| CRC byte order flipped (high then low) | RED | RED | — |
| the CRC is not reseeded per tile | RED | RED | — |
| the `+1` of the `/255` reciprocal identity dropped | RED | RED | **RED** |
| the white-rail clamp removed | RED | RED | **RED** |

## Formal properties

`tests/formal/raster_resolve_quant.sby` + `tests/formal/raster_resolve_quant_fv.sv` — **PASS (bmc + cover)**, both tasks, `smtbmc boolector`, depth 2, all 8 covers reached. The DUT is `zhao_raster_quant`, the exact module `zhao_raster_resolve` instantiates three times per pixel, elaborated at **both** shipping parameter sets.

- `a_exact_five` / `a_exact_six` — the shipping output IS `min(MAXQ, floor(num/255))` on `resolve.cpp`'s numerator, stated division-free so the solver never sees a divider: `q == MAXQ ⇒ num ≥ MAXQ·255`, and `q < MAXQ ⇒ q·255 ≤ num < (q+1)·255`. Together those say exactly "the floor, clamped at MAXQ, and clamped only when the floor really exceeded it". A wrong amplitude, a wrong rounding term, an off-by-one in the reciprocal identity or a misplaced clamp all break it.
- `a_no_wrap_five` / `a_no_wrap_six` — **the white rail as a theorem.** The result can never exceed its RGB565 field, for any input. This is the 2026-08-16 defect discharged as a proof rather than pinned as a regression vector.

The free inputs are `v` (8 bits) and `B` (4 bits), which is **total, not sampled**: the working colour channel *is* a u8 and the 4×4 Bayer matrix's entries are exactly 0…15, so free `(v, B)` is all 4,096 inputs the block can ever be handed, at each parameter set. Depth 2 is the full state space, not a bound — the harness is purely combinational and every input is unconstrained. The load-bearing cover is `c_six_clamped`: green's clamp is shown to *actually fire* (the unclamped floor really is 64), without which `a_no_wrap_six` would also hold for a quantizer that simply never reaches the rail.

**Not proved formally, and why.** The Bayer *matrix* (which of the 16 values sits at which `(y&3, x&3)`), the absolute phase arithmetic, the 256-pixel walk, the skid-FIFO credit rule and the CRC accumulation are not proved: they are covered by the differential lanes against `zref::TileResolve` — the 1,024-tile channel sweep and the 16-phase grid decide the matrix and the phase *exhaustively over the value space*, and the mutation table above shows each of them going red. What the proof adds is the arithmetic every one of those pixels flows through, over inputs no differential run enumerates twice.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is three quantizers (each a constant multiply that folds to a shift-and-subtract, a 15-bit adder, a 16-bit adder for the `/255` identity, and a 6-bit compare), the 16-entry Bayer LUT, ~100 flops of tile state, a 2 × 33-bit skid FIFO, and the CRC.

The two paths to watch, neither measured: the CRC does **two** chained `zhao_crc32c_step` calls per cycle (each is 8 XOR/shift stages), i.e. twice the combinational depth of `zhao_debug_crc`'s single step — if that misses timing, the honest fix is one byte per cycle at half rate, not a different polynomial; and `tr_valid_o` is combinational in `fb_ready_i` through the occupancy credit.

## Integration capture cases

None yet — the block is standalone. It is not in `ZHAO_SHELL_RTL`, has no capture, and has never run on hardware. **It has also never been composed with `zhao_raster_tilestore`**: the two ports are written to fit (this block's `tr_*` master is exactly that block's `res_*` slave, and the tile word layout is shared through `zref::TileStore::Word`), but no test instantiates both, and the driver plays the store from a flat array instead. That composition, and ZH-024's "resolve one exact tile to a hardware framebuffer", are the next increment. Simulated is not synthesized and neither is on-hardware.

## Notes

**FINDING — the BLACK rail is not clean, and the oracle does not fix it.** Pure black does not resolve to `0x0000`. Green's amplitude is 32, so at Bayer values `B ≥ 8` the green numerator is `0·63 + 32B + 16 ≥ 272 ≥ 255` and `g6` comes out as **1**: exactly half of a pure-black tile resolves to `0x0020`, a green speckle on black. Red and blue, at amplitude 16, top out at `15·16 + 8 = 248 < 255` and stay exactly 0.

This is the black-rail counterpart of the white-rail defect fixed on 2026-08-16, and unlike that one it is **not** fixed in `resolve.cpp` — a clamp cannot fix it, because the value genuinely computes to 1. It follows directly from green's doubled amplitude, which is the same design choice the white-rail note calls out. This block reproduces it bit for bit, because the oracle is the law and changing `resolve.cpp` would move every golden capture's canvas CRC. It is recorded here, and pinned by `test_rails`, as an observed law rather than an endorsed one. **If it is to be changed, that is a `resolve.cpp` + golden-capture decision, not an RTL one.**

**Ledger note correction.** `design/blocks.yml` says "Dither matrix is a generated table (W3 fixgen), never hand-typed." That is stale: `resolve.cpp`'s header records that fixgen has no dither table (checked at W3.5 start) and defines the matrix in the reference itself, and no generated table exists in the tree today. The RTL transcribes the reference matrix and cites it. The ledger note has been left as written — correcting a ledger field is a validator-gated edit and not this increment's call.

Deliberately not built in this block, so the next wave knows: no framebuffer address generation or VRAM write; no depth/stencil resolve or `DEPTH_STENCIL_CRC`; no multi-tile pipelining (one tile in flight); no error output for a spurious tile-read response; no `tile_references` catalog-id binding or `frame_tick` shadow latch; no composition test with RASTER.TILESTORE; no half-rate CRC fallback.
