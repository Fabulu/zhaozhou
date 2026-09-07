# Contract — TERRAIN.PAGELOADER (Whole-page prefetch, HPS DDR → local SDRAM)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_pageloader.sv`
> Reference model: `zref::terrain::page_load` — `reference/include/zref/zref_terrain_page.hpp`
> Tests: `tests/terrain/pageloader_rtl_directed.cpp` (+ `tests/terrain/tb_pageloader.sv`)

## Purpose

Move **one whole 21,376-byte terrain page** from the HPS staging arena into one
slot of `TERRAIN.PAGE_POOL`, verify it, and tell `TERRAIN.RESIDENCY` exactly
what happened. Step 3 of `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §5 — the
third of the three items that document calls "immediately buildable, every law
they need is already written".

## Exclusions

It does not decide residency, does not publish a page, does not evict, does not
generate mips (`TERRAIN.MIPGEN`, ruling T8), does not write anything back
(`SW.STREAM`'s F-sheet journal, T4), does not interpret a single layer byte, and
does not choose which pages are wanted (`SW.STREAM` / `TERRAIN.SEQ`). It moves
bytes and reports.

**Most of all: it does not publish.** `fin_*` is a report; the directory
publishes or FAULTs. That split is what makes "a half-loaded or CRC-failed page
is never rendered" (T7) a property of the *system* rather than a promise made by
the block that just wrote the bytes.

## The laws it executes, with citations

| law | where it is written |
|---|---|
| page stride 21,376 B, body 21,320 B, header 64 B | `spec/terrain_rules.md` §2 layer table |
| streams **whole**, 334 × 64-B bursts, immutable in flight | `spec/terrain_rules.md` §7 |
| `page_crc32c` covers bytes **[64, 21320)** | `spec/terrain_rules.md`:126 (§2.1 header) |
| the header restates `{island_id, patch_ix, patch_iz}`; "redundancy is a corruption check" | `spec/terrain_rules.md` §2.1 |
| destination `TERRAIN.PAGE_POOL` `0x0400_0000`, 1,024 × 21,376 B | ruling **T2** / `spec/memory_rules.md` §5b |
| bursts are 64-B aligned, len 1..64, 64-bit beats, sim profile 16 + 1/beat | `design/contracts/MEM.HPS.BRIDGE.md` |
| every VRAM write goes through MEM.GUARD | `spec/memory_rules.md` §5 |
| upload acceptance order (align → reachable → source arena → destination → epoch); **a refusal is not a clamp** | `reference/include/zref/zref_mem_upload.hpp` |
| a half-loaded or CRC-failed page is never rendered | ruling **T7** |
| loader completion carries **success/failure and CRC identity** | ruling **T10** |
| the job's `expected_page_crc32c` travels in the sealed patch-list record | ruling **T5** / `design/contracts/SW.STREAM.md` |
| ceiling 32 whole pages per frame ≈ 41 MB/s, **provisional, not a board claim** | ruling **T7** |

### One place where the architecture sketch is superseded, and by what

`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §2.3 proposes *"`fin` withheld on
mismatch"*. That is **not** what this block does, and the reason is written
down: §2.3 predates `zhao_terrain_residency_v2`, whose `fin_*` port carries
`fin_ok_i` **and** `fin_crc_i` for exactly this purpose, and T10 says the
completion "carries success/failure and CRC identity".

Withholding would be worse than useless. The page still never becomes ground —
the directory only publishes on a good `fin` — but the slot never leaves
`LOADING` either, so a single corrupt page permanently retires one of the 1,024
slots with nothing counted anywhere. **Every job produces exactly one
completion.**

## Input and output packet layouts

**Job in** (`j_*`, ready/valid, one in flight), fields taken from T5's 32-byte
patch-list record plus the slot the sequencer claimed:

    slot:u11  gen:u8  epoch:u32  island_id:u32  patch_ix:i16  patch_iz:i16
    hps_addr:u64  expect_crc:u32  src_id:u32

