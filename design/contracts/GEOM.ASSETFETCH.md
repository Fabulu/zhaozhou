# Contract — GEOM.ASSETFETCH (the meshlet asset reader)

> Ledger: `design/blocks.yml` · gpu clock · maturity **SPECIFIED**
> RTL: *not yet written*
> Reference: **`zref::AssetFetch`** (`reference/include/zref/zref_assetfetch.hpp`)

## Why it exists

Docket **D22** says nineteen of twenty geometry blocks are not in the console,
and then says the honest form of that fact:

> *composing the front end needs **one asset fetcher serving three consumers** —
> descriptors, the `u8` index stream, and vertex records — and that fetcher
> needs a region `MEM.GUARD` does not yet have.*

The region landed 2026-09-04 (`spec/memory_rules.md` §5f, `ZHAO_GEOM_ASSET_BASE`).
**This block is the other half.** Until it exists, `GEOM.ASSEMBLE`'s `ix_req_o`
and `GEOM.VDECODE`'s `v_bytes_i` are ports with nothing on the other side, and
every block behind them is unreachable no matter how well it is verified.

## What it is NOT

* **Not a cache.** See below — the sizes make a cache the wrong shape.
* **Not a descriptor reader.** `GEOM.MESHFETCH` already owns a
  `zhao_guard_req_t` port and reads its own 64-byte descriptor. Routing it
  through here would re-implement a verified block to save one arbiter input.
  The "three consumers" of D22 are three *streams*; two of them are ours.
* **Not a decoder.** 32 bytes go to `GEOM.VDECODE` exactly as they lay in
  memory. This block never interprets a vertex.
* **Not an index validator.** `GEOM.ASSEMBLE` already refuses a triplet that
  addresses past `vertex_count` and counts it (`refused_index_o`). A second
  opinion here would be a second law.

## THE SIZES DECIDE THE ARCHITECTURE, AND THEY SAY "BUFFER", NOT "CACHE"

The frozen ruling limits (`GEOM.MESHFETCH.md`: `vertex_count ≤ 64`,
`triangle_count ≤ 126`) bound a meshlet's whole asset footprint:

| stream | record | worst case | bytes | 64-B lines |
|---|---|---|---|---|
| `u8` indices | 3 B / triangle | 126 triangles | **378** | 6 |
| vertex records | 32 B / vertex | 64 vertices | **2,048** | 32 |
| | | **total** | **2,426** | **38** |

A whole meshlet is **38 lines**. So the block reads the entire footprint into a
private buffer and then serves both consumers out of it. That choice is not
about simplicity, it is about three specific properties a cache would not have:

1. **The guard traffic becomes ONE SEQUENTIAL BURST per meshlet.** Banks come
   from address bits `[26:25]` and W2.7 measured ~82 of 192 Duo lines starved
   from row thrash; a strided cache miss stream re-opens rows, a linear
   prefetch keeps one open. **This is the W2.7 lesson applied at a third site.**
2. **`GEOM.ASSEMBLE` and `GEOM.VDECODE` never see a miss.** `ix_req_o` is a
   combinational request with no ready signal at all — the port has no way to
   express "wait". A cache behind it would have to stall a port that cannot be
   stalled. **The buffer is not a performance choice; the consumer's interface
   requires it.**
3. **Determinism.** Same meshlet, same beats, same order, every run — so the
   differential test is exact rather than statistical.

**Double-buffered**: meshlet *N+1* is fetched while *N* is served, so the
prefetch latency is paid once rather than per meshlet. Cost is ~4.8 KB of block
memory (≈5 M10K).

**THE LIMITS ARE KNOBS.** `MAX_VERTICES`, `MAX_TRIANGLES` and the buffer depth
are named parameters. If a later ruling raises `vertex_count`, this block gets
bigger — it does not silently truncate. A footprint that does not fit is
**refused and counted**, never wrapped.

## The law

    index triplet n:   3 bytes at  asset_base + index_offset  + 3*n
    vertex record v:  32 bytes at  asset_base + vertex_offset + 32*v

Both offsets come from `GEOM.MESHFETCH`'s result record and are **byte offsets
into the asset pool**, not absolute addresses. The absolute address is
`ZHAO_GEOM_ASSET_BASE + offset`, formed here, so that no upstream block holds an
absolute VRAM address and a pool move stays a one-constant edit.

**The local `u8` index becomes a global vertex id by `vertex_offset + local`,
and that is `GEOM.ASSEMBLE`'s arithmetic, not ours.** We serve bytes.

## Alignment — an existing ruling ENFORCED, not a new one

    vertex_offset % 32 == 0        index_offset % 8 == 0

`GEOM.VDECODE`'s contract already says vertex records are *"32 bytes per vertex,
**naturally aligned**"*. This is that sentence checked rather than hoped for,
and the reason it is checked here is that **the buffer layout depends on it**:

