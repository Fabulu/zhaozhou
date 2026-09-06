# Contract — SW.STREAM (Streaming and asset preparation)

> Ledger: `design/blocks.yml` · owner ZH-077 · phase 3 · maturity SPECIFIED
> Reference model: `zref::swstream::WorldStreamer` —
> `reference/include/zref/zref_sw_stream.hpp`
> Tests: `tests/terrain/sw_stream_directed.cpp` (177 checks)
> Fire-test: `tools/sw_stream_firetest.py` (10 perturbations, all shown to fire)

**The ledger cannot hold that reference-model pointer.**
`design/schema/blocks.schema.json` forbids `reference_model`, `counters`,
`budget_group` and `resource_budget` on any block with `kind: software`. So the
pointer lives here and in the ledger's `notes` prose, and the counters below are
this contract's, not the ledger's. A previous lane tripped exactly this; it is
written down so the next one does not.

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

**Extended 2026-09-06** with the reference model, the island-scale test, the
counters, and the four questions that turned out to be genuinely unruled once
the rulings were read back against a working model.

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

It does not move a page from HPS DDR into local SDRAM either — that is
`TERRAIN.PAGELOADER`. **SW.STREAM stages; the loader fetches.** The seam between
them is the 32-byte record's `hps_page_addr` and `expected_page_crc32c`, and the
loader's contract names this file as the record's author.

## The laws it executes, with citations

| law | where it is written |
|---|---|
| the terrain key is `{resource_epoch, island_id, patch_ix, patch_iz}`; two islands may legally overlap in local patch coordinates | ruling **T1** |
| `TERRAIN.PAGE_POOL` at `0x0400_0000`, 1,024 × 21,376 B; every region deny-by-default with state-aware permissions | ruling **T2** / `spec/memory_rules.md` §5b |
| terrain load / writeback / staging is `ZHAO_CLIENT_TERRAIN_BUILD = 6`, **best-effort**, and does not join guaranteed round-robin because a page is late | ruling **T3** |
| HPS owns canonical B and D and never writes them back; **layer F has no canonical HPS mirror** and needs a journal with an ACK barrier | ruling **T4** |
| `TerrainEpoch` @ `0x0220`/16 B, `SubmitTerrainSet` @ `0x0230`/32 B, the 32-byte patch-list record, and the canonical order | ruling **T5** |
| the 256 composed slots are for **live composed** patches only; the degradation ladder; >256 required dynamic after degradation is the **hardware's** frame fault | ruling **T6** |
| working set = both views' current visible set ∪ a one-patch Moore ring ∪ the 30-frame prediction ∪ gameplay-required; **union the views before deduplication** | ruling **T7** |
| ceiling **32 whole pages per frame** ≈ 41 MB/s, provisional, not a board claim | ruling **T7** |
| a half-loaded or CRC-failed page is never rendered; a normal miss draws the declared proxy and is recorded; **do not freeze the old camera frame for ordinary traversal** | ruling **T7** |
| the height mip law is exact nested decimation on the FPGA; **HPS does not implement a second mip law** | ruling **T8** |
| 256 sets × 4 ways; replacement state updates on canonical claim acceptance; **no software "two islands must not collide" restriction exists** | ruling **T9** |
| loader completion carries success/failure **and CRC identity** | ruling **T10** |
| `END_FLUSH` drains, waits for pins, writes back every `dirty_F`, waits for ACKs; `BEGIN` installs a strictly newer nonzero epoch; `ABORT` is reset/fault only and **must record that it discarded** | ruling **T11** |
| the responsibility list, deadline-driven software backpressure, **never expose a half-built page list to CMD.DMA** | ruling **T12** |
| page stride 21,376 B; streams whole; `page_crc32c` over bytes [64, 21320) | `spec/terrain_rules.md` §2, §2.1, §7 |
| what a camera sees, once, for both the streamer and TERRAIN.VISIBLE | `zref::island::visible_set` — `reference/include/zref/zref_island.hpp` |
| an upload's source must lie wholly inside its declared arena, checked in 64 bits so the sum cannot wrap | `zref::mem::upload_source_in_arena` — `reference/include/zref/zref_mem_upload.hpp` |

### The architecture document's OPEN list is closed, and this is where