**Completion out** (`fin_*`, ready/valid), port-compatible with
`zhao_terrain_residency_v2`'s `fin_valid_i / fin_slot_i / fin_gen_i /
fin_epoch_i / fin_ok_i / fin_crc_i`, plus `fin_verdict_o` and `fin_src_id_o`
which the directory ignores and a trace does not.

**Memory ports:** one `zhao_hps_burst_req_t/rsp_t` bridge client (reads), one
`zhao_guard_req_t/rsp_t` + `wdata/wvalid/wready/wlast` guard client (writes).
Both client identities are **input ports**, see *Memory ownership*.

### The slot is one bit wider than the pool

`SLOTW` defaults to `$clog2(REGION_SLOTS) + 1` = 11. At exactly 10 bits a slot
index **cannot express 1024**, so a producer that computed a bad slot would
arrive silently truncated — slot 1024 presenting as slot 0 and overwriting a
live page. "A refusal is not a clamp", and a truncation is a clamp with no
counter. The extra wire is what makes `kPageOutsidePool` reachable at all.

## The CRC, and why the range is the whole risk

CRC-32C, `zhao_crc32c_fold` (the one folder, eight bytes per clock in a shallow
tree), initialised `0xFFFF_FFFF` and finalised `~crc`, matching
`zhao_abi::zhao_crc32c(0, …)`. Accumulated **in flight**, one fold per arriving
beat, so a page costs no extra bandwidth.

**The range is beat-aligned and that is not a coincidence worth leaving
unstated.** 64 / 8 = 8 and 21320 / 8 = 2665, so every one of a page's 2,672
64-bit beats is wholly inside the CRC range or wholly outside it. The block
never folds a partial beat, which deletes the entire "the tail byte count was
off by one" class of defect. `zref_terrain_page.hpp` static_asserts it.

A CRC over the wrong range still produces a *number*. It agrees with a staging
tool that made the same mistake and disagrees with one that did not, and either
way it surfaces as "terrain sometimes does not load". So both edges are pinned
by corrupting one byte on each side of each: byte 63 and the 56-byte pad must
not change the verdict; bytes 64 and 21,319 must.

### Two declared holders of one number, and neither governs alone

The job carries SW.STREAM's `expected_page_crc32c` (T5). The page carries its
own `page_crc32c` at header +32 (§2.1), over the identical range. **Nothing in
the tree rules which one governs.** Rather than pick silently, the block
requires both to match the CRC it computed, behind `CHECK_HEADER_CRC`; a
disagreement between the two is itself a corruption and is refused. If a ruling
later names one as authoritative, the parameter is where that lands.

## Overflow and malformed-input behaviour

Verdicts are `zref::terrain::PageLoadVerdict`, whose values 0..7 **are**
`zref::mem::UploadVerdict` — one law, checked by static_assert, so the loader's
refusal taxonomy cannot drift from the console's upload taxonomy.

| verdict | meaning | when |
|---:|---|---|
| 0 | `kPageOk` | complete, identity good, CRC good |
| 1 | `kPageUnaligned` | `hps_addr` not 64-B aligned |
| 2 | `kPageZeroLength` | **unreachable**: the length is the constant 21,376 |
| 3 | `kPageOutsidePool` | `slot >= REGION_SLOTS` |
| 4 | `kPageEpochStale` | `job.epoch != cfg_epoch_i`, at full 32 bits |
| 5 | `kPageCrcFail` | payload disagrees with either declared CRC |
| 6 | `kPageSourceOutsideArena` | source, or its 21,376-byte tail, outside the staging arena |
| 7 | `kPageSourceUnreachable` | `hps_addr` has a non-zero upper half |
| 8 | `kPageHeaderIdent` | the page is a valid page **of another patch** |
| 9 | `kPageIncomplete` | bridge `err` or guard denial stopped the transfer |

**Refused before any byte moves** (1, 3, 4, 6, 7) — counted in
`pages_refused_o`, zero bursts issued. **Faulted after bytes moved** (5, 8, 9) —
counted in `pages_faulted_o`; the slot holds garbage that is unreachable because
the directory never publishes it, and the next claim overwrites it.

**Verdict 8 outranks verdict 5, deliberately.** Another patch's page has a
perfectly valid CRC of its own, so no CRC can catch a staging-pointer error;
and reporting a corruption when the pointer is what is wrong sends the next
person to the disk.

**The destination is computed, never accepted.** The job names a slot; the
address is `REGION_BASE + slot × PAGE_BYTES`, formed here, with the slot range
refused before the arithmetic runs. Writing outside the granted region is not a
rule this block obeys, it is a state it cannot reach — and MEM.GUARD stays the
independent check rather than the only one. The multiply is not a multiply:
`PAGE_BYTES` is an elaboration constant, so the scaling is one shifted copy of
`slot` per set bit of it — five shifts, four adders, no DSP.

## Backpressure rules

Every seam stalls, and every one is exercised:

* **`j_ready_o`** is high only in `S_IDLE`. One page in flight, always.
* **The bridge** is a registered accept pulse then a beat stream; first-beat
  latency and inter-beat gaps are both stimulus knobs in the bench.
* **MEM.GUARD answers in two cycles and its two bits are never both high.**
  `rsp.ready` is the level `!fwd_active`; `rsp.ok` pulses the cycle *after* the
  accept. `S_GREQ` waits on `ready`; `S_GVERD`, a separate state, reads `ok` /
  `violation`. Testing both in one arm reads every pass as a denial — the defect
  found in both geometry fetchers on 2026-09-06, which is why
  `tools/rtl/check_guard_verdict.py` exists and why this block is registered in
  its client list.
* **`guard_wready_i`** may stall between beats; `guard_wdata_o` and
  `guard_wlast_o` are functions of the beat counter alone, so a stalled consumer
  sees a held beat.
* **`fin_ready_i`** may be low indefinitely; the completion is **held**, never
  dropped. A dropped completion strands a residency slot in `LOADING` forever.

## Memory ownership

Reads the HPS staging arena declared by `cfg_hps_arena_base_i/bytes_i` and
nothing else. Writes `TERRAIN.PAGE_POOL` slot `job.slot` and nothing else.

**The client identity is RULED AND NOW ENACTED (2026-09-06).** T3 adds
`ZHAO_CLIENT_TERRAIN_BUILD = 6` as a best-effort background client. All three
amendments this contract asked for have landed:

1. `zhao_pkg::zhao_client_e` declares `ZHAO_CLIENT_TERRAIN_BUILD = 3'd6`, with
   5 left as a deliberate hole — that hole IS T3's "do not spend it
   pre-emptively".
