# Contract — TERRAIN.WRITEBACK (Dirty-page evacuation: the F sheet to the journal)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_writeback.sv`
> Reference model: `zref::terrain::sheet_writeback` — `reference/include/zref/zref_terrain_page.hpp`
> Tests: `tests/terrain/writeback_rtl_directed.cpp` (+ `tests/terrain/tb_writeback.sv`)

## Purpose

When `TERRAIN.RESIDENCY` evicts a page whose **layer F is dirty**, copy exactly
that 8,192-byte surface sheet out of its `TERRAIN.PAGE_POOL` slot into the HPS
terrain journal, and **hold the slot hostage until the journal acknowledges**.
Step 6 of `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §5 — the mirror image of
`TERRAIN.PAGELOADER`, running the other way, with a barrier the loader does not
have.

`spec/terrain_rules.md` §5 warned that this block should be built last "since its
payload may be 'nothing for B/D'". **That is settled and the payload is not
nothing.** See the next section.

## Why B and D are absent, said rather than left to be wondered about

Ruling **T4** is this block's whole charter and it removes two thirds of what
§2.4 of the architecture document imagined:

> HPS owns canonical layers **B and D** and keeps them current from the same
> deterministic commands. **Do not write B or D back on eviction.**

So the absence of layers B (top scar delta) and D (cell state) from this block's
payload is a **ruling being executed, not an omission**. `SW.CPUCOLL`'s canonical
mirror (`spec/terrain_rules.md`:462-464, "CPU owns canonical scars") is the
reload source for both, and it is current by construction because the same
deterministic command stream that deformed the FPGA's copy deformed it. Writing
them back would create a *second* writer of a structure that already has an
owner — the exact "no shared mutable structure ever has two writers" law
MEM.HPS.BRIDGE states — and would make an eviction able to *corrupt* the
canonical copy rather than merely fail to save it.

T4 then says what does move:

> **Layer F (8 KiB surface sheet) has no canonical HPS mirror.** Therefore:
> track `dirty_F` separately; on dirty eviction or explicit save copy exactly F
> to the HPS terrain journal; **wait for journal acknowledgement before the slot
> may enter LOADING**; reload F from the journal when the page returns.

One layer, 8,192 bytes, behind a barrier. That is the block.

`modified_BD` still exists in `zhao_terrain_residency_v2` and is still counted —
T4 requires all three flags — and this block **never reads it**. A job arrives
because `dirty_F` was set; `modified_BD` is somebody else's evidence.

## Exclusions

It does not decide *which* page is evicted (`TERRAIN.RESIDENCY`'s replacement
ladder, T9), does not free the slot (the directory does, on the ACK this block
delivers), does not load anything (`TERRAIN.PAGELOADER`), does not restore a
saved sheet on the way back in (that is the loader's and the sequencer's, and
`TERRAIN.PAGELOADER.md` already says so under *Restoring a saved F sheet*), does
not interpret one byte of layer F (`SURFACE.STAMP` writes it; nobody here reads
what a tag means), does not maintain the journal's index or its free list
(`SW.STREAM`, T12), and **does not invent a retry policy** — no ruling names
one, so a refused sheet is refused loudly.

Most of all: **it never fabricates an ACK.** Every `wb_*` this block presents to
the directory is caused by a matched acknowledgement from the journal. A block
that could manufacture a barrier release is a block that can silently heal
terrain.

## The laws it executes, with citations

| law | where it is written |
|---|---|
| B/D are never written back; **F must be**, behind an ACK barrier | ruling **T4** |
| "wait for journal acknowledgement before the slot may enter LOADING" | ruling **T4** |
| three flags, not one: `modified_BD` / `dirty_F` / `mips_stale` | ruling **T4** |
| layer F is `64×64 × {tag u8, strength u8}` = 8,192 B, **inside the page** | `spec/terrain_rules.md` §2 layer table; T2 "no separate permanent E/F/H pools" |
| page stride 21,376 B; the 64-B header restates `{island_id, ix, iz}` and "redundancy is a corruption check" | `spec/terrain_rules.md` §2 / §2.1 |
| source region `TERRAIN.PAGE_POOL` `0x0400_0000`, 1,024 × 21,376 B | ruling **T2** / `spec/memory_rules.md` §5b |
| F-sheet writeback is `TERRAIN_BUILD` (client 6), **best-effort/background**, never promoted for lateness | ruling **T3** |
| every local-SDRAM access goes through MEM.GUARD | `spec/memory_rules.md` §5 |
| bursts are 64-B aligned, len 1..64, 64-bit beats, sim profile 16 + 1/beat | `design/contracts/MEM.HPS.BRIDGE.md` |
| acceptance order (align → reachable → arena → region → epoch); **a refusal is not a clamp** | `reference/include/zref/zref_mem_upload.hpp` |
| "no `dirty_F` reuse before the writeback ACK" | ruling **T10** / `design/contracts/TERRAIN.RESIDENCY.md` |
| `END_FLUSH` writes back every `dirty_F` slot and waits for **all** journal ACKs | ruling **T11** |

## The two accesses, established from the sources

This is the question the guard lane deliberately left open, so it is answered
here from the written map rather than assumed.

**The source is in local SDRAM, and it is inside a page.** T2, restated verbatim
in `spec/memory_rules.md` §5b: *"There are no separate permanent E/F/H pools —
those layers live **inside** the 21,376-byte page."* So there is no F pool to
read; the sheet is at a fixed offset within the evicted page's `TERRAIN.PAGE_POOL`
slot, and getting at it is a **MEM.GUARD read by client 6 of the page pool.**

**The destination is in HPS DDR.** T4 names "the HPS terrain journal"; the
journal is SW.STREAM's (T12: "maintain the F-sheet journal"), it is what
acknowledges, and it is what a returning page reloads F from. It is therefore
across MEM.HPS.BRIDGE, as a **write**.

**Neither side is the other way round, and the third candidate is refused.**
T2 also lists `TERRAIN.WRITEBACK_STAGING / journal`, 64 × 8 KiB, at
`0x0578_0000` in bank 2 — a *local* region with "journal" in its name. v1 does
**not** use it and it stays unmapped in MEM.GUARD, deliberately:

* the thing that acknowledges is the HPS, and the HPS cannot read local SDRAM —
  the bridge runs FPGA → HPS DDR, not the reverse;
* a store-and-forward through a local staging slot would cost **two extra guard
  passes over 8 KiB per sheet** and would buy decoupling that the ACK barrier
  immediately gives back: T4 holds the slot until the ACK either way, so the
  page's bytes are not needed sooner;
* a window opened ahead of its writer is a hole with a plan attached
  (`spec/memory_rules.md` §5b's own words).

If a later measurement shows the bridge's latency wants absorbing, the staging
region is where that lands, and it is one more state in this machine rather than
a redesign. Stated so the choice is visible.

### MEM.GUARD MUST CHANGE, AND HERE IS THE NARROWEST FORM

`zhao_mem_guard.sv` today:

    assign terrain_ok = req.write
                      && (addr32 >= ZHAO_TERRAIN_PAGE_POOL_BASE)
                      && (end32  <= ZHAO_TERRAIN_PAGE_POOL_BASE
                                    + ZHAO_TERRAIN_PAGE_POOL_SPAN);

and its own comment names this block as the reason the read was withheld:

> T3 also names F-sheet writeback as TERRAIN.BUILD traffic, and that will one
> day need to READ this pool -- when the block that does it exists, it brings
> its own arm and its own proof. Admitting the read now would admit it for a
> writeback path that has never been written.

**The path now exists.** What it needs is exactly one direction bit on the arm
that is already there — the same constant window, the same single client:

    assign terrain_rd_ok = !req.write
                         && (addr32 >= ZHAO_TERRAIN_PAGE_POOL_BASE)
                         && (end32  <= ZHAO_TERRAIN_PAGE_POOL_BASE
                                       + ZHAO_TERRAIN_PAGE_POOL_SPAN);
    ZHAO_CLIENT_TERRAIN_BUILD: pass_ok = shape_ok && (terrain_ok || terrain_rd_ok);

**It is not made here.** MEM.GUARD is formally proven, its proof was re-run
today, and this block takes the region base and the client identity as
parameters/ports exactly as `TERRAIN.PAGELOADER` did in the pass before its
amendment landed. Until the arm exists, `guard_denied_o` is what this block
reports and every write of a sheet faults as `kSheetIncomplete` — loudly,
counted, and with the slot never released. That is the correct behaviour for an
un-granted client and it is tested.

**Four narrower forms were considered and each is impossible or forbidden:**

1. *A separate, smaller window covering only the F extents.* The F extents are
   `base + slot·21376 + 10694`, and recovering `slot` from an address needs a
   divide by 21,376 — not a power of two — on the guard's verdict path. That is
   the identical obstacle `spec/memory_rules.md` §5b already records for
   state-awareness, in the block whose counter enable was the largest
   negative-slack family in the composed shell.
2. *A read lease naming one slot*, on the `fb_writer` precedent. It needs the
   residency→guard interface that is "not ruled anywhere", and it would add a
   map input to a window whose freedom from map inputs is currently a stated
   property (§5b: "no map input reaches this window, so unlike the framebuffer
   one it is **not frame-scoped**").
3. *A second client id for the reader.* T3 forbids it in as many words: "Client
   ID 5 remains available for a measured split if board evidence proves ENGINE1
   arbitration is the limiter. **Do not spend it pre-emptively.**"
4. *No guard change — route the read through ENGINE1.* ENGINE1 has no bank-2
   window, so this would open bank 2 to a render-geometry client in order to
   avoid opening it wider to the client that already owns it. Strictly worse.

**What the amendment costs the theorem, honestly.** `a1_terrain_wo` becomes a
direction statement rather than a write-only one; `a1_terrain_owner` (no other
client reaches the pool, in either direction) is unchanged and is the property
that matters for ownership; `a1_map` is unchanged because the bounds do not
move. The no-escape argument that carried `GEOM.ASSET_POOL` carries here
verbatim: **a read cannot alter a frame buffer.** A new cover
(`c_forward_terrain_rd`) keeps the arm non-vacuous, and the write arm's own
cover must still be reachable — a merged arm that admitted both directions with
one comparison would pass both covers and lose the ownership statement, which is
why the two arms are written separately above.

**The new residual, and this block's answer to it.** With reads admitted,
client 6 can read *any* page in the pool, so a faulty writeback could journal
**another patch's scars**. Nothing in MEM.GUARD can see that. So the block reads
the evicted page's **64-byte header first** and refuses (`kSheetHeaderIdent`)
before a single journal byte moves if `{island_id, patch_ix, patch_iz}` does not
match the job. That is `spec/terrain_rules.md` §2.1's "redundancy is a
corruption check" spent on exactly the failure the amendment creates, and it
costs one 64-byte read out of 130.

### MEM.HPS.BRIDGE needs one line of contract, and no RTL

`zhao_hps_bridge.sv` **already carries writes**: `req.write`, and a
`wr_valid / wr_data / wr_last` beat channel the client streams after the grant.
Nothing about the bridge RTL changes.

What is missing is the *permission*, which the bridge states in prose rather
than enforcing in logic. `design/contracts/MEM.HPS.BRIDGE.md`, *Memory
ownership*: the FPGA "writes ONLY the words the descriptor law grants: the
`state` word of a FRAME_RING descriptor it owns, the `fpga_read_ptr` of
PCM_RING, and designated trace-arena extents." **Requested amendment: add the
F-sheet journal arena to that list**, as a fourth granted extent, declared by
`cfg_journal_base_i / cfg_journal_bytes_i` and owned by SW.STREAM. The block
enforces its own half at full width (`kSheetOutsideJournal` /
`kSheetUnreachable`) so that the grant is checked *before* the first burst even
without a hardware guard on that side — which is the only mechanism available,
since there is no MEM.GUARD equivalent for HPS DDR.

### A bridge defect found on the way, reported not fixed

`zhao_hps_bridge.sv` declares `output logic [4:0][31:0] hps_bytes` — **five**
per-client accumulators — and indexes it `hps_bytes[busy_client]` with a 3-bit
client id. `ZHAO_CLIENT_TERRAIN_BUILD = 6` is out of range, so an out-of-bounds
write is discarded and **every byte this block and TERRAIN.PAGELOADER move
across the bridge is silently unaccounted** in `hps_ddr_bytes_by_client`. It is
a counter, not a safety path, and MEM.HPS.BRIDGE is not this lane's to edit; it
is recorded here because a §25 budget group that reads zero for a client that
moves 41 MB/s is exactly the broken instrument the charter refuses.

## Input and output packet layouts

**Job in** (`j_*`, ready/valid). Field for field the victim record
`zhao_terrain_residency_v2` hands back on a dirty claim, plus what the journal
needs:

    slot:u11  gen:u8  epoch:u32  island_id:u32  patch_ix:i16  patch_iz:i16
    journal_addr:u64  seq:u32  src_id:u32

`slot/gen/epoch` are the handle the ACK must be able to name again. `seq` is the
ticket the journal echoes. `src_id` is the source id of whatever command caused
the deformation, carried so a refusal is traceable to it.

**Completion out** (`done_*`, ready/valid) — **every job produces exactly one**,
whatever happened:

    slot:u11  gen:u8  epoch:u32  ok:1  verdict:u4  seq:u32  src_id:u32

**Barrier release out** (`wb_*`, ready/valid) — port-compatible with
`zhao_terrain_residency_v2`'s `wb_valid_i / wb_slot_i / wb_gen_i / wb_epoch_i`,
and **emitted only on a matched, good ACK**.

**Journal acknowledgement in** (`ack_*`, ready/valid): `seq:u32`, `ok:1`. In
Verilator the harness is the HPS and answers it; in production it is SW.STREAM's
doorbell.

**Memory ports:** one `zhao_guard_req_t/rsp_t` + `beat_valid/beat_data/beat_last`
guard client (**reads**), one `zhao_hps_burst_req_t/rsp_t` +
`hps_wdata/wvalid/wlast` bridge client (**writes**). Both client identities are
input ports, for the reason PAGELOADER gives: which client a deployment presents
is configuration.

## The sheet is not 64-byte aligned inside the page, and that is the datapath

Summing `spec/terrain_rules.md` §2's layer table in order — header 64, A 2,178,
B 2,178, C 2,178, D 1,024, E 3,072 — puts **layer F at page byte 10,694**,
running to 18,886, and the whole body ends at 21,320 exactly as the table's own
total says. `10,694 = 64 × 167 + 6`.

So the source is **six bytes off a burst boundary**, and every naive plan fails
somewhere: an unaligned 64-B read is refused by the bridge on the far side and
straddles two SDRAM bursts on this one; a journal entry padded out to the
aligned superset is not "exactly F"; and reading byte-at-a-time is 8,192 guard
requests.

**What the block does instead.** It reads the aligned superset —
`CHUNK_START = 10,688`, 129 chunks of 64 B — and realigns by a **constant lane**
as the beats stream:

    LANE = F_OFF % 8 = 6
    out[j] = { src[j+1], src[j] } [ 8*LANE +: 64 ]

which for lane 6 is literally `{src[j+1][47:0], src[j][63:48]}`: a constant
part-select of a 128-bit concatenation, i.e. **wiring**. No barrel shifter, no
DSP, no multiplexer tree. `LANE = 0` degenerates to `out[j] = src[j]`, so an
aligned layout would cost nothing and take one fewer chunk.

129 chunks in, 128 bursts out. The one-chunk difference is real and is where the
off-by-one lives, so it is asserted as an exact count rather than derived: **130
guard requests (1 header + 129 sheet chunks), 128 bridge write bursts, 1,024
write beats.**

**One elaboration guard.** The scheme above assumes the sheet begins in the
*first beat* of its chunk, i.e. `(F_OFF % 64) < 8`. That is true of the v1
layout (`10,694 % 64 = 6`) and a `$fatal` refuses any override that breaks it,
for the same reason PAGELOADER refuses a non-beat-aligned CRC window: a truncated
divide produces a confident wrong answer over a page that is otherwise perfect.

## The ACK barrier

The heart of the block. **A page is not free, and its slot is not reusable,
until the far side has acknowledged.** The directory already enforces its half —
`TERRAIN.RESIDENCY.md`: *"No `dirty_F` reuse before the writeback ACK. This is
T4's barrier and the most important rule in the file"* — and its directed suite
already has *"a dirty victim with a delayed writeback ACK — the slot does not
become resident until the journal acknowledges, however many loader completions
arrive first."* This block is written to be consistent with that case, not to
restate it.

**The ticket table.** `ACK_SLOTS` entries (default 4), each holding
`{slot, gen, epoch, seq, src_id}` and a wait counter. A ticket is allocated only
when the **last** journal beat has retired, and retires only when a matching ACK
arrives. `j_ready_o` is low when the table is full, so eviction pressure
**backpressures**; it never displaces a ticket and never drops a job. T2's
64 × 8 KiB staging region is what bounds the eventual maximum; 4 is a default
that is small enough for the suite to fill on purpose.

**The five failure modes, and where each is answered.**

| failure | what the block does | why not the alternative |
|---|---|---|
| **an ACK that never comes** | the ticket waits, forever if need be. `ack_wait_max_cycles_o` (CYCLES) records the longest wait; `acks_overdue_o` (EVENTS) counts tickets that passed `ACK_DEADLINE_CYCLES` **while still waiting**. The slot is never released. | A timeout that released the slot would be a fabricated ACK with a stopwatch attached, and the scars would silently heal. A timeout that *faulted* the ticket would free the table at the cost of retiring one of 1,024 slots permanently — which may well be right, and is **not ruled**, so it is not invented. The watchdog reports and does not act. |
| **an ACK for a page you did not send** | no ticket matches `seq`: the ACK is consumed (there is nothing to hold it for), counted in `acks_unmatched_o`, and traced. Nothing is presented to the directory. | Forwarding an unmatched ACK is the one action that can release an arbitrary slot; it is the whole reason the table matches on identity rather than counting. |
| **a duplicate `seq` still in flight** | refused **before any byte moves**, as `kSheetSeqInFlight`. | Two live tickets with one `seq` make the match ambiguous, and an ambiguous barrier is not a barrier. Refusing the second is cheap; guessing is not. |
| **an ACK arriving after the epoch changed** | **forwarded anyway**, with the ticket's own `epoch` and `gen` — never the live one — and counted in `acks_after_epoch_o`. | The directory checks every non-claim event against the stored `{epoch, generation}` and rejects a stale one on identity, counted (`TERRAIN.RESIDENCY.md`). That is *its* ledger. Rewriting the epoch here would forge an identity the directory trusts; dropping it here would strand an `EVICT_PENDING` slot that the directory might still have been able to retire. Delivering the truth and letting the owner of the ledger judge it is the only option that does not make a policy decision in the wrong block. |
| **eviction pressure while a writeback is outstanding** | `j_ready_o` falls when the table is full; `jobs_stall_cycles_o` (CYCLES) measures the pressure. The engine keeps draining. | Dropping a job leaks a dirty page; displacing a ticket un-holds a slot whose scars are not yet safe. |

**And a sixth the ruling implies:** `ack_ok = 0` — the journal *refuses* the
sheet. The ticket retires (the table is freed and the job completes) but **no
`wb_*` is emitted**: the slot stays `EVICT_PENDING`. `acks_nak_o` counts it and
the trace names the patch. Retry is not ruled anywhere, so the block refuses
loudly rather than inventing one; T11's `ABORT` ("legal only for reset/fault
recovery, may discard dirty presentation state, and **must record that it did
so**") is the escape hatch that exists, and it is not this block's to pull.

**`done` and `wb` are two different statements** and are two different ports.
`done_*` means *this job is finished, one way or another* — the completion law
PAGELOADER states, because a job that produces no completion strands whatever
was waiting on it. `wb_*` means *the slot may load*. Only the second is a
barrier release, and only the second is gated on the ACK.

## Backpressure rules

Every seam stalls, and every one is exercised — because a sibling block passed
21 checks over every input it had and still dropped answers, since every phase
held the consumer's `ready` high.

* **`j_ready_o`** is high only when the engine is idle *and* a ticket is free
  *and* no immediate completion is pending. One transfer in flight, always.
* **MEM.GUARD answers in two cycles and its two bits are never both high.**
  `rsp.ready` is the level `!fwd_active`; `rsp.ok` pulses the cycle *after* the
  accept. `S_RREQ` waits on `ready`; `S_RVERD`, a separate state, reads
  `ok` / `violation`. Registered in `tools/rtl/check_guard_verdict.py`'s client
  list in the same change that created the block.
* **Read beats** arrive at the SDRAM's pace; the machine counts them and does
  not assume a cadence.
* **The bridge** is a registered accept pulse then a beat stream the client
  sources; `hps_wdata_o` and `hps_wlast_o` are functions of the beat counter
  alone, so a stalled consumer sees a held beat.
* **`wb_ready_i`** may be low indefinitely — the directory serialises all
  mutations through one port and gives the writeback ACK priority 0, but a
  claim already in flight will hold it off. The release is **held**, never
  dropped: a dropped `wb` strands a slot in `EVICT_PENDING` forever, which is
  the exact mirror of the dropped-`fin` defect PAGELOADER guards against.
* **`done_ready_i`** likewise.
* **`ack_ready_o`** is high whenever the block is out of reset. That is not a
  missing stall: an acknowledgement is a fact that already happened on the far
  side, and refusing to hear it does not un-happen it. What an ACK can do —
  release a barrier — is gated by the table, not by the port.
* **Retirement is serialised**: at most one ticket may be retiring, so several
  ACKs landing together queue in the table rather than racing on `wb_*`.

## Memory ownership

Reads `TERRAIN.PAGE_POOL` slot `job.slot` — bytes `[0, 64)` and
`[10,688, 18,944)` of that page — **and nothing else**. Writes exactly 8,192
bytes at `job.journal_addr` inside the arena declared by
`cfg_journal_base_i / cfg_journal_bytes_i` — and nothing else.

**The source address is computed, never accepted.** The job names a slot; the
address is `REGION_BASE + slot × PAGE_BYTES`, formed here, with
`slot >= REGION_SLOTS` refused before the arithmetic runs — so reading outside
the granted region is not a rule this block obeys, it is a state it cannot
reach. `SLOTW` is one bit wider than the pool for PAGELOADER's reason: at
exactly `$clog2(1024) = 10` bits, slot 1024 arrives silently truncated to slot 0
and the block journals a live page's sheet. The multiply is not a multiply —
`PAGE_BYTES` is an elaboration constant, so the scaling is five shifts and four
adders.

**The destination address is accepted and therefore checked**, at full 64 bits,
in `zref::mem::upload_verdict`'s own order.

## Q formats and rounding

None. Addresses, byte counts, a sequence number and counters.

## Overflow and malformed-input behaviour

Verdicts are `zref::terrain::SheetWritebackVerdict`, whose values 0..7 **are**
`zref::mem::UploadVerdict`, checked by `static_assert` — the same borrowing
PAGELOADER does, so the two directions of terrain traffic cannot end up with two
refusal taxonomies.

| verdict | meaning | when |
|---:|---|---|
| 0 | `kSheetOk` | 8,192 bytes journalled, ACK matched, `ok` |
| 1 | `kSheetUnaligned` | `journal_addr` not 64-B aligned |
| 2 | `kSheetZeroLength` | **unreachable**: the length is the constant 8,192 |
| 3 | `kSheetOutsidePool` | `slot >= REGION_SLOTS` |
| 4 | `kSheetEpochStale` | `job.epoch != cfg_epoch_i`, at full 32 bits |
| 5 | *(absent)* | **layer F carries no CRC.** See below. |
| 6 | `kSheetOutsideJournal` | the entry, or its 8,192-byte tail, outside the arena |
| 7 | `kSheetUnreachable` | `journal_addr` has a non-zero upper half |
| 8 | `kSheetHeaderIdent` | the slot holds a valid page **of another patch** |
| 9 | `kSheetIncomplete` | a guard denial or a bridge `err` stopped the transfer |
| 10 | `kSheetSeqInFlight` | `seq` already belongs to an outstanding ticket |
| 11 | `kSheetJournalNak` | the journal acknowledged with `ok = 0` |

**Refused before any access** (1, 3, 4, 6, 7, 10) — `sheets_refused_o`, zero
guard requests, zero bridge bursts. **Faulted after the block touched memory**
(8, 9, 11) — `sheets_faulted_o`. Of these, **8 writes zero journal bytes**
(the identity check happens on the header read, before the sheet is even read)
and that is asserted, not assumed: a corruption check that fires after the
corruption is filed has not checked anything.

**Verdict 8 outranks everything the sheet's contents could say**, for
PAGELOADER's reason inverted: another patch's page is internally perfect, so no
content test can catch it, and reporting a content problem sends the next person
to the wrong place.

**Why there is no verdict 5.** `spec/terrain_rules.md` §2.1 gives the page one
CRC, `page_crc32c`, over `[64, 21320)` — the *whole body at load time*. Layer F
has no CRC of its own, and the page's CRC cannot be recomputed over a sheet that
`SURFACE.STAMP` has since modified. So there is nothing this block can check the
bytes against, and it does not pretend otherwise: the integrity of a journalled
sheet rests on the header identity, on the exact byte count, and on the ACK.
Giving layer F its own CRC word is a **page-format change** and belongs to
`SW.STREAM` and the packer, not here; it is listed under *What is not yet
established*.

## Latency (fixed or variable)

`variable`. Per sheet, non-overlapped:

    1 header read + 129 sheet chunk reads + 128 journal write bursts

At the bench's zero-latency profile that is about **2,600 gpu clocks**; at the
stalled profile the suite also runs (guard hold 5, read latency 16, read gap 2,
bridge latency 7, write gap 3) about **11,000**. Both are printed by the test
rather than derived here, and neither is a board claim —
`spec/terrain_rules.md`:456-457 forbids freezing the streaming figure before
ZH-004 reports, and nothing here does.

The ACK wait is unbounded and is **not** part of this figure — it is the far
side's, and the block's own contribution ends when the last beat retires.

The ACK wait is unbounded and is **not** part of this figure — it is the far
side's, and the block's own contribution ends when the last beat retires.

## Target throughput

One sheet per transfer, up to `ACK_SLOTS` sheets awaiting acknowledgement.
8,192 bytes is **38.3 %** of a page's 21,376, so a frame that evicts dirty pages
at the T7 prefetch ceiling of 32 pages costs at most 262,144 bytes of writeback
against 684,032 bytes of load — real, and on the same best-effort background
client that T3 says must never be promoted for lateness.

## Counters and traces

**Every one of these reaches the bench top and is asserted.** Twelve island
signals were once declared, connected and read by nothing; a tripwire nobody
reads is decoration.

**Events** — `sheets_written_o`, `sheets_refused_o`, `sheets_faulted_o`,
`hdr_ident_fails_o`, `guard_denied_o`, `bridge_errs_o`, `acks_ok_o`,
`acks_nak_o`, `acks_unmatched_o`, `acks_after_epoch_o`, `acks_overdue_o`,
`seq_conflicts_o`.

**Bytes** — `wb_bytes_o` (saturating, `zhao_sat_add32`): journal bytes that
actually retired.

**Levels and cycles, named as such** — `outstanding_hwm_o` (the high-water
*count* of concurrently outstanding tickets), `ack_wait_max_cycles_o` (the
longest single ACK wait, **in cycles**), `jobs_stall_cycles_o` (**cycles** the
job port was offered a job it could not take).

The last three are spelled with their unit in the name on purpose. Two counters
were found in one block this week that counted *cycles* while claiming *events*;
one of them reported the producer's patience — 115 offers over 1,783 cycles — as
if it were a throughput figure.

**Trace**, latched on every fault: `fault_island_o`, `fault_ix_o`,
`fault_iz_o`, `fault_seq_o`, `fault_src_id_o`, `fault_verdict_o` — so a refusal
names the patch, the ticket and the command that dirtied it.

## Scalar reference function

`zref::terrain::sheet_writeback` (`reference/include/zref/zref_terrain_page.hpp`),
with `zref::terrain::sheet_extract`, `zref::terrain::sheet_pre_verdict` and
`zref::terrain::SheetWritebackLedger` as the payload, acceptance and counter
oracles.

**Composed, not authored.** The acceptance law is a call to
`zref::mem::upload_verdict` with the roles the other way round — the guard
region holds the *source* and the arena holds the *destination* — so the order
of refusals is the console's one upload law rather than a second one written for
this direction. The slot address is `zref::terrain::page_vram_addr`, already
there for the loader. The page geometry constants are already there. What is
genuinely new and lives in the same header rather than a new one:

* `kLayerAOff … kLayerHOff` and `kLayerFBytes` — **the layer offset table**,
  which the tree did not have. `spec/terrain_rules.md` §2 gives extents in a
  table and offsets nowhere, so every reader has been summing the column by
  hand. Six `static_assert`s pin the running sum, and the last one pins
  `kLayerHOff + kLayerHBytes == kPageBodyEnd`, so a wrong entry cannot agree
  with itself.
* `sheet_extract` — the byte-exact 8,192 bytes, which is the oracle the
  realignment is compared against.

**The oracle is the VERDICT, the RANGE and the BYTES — not the burst
machinery**, and that split is deliberate. A dropped burst fails loudly the
moment the sheet is reloaded; a sheet extracted from the wrong offset, a sheet
journalled to the wrong entry, or a barrier released without an ACK are all
*silent*, and the last one silently heals terrain that a player broke.

## Directed tests

`tests/terrain/writeback_rtl_directed.cpp` — **294 checks**, whole suite
**1,083,921 gpu clocks**, reported by the test rather than derived here (a
derivation quoted as a measurement is how this tree has been wrong before).

* the golden sheet, unstalled: verdict, `ok`, `seq`, `src_id`, and **all 1,024
  journal words compared against `zref::terrain::sheet_extract`** — not sampled;
  the first and last journal addresses; the bytes on either side of the entry
  untouched;
* **exactly 130 guard requests, 128 bridge bursts and 1,024 write beats** — the
  how-many-times half, which is the only thing that can see a machine doing its
  work twice;
* the same sheet with every stall source engaged (guard hold, read-beat gap,
  bridge latency, `wb_ready` and `done_ready` withheld): byte-identical journal
  entry and the same three exact counts;
* **the alignment boundary at the byte**: the journal must contain page byte
  10,694 as its first byte and 18,885 as its last. Corrupting page bytes 10,693
  and 18,886 must NOT change the entry; corrupting 10,694 and 18,885 must;
* the ACK barrier, one case per row of the table above, each asserting that
  `wb_valid` is low for the whole window in which it must be;
* six pre-transfer refusals, each asserting the verdict, oracle agreement,
  **zero guard requests and zero bridge bursts**, the counter, and the trace;
* a valid page of the **wrong patch** in the slot: `kSheetHeaderIdent`, with
  **zero journal bytes written**;
* the transfer stopping part way: a guard denial and a bridge `err`, each
  faulted, counted, traced, with **no ticket allocated and no `wb`**, and a good
  sheet journalled afterwards (an abort is not a wedge);
* the table filled to `ACK_SLOTS` and `j_ready` observed low, then drained in
  order;
* the real `zhao_mem_guard`, watching, and a second real `zhao_mem_guard` asked
  directly about the read this block needs — **which it refuses today**, and
  that refusal is this contract's evidence for the amendment above, exactly as
  `shadow_ok_seen` staying zero was PAGELOADER's before its window landed.

## Randomized differential tests

Same file. Random slot, island, coordinates, generation, epoch, journal entry,
`seq`, `src_id` and stall profile, with a randomly chosen malformation from
{none, unaligned journal, slot out of pool, stale epoch, outside arena,
unreachable, wrong patch in the slot, guard denial, bridge err, journal NAK,
duplicate seq} and a randomly chosen ACK behaviour {prompt, delayed, reordered,
unmatched-first}. Every draw is compared against `zref::terrain::sheet_writeback`
for verdict, `ok`, whether a `wb` was emitted, and the journal bytes; **the whole
ledger is compared at the end** — a block that answers every draw correctly while
counting the wrong thing passes the per-draw comparison and fails there.

**The draw is from an LCG's HIGH bits and the distribution is measured, not
hoped for.** Low bits of a 32-bit LCG have period 2^k in bit k — bit 0 alternates
— so a selector taken from `rand % 11` is not a selector, it is a short cycle
that will visit two or three malformations and report full coverage. The suite
draws from bits [31:24] and then **asserts the observed mix**: every
malformation seen at least twice, every ACK behaviour seen at least twice, and
both `wb`-emitted and `wb`-withheld outcomes present. A comparison that can be
satisfied by one outcome repeated has not compared anything.

**Source ids propagate for real.** Every job carries a distinct `src_id`, every
completion is checked to carry that id back, and every fault's trace is checked
to name it — including on the pre-transfer refusals, where the id is the only
thing that can tell an operator *which* command dirtied the page that could not
be saved.

## Proof that the suite can fail

Twelve perturbations, each applied to a COPY of the RTL, verilated, compiled,
run, and discarded. (A copy, not the file: the first attempt patched in place
and restored in a `finally`, a run timed out, the kill skipped the `finally`,
and the working tree was left holding deliberately broken RTL — caught only
because it was checked. The perturbation now lands in the scratchpad and the
real file is read-only throughout.)

| perturbation | what fired |
|---|---|
| realignment lane `F_OFF % 8` → `+ 1` | 9 checks; `golden: all 1,024 journal words match the oracle (expected 0, got 1024)`, both sheet edges, 21 random draws |
| the chunk prefetch skipped (129 chunks becomes 128) | 15 checks; `golden: exactly 130 guard requests (expected 130, got 129)`, `exactly 1,040 read beats (expected 1040, got 1032)`, `the last read is the 129th sheet chunk (expected 67127744, got 67127680)` |
| an ACK matches ANY outstanding ticket, not its own sequence | 10 checks — see below |
| a NAK releases the barrier anyway | 14 checks; `nak: NO barrier released -- the scars are not safe (expected 1, got 0)`, `nak: verdict kSheetJournalNak (expected 11, got -1)`, retirement order, 262 random draws |
| `CHECK_HEADER_IDENT` → 0 | 14 checks; `ident: a valid page of the WRONG patch is refused (expected 8, got -1)`, `ident: ZERO journal bytes written (expected 0, got 1024)` |
| the identity refusal issues the read it is refusing | 1 check; `ident: EXACTLY ONE guard request -- the header, and nothing else (expected 1, got 2)` |
| `hps_wlast_o` one beat early | 2 checks; `wlast is exactly the 8th beat of every burst (expected 0, got 256)` |
| `REGION_BASE` dropped from the source address | 2 checks; `the first read is the page header (expected 67108864, got 0)` |
| refusal order: the epoch tested first | 2 checks; `all wrong at once -- the slot first (expected 3, got 4)` |
| `acks_overdue_o` counts cycles, not the event | 1 check; `overdue: counted ONCE -- an event, not a cycle count (expected 1, got 201)` |
| `R_WB` returns to idle without `wb_ready` | 2 checks; `golden: wb held stable for 40 stalled cycles (expected 40, got -1)` |
| `R_DONE` returns to idle without `done_ready` | 2 checks; `golden: done held stable for 30 stalled cycles (expected 30, got -1)` |
| the duplicate-sequence check removed | 10 checks; `seq: ZERO guard requests for the duplicate (expected 0, got 130)`, `ledger: seq_conflicts (expected 10, got 0)` |
| `S_RREQ` and `S_RVERD` merged into one arm | 14+ checks; the machine wedges at 2 guard requests of 130 — **and** the static gate, below |

**Three of these are worth more than the others.**

**The ACK-matching one exposed a hole in this suite and it was closed.** With
matching reduced to "any outstanding ticket", the two directed unmatched-ACK
cases still PASSED, because both ran with an **empty ticket table** — there was
nothing to mis-match. The failure surfaced only in the random phase and the
ledger, whose loudest line was `random: every draw agreed with the oracle
(expected 0, got 230)` — a number nobody would read as *an acknowledgement for
one page released a different page's slot*. Section 7a′ now holds a real ticket
while a stranger's ACK arrives, and says so in one line.

**The merged-guard-arm one exposed a hole in `check_guard_verdict.py` — the
gate that exists for exactly this defect — and it read CLEAN.** Its depth walk
counted `begin`/`end` from column 0 of the arm's opening line, and the
commonest arm shape in this tree is

    end else if (guard_rsp_i.ready) begin

whose LEADING `end` closes the *previous* arm and cancelled this arm's `begin`.
Depth went to zero on the opening line, the walker stopped there, and the body
holding the `.ok` was never read. The tool has been shown to fire only on a
shape whose opening line carries no `end`. Fixed (the walk now starts at the
`ready` match on the opening line), a second self-test example in the missed
shape added, all ten clients re-checked clean, and the fixed gate then reported

    zhao_terrain_writeback.sv:900 tests .ok in the SAME arm as .ready

on the same perturbation. **This is the broken-instrument law exactly: the
defect made the answer look better, and nobody audits good news.**

**The `REGION_BASE` one fires only two checks, and that is a limit worth
stating.** The journal entry still came out byte-correct, because the bench's
played memory maps `(addr - window_base) >> 3` and the underflow wrapped back
into the same image. Only the two ADDRESS checks saw it. On the real machine
MEM.GUARD would refuse every one of those reads — which is why
`guard_denied_o` is a fault counter and not a steady state — but this bench
cannot show that while the guard has no read arm at all.

## Formal properties

None yet. Two are worth an SBY and both are structural, therefore cheap:

* **containment** — no guard request this block emits lies outside
  `[REGION_BASE + slot·PAGE_BYTES, + PAGE_BYTES)`;
* **the barrier** — `wb_valid_o` is never raised without a matching ACK having
  been accepted for a ticket with that exact `{slot, gen, epoch}`. That is the
  property the whole block exists to have, and it is a two-signal implication.

## Integration capture cases

None. The block is not in the shell; `TERRAIN.SEQ` (step 5) is its job source
and does not exist yet, and the guard arm above is not enacted. Composition is a
pass, not a question.

## Synthesis / resource ceiling

Unfitted. Budget group `geometry_mantle`. Expected cost is two 64-byte staging
buffers (16 × 64 b), a 4-entry ticket table, a handful of counters and a
twelve-state machine; the realignment is a constant part-select and the address
scaling is shifts and adders by construction, so the acceptance question at fit
time is **zero DSP**, not Fmax.

## What is not yet established

* **The guard read arm.** Ruled by T3/T4 in substance, not enacted. The narrowest
  form is written out above; making it is MEM.GUARD's lane and its proof's.
* **The journal arena grant** in `MEM.HPS.BRIDGE.md`'s granted-writes list.
* **Layer F has no CRC**, so a sheet corrupted *in local SDRAM* between the stamp
  and the eviction is journalled as-is. Giving F its own checkword is a
  page-format change (`SW.STREAM` + the packer), and inventing one here would put
  a second definition of a page in the tree.
* **What happens to a slot whose ACK never arrives, or whose journal NAKs.** The
  block reports and waits. Whether such a slot is retired, retried, or discarded
  by `ABORT` is unruled, and T11's `ABORT` is the only mechanism that exists.
* **`ACK_SLOTS` = 4** is a default, not a measurement. T2's 64 × 8 KiB staging
  region is the eventual ceiling and nothing has measured the pressure yet.
* **Overlap.** v1 reads a chunk, then writes it. Overlapping roughly halves the
  per-sheet cost and needs a second buffer; nothing measured says it is required.
* **`END_FLUSH`.** T11 requires a drain that writes back *every* `dirty_F` slot
  and waits for all ACKs. The enumeration is the directory's and the sequencer's
  — this block is the evacuation, not the sweep — but the `outstanding_hwm_o` and
  ticket-empty condition it exposes are what "wait for all journal ACKs" will be
  written against.
