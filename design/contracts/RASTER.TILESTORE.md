# Contract — RASTER.TILESTORE (Tile store)

> Ledger: `design/blocks.yml` · owner ZH-021 · phase 4 · maturity SPECIFIED

## Purpose and exclusions

Hold the working set of the tile renderer: two ping-pong 16×16 tiles at 64 bits per pixel, with a single-cycle clear. RTL: `fpga/rtl/raster/zhao_raster_tilestore.sv`.

Exclusions — none of these are in this block: fragment maths of any kind (Z test, blend, stencil op, alpha test — RASTER.FRAGMENT owns those, and this block never inspects the word it stores), dither and framebuffer writes (RASTER.RESOLVE), VRAM traffic (MEM.GUARD / MEM.VRAM.ARBITER — this block touches no external memory at all), tile scheduling and the addressing of tiles within a frame (the tile index is not an input; it is the caller's bookkeeping), coverage (RASTER.EDGEWALK). It is a two-banked 256×64 memory with a clear and a swap.

**Byte enables are deliberately not built**, see Q formats below.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release, in the style of every other block in this tree (`always_ff @(posedge clk or negedge rst_n)`). On reset: bank 0 is the front bank, both banks' present bits are clear and both clear words are 0 — so **every address of both banks reads as all-zero before anything is written**, on both read ports, and the directed suite checks all 512 of them. `rd_valid_o` / `res_valid_o` low, `tile_references_o` 0. The RAM arrays themselves are not reset (they are M10K); the present bits make that unobservable.

## The tile word (charter §8, bit positions defined here)

The charter fixes the four fields and their widths — "24-bit RGB working colour, 8-bit effect tag/strength, 24-bit inverse-W/depth, 8-bit stencil … exactly 64 bits per active pixel, or 2 KiB per tile" — but not their bit positions. This block fixes them once, in charter order, MSB first:

| bits | field |
|---|---|
| `[63:56]` | working colour R |
| `[55:48]` | working colour G |
| `[47:40]` | working colour B |
| `[39:32]` | effect tag/strength (`spec/stars_and_flares.md` §1: `(channel << 6) | strength`) |
| `[31:8]` | inverse-W depth |
| `[7:0]` | stencil |

The block is field-agnostic — it stores 64 bits and never decodes them — but `zref::TileStore::Word`, `zref::TileResolve`, `zhao_raster_resolve` and every test read this packing, so it is load-bearing. It is stated here and in the RTL header, and nowhere else.

Addresses are `{row[3:0], col[3:0]}`, row 0 = top, col 0 = left.

## Input and output packet layouts

| channel | fields | meaning |
|---|---|---|
| clear (`clear_valid_i` / `clear_ready_o`) | `clear_data_i` 64 | reset the FRONT bank to this word, in one cycle |
| write (`wr_valid_i` / `wr_ready_o`) | `wr_addr_i` 8, `wr_data_i` 64 | full-word write to the FRONT bank |
| read A (`rd_valid_i` / `rd_ready_o`) | `rd_addr_i` 8, `rd_src_id_i` 16 | the fragment working view of the FRONT bank |
| read A out | `rd_valid_o`, `rd_data_o` 64, `rd_src_id_o` 16 | one cycle later; `source_ids: true` passthrough |
| read B (`res_valid_i` / `res_ready_o`) | `res_addr_i` 8 | RASTER.RESOLVE's `tile_read` view of the BACK bank |
| read B out | `res_valid_o`, `res_data_o` 64 | one cycle later |
| swap (`swap_valid_i` / `swap_ready_o`) | — | exchange the front and back roles |
| status | `front_bank_o` 1, `tile_references_o` 32 | which PHYSICAL bank is front; the counter |

### In-cycle ordering (normative)

1. **clear** (front bank only) happens first;
2. then **write** (front bank only) — a write is NOT accepted in a cycle where a clear is accepted, so 1 and 2 never race;
3. **reads** on both ports observe the result of 1 and 2. A read returns **NEW** data for a same-cycle same-address write (write-first), because a read-modify-write fragment pipeline must never see the pixel it just wrote as stale. A read in the same cycle as a clear returns the **new** clear word;
4. **swap** is last: an access accepted in the same cycle as a swap targets the roles as they were **before** the swap, while `front_bank_o` shows the post-swap role.

Every one of those four rules has its own directed case.

## Backpressure rules

`ready_valid` on every channel. No port has a stall condition of its own: `clear_ready_o`, `rd_ready_o`, `res_ready_o` and `swap_ready_o` are constant 1 and the directed suite asserts they never drop. The single cross-port rule is `wr_ready_o = !clear_valid_i` — a clear locks the write port for that cycle, which is what makes ordering rule 2 sound. Nothing in the block reads a downstream ready, so there is no combinational valid←ready path anywhere.

## Memory ownership

Two private banks of 256 × 64 bits (2 KiB each, 4 KiB total — the charter's "two ping-pong tiles consume roughly 4 KiB"). No VRAM, no arena, no external memory, no DMA. The banks are declared as two separate arrays with one read address and one write enable each, because each PHYSICAL bank carries at most one write and one read at any instant (port A reads it iff it is front, port B iff it is back — mutually exclusive), which is what a simple dual-port M10K offers. The role mux sits on the addresses in front of the RAM and on a registered bank tag behind it, never as a second read port.

**The clear is O(1), not 256 cycles.** A per-word PRESENT bit shadows each bank; `clear` drops all 256 present bits of the front bank and latches the clear word, and a read of a not-present word returns the clear word. The charter's per-tile pass order clears every tile at pass 1, 360 times a frame in Z60 — 360 × 256 = 92k scrub cycles a frame is not a budget worth paying, and a scrubbing clear would also compete with the fragment pipeline for the write port. Cost: 2 × 256 present flops + 2 × 64 clear-word flops ≈ 640 flops. The present bits must be flops, not RAM — 256 of them are cleared at once.

## Q formats and rounding

None. The block stores and returns 64-bit words unaltered; there is no arithmetic on the data path and therefore no rounding, saturation or format question. The only arithmetic in the module is the address decode and the saturating reference counter.

**No byte enables, and why.** A partial write to a not-present word would set its present bit while the un-enabled bytes still held pre-clear RAM garbage. Byte enables would therefore need either per-BYTE present bits (4,096 flops) or a read-modify-write port the M10K does not have. The fragment pipeline reads the pixel before it writes it (Z test, then blend), so it already holds the whole word; full-word writes cost it nothing and buy the O(1) clear its correctness. If a future block genuinely needs a partial write, the honest fix is per-byte present bits, not a silent partial write.

## Latency (fixed or variable)

**`fixed:1`** on both read ports, exactly as the ledger declares: a read accepted in cycle *N* presents `rd_valid_o` / `rd_data_o` (or `res_*`) in cycle *N+1*, unconditionally and with no stall path. `clear` and `swap` take effect at the end of the cycle they are accepted in. There is no pipeline to drain and no in-flight state, so there is no reset-ordering hazard.

## Target throughput

One accepted access per port per clock — the ledger's "1 tile access per clock", available simultaneously on the write port, read port A and read port B. The clear costs one cycle of the write port and nothing else; the swap costs nothing.

## Overflow and malformed-input behaviour

- **Every address is legal.** The address is 8 bits and the tile is exactly 256 words, so there is no out-of-range address to reject and no scribble to guard against.
- **Every data value is legal.** The block never decodes the word.
- **A write in a clear cycle** is refused, not silently dropped: `wr_ready_o` is low, so a producer obeying ready/valid simply retries. A producer that ignores `wr_ready_o` loses that write — that is a protocol violation on its side, and the reference model reproduces the same refusal.
- **A swap mid-stream** is legal and defined (ordering rule 4); it is the caller's business whether it makes sense.
- **`tile_references_o` saturates** at `0xFFFF_FFFF` per `spec/counters.md` §4 — a counter never wraps.
- There is no input that can make the block hang: it has no state machine, only ordering.

## Counters and traces

`tile_references_o` counts **accepted data accesses** — writes plus both read ports, up to 3 per cycle — saturating at 32 bits. `clear` and `swap` are commands, not accesses, and are not counted; the directed suite pins both.

The catalog id and the `frame_tick` shadow latch (`spec/counters.md` §3/§5) are **not** implemented here, exactly as RASTER.EDGEWALK deferred `covered_fragments`: `tile_references` exists in the catalog (`design/blocks.yml` `counter_catalog:`) but has no live owner in wave 2, and it is also claimed by GEOM.BINNER (triangle × tile list references) and by RASTER.RESOLVE (resolved tiles). Reconciling three producers of one catalog entry is a `spec/counters.md` amendment that belongs with the DEBUG.COUNTERS integration wave, not with this block. What ships here is the local, saturating, differentially-verified counter.

## Scalar reference function

`zref::TileStore` (`reference/include/zref/zref_tilestore.hpp`, `reference/src/zrender/tilestore.cpp`).

Unlike `zref::EdgeWalk`, this **is** a second implementation — a memory has no pre-existing frozen law to call. It is therefore written in the plainest possible way (two arrays, a present bit, a clear word) and implements the *contract*, not a shared clever idea: the RTL's role mux, registered bypass and split banks have no counterpart in it, so "RTL == oracle" tests exactly those mechanics. What it is *not* is a second definition of anything — the tile word layout is `zhao_raster_tilestore.sv`'s, quoted, and `zref::TileResolve` reads this same `Word` struct rather than re-deriving field positions.

`step()` takes one cycle of port stimulus, applies one clock edge, and returns what the RTL presents: `wr_ready` is combinational in the stimulus cycle, `rd_*` / `res_*` are the following cycle (the fixed 1-cycle latency), `front_bank` / `tile_references` are the post-edge values. The driver lines the two up beat for beat with no fudge factor.

## Directed tests

`tests/raster/raster_tilestore_directed.cpp` (driver `tests/raster/raster_tilestore_dev.hpp`) — **31 checks over 4,955 cycles**, every one of them stepped through the RTL and `zref::TileStore` in lockstep with all outputs compared. Cases: reset (all 512 addresses of both banks read zero); the charter §8 word roundtrip exhaustively over all 256 addresses, checked both as a 64-bit word and field by field; the clear (every address reads the clear word, a write after it is visible, a re-clear drops it again); the clear/write lock; a clear and a read in the same cycle; the write-through-read bypass swept across all 256 addresses, with the neighbour asserted untouched; ping-pong (a tile is invisible on the resolve port before the swap and exact after it, both banks live and distinct at once, a second swap hands over the second tile); isolation (a front-bank write aliasing the address the resolve port is reading in the same cycle must not leak — the dead port-B bypass asserted rather than assumed — and a front-bank clear must not touch the back bank); swap ordering; `source_id` passthrough; the counter rules; and 512 cycles with every channel active at once.

## Randomized differential tests

`tests/raster/raster_tilestore_random.cpp` — deterministic from two fixed seeds (the PCG shape of the other random lanes).

**Lane A** — free-running traffic: every channel independently activated by its own PCG stream over a *narrow* address window (4…64 addresses), so write/read collisions are common rather than a 1-in-256 accident. Every output compared every cycle. The lane asserts its own coverage: at 20,000 cycles it fires **698 write-through-read bypasses, 299 clear/write races and 337 swaps**, and the test fails if any of those counts is zero.

**Lane B** — the render/resolve duty cycle: clear the working tile, write all 256 pixels in a PCG order with read-modify-write reads mixed in, swap, and stream the finished tile out of the resolve port *while the next tile is being written into the working bank*. It asserts what the ping-pong is for — the tile the resolve port streams is bit-identical to the tile written one pass earlier, whatever the working bank is doing (5,888 words checked in the fast run).

Default 20,000 cycles + 24 passes (CTest `fast`); `--nightly` 300,000 cycles + 360 passes (91,904 resolve-port words). Failing vectors are serialized per charter §29-17.

**Mutation evidence** (2026-08-18): five deliberate RTL defects were injected one at a time and **each was caught by BOTH lanes**:

| mutation | directed | random |
|---|---|---|
| ping-pong never swaps | RED | RED |
| read-during-write returns OLD data (bypass disabled) | RED | RED |
| clear latches the word but keeps the present bits | RED | RED |
| read port A addresses the BACK bank | RED | RED |
| the clear command counts as a tile reference | RED | RED |

## Formal properties

**None, and that is a deliberate statement rather than an omission.** Everything this block does is a memory identity — "what you wrote is what you read, in the bank the roles say" — which a bounded model check would restate rather than prove, and whose interesting content (256 addresses × 2 banks × the ordering rules) is *exhaustively* covered by the directed suite instead: all 256 addresses are written and read back, all 256 are swept through the bypass, and all four ordering rules have their own case. A `.sby` here would produce a proof with nothing to cover that the directed lane does not already decide. Per the charter's own standard, a proof with no cover task is worse than no proof.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is 2 × (256 × 64) bits of block RAM (4 KiB, i.e. 4 M10Ks at 10 Kbit if the tool packs 64-bit words two-deep, more if it does not), ~640 flops for the present bits and clear words, and roughly 200 flops of read pipeline (two × {64-bit RAM word register is inside the M10K, plus a 64-bit clear word, a 64-bit bypass word, a present bit and a bank tag}) plus the 32-bit counter. The 2 × 64-bit bypass registers are the one avoidable cost: dropping write-first semantics in favour of read-old would remove them and push the merge onto the fragment pipeline.

Two things to watch at fit time, neither measured: the 64-bit 3-way output mux (bypass / RAM / clear word) sits *after* the M10K output register, so it is combinational logic on the read path; and the 256-bit present vector has a 256-fanout clear.

## Integration capture cases

None yet — the block is standalone. It is not in `ZHAO_SHELL_RTL`, has no capture, and has never run on hardware. The phase-4 gate's FPGA leg (ZH-024, "resolve one exact tile to a hardware framebuffer") belongs to this block together with RASTER.RESOLVE and RASTER.EDGEWALK, and none of it has been attempted: simulated is not synthesized.

## Notes

**Ledger citation correction.** `design/blocks.yml` says "M10K budget per §7.3" under `notes:`, and the surrounding fields point at `spec/memory_rules.md`. `spec/memory_rules.md` has seven sections and no §7.3; the "two active 16×16 tile stores" M10K/MLAB tenancy list is **charter §7.3**. This contract cites the charter. The ledger note has been left as written — correcting a ledger field is a validator-gated edit and not this increment's call.

Deliberately not built in this block, so the next wave knows: no byte enables (reasoned above); no third read port; no more than two banks; no clear of the BACK bank (the clear targets the working tile by definition); no address generation, tile indexing or frame-level addressing; no `tile_references` catalog-id binding or `frame_tick` shadow latch; no ECC or parity; no formal property (reasoned above).
