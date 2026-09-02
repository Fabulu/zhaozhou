# Contract — SW.STREAM (Streaming and asset preparation)

> Ledger: `design/blocks.yml` · owner ZH-077 · phase 3 · maturity SPECIFIED

## Purpose and exclusions

SW.STREAM is the **HPS-side** half of the terrain world layer. It decides which
ground exists in local SDRAM, stages it there, and hands the hardware a sealed,
deterministic list. It runs on the ARM; it is not a fabric block, and every
heading below that names a packet is a packet crossing the HPS→FPGA boundary
rather than a fabric interface.

**Filled 2026-09-02 from ruling T12, which named this file as the last real
stub.** Before that it carried five `TODO` sections while an audit of the
repository reported "zero contracts are unwritten by accident" — which was
false, and false about the one contract sitting on the critical path of the
8 km world. Everything below is transcribed from rulings T1–T12; where a ruling
does not decide something, this file says so rather than inventing it.

**Responsibilities (T12), exactly:**

* parse `ISLAND_TABLE` and the sparse page maps;
* maintain the HPS-canonical layers **B and D**;
* maintain the **F-sheet journal**;
* build the deterministic current / predicted / prefetch set;
* stage **complete** 21,376-byte pages in HPS DDR;
* validate cartridge and resource bounds **before** staging;
* emit `TerrainEpoch` and `SubmitTerrainSet`;
* consume residency / load / writeback / pressure counters;
* preserve the sealed list bytes for capture and replay.

**Exclusions.** It does not generate mips — that is `TERRAIN.MIPGEN` on the
FPGA, and T8 is explicit that **HPS does not implement a second mip law**. It
does not choose residency slots (T9/T10 own that). It does not write layers B
or D back on eviction (T4). It does not run during replay: **the sealed list is
capture data, and replay does not rerun the HPS visibility walk** (T5).

## Input and output packet layouts

### Outbound — `TerrainEpoch` @ `0x0220`, 16 bytes

    epoch:u32
    op:u8            BEGIN = 0, END_FLUSH = 1, ABORT = 2
    flags:u8
    reserved:u16
    island_table_handle:u32
    source_id:u32

Opcodes are **confirmed by the ZIDL generator before commit**, not asserted
here.

### Outbound — `SubmitTerrainSet` @ `0x0230`, 32 bytes

    resource_epoch:u32
    list_offset:u32
    list_bytes:u32
    list_crc32c:u32
    patch_count:u16
    view_mask:u8
    flags:u8
    sequence:u32
    reserved0:u32
    reserved1:u32

**One `SubmitTerrainSet` covers the whole required + prefetch set.** Not one
`DrawProcedural` per patch, and not an overload of an existing terrain field
command (T5).

### The patch-list record, 32 bytes

    island_id:u32
    patch_ix:i16
    patch_iz:i16
    hps_page_addr:u64
    expected_page_crc32c:u32
    flags:u16        REQUIRED, PREFETCH, DYNAMIC, DUAL, HAS_SAVED_F
    view_mask:u8
    priority:u8
    source_id:u32
    reserved:u32

### Canonical order — this IS the determinism

Required before prefetch; smaller `priority` first; view-union key; `island_id`
ascending; `patch_iz` ascending; `patch_ix` ascending; `source_id` ascending.
Two runs of the same frame produce byte-identical list bytes, which is what
makes `list_crc32c` a usable identity rather than a checksum.

### Inbound — counters consumed

Residency, load, writeback and pressure counters from the terrain hardware.
**Consumed to steer the next frame's set, never to mutate a list already
sealed.**

## Backpressure rules

**Deadline-driven software backpressure** (T12), and the asymmetry is the whole
rule:

* SW.STREAM **may defer `PREFETCH` records**.
* SW.STREAM **may not mutate a sealed `REQUIRED` list**.
* If staging cannot meet the deadline, it selects proxy/fallback **before**
  sealing. Not after.

**It never exposes a half-built page list to `CMD.DMA`.** A list is built,
sealed, CRC'd, and only then submitted.

This supersedes the stub's `Backpressure: none`, which was not a decision.

## Memory ownership

**HPS DDR**: canonical layers B and D; the F-sheet journal; the staging area
holding complete 21,376-byte pages; the sealed list bytes.

**Local SDRAM bank 2 is NOT this block's** — it is the terrain guard map (T2),
written by the loader under `ZHAO_CLIENT_TERRAIN_BUILD = 6` (T3), and every
region is **deny-by-default in MEM.GUARD with state-aware permissions**: a
loader may write only a `LOADING` slot.

**Layer F has no canonical HPS mirror** (T4). That is why the journal exists,
and why the ordering below is a barrier and not an optimisation:

    dirty eviction or explicit save
      -> copy exactly F to the HPS terrain journal
      -> WAIT for journal acknowledgement
      -> only then may the slot enter LOADING
      -> reload F from the journal when the page returns

B and D are **never written back on eviction**: HPS keeps them current from the
same deterministic commands, so the FPGA copy is a cache of something the HPS
can always reproduce. F is not, and losing it loses ground the player destroyed.

## Q formats and rounding