2. `zhao_vram_arbiter` carries **seven** client ports. Widened rather than
   packed, because the array index IS the client id in that block
   (`ctrl_req.client = zhao_client_e'(offer_client)`); port 5 exists so the
   identity holds and is dead by construction — no arbitration arm names it and
   `port_grant[5]` is forced low, so a request there is refused rather than
   accepted into a slot that could never be served. TERRAIN_BUILD is arbitrated
   BELOW DEBUG, never promoted for lateness (T3).
3. `zhao_mem_guard` has `TERRAIN.PAGE_POOL`: `[0x0400_0000, 0x054E_0000)`,
   **client 6 alone**, constant bounds, no map input consulted. THIS BLOCK USES
   THE WRITE ARM (`terrain_ok`) AND ONLY THAT ARM -- it never reads local SDRAM.
   The window gained a second, READ arm (`terrain_rd_ok`) on 2026-09-06, with
   `TERRAIN.WRITEBACK`, which evacuates layer F; that direction is that block's
   and is stated here only so "write-only" is not read off this page as still
   true of the region. Same client, same constants, separate arm, separate
   theorem (`a1_terrain_wr_owner` / `a1_terrain_rd_owner`).

**What is NOT enacted, said plainly.** T2 asks for a *state-aware* permission —
"a loader may write only a `LOADING` slot" — and the window is spatial. The
guard has one muxed request port and no residency context; the interface that
would carry slot state to it is not ruled anywhere. The residual is that a
faulty TERRAIN_BUILD could write a page slot other than the one it was told to
load: it cannot reach a framebuffer, the asset pool, the rest of bank 2, or
anything outside the map, so the no-escape theorem is unchanged and the worst
case is corrupt ground for one page, caught by the CRC before publication. This
block already refuses an out-of-pool slot itself (`SLOTW` is one bit wider than
the pool so the refusal is reachable rather than truncated).