`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §7 lists ten items as "genuinely
unwritten in the tree". **All ten were ruled on 2026-09-02 and that document has
not been updated.** Anyone reading §7 as current will re-ask questions that have
answers:

| §7 OPEN | answered by |
|---|---|
| 1 residency key law | T1 — `{resource_epoch, island_id, patch_ix, patch_iz}`; the tag gains `island_id` |
| 2 guard map | T2 — the exact bank-2 table |
| 3 memory client | T3 — client 6, `TERRAIN_BUILD`, best-effort |
| 4 writeback vs canonical mirror | T4 — B/D never written back; **F must be**, behind an ACK barrier |
| 5 command ABI | T5 — one `SubmitTerrainSet` for the whole set, not one `DrawProcedural` per patch |
| 6 composed-cache overflow | T6 — the five-step ladder, then a frame fault |
| 7 **prefetch policy** | T7 — working set, look-ahead, and the 32-page ceiling |
| 8 mip derivation side | T8 — the FPGA, after CRC, exact decimation, no second HPS law |
| 9 visible-set collision law | T9 — set-associative replaces direct-map; **no software non-collision rule exists** |
| 10 level teardown | T11 — `END_FLUSH` / `BEGIN` / `ABORT`, not full console reset |

§7 item 7 is the one this block was blocked on. It is not open.

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

**Serialised by hand, little-endian, never `memcpy`'d from a struct.** A C
struct inserts two alignment bytes after `patch_iz` on most ABIs, and the sealed
bytes would then depend on the host that produced them. The sealed list is
capture data (T5); a capture that replays only on the machine that made it is
not capture data. `zref::swstream::encode_record` writes field by field and the
test asserts `bytes == 32 × patch_count` on every frame.

### Canonical order — this IS the determinism

Required before prefetch; smaller `priority` first; view-union key; `island_id`
ascending; `patch_iz` ascending; `patch_ix` ascending; `source_id` ascending.
Two runs of the same frame produce byte-identical list bytes, which is what
makes `list_crc32c` a usable identity rather than a checksum.

**Six of those seven keys are unambiguous. "View-union key" is not** — see
*Open questions* below. The model sorts `view_mask` ascending and says so at the
comparator rather than in a commit message.

### Inbound — counters consumed

Residency, load, writeback and pressure counters from the terrain hardware.
**Consumed to steer the next frame's set, never to mutate a list already
sealed.**

## The working set, and what happens when the camera outruns it

T7's working set, in T7's order:

1. the current visible set **for both views**;
2. the **predicted** visible set 30 frames (0.5 s) ahead from camera velocity;
3. a **one-patch Moore ring** around the current visible set;
4. explicitly **gameplay-required** patches.

**Union the views before deduplication.** The union is the OR of the view masks
and it happens before anything is dropped; deduplicating per view and then
concatenating loses the `DUAL` flag, which is exactly the information saying a
page serves split-screen. The reference model merges by taking the *strongest*
class and OR-ing the masks, and the test asserts that a patch both views want
appears exactly once with both bits set — **and that patches wanted by only one
view still exist**, so a run where everything came back DUAL cannot pass.

**The ring is around the SET, not around the window.** T7 says "a one-patch
Moore ring around *it*", and *it* is the visible set. On a sparse island those
differ: a window's ring includes cells no ground is adjacent to. Ringing the set
keeps prefetch proportional to the ground rather than to the window's area.

**The prediction uses floor, not truncation toward zero.** Truncating makes the
predicted centre lag by up to a patch when travelling in the negative direction
and not at all in the positive one — a working set that depends on which way the
player faces, which is a determinism bug shaped exactly like a rounding
preference.

### The budget counts TRANSFERS, not list entries

T7 states the ceiling and then derives 41 MB/s from it: 32 × 21,376 × 60. That
derivation is only true if each of the 32 is a page **actually moved**. A record
naming an already-resident page moves nothing and therefore cannot consume a
unit of a bandwidth budget. This is read off the ruling, not chosen — and the
fire-test perturbation that makes a resident page spend a unit (P7) drops the
traversal's loads from 1,152 to 340, so the two readings are not equivalent.

### More than 32 pages in one frame — this IS ruled

The question "what happens when the visible set demands more than 32 pages in
one frame?" was checked against every ruling before anything was invented, and
T7 answers it in two halves that have to be read together:

* **The order decides who wins the cut.** "Order: required current, then
  predicted visible, then neighbour ring, canonical ties."
* **The losers are not a fault.** "A normal streaming miss uses the island's
  declared proxy / coarse silhouette / open sky and records the miss. **Do not
  freeze the old camera frame for ordinary traversal** — only hard internal
  overflow/corruption repeats the prior complete frame."

So a camera that outruns the bandwidth renders proxy ground at its leading edge
and counts every patch it did so for. It does **not** fault the frame. T6's
frame fault is a *different* pressure — more than 256 required **dynamic**
patches in the composed cache after legal degradation — and reusing it here
would fault frames the rulings say to render.

The cut is taken **in canonical order, before sealing**, which is what makes it
deterministic and what satisfies T12's "selects proxy/fallback **before**
sealing. Not after."

**Prefetch absorbs the pressure first**, because T12 permits deferring PREFETCH
and forbids mutating a sealed REQUIRED list. Measured over the test's 36-frame
traversal: 1,152 pages loaded, 1,512 required patches deferred to proxy, 3,657
prefetch records deferred, and **not one frame over 32**.

The one thing T7 does *not* answer is the gameplay-required case. See *Open
questions* 1 — it is not smoothed over here.

## Backpressure rules

**Deadline-driven software backpressure** (T12), and the asymmetry is the whole
rule:

* SW.STREAM **may defer `PREFETCH` records**.
* SW.STREAM **may not mutate a sealed `REQUIRED` list**.
* If staging cannot meet the deadline, it selects proxy/fallback **before**
  sealing. Not after.

**It never exposes a half-built page list to `CMD.DMA`.** A list is built,
sealed, CRC'd, and only then submitted.

**The seal is modelled as a refusal with a counter, not as an ordering the
caller is trusted to respect** — the same discipline `zref_fjournal.hpp` gives
the F barrier. `try_mutate_after_seal` returns false and increments
`seal_mutations_refused`.

**A deferred record must be ABSENT from the list, not merely counted.** That
sentence exists because a fire-test perturbation counted a prefetch record as
deferred and sealed it anyway, and **nothing in the suite noticed**: the counter
and the list had diverged while every result-checking assertion stayed green.
The invariant that catches it is now asserted on every frame —

    patch_count == loads_planned + already_resident

— every record is either a page being moved this frame or one already in
memory. A record that is neither is a promise the frame cannot keep.

This supersedes the stub's `Backpressure: none`, which was not a decision.

## Memory ownership

**HPS DDR**: canonical layers B and D; the F-sheet journal; the staging area
holding complete 21,376-byte pages; the sealed list bytes.

**Local SDRAM bank 2 is NOT this block's** — it is the terrain guard map (T2),
written by the loader under `ZHAO_CLIENT_TERRAIN_BUILD = 6` (T3), and every
region is **deny-by-default in MEM.GUARD with state-aware permissions**: a
loader may write only a `LOADING` slot.

**The staging arena is a finite pool of whole-page slots**, and it refuses when
exhausted rather than growing. Its *size* is not ruled anywhere (see *Open
questions* 4), so the model parameterises it, clamps the requested slot count to
what the declared arena can actually hold, and counts `refused_staging_full`. A
model that allocated without a ceiling would make an unruled resource look
infinite; one that hard-coded a guess would make a guess look ruled.

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

The FPGA half of that barrier is `zref::terrain::Streamer`
(`reference/include/zref/zref_fjournal.hpp`) and is **not duplicated here**.
SW.STREAM owns the `Journal`; the slot state machine is the hardware's.

## Q formats and rounding

**None of its own.** SW.STREAM moves bytes and builds lists; it performs no
fixed-point arithmetic that any other block consumes. Page payload formats
belong to `TERRAIN.PATCH`, and the height mip law is `TERRAIN.MIPGEN`'s (T8):

    mip17[i,j] = fine33[2*i, 2*j]    i,j in 0..16
    mip9 [i,j] = fine33[4*i, 4*j]    i,j in 0..8

**No averaging.** Nested decimation keeps shared vertices exact and introduces
no rounding — so there is no rounding mode for this block to match.

The one place arithmetic does happen is the 30-frame prediction: Q10 patches per
frame, arithmetic shift, floor. Stated above and asserted nowhere else.

Integrity is CRC-32C over staged page payloads (`expected_page_crc32c`) and over
the sealed list (`list_crc32c`), both through `zhao_abi::zhao_crc32c` — the
generated one, so the HPS and the fabric fold cannot disagree.

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

Working set (T7) as above. Order: required current, then predicted visible, then
neighbour ring, canonical ties.

## Overflow and malformed-input behaviour

* **Bounds are validated before staging**, not after. A cartridge or resource
  reference outside its declared bounds is rejected and recorded; nothing
  partially validated reaches the staging area, and **no staging slot is
  allocated** for a page that fails the check.
* **A half-loaded or CRC-failed page is never rendered** (T7). It never earns a
  record, so nothing downstream can publish it — that is what makes T7 a
  property of the system rather than a promise made by the stager.
* **Prefetch pressure degrades prefetch**, never the required set. If the
  deadline cannot be met, proxy/fallback is chosen before sealing.
* **Required pressure degrades to proxy, and is counted.** It is not a frame
  fault; see *More than 32 pages in one frame*.
* **More than 256 required dynamic patches after legal degradation is a frame
  fault**, and it is the hardware's (T6): fault the frame, drain, repeat the
  previous complete frame, record the rejected source IDs and keys. SW.STREAM's
  obligation is to avoid producing that list, and to consume the record when it
  happens.
* **A coordinate the residency key cannot express is refused, never wrapped.**
  T1's key is exact, not hashed. `zref::island::Streamer::key_fits` is the bound
  (8 bits island, 12 bits each axis); a coordinate outside it would alias onto
  another patch — a wrong-ground bug that renders perfectly.
* **`ABORT` is legal only for reset and fault recovery.** It may discard dirty
  presentation state and **must record that it did so** (T11).
* **Two islands may legally overlap in local patch coordinates** (T1). There is
  no software "islands must not collide" restriction to enforce, and writing one
  would be inventing a constraint the ruling refuses.

## Counters

*(Contract-level. The ledger schema forbids a `counters` field on a software
block; this list is the authority.)*

**Working set.** `frames`, `candidates_examined`, `candidates_unique`,
`dual_patches`.

**Budget.** `loads_planned`, `already_resident`, `prefetch_deferred`,
`required_deferred`, `gameplay_required_deferred`, `proxy_patches`,
`budget_exhausted_frames`.

**Staging.** `staged_ok`, `staged_reused`, `refused_source_bounds`,
`refused_staging_full`, `staged_incomplete`, `staged_crc_fail`, `refused_key`.

**Seal.** `lists_sealed`, `list_bytes_sealed`, `seal_mutations_refused`.

**Journal.** `f_journalled`, `f_acked`, `f_restored`.

`already_resident` exists for one reason worth saying out loud: it is the second
term of `patch_count == loads_planned + already_resident`, the invariant that
separates "a record was deferred" from "a record was counted as deferred and
shipped anyway". Without it that defect is invisible in every other number this
block reports.

## Scalar reference function

`zref::swstream::WorldStreamer` — `reference/include/zref/zref_sw_stream.hpp`.

**It composes; it barely defines.** This tree has been bitten repeatedly by two
definitions of one law drifting, and `zref::island::visible_set` was extracted
out of `zref::island::Streamer` on purpose so that TERRAIN.VISIBLE's RTL and the
streamer could not disagree about what a camera sees. So:

| what | whose definition |
|---|---|
| what a camera sees | `zref::island::visible_set` |
| which patches exist | `zref::island::Directory` |
| a patch's stable identity, and its bound | `zref::island::Streamer::resource_index` / `::key_fits` |
| is a source inside its declared bounds | `zref::mem::upload_source_in_arena` |
| the F-sheet journal | `zref::terrain::Journal` |
| CRC-32C | `zhao_abi::zhao_crc32c` |

**`zref::island::Streamer` is not superseded.** It answers a different question
— what is resident, against a `residency::Arena` — and this block explicitly
does not choose slots (T9/T10 own that). `WorldStreamer` tracks only what it
*believes* is resident, from completions it was told about, which is exactly the
information the real HPS has.

What is genuinely new, because nothing in the tree had it: the working set, the
page budget, the canonical order and the seal, and the stage-before-seal
barrier.

## Directed tests

`tests/terrain/sw_stream_directed.cpp` — **177 checks**, at island scale.

The island is 125 × 125 patches (8 km at the canonical 2 m pitch) with 5,625
patches of solid ground, against a 32-page budget — more than twenty budgets
deep, and that is *asserted*, so a traversal must outrun the budget rather than
incidentally fitting inside it.

* **The budget under a camera that outruns it.** A radius-6 window is 13 rows
  deep; at three patches per frame the leading edge presents 39 new required
  patches against a ceiling of 32 — a deficit of seven per frame that never
  catches up. No frame exceeds 32; the budget is *reached* (so the ceiling check
  tests a constraint that binds); required pages are deferred on **every** one
  of 36 frames; every deferral is counted as a proxy patch; prefetch absorbs the
  pressure first. The budget is also shown to be a live parameter — at 8
  pages/frame the worst frame loads 8.
* **Two views unioned before deduplication.** A patch both views want appears
  exactly once, flagged `DUAL`; the non-overlapping edges are *not* flagged, so
  an all-DUAL run cannot pass.
* **The canonical order.** Two independently constructed streamers fed the same
  frame seal byte-identical bytes and the same `list_crc32c`; the list has zero
  inversions under the comparator's own law; every REQUIRED record precedes
  every PREFETCH record, and prefetch records are asserted to *exist* so that
  check is not vacuous.
* **Staged complete before publication.** Three patches inside one window are
  sabotaged — one whose cartridge source runs off the end, one that arrives
  short, one whose bytes disagree with the declared CRC. None reaches the sealed
  list; their neighbours stage normally; each is counted under its own verdict;
  every staged address lies inside the arena and no two records share one.
* **The seal is a one-way door**, refused and counted.
* **A dirty page evicted and returned to.** The patch is deformed; the barrier
  refuses `LOADING` before the journal write *and* again before the ACK; the
  camera flies 40 patches away (asserted to have left the working set) and comes
  back: the record carries `HAS_SAVED_F`, the journal still holds the exact
  sheet, and the reload beats the flat bytes the loader just wrote.
* **The unruled case is reported, not decided.** A gameplay-required patch that
  cannot fit the budget raises `unruled_gameplay_starvation` and is counted.

## Randomized differential tests

**Still planned**, and named as absent rather than implied. A randomised camera
traversal over a multi-island world, comparing the sealed list against an
independent model built from this contract's ordering rules — **not from the
reference model**, which would only prove two copies agree.

The property worth the file is the same one the residency lane found: **a dirty
F sheet lost without a journal ACK silently heals terrain the player
destroyed.** No frame is wrong, no single-frame check can see it, and it is
unrecoverable because the data is already overwritten.

## Proof that the suite can fail

`tools/sw_stream_firetest.py`. Ten perturbations, each breaking one law the
model claims to enforce, each rebuilt and run, the header restored from a
pristine copy afterwards and **the restore verified byte-identical rather than
assumed**. Baseline: 177 checks, exit 0.

| perturbation | what fired |
|---|---|
| P1 the 32-page ceiling not enforced | 8 checks; `NO FRAME EXCEEDS T7's 32-PAGE CEILING (expected 0, got 26)`, `and the budget is actually REACHED (expected 32, got 289)` |
| P2 the view union overwrites instead of OR-ing | `the overlap is flagged DUAL (expected 1, got 0)` |
| P3 T5's required-before-prefetch key removed | 4 checks; `every REQUIRED record precedes every PREFETCH record (expected 0, got 86)` |
| P4 a CRC-mismatched page accepted | 3 checks; `NOT ONE of the three pages that failed to stage completely reaches the sealed list (expected 0, got 1)` |
| P5 the seal allows mutation | 2 checks; `a sealed list REFUSES mutation (expected 0, got 1)`, `and the refusal is counted (expected 1, got 0)` |
| P6 dirty eviction never reaches the journal | `SW.STREAM's OWN journal write lands the sheet in the journal (expected 1, got 0)` |
| P7 the budget counts list entries, not transfers | `and prefetch records exist, so the check above is not vacuous (expected 1, got 0)`; loads fall 1,152 → 340 |
| P8 bounds validated after staging | 3 checks; `the out-of-cartridge page is refused on BOUNDS, before staging (expected 1, got 0)` |
| P9 the gameplay-starvation flag never raised | `a gameplay-required patch that cannot fit the page budget raises unruled_gameplay_starvation (expected 1, got 0)` |
| P10 a deferred prefetch record sealed anyway | 72 checks; `every sealed record is either loading now or already resident (expected 32, got 230)`, `no sealed record names a page that was never staged (expected 0, got 111)` |