**None of its own.** SW.STREAM moves bytes and builds lists; it performs no
fixed-point arithmetic that any other block consumes. Page payload formats
belong to `TERRAIN.PATCH`, and the height mip law is `TERRAIN.MIPGEN`'s (T8):

    mip17[i,j] = fine33[2*i, 2*j]    i,j in 0..16
    mip9 [i,j] = fine33[4*i, 4*j]    i,j in 0..8

**No averaging.** Nested decimation keeps shared vertices exact and introduces
no rounding — so there is no rounding mode for this block to match.

Integrity is CRC-32C over staged page payloads (`expected_page_crc32c`) and over
the sealed list (`list_crc32c`).

## Latency (fixed or variable)

`variable`, and **deadline-driven rather than bounded**. A page load is HPS DDR
→ bridge → local SDRAM under a best-effort client that **does not join
guaranteed round-robin merely because a page is late** (T3).

The consequence is a hardware rule, not a software one: when a page is not ready
the renderer uses the island's **declared proxy / coarse silhouette / open
sky** and records the miss. **It does not freeze the old camera frame for
ordinary traversal** (T7).

## Target throughput

**Ceiling: 32 whole pages per frame.** At 21,376 B and 60 Hz that is ≈ 41 MB/s.

**Provisional, and not a board claim.** Board counters may reduce it
immediately; raising it requires sustained measured bridge + SDRAM evidence.

Working set (T7): the current visible set **for both views**; a one-patch Moore
ring around it; the predicted visible set **30 frames (0.5 s) ahead** from
camera velocity; and explicitly gameplay-required patches. **Union the views
before deduplication.** Order: required current, then predicted visible, then
neighbour ring, canonical ties.

## Overflow and malformed-input behaviour

* **Bounds are validated before staging**, not after. A cartridge or resource
  reference outside its declared bounds is rejected and recorded; nothing
  partially validated reaches the staging area.
* **A half-loaded or CRC-failed page is never rendered** (T7). The page stays
  out of the resident set and the miss is recorded.
* **Prefetch pressure degrades prefetch**, never the required set. If the
  deadline cannot be met, proxy/fallback is chosen before sealing.
* **More than 256 required dynamic patches after legal degradation is a frame
  fault**, and it is the hardware's (T6): fault the frame, drain, repeat the
  previous complete frame, record the rejected source IDs and keys. SW.STREAM's
  obligation is to avoid producing that list, and to consume the record when it
  happens.
* **`ABORT` is legal only for reset and fault recovery.** It may discard dirty
  presentation state and **must record that it did so** (T11).
* **Two islands may legally overlap in local patch coordinates** (T1). There is
  no software "islands must not collide" restriction to enforce, and writing one
  would be inventing a constraint the ruling refuses.

## Directed tests

Planned. Each names the rule it exists for rather than the function it calls:

* `sw_stream_canonical_order` — the same visible set, shuffled at input, seals
  byte-identical list bytes and the same `list_crc32c`.
* `sw_stream_seal_is_immutable` — a REQUIRED list, once sealed, is not mutated
  by late pressure counters; a PREFETCH record is deferred instead.
* `sw_stream_f_journal_barrier` — a slot with `dirty_F` cannot enter LOADING
  before the journal ACK, and F reloads exactly on return.
* `sw_stream_bounds_before_staging` — an out-of-bounds resource reference never
  reaches the staging area.
* `sw_stream_epoch_teardown` — `END_FLUSH` drains, waits for pins to reach zero,
  writes back every `dirty_F`, waits for ACKs, then ACKs; `BEGIN` installs a
  strictly newer nonzero epoch with no resident hit from the older one.
* `sw_stream_overlapping_islands` — two islands at the same local coordinates
  both stage and both stay distinguishable by `island_id`.

## Randomized differential tests

Planned: a randomised camera traversal over a multi-island world, comparing the
sealed list against an independent model built from the contract's ordering
rules — **not from this block's implementation**, which would only prove two
copies agree.

The property worth the file is the same one the residency lane found: **a dirty
F sheet lost without a journal ACK silently heals terrain the player
destroyed.** No frame is wrong, no single-frame check can see it, and it is
unrecoverable because the data is already overwritten.

## Integration capture cases

Planned: the 8 km traversal the world layer is specified against — fly across
the world, force residency churn, deform patches, leave them, return later, and
verify geometry, scars, breaches, LOD seams and both split-screen views remain
exact.

**Replay does not rerun the visibility walk.** The sealed list bytes are capture
data (T5), so a replay that produces different list bytes has already failed
before any pixel is compared.

**Nothing here is on hardware.** No board, no programmed device, no measured
bridge bandwidth. The 32-pages-per-frame ceiling above is provisional for
exactly that reason.

## Notes

leaf. Software block; wave-1-active. Contract authoritative now; C++ artifacts
named above are the shape of the evidence to come, and no maturity advance
happens without that evidence committed (rules V2/V3).

**Not ruled anywhere, and therefore not decided here:** the `ISLAND_TABLE`
binary layout, the sparse page-map encoding, the journal's on-disk format, and
the staging area's size. Each needs a ruling before this block advances past
SPECIFIED. They are listed rather than guessed because a plausible invented
layout is harder to notice than an empty heading.