`cfg_vram_client_i` stays an **input port** rather than becoming a hard-wired
constant. The identity is now legal, but which client a deployment presents is
still configuration, and hard-wiring it would remove a knob to save nothing.
`guard_denied_o` remains a real fault counter: with the window in place a denial
means the loader computed an address the pool does not contain, which is a bug,
not a steady state.

**The bench measures both directions rather than asserting either.** The real
`zhao_mem_guard` is instantiated as an observer on the loader's own request
wires and now counts: 334 passed, 334 forwarded, 0 refused on a clean page —
exactly, not "at least". A second real guard on bench-driven wires is asked
about the refusals the observer can never see, because the loader only issues
legal requests: a different client writing in the pool (all seven other ids,
including the unspent 5 and the NONE encoding), the ruled client writing outside
it (both edges at the byte, both framebuffer slots, the asset pool, and two of
the bank-2 regions T2 names but nobody has opened), and a READ where a write was
granted. Every one of those checks was shown to FAIL under a deliberate
perturbation of the guard and restored.

## Q formats and rounding

None. Addresses, byte counts, a CRC and counters.

## Latency (fixed or variable)

`variable`. Per page, non-overlapped v1:

    334 × (bridge latency + 8 read beats + 2 guard cycles + 8 write beats)

which derives to 334 x 34 = 11,356 at the frozen sim profile. The **measured**
figure is **12,028 gpu clocks** -- about 120 us at the 100 MHz placeholder --
printed by the directed test that produces it rather than computed here, because
a derivation quoted as a measurement is how this tree has been wrong before. The
36 clocks per burst include the guard's own accept-then-verdict pair and the
job's entry and exit, which the derivation above skips.

The architecture document's 8,016-cycle figure assumes the read of burst *n+1*
overlaps the write of burst *n*; this version does not overlap, and the
difference is one page buffer plus a second address cursor when a measurement
says it is needed. Stated rather than quietly matched.

## Target throughput

One page per 12,028 measured clocks; T7's ceiling of 32 pages/frame ≈ 41 MB/s is
**provisional and not a board claim**. 32 non-overlapped pages is 384,896 clocks
— **23.1 % of the 1,666,667-clock frame** — real, affordable, and dependent on a
bridge share this block does not own.

## Counters and traces

`pages_loaded_o`, `pages_faulted_o`, `pages_refused_o`, `crc_fails_o`,
`hdr_ident_fails_o`, `incomplete_o`, `guard_denied_o`, `bridge_errs_o`,
`load_bytes_o` (saturating, `zhao_sat_add32`).

Trace, latched on every fault: `fault_island_o`, `fault_ix_o`, `fault_iz_o`,
`fault_src_id_o`, `fault_verdict_o`, `fault_crc_seen_o`, `fault_crc_expect_o` —
so a refusal is traceable to `(island, ix, iz)` and to the source id that asked
for it, which is the MEASURE.HISTOGRAM discipline of refusing loudly.

## Scalar reference function

`zref::terrain::page_load` (`reference/include/zref/zref_terrain_page.hpp`),
with `zref::terrain::page_payload_crc`, `zref::terrain::page_vram_addr` and
`zref::terrain::PageLoadLedger` as the CRC, address and counter oracles.

**The oracle is the VERDICT, the RANGE and the ADDRESS — not the burst
machinery.** That distinction is deliberate. A dropped burst fails loudly the
moment anything reads the page; a CRC over the wrong range, a page written to
the wrong slot, or a refusal taken in the wrong order are all *silent*, and two
of them produce real terrain that is simply not the terrain that was asked for.