**Two of these bought real changes rather than confirming the suite.**

**P6 fired nothing on the first run**, and the reason is instructive: the test
asserted the journal's contents only at the end, by which time
`zref::terrain::Streamer::begin_evict` had written the same entry. Deleting
SW.STREAM's own journal write therefore changed nothing. Two writers of one
fact, one of them under test, is not a test of that one. The assertion now runs
*between* the two writes.

**P10 fired nothing either**, and it is the sharper of the pair. Counting a
prefetch record as deferred and then sealing it anyway left every
result-checking assertion green: the list was longer and the counter still said
"deferred". This is CLAUDE.md's 2026-09-05 law in its exact shape — a test that
checks *what* came out cannot see *how many*. The invariant
`patch_count == loads_planned + already_resident` was added because breaking the
model on purpose showed the suite could not see it, and `Frame::already_resident`
was added to the model to make the invariant expressible at all.

A third finding came from the same discipline without a perturbation: the budget
test originally moved the camera one patch per frame at radius 4 and deferred
required pages on **one of sixty frames**. It was passing while exercising a
cold start. 45 required minus a 32-page budget is 13, and 13 was the number
printed — checkable by hand, and checked.

## Integration capture cases

**None. Nothing is integrated and nothing is on hardware.**

Planned: the 8 km traversal the world layer is specified against — fly across
the world, force residency churn, deform patches, leave them, return later, and
verify geometry, scars, breaches, LOD seams and both split-screen views remain
exact.

