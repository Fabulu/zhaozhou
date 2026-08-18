# Contract — TEXTURE.CACHE (Texture/material cache)

> Ledger: `design/blocks.yml` · owner ZH-061 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

M10K-backed texture / palette / material cache: four independent direct-mapped lookup lanes with their own tag checks, one shared fill port to VRAM through MEM.GUARD, an invalidate port, and the `cache_hits` / `cache_misses` counters. RTL: `fpga/rtl/texture/zhao_texture_cache.sv`.

Exclusions — none of these are in this block: address generation, format decode, palette **lookup** (a palette page is cached like any other bytes; the indexing is TEXTURE.TMU's second access), filtering, mip selection, wrap folding; any VRAM protocol beyond a line request and its beats — MEM.GUARD owns the region check, this block emits an address and counts; any write path other than its own fill; any coherence protocol (there is exactly one writer); and any eviction **policy** — direct-mapped has none, which is the point.

### What is law here, and what is a choice

**Law** (ledger, charter §7.3, `spec/stars_and_flares.md` §1): that this cache exists, that it is M10K-backed, that it checks tags, that it counts hits and misses, that it fetches misses through the guard, and that a palette upload invalidates the page it replaces.

**A choice, stated because no text in this repository states it:** the **capacity**. The ledger's note says "M10K budget per §7.3; capacity frozen post-Phase-0", but charter §7.3 is a tenancy *list* with no numbers (the same §7.3 mis-citation `design/contracts/RASTER.TILESTORE.md` already records — `spec/memory_rules.md` has seven sections and no §7.3), `spec/memory_rules.md` carries the Phase-2 framebuffer region map and nothing about M10K, and no phase-0 record states a texture-cache size. The defaults below are chosen and parameterised so the first board-fit number replaces them without touching logic:

| parameter | default | consequence |
|---|---:|---|
| `LANES` | 4 | one per bilinear tap (see below); not meant to vary |
| `LINES` | 16 | direct-mapped lines per lane |
| `LINE_BYTES` | 16 | 8 halfword fill beats a line |

Data = 4 × 16 × 16 B = **1 KiB** (4 × 2,048 bits: one M10K per lane, organised 128 × 16, which is the shape the halfword port gives it). Tags = 4 × 16 × (24 tag + 1 valid) = **1,600 flops**.

One KiB is a deliberately modest working set: this machine renders into 16×16 tiles, so the texel footprint in flight is a tile's worth of one surface, and the ledger's own "1 cache access per clock" makes this a bandwidth filter in front of VRAM rather than a texture pool. Growing `LINES` is a parameter edit; growing it *without a board-fit number to point at* would be inventing a budget.

### Why four lanes

A lane is a complete direct-mapped cache — its own tags, its own valid bits, its own data RAM. The lanes are **independent**: a fill in lane 2 cannot evict a line in lane 0, and the same line may legally be resident in two lanes at once (duplication, never incoherence — the cache is read-only and its only writer is its own fill port).

There are four because of the bilinear footprint. TEXTURE.TMU's bilinear sample reads texels (i,j), (i+1,j), (i,j+1), (i+1,j+1), and the ledger requires that to be one request ("1 sample per clock (bilinear = 1 request)"). Four lanes make it one cache **access** too: the TMU drives tap *k* on lane *k*, so a bilinear sample is a single beat rather than four serialised lookups. Because a tap's position within its footprint is fixed, each lane sees a stable parity class of texel coordinates as the sample point walks a triangle, so per-lane direct-mapped tags do not thrash against each other — which is why a quad-banked texture cache is the classical shape and not merely four caches bolted together. A nearest sample enables lane 0 only.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release (`always_ff @(posedge clk or negedge rst_n)`). On reset: every valid bit and tag is cleared, the response stage is invalid, the fill engine is idle, the per-transaction accounting is empty and both counters are 0.

**The data RAM is deliberately NOT reset.** A reset loop over the data array is exactly what stops M10K inference, and no read can reach it while `valid_r` is 0 — an access is only accepted when every enabled lane hits, and a lane hits only on a valid line.

## Input and output packet layouts

| channel | fields | meaning |
|---|---|---|
| miss_addresses (`acc_valid_i` / `acc_ready_o`) | `acc_en_i` 4, `acc_addr_i` 4 × 32, `acc_src_id_i` 16 | up to four independent lookups, resolved together. Lane *k*'s byte address is `acc_addr_i[32k +: 32]` |
| cached_texels (`smp_valid_o` / `smp_ready_i`) | `smp_data_o` 4 × 16, `smp_src_id_o` 16 | one halfword per lane, one cycle after acceptance |
| fill_requests (`fill_valid_o` / `fill_ready_i`) | `fill_addr_o` 32 (line-aligned), `fill_src_id_o` 16 | one line request to MEM.GUARD |
| fill data (`fill_data_valid_i`) | `fill_data_i` 16 | `LINE_BYTES/2` halfword beats, ascending, never refused |
| invalidate (`inv_valid_i`) | `inv_all_i` 1, `inv_addr_i` 32 | see below |
| status | `idle_o` | nothing in the response stage and no fill in flight |
| counters | `cache_hits_o` 32, `cache_misses_o` 32 | saturating |

**Address split** (with the defaults): `off = addr[3:0]`, `index = addr[7:4]`, `tag = addr[31:8]`.

**The halfword port.** A lane returns the 16-bit **little-endian** halfword at `addr & ~1` — never a whole line (4 × 128 bits of output for a consumer that wants at most 4 × 16) and never a single byte (every direct-colour format in charter §15 is 16 bits per texel: RGB565, ARGB1555, ARGB4444). An 8-bpp or 4-bpp consumer takes the byte with `addr[0]` and the nibble with the texel index's LSB; that selection is TEXTURE.TMU's, because the *format* is TEXTURE.TMU's. Little-endian because every 16-bit pixel word in this machine already is (`spec/capture_format.md`: "RGB565 LE").

**The fill is a burst of halfwords, not a line.** One request handshake, then 8 beats of 16 bits in ascending address order. There is one fill outstanding at a time, so the beats need no tag and no reordering and are never refused — hence no ready on the data direction. The halfword shape is not cosmetic: dropping a whole 16-byte line into the RAM in one cycle would need eight write ports, and one write port is what an M10K has.

## Backpressure rules

`ready_valid` on the access and response channels.

`acc_ready_o = (need_c == 0) && !fill_busy_r && (!s1_v_r || smp_ready_i)` — a function of the offered address, of internal state, and of the **other** channel's ready. It is never a function of `acc_valid_i`.

**The miss law: an access is accepted only when it can be served.** There is no miss "response". An access is accepted iff every *enabled* lane already holds its line; otherwise `acc_ready_o` stays low and the block fills the missing lines one at a time. The offered access — which ready/valid requires the master to hold stable — is re-checked combinationally after every fill, so it is accepted the moment the last hole closes. With at most four enabled lanes an access takes at most four fills, which is why `latency: variable` is the honest ledger entry, and why no outstanding-miss queue, no MSHR and no hit-under-miss machinery exists here: **the master is the queue, and it is one deep.**

**There is no fill bypass.** A filled line is written into the RAM and read back out of it on the accepting cycle; fill data never shortcuts to `smp_data_o`. That costs one cycle per fill and removes an entire class of forwarding bug — the RAM is the single source of every texel this block ever emits.

`inv_valid_i` is accepted **unconditionally**, exactly like RASTER.TILESTORE's `clear`.

## The invalidate — a ledger requirement, not a convenience

`spec/stars_and_flares.md` §1 rebuilds each near star's 64-entry CLUT page on the ARM **every frame** and re-uploads it (≤512 B/frame). Nothing in that upload path touches this block, so a resident copy of the old page would paint frame *N+1* with frame *N*'s palette — "never a stale-frame paint", and the ledger names the palette-ordering capture tests as the tripwire.

- `inv_all_i = 1` — every valid bit in every lane, one cycle (the flush a resource-epoch change wants).
- `inv_all_i = 0` — the **line** containing `inv_addr_i`, in every lane (the per-page invalidate; a 512 B palette page is 32 of them).

**In-cycle ordering: the invalidate is applied LAST**, so an invalidate racing a completing fill wins — the same ruling RASTER.EARLYZ makes for `tile_begin`. Invalidating a line a fill is midway through also cancels that fill's **tag write**, so a torn line can never become valid. The visible consequence, pinned by a directed case: the access that fill was serving is still unserved, so the block fetches the line **again** inside the same access — two fills for one lookup.

## Memory ownership

Four halfword-wide RAMs (`LINES × LINE_BYTES/2` entries each = 128 × 16 bits), read synchronously on the accepting cycle and written only by the fill port — the shape an M10K infers from. Tags and valid bits are **flops**, not RAM: `inv_all_i` clears every valid bit in one cycle and all four lanes' tags are compared in the same cycle the access is offered. Same reasoning as RASTER.TILESTORE's present bits.

No VRAM ownership: the block emits `fill_addr_o` and MEM.GUARD decides whether it is legal.

## Q formats and rounding

**None.** This block stores and returns bytes; it performs no arithmetic on their values. The only arithmetic anywhere is address decomposition (pure bit slicing), the fill beat counter, and the two saturating counters. There is no rounding in this block and therefore no rounding law to get wrong.

## Latency (fixed or variable)

**`variable`**, exactly as the ledger declares. An all-hit access responds **one cycle** after acceptance. An access with *m* missing lanes costs *m* fills first, each a request handshake plus 8 data beats at whatever rate the memory supplies them — that is where the variability lives and it is the memory's, not this block's.

## Target throughput

The ledger's "1 cache access per clock", and it is reachable: the response stage is one deep and `acc_ready_o` allows a new acceptance in the cycle the previous response is taken, so a stream of all-hit accesses retires one per clock. Misses stall the port for the duration of their fills, by design.

## Overflow and malformed-input behaviour

- **Every address is legal.** 32 bits, every value decomposes; there is no alignment requirement (an odd address returns the halfword at `addr & ~1`).
- **Every enable mask is legal**, including all-zero: an access with no enabled lane hits trivially, returns stale lane data that the master declared it does not want, and counts nothing.
- **An invalidate at any time is legal**, including mid-fill (defined above) and with work in the response stage.
- **A fill beat arriving with no fill outstanding is ignored** — the beat handler is gated on `fill_busy_r && !fill_req_r`.
- **Both counters saturate** at `0xFFFF_FFFF` per `spec/counters.md` §4; a counter never wraps.
- The block cannot hang on any input: the fill engine's only wait states are `fill_ready_i` and `fill_data_valid_i`, which are the memory's obligations.

## Counters and traces

`cache_hits_o` and `cache_misses_o`, both saturating 32-bit.

**They count the FIRST look, per lane, once per transaction.** Every access is eventually accepted with all its lanes hitting, so counting the tag check *at* acceptance would make `cache_hits` equal the lookup count and the hit **rate** identically 1 — a counter that cannot report the thing it is for. So each enabled lane is accounted on the first cycle its access is offered, as a hit or a miss, and `acct_r` (4 bits) marks the lanes already counted; it is cleared **only at acceptance**.

That bookkeeping must not be conditioned on `acc_valid_i`, and this is worth writing down because two earlier forms of it were wrong: a master is entitled to drop `acc_valid_i` while it waits, and any state cleared by that signal forgets the first look and re-takes it *after* the fills — which reported a hit rate near 1 under exactly that stimulus. `cache_misses` is likewise counted at the first look rather than at fill start, so the pair is one instant's decision and is timing-independent by construction.

Consequences, both asserted: `hits + misses` is the enabled-lane lookup count, and `misses` equals the number of lines fetched exactly.

A transaction runs from an access's first offer to its acceptance. Withdrawing an offered access and substituting a **different** one before acceptance is outside what this accounting models — the substitute inherits the withdrawn access's ledger. No master in this machine does that, and ready/valid does not require it to be supported.

The catalog id and the `frame_tick` shadow latch (`spec/counters.md` §3/§5) are **not** implemented here, exactly as the RASTER blocks deferred theirs; `cache_hits` / `cache_misses` are claimed by three blocks in `design/blocks.yml`, and reconciling multiple producers of one catalog entry belongs with the DEBUG.COUNTERS integration wave.

## Scalar reference function

`zref::TextureCache` (`reference/include/zref/zref_texture.hpp`, `reference/src/zrender/texture.cpp`), with `zref::TextureMemory` as the flat backing store a fill reads from.

Like `zref::TileStore` and unlike `zref::EdgeWalk`, this **is** a second implementation — nothing earlier in this repository maintains a tag array — so it is written in the plainest way that can be checked by eye: four arrays, one `resident()` predicate, one fill loop. It has no response stage, no fill FSM, no halfword beats and no backpressure, which is exactly what makes "RTL == oracle" test those mechanics.

## Directed tests

`tests/texture/texture_cache_directed.cpp` (driver `tests/texture/texture_cache_dev.hpp`) — **134 checks**, every one stepped through the RTL and `zref::TextureCache` with every returned halfword, the whole fill-request sequence and both counters compared.

Cases: reset (the first touch of any line is a miss and exactly one fill); miss-then-hit (the second access issues no fill, and returns bytes *identical* to the miss response — which they would not be if the fill shortcut to the output); the halfword port (all 16 offsets of a line, odd offsets returning the same halfword as their even partner, little-endian against the pool); whole-line coverage (two lines walked cost exactly two fills); **tag collision** (six accesses at one index with three tags thrash to six fills, zero hits, and every refetched line still returns the right bytes — eviction loses residency, never correctness); **lane isolation** (four different tags at the same index in four lanes cost four fills *once* and hit thereafter; the same line in all four lanes is fetched four times, which is legal duplication); the invalidate (line, then the neighbouring line proving it was not a flush, then `inv_all`); **the invalidate/fill race**, hand-driven with the driver's one-shot arm: an invalidate on **every early beat** (the torn-line case the `fill_kill_r` guard exists for — the same access must fetch the line twice), on the **last** beat in both `inv_all` and per-line form, and on a **different line**, which must leave the in-flight fill valid; four missing lanes (one access, four fills, lowest lane first, deterministic order) and a mixed access (two resident, two not); **36 timing patterns** (six ready/valid seeds × three fill latencies × two beat gaps) that must agree byte for byte; and the counters.

## Randomized differential tests

`tests/texture/texture_cache_random.cpp` — deterministic from fixed seeds (the PCG shape of the other random lanes). Three lanes:

**Lane A — hot set.** Addresses drawn from a window a few ways wide with random enable masks, so first-look hits, tag collisions and evictions are common events rather than accidents.

**Lane B — the sampler walk.** A 2×2 footprint stepping across a 64-wide surface, four lanes enabled — what the block is actually for. The lane asserts its own usefulness: the walk must hit far more often than it fetches, or the cache is not a cache.

**Lane C — cold sweep plus the stars §1 upload duty cycle:** wide-spread addresses, then a page invalidate or a full flush, then the same traffic again.

**Lane D — the mid-fill invalidate**, and it is deliberately **not** differential: `zref::TextureCache` has no fill beats, so it cannot express an invalidate landing while a line is on its way in. It checks the RTL law directly instead — an invalidate on any beat *before* the last must kill the torn line, so the same access has to fetch it a second time — over random lines, random beats and random fill timings. It exists because a last-beat-only directed case was green against a mutation that deleted the kill entirely (see Mutation evidence).

The lanes assert their own coverage and fail if any bucket is empty; at the fast setting they fire **113 accesses served with no fill at all, 360 quad accesses missing on all four lanes, 9,589 evictions, 70 invalidate cycles and 60 mid-fill invalidates**. Default 220 + 45 + 70 + 60 batches (CTest `fast`); `--nightly` 3,000 + 600 + 900 + 700. Failing vectors are serialized per charter §29-17.

## Mutation evidence (2026-08-18)

See `design/contracts/TEXTURE.TMU.md` for the shared table — the two blocks' mutations were run as one sweep, one mutation per invocation, each under a harness that asserts the built `.exe`'s SHA-256 changed before believing the result and asserts the reverted tree is green again before the next mutation.

Three of this block's mutations bear on it directly: **a hit reported on a tag mismatch**, **the valid bit ignored** and **an invalidate mid-fill no longer cancelling the tag write** — all three RED in both lanes.

The third one found a real hole and it was fixed rather than noted. Both lanes were GREEN on it at first, because the directed case armed its invalidate on the fill's **last** beat, and on that beat the invalidate clears the valid bit in the same cycle the tag is written — so the `fill_kill_r` guard is invisible there. Every EARLIER beat is the torn-line case the guard exists for. The directed suite now sweeps every early beat, and `texture_cache_random.cpp` grew **lane D**, which arms a mid-fill invalidate at a random beat on a random line under random fill timings. Lane D is deliberately NOT differential and says so at its own definition: `zref::TextureCache` has no fill beats, so it cannot express the race at all; it checks the RTL law directly.

## Formal properties

**None on this block, and that is a statement rather than an omission.** The content here is a tag array and a fill FSM. A bounded model check of "a hit implies the tag matched" would restate the one line of RTL that computes it; the interesting properties — that an access is never accepted with a hole, that a torn line never becomes valid, that the counters are timing-independent — are about *sequences*, and the differential lanes drive them against an oracle over 36 timing patterns and tens of thousands of random accesses, which is a stronger check than a depth-bounded restatement.

What *is* proved formally in this subsystem is the arithmetic core that has real content: `tests/formal/texture_bilerp.sby` on `zhao_texture_bilerp`. Per the charter's own standard, a proof with nothing to cover is worse than no proof.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is 4 × 128 × 16 bits of RAM (1 KiB, four M10K at the defaults) plus ~1,700 flops: 1,600 tag+valid, the fill engine (~40), the response stage (4 × 16 + 16), the accounting (4) and two 32-bit counters. No DSP.

Two things to watch at fit time, neither measured: four parallel tag comparisons feed `acc_ready_o` combinationally from the offered address, which is the block's longest path into a handshake; and `inv_all_i` has a 64-fanout clear. If the tag compare becomes critical, the honest fix is to register the hit vector and pay a cycle on every access, which changes `latency` from 1 to 2 on hits and nothing else.

## Integration capture cases

None. **Not composed with TEXTURE.TMU in RTL**: the ledger registers four TEXTURE blocks and none of them is "the composition", and registering one is a validator-gated ledger edit. The TMU's lane models this block's port with configurable latency instead (the same choice `raster_fragment_dev.hpp` documents for the tile store), and this block is verified against its own oracle here. Verilator-only — no Quartus fit, no capture, never on hardware. Simulated is not synthesized and neither is on-hardware.

## Notes

Deliberately not built in this block, so the next wave knows: no set associativity and no replacement policy (direct-mapped has neither); no MSHR, no hit-under-miss and no more than one outstanding fill; no prefetch; no write path; no coherence; no compression; no `frame_tick` shadow latch or catalog-id binding; no formal property (reasoned above).