The header composes rather than copies: the acceptance law is a call to
`zref::mem::upload_verdict`, the CRC is a call to `zhao_abi::zhao_crc32c`, and
the verdict enum's first eight values are `static_assert`ed equal to
`zref::mem::UploadVerdict`'s. This tree has been bitten twice by two definitions
drifting; there is one definition here, used from two languages.

## Directed tests

`tests/terrain/pageloader_rtl_directed.cpp` — **184 checks**, whole suite
690,868 gpu clocks. (The figure here read **154** until 2026-09-06 while the
suite measured 179: a contract number left behind by two passes of test growth.
Corrected to the measured value when the guard read arm added five more.)

* the golden page, unstalled: verdict, `ok`, CRC, slot, generation, epoch and
  source id against the oracle; **all 2,672 words compared**, not sampled; the
  first and last write addresses against `zref::terrain::page_vram_addr`; the
  neighbouring slot untouched;
* **exactly 334 bursts, 334 guard requests and 2,672 write beats** — the
  how-many-times half, which is the only thing that can see a machine doing its
  work twice;
* the same page with every stall source engaged (bridge latency 16, beat gap 2,
  guard hold 5, write pacing 3): byte-identical image, identical CRC, and the
  same three exact counts;
* the CRC range at all four edges: bytes 63 and the pad must not fault, bytes 64
  and 21,319 must;
* a valid, correctly-CRC'd page **of the wrong patch** — refused as
  `kPageHeaderIdent`, which no CRC can do — and identity beating CRC when both
  are wrong; a wrong `format_version`; the header's own CRC word disagreeing
  with the job's;
* eight pre-transfer refusals, each asserting the verdict, oracle agreement,
  **zero bytes moved, zero bursts issued**, the counter, and the source id in
  the trace — including two ordering cases where several things are wrong at
  once and `upload_verdict`'s order decides which is reported;
* the transfer stopping part way: guard denial, bridge `err` against a request,
  bridge `err` mid-beat — each faulted, counted and traced, and a good page
  loading afterwards (an abort is not a wedge);
* `fin` held for 40 cycles with `fin_ready` low, payload compared against itself
  every cycle;
* **zero guard denials on a clean page** (see below);
* the real `zhao_mem_guard`, watching: never passed one of these writes,
  refused them loudly.

## Randomized differential tests

Same file, 48 draws: random slot, island, coordinates, generation, source id and
stall profile, with a randomly chosen malformation from {none, corruption inside
the CRC range, corruption outside it, wrong patch in the header, wrong header
CRC word, unaligned address, slot out of pool, stale epoch}. Every draw is
compared against `zref::terrain::page_load` for verdict, `ok`, CRC and source
id, and **the whole ledger is compared at the end** — a block that answers every
draw correctly while counting the wrong thing passes the per-draw comparison and
fails here. The observed mix is asserted to contain clean loads, CRC failures,
identity failures and pre-transfer refusals, so the comparison cannot be
satisfied by one outcome repeated.

## Proof that the suite can fail

Nine perturbations, each applied to the RTL, built, run, and reverted.

| perturbation | what fired |
|---|---|
| `CRC_LO` 64 → 56 | 7 checks; golden verdict 0 → 5, CRC `0x1528E21F` → `0x9F42CB51` |
| `CRC_HI` 21320 → 21328 | 7 checks; CRC → `0x8B0C156D` |
| S_GREQ and S_GVERD merged into one arm | 4 checks — see below |
| `REGION_BASE` dropped from the address | 7 checks; `first write is at slot 0's base (expected 67108864, got 0)`, 2,672 out-of-window writes |
| `S_FIN` returns to idle without `fin_ready` | `golden: fin held stable for 40 stalled cycles (expected 40, got -1)` and 41 random draws |
| `CHECK_HEADER_IDENT` → 0 | 7 checks; `a valid page of the wrong patch is refused (expected 8, got 0)` |
| refusal order: epoch tested first | 4 checks; `all wrong at once: slot first (expected 3, got 4)` |
| `guard_wlast_o` off by one beat | `wlast is exactly the 8th beat of every burst (expected 0, got 668)` |
| every burst's write issued **twice** | 5 checks; `exactly 334 guard requests (expected 334, got 668)`, `exactly 2,672 write beats (expected 2672, got 5344)` |