**Replay does not rerun the visibility walk.** The sealed list bytes are capture
data (T5), so a replay that produces different list bytes has already failed
before any pixel is compared.

No board, no programmed device, no measured bridge bandwidth. The
32-pages-per-frame ceiling above is provisional for exactly that reason.

## Open questions that genuinely need a ruling

Each was checked against T1–T12 and against the specs before being written here.
They are listed rather than guessed because a plausible invented policy is
harder to notice than an open question.

### 1. What happens when a GAMEPLAY-REQUIRED patch cannot fit the budget

**The gap.** T7 lists "explicitly gameplay-required patches" as the fourth
member of the working set and gives them **no rank** and **no overflow
behaviour**. T7's overflow behaviour — draw the declared proxy, record the miss,
do not freeze the frame — is written for "a normal streaming miss". A patch the
player is standing on is not a normal streaming miss: proxy ground under a
player is a player falling through the world, and the console has no rule
against that today.

T6's ladder *does* rank gameplay-required above other live patches, but T6
governs the **256-slot composed-cache** pressure, not the **32-page load**
pressure, and no ruling says the two ladders are the same ladder.

**What the model does.** Ranks gameplay-required at priority 0, above ordinary
required-current, as an interpretation of T6's ladder, exposed as the editable
constant `kPriorityGameplay`. When one is starved it sets
`Frame::unruled_gameplay_starvation` and counts `gameplay_required_deferred`.
**It refuses loudly; it does not choose a behaviour.**