* an **unaligned** 32-byte record spans five 64-bit words, so serving one needs
  a **320-to-256 funnel shifter, per vertex**;
* **aligned**, a record is exactly four consecutive words and the shifter does
  not exist. The pool base is 64-byte aligned, so a record sits at byte 0 or
  byte 32 of a line and never at an odd place.

A triplet still straddles — 3 bytes at byte `3n` cannot be helped — so that path
reads two words and selects three bytes from sixteen, which is cheap.

**The asset builder pays nothing for this.** Padding a stream to 32 bytes is
free at authoring time; the shifter is not free in silicon.

**A misaligned offset is refused even when its stream is never read.** A meshlet
with no triangles still carries an `index_offset`, and admitting a malformed one
because it happens to be unused would let a corrupt descriptor through on a
technicality.

## Refusals, each counted, none silent

| condition | why |
|---|---|
| `vertex_offset % 32` or `index_offset % 8` | malformed asset — see above |
| `vertex_count > MAX_VERTICES` | footprint exceeds the buffer |
| `triangle_count > MAX_TRIANGLES` | ditto |
| any beat's guard request denied | the pool bounds are the guard's to enforce |
| footprint crosses the pool's end | refused **before** the first beat |

A refused meshlet emits **no triangles** — it does not emit the part that fit.
Partial geometry is worse than absent geometry, because it looks like a
modelling error rather than a fault.

## The release handshake, and why it is explicit

The buffer is handed downstream with `s_valid` / `s_ready`, and returned with a
`release` pulse. **Release is EXPLICIT rather than inferred** from "all vertices
streamed and the last triplet asked for", because the two consumers finish
independently — and a buffer released on a guess is a buffer overwritten under
a reader.

**Early release is LEGAL and the block must survive it.** `GEOM.ASSEMBLE` can
refuse every triplet in a meshlet (`refused_index_o`) and want nothing further,
while vertex records are still queued. So release tears down the vertex stream
and any pending index-read state along with the buffer.

The first RTL did not, and the failure is worth naming: `v_valid` stayed
asserted into the next meshlet with the previous meshlet's record still on the
wires. **Stale valid is the worst kind of wrong, because the consumer has no way
to tell it from a fresh one** — every other bug in this block produces bytes
that can be checked against an address.
ENFORCED-BY: `tests/geometry/assetfetch_rtl_directed.cpp` (early release)

## Backpressure

Ready/valid on the meshlet input. `GEOM.ASSEMBLE` holds `ix_req` throughout its
fetch state, so that level is one request **episode**, not a request every cycle.
ASSETFETCH captures the triplet index once, issues one synchronous two-copy RAM
read, returns exactly one `ix_valid` pulse, and will not accept another episode
until `ix_req` deasserts. `v_*` is ready/valid because `GEOM.VDECODE` has a
`v_ready_o`.

ENFORCED-BY: `tests/geometry/assetfetch_rtl_directed.cpp` (held index request,
post-accept live-index poison, deassert/rearm)

## Counters

`meshlets_fetched`, `beats_read`, `guard_denied`, `refused_footprint`,
`prefetch_stall_cycles` — the last one is the evidence for whether double
buffering is enough, rather than an assumption that it is.

## Open, and deliberately not decided here

* **The pool's internal layout.** §5f left it open on purpose; this block takes
  offsets from a descriptor and does not care how the pool is carved.
* **Whether PARAMBUF and assets need distinct local-arbiter priorities.** That
  wants a measurement, and the `prefetch_stall_cycles` counter is what will
  produce it.
* **Whether 22 MiB is the right pool size.** Wants a real asset set.

## Scalar reference function

`zref::assetfetch::plan` (`reference/include/zref/zref_assetfetch.hpp`), with
`zref::assetfetch::triplet` and `zref::assetfetch::vertex_record` serving the
two consumers and `zref::assetfetch::lines_covering` counting beats.

`plan` is the one the ledger cites because it is the one that DECIDES: the
admission test and the refusal taxonomy are the block's own law, while the two
serving functions are address arithmetic over an already-admitted plan.

## Directed tests

`tests/geometry/assetfetch_directed.cpp` — **20 checks, passing.** Edges rather
than a sweep, because `plan()` is short enough that a random sweep would pass on
the first attempt and prove nothing: 64 vs 65 vertices, 126 vs 127 triangles, a
footprint ending exactly on the pool's last byte, an offset that wraps 32 bits,
a 64-byte block starting mid-line so the beat count is not `bytes/64`, and the
empty meshlet, which is legal and must not be refused.

**One check failed on its first run against a correct oracle**, and the fixture
was at fault: the pool pattern was `i*31+7`, which is constant modulo 256 at
every 256-aligned offset, so the two streams at 1024 and 8192 held the same byte
and "the streams are read from distinct addresses" could not see a difference.
The high byte is now in the mix. **A pattern that cannot distinguish positions
cannot prove positions are distinct.**