Two of these are worth more than the others.

**The merged-guard-arm case is why the suite has 154 checks and not 150.** The
first version of this file caught it with exactly one check, and that check's
message was `guard_denied counted (expected 3341, got 3342)` — a number nobody
would have read as "this block treats every guard pass as a denial". The page
still loaded, byte for byte, because the verdict state catches it one cycle
later; every result-checking assertion stayed green. Only a count moved. The
missing property — *zero denials on a clean load* — was added because breaking
the block on purpose showed the suite could not see the repository's own
historical bug.

**The double-work case produced a byte-identical VRAM image.** Every word
matched, every CRC matched, `pages_loaded` was right. Only the burst, request
and beat counts moved, and `load_bytes_o` doubled. That is `CLAUDE.md`'s
2026-09-05 law reproduced on purpose: a test that checks *what* came out cannot
see *how many times* the machine did it, and the throughput budget is written
against the second number.

Both static gates were also shown to fire on this file: `check_guard_verdict.py`
reports `zhao_terrain_pageloader.sv:538 tests .ok in the SAME arm as .ready`
under the merged-arm perturbation, and `check_ingress_capture.py` reports
`LATE INGRESS READ … j_island_i` when one captured field is replaced by its live
pin.

## Formal properties

None yet. The one worth an SBY is the containment theorem — *no guard request
this block emits lies outside `[REGION_BASE + slot·PAGE_BYTES,
+ PAGE_BYTES)`* — which is currently structural (the address is computed from a
range-checked slot) and therefore cheap to prove — and the guard now HAS a
terrain window to check it against, so the excuse for not proving it has
expired.

## Integration capture cases

**None yet, but the reason has changed.** The three amendments above have
landed, so the block is no longer unintegrable: there is a client identity, an
arbiter port that carries it, and a guard window that admits its writes. What
remains is composition work rather than a ruling — `zhao_shell_top` ties
`client_req[6]` off because TERRAIN.PAGELOADER is not in the shell yet, and
wiring it in needs its own guard instance, a share of the HPS bridge and the
residency directory. That is a pass, not a question.

## Synthesis / resource ceiling

Unfitted. Budget group `geometry_mantle`. Expected cost is one 64-byte staging
buffer (8 × 64 b), one `zhao_crc32c_fold`, a handful of counters and a nine-state
machine; the address scaling is shifts and adders by construction, so the
acceptance question at fit time is **zero DSP**, not Fmax.

## What is not yet established

* **The client and the window.** Ruled (T2, T3), not enacted. Above, in four
  numbered items.
* **Which CRC holder governs** if the job's `expected_page_crc32c` and the
  page's own `page_crc32c` ever disagree. Both are checked today; a ruling would
  turn `CHECK_HEADER_CRC` from a policy into a constant.
* **Overlap.** v1 reads a burst, then writes it. Overlapping the two roughly
  halves the per-page cost and needs a second buffer; nothing measured yet says
  it is required, and the 32-page ceiling is provisional anyway (T7).
* **Restoring a saved F sheet.** T4/T8 make a page resident only after "any
  restored F sheet complete". That restore is a second, smaller transfer from
  the HPS journal, and it is `SW.STREAM`'s and the sequencer's to order — this
  block loads the page the record points at and has no opinion about what else
  the slot needs before it is ground.
* **Board bandwidth.** Every timing number here is the frozen sim profile.
  `spec/terrain_rules.md`:456-457 forbids freezing the streaming figure before
  ZH-004 reports, and nothing here does.