**Recommendation.** Rule that a gameplay-required page miss is a *distinct,
counted, hard* event with a declared response, and pick one of:

* **(a)** it is the one traversal case that may repeat the prior complete frame,
  which T7 otherwise reserves for "hard internal overflow/corruption" — cheap,
  honest, and visibly a hitch;
* **(b)** the game is required to bound player velocity so the case cannot
  arise, and a nonzero counter is a **content bug**, gated in CI;
* **(c)** collision falls back to the island's declared coarse proxy *for
  physics as well as for pixels*, which makes it a constraint on proxy fidelity
  rather than on streaming.

**(b) with (a) as the backstop** is the recommendation: it puts the constraint
where the budget actually is — content — and leaves a defined behaviour for when
content is wrong, without inventing a fourth rendering mode.

### 2. What "view-union key" means in T5's canonical order

**The gap.** T5's sort keys are "required before prefetch; smaller priority
first; **view-union key**; island_id asc; patch_iz asc; patch_ix asc; source_id
asc." Six are exact. The third is a phrase, and the sealed bytes are capture
data — two implementations reading it differently produce different
`list_crc32c` for the same frame, which is a replay failure before any pixel is
compared.

**What the model does.** Sorts `view_mask` ascending: view-0-only (`0b01`), then
view-1-only (`0b10`), then DUAL (`0b11`). That groups by owning view and keeps
DUAL contiguous.

**Recommendation.** Ratify `view_mask` ascending. It is one comparator line
either way; the value of ratifying it is that it stops being a phrase.

### 3. Whether the 32-page budget counts transfers or list entries

**The gap.** T7 says "32 whole pages per frame" and derives 41 MB/s. The
derivation only holds if the 32 are pages actually moved.

**What the model does.** Counts transfers; an already-resident record is free.
Fire-test P7 shows the two readings are not equivalent — loads fell from 1,152
to 340 over the same traversal.

**Recommendation.** Confirm "transfers". It is the reading the ruling's own
arithmetic requires, and it is raised here only because a reader could take the
other one and the difference is a factor of three.

### 4. Four layouts nobody has written

Unchanged from the 2026-09-02 pass, and still unruled:

* the `ISLAND_TABLE` binary layout;
* the sparse page-map encoding;
* the journal's on-disk format;
* **the staging area's size**.

The staging area is the one with teeth now: the model has to allocate whole-page
slots and refuse when full, so an unruled size is a live parameter
(`kStagingSlotsDefault`, clamped to what the declared arena holds) rather than a
missing paragraph. At 21,376 B a page, 512 slots is 10.4 MiB and 1,024 is
20.9 MiB of HPS DDR — a real number somebody has to choose against the rest of
the HPS budget.

## What is not yet established

* **Everything above the reference model.** There is no HPS implementation, no
  `ISLAND_TABLE` parser, no cartridge, and no `CMD.DMA` submission. The model is
  the policy; the block is not built.
* **The four unruled questions above**, one of which — gameplay starvation — can
  drop a player through the world and is not a formatting detail.
* **The staging arena's real occupancy.** The model never frees a slot on its
  own; `release_staging` exists and the caller decides, because "when is a
  staged page cold" is a policy no ruling states. A long traversal with no
  release will exhaust the arena and count `refused_staging_full`, which is the
  honest behaviour and not the shipping one.
* **The randomised differential suite**, named above as planned and not written.
  The directed suite tests the model against the contract's rules; nothing yet
  tests the contract's rules against an independently derived list.
* **Anything about the HPS's own timing.** Every number here is a page count or
  a byte count. There is no measurement of how long the ARM takes to build a
  frame's working set, and the 8 km traversal is the thing that would produce
  one.
* **Board bandwidth.** The 32-pages-per-frame ceiling is the frozen sim
  assumption. `spec/terrain_rules.md`:456-457 forbids freezing the streaming
  figure before ZH-004 reports, and nothing here does.

## Notes

leaf. Software block; wave-1-active. Contract authoritative now.

**The ledger schema forbids `reference_model`, `counters`, `budget_group` and
`resource_budget` on `kind: software`.** The reference-model pointer is
therefore in the ledger's `notes` prose and at the top of this file; the counter
list is the *Counters* section above. This is a schema fact, not an omission,
and it is stated because a previous lane tried to add the fields and was
rejected.

**Two reference models now sit under this block, and neither supersedes the
other.** `zref::island::Streamer` decides what is resident against a residency
arena; `zref::swstream::WorldStreamer` decides what to ask for, in what order,
inside what budget, and hands over sealed bytes. They compose — the second uses
the first's key function and the shared `visible_set` — and the split follows the
block's own exclusion list: SW.STREAM does not choose residency slots.
