# Contract — TERRAIN.SEQ (terrain command sequencer)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_seq.sv`
> Reference model: `zref::terrain::seq::Sequencer` — `reference/include/zref/zref_terrain_seq.hpp`
> Test: `tests/terrain/seq_rtl_directed.cpp`

## Purpose

Take the frame's **sealed visible-patch set** and drive the terrain pipeline
with it. For each record of the set, in list order: look the patch up in
`TERRAIN.RESIDENCY`; on a miss, claim a slot, evacuate a dirty victim and ask
`TERRAIN.PAGELOADER` for the page; on a hit, allocate a composed-cache slot if
the patch needs live composition, pin the page and issue the patch job into
`TERRAIN.PATCH` → `LOD` → `TESS` → `NORMALS` → `PROJECT`.

**Exclusions.** It does not decide visibility (`TERRAIN.VISIBLE`'s, and the
sealed list arrives already built), does not decide LOD (`TERRAIN.LOD`'s), does
not compose (`TERRAIN.PATCH`'s), does not own the directory
(`TERRAIN.RESIDENCY`'s), does not move page bytes (`TERRAIN.PAGELOADER`'s), does
not choose what to degrade (`SW.STREAM`'s, per T6's ladder), **does not reorder
anything**, and holds no state across frames beyond its counters.

## Why it exists at all: this is the sentence's missing verb

`reports/Missingterrain` names the hole in the owner's own words, explaining
why the shipped world is "a little spot" rather than an 8 km island:

> Nothing currently does: camera moved → inspect island directory → determine
> visible patch coordinates → union the two players' working sets → prefetch
> missing pages → allocate local-SDRAM residency slots → preserve dirty scars
> from evicted pages → **issue all visible patches to the terrain engine**.

Everything before *prefetch* is now built: `TERRAIN.ISLAND` answers about one
patch, `TERRAIN.VISIBLE` walks the window, and `zref::swstream::WorldStreamer`
unions the two views, applies T7's prefetch policy and seals the list.
Everything after *issue* is built: LOD, TESS, NORMALS and PROJECT are
port-compatible and tested pairwise. **This block is the verb in the middle.**

## The ABI is ONE `SubmitTerrainSet`, and that is ruled, not chosen

`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §2.6 left the input ABI open —
"N `DrawProcedural` records, one per visible patch, or one new set-command?" —
and marked it OPEN 5. **OPEN 5 was answered on 2026-09-02 by ruling T5**, and
§7's table now says so:

    TerrainEpoch     @ 0x0220, 16 B  — epoch begin / end-flush / abort
    SubmitTerrainSet @ 0x0230, 32 B  — one per frame, whole required+prefetch set
    patch-list record, 32 B          — island, ix, iz, hps addr, crc, flags,
                                       view_mask, priority, source_id

> "**One SubmitTerrainSet covers the whole required+prefetch set.** Do not emit
> one DrawProcedural per patch and do not overload an existing terrain field
> command."

So this block consumes a header plus a stream of T5's 32-byte records, already
in T5's canonical order and already CRC-sealed upstream. `TerrainEpoch` is
`SW.STREAM`'s and `CMD.SCHEDULER`'s to sequence (T11's drain, pin-zero wait and
journal barrier are a level-teardown protocol, not a per-frame one); this block
receives the live `resource_epoch` as frame state and stamps it on every
outbound transaction.

## It does not sort, deduplicate or reorder, and that is the determinism anchor

`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §4's determinism table names
**submission order = visible_list order** as the anchor for the entire world
layer, and `reports/BINNER_CAPACITY_FOR_8KM_MAPS.md` establishes that painter
order is semantically observable downstream. T5 already fixed the canonical
order — required before prefetch, smaller priority first, view-union key,
island ascending, iz then ix, source_id — and `zref::swstream::canonical_less`
implements it once.

A second sort here would be a second copy of that rule, and **its first
divergence would draw the same ground in a different order**, which no counter
sees and no picture shows. The block walks the list.

## A miss is SKIPPED, never waited on

§2.6, verbatim:

> A patch that is not resident when its turn comes is **SKIPPED and counted, not
> stalled on** — the frame must never wait on a 80 µs page load mid-walk;
> prefetch exists so this is rare, and the counter makes it visible.

So there is no completion wait anywhere in the FSM. A miss claims a slot, emits
a writeback job if the victim is dirty, emits a load job, and moves on; the page
arrives for a **later** frame. One frame of missing ground at a streaming edge,
loudly counted, beats a deadline fault — and the counter that makes it visible
is `skipped_not_resident`.

**Only a REQUIRED miss is a skip.** A `PREFETCH` record that missed is a
prefetch record doing its job. Counting it would bury the number that means
"ground the player should be seeing is absent" underneath the number that means
"the streamer is streaming", and the skip counter exists for the first one.

### "Not REQUIRED" means "prefetch", exactly, and that is the producer's own encoding

The block reads one bit — `REQUIRED` — and treats its absence as prefetch. That
is not an assumption about the flag word: `zref::swstream::WorldStreamer` sets
the pair as `is_required(cls) ? kFlagRequired : kFlagPrefetch`
(`reference/include/zref/zref_sw_stream.hpp:484`), so exactly one of the two is
present on every record it seals, and T5's canonical order sorts on precisely
that partition ("required before prefetch",
`zref::swstream::canonical_less`).

Testing `REQUIRED` rather than `PREFETCH` is the safe direction of the two. A
malformed record with neither bit set is treated as prefetch: the page is
fetched, nothing is drawn, and nothing is composed. The other way round it
would be issued to the engine and would consume a composed slot.

**A record that is not REQUIRED never reaches the engine and never consumes a
composed slot**, so T6's fault is by construction only ever raised by patches
the frame actually had to draw.

## T6 and T7 are DIFFERENT OVERFLOWS and are kept apart by construction

Both rulings describe running out of something. They mean opposite things, and
§7's surviving cautions say conflating them "would fault frames the rulings say
to render".

| | pressure | this block's response |
|---|---|---|
| **T6** | more than `COMPOSE_SLOTS` **required dynamic** patches want live composition in one frame | **FAULT THE FRAME.** Latch `frame_fault_o`, record the rejected `source_id` and key, drain the rest of the sealed list, issue nothing further. |
| **T7** | more than `cfg_load_budget_i` whole pages wanted in one frame | **DEFER THE LOAD and keep going.** `loads_deferred_o`. Proxy-and-continue, recorded, explicitly **not** a fault. |

T6's five-step degradation ladder — bake and retire, drop optional visual-only
fields, fall back to material/geometry, retain gameplay-required, then rank by
projected importance — is **`SW.STREAM`'s, applied before sealing**. §2.6
forbids this block inventing degrade policy. By the time a list is sealed the
legal degradation has happened, so an overflow here is exactly the case T6 says
to fault on.

Symmetrically, **T7's ceiling is `SW.STREAM`'s policy** ("SW.STREAM may defer
PREFETCH records but may not mutate a sealed REQUIRED list"), and
`cfg_load_budget_i` is the hardware backstop underneath it — an editable input
rather than a literal, because T7 says board counters may reduce it immediately.

The budget is checked **before the claim**, not after. Claiming and then
declining to load would leave a slot in `LOADING` that nobody is filling.

## The composed-cache allocator is one register, and its width is load-bearing

Slot *n* goes to the *n*-th record of **this frame** that needs composition.
No history, no timing, no hash — a pure function of the frame's own command
list, exactly as §2.5 specifies, with the persistent-cache alternative recorded
there and rejected because `terrain_rules` §4.2's law is already "produced once
per frame". Every frame-scoped register is cleared on `fr_start_i`.

**The cursor is one bit wider than a slot index.** `COMPOSE_SLOTS` is 256 and
`CSLOTW` is 8, so a `CSLOTW`-wide cursor compared against `COMPOSE_SLOTS` would
be comparing against zero: the allocator would wrap onto slot 0 and hand the
257th patch the first patch's composed lattice. Every height would still be a
real composed height and every counter would agree; the frame would simply draw
one patch's ground in another patch's place. The extra bit is what makes
"exhausted" representable, which is what T6's fault needs in order to exist.

**A static/baked patch gets no slot at all**, per T6: "Static/baked visible
pages render from resident page layers and consume no dynamic slot." It issues
with `is_cslot_valid_o` low.

### The index is cleared with its valid bit — a defect this block actually had

The first version held the last dynamic patch's slot index while dropping the
valid bit. The differential caught it on the first run it was ever given
(`A2 allocator`, 16 divergences, `cs=0/1` against the oracle's `cs=0/0`).

It looks harmless — the consumer is told the slot is not valid — and it is not,
for two reasons. It makes `is_cslot_o` on a **static** issue a function of which
patch composed earlier in the frame, so the same frame replayed after a
different previous frame drives different bits on that port; the whole point of
a frame-scoped allocator is that no history reaches it, and a held index is
history. And a consumer that dropped the valid bit would compose into the
previous patch's slot, overwriting a lattice something else is about to read:
real composed heights, in the wrong patch's place, with every counter agreeing.

Zero is a legal slot, so this is **not** a poison value — there is no spare
encoding in `CSLOTW` bits when `COMPOSE_SLOTS` is `2**CSLOTW`, exactly as
`TERRAIN.COMPCACHE` found for its two-bit substance. The guarantee is only that
the port is a pure function of this frame; `is_cslot_valid_o` remains the sole
authority on whether to look at it.

## The writeback precedes the load, and only the ORDER can say so

T4: layer F (the 8 KiB surface sheet) has **no canonical HPS mirror**, so a
dirty victim's sheet must reach the journal, and T10 forbids reusing a `dirty_F`
slot before the writeback ACK. This block emits the writeback job for the
victim **before** the load job for the same slot; that ordering is the barrier
it is responsible for, and waiting for the ACK is `TERRAIN.RESIDENCY`'s
(`wb_*` port) rather than a stall here.

Both counters read 1 whichever way round the two jobs go out. **Only the order
distinguishes a journalled scar from a scar written into a page that has already
been overwritten**, which is why the test asserts the sequence and not the
counts — and why the mutant that swaps them produces 20 failures rather than
none.

The writeback carries the **evicted page's** key and generation, not the
incoming record's. Journalling one page's scars under another page's name is a
loss that only shows up the next time that page is loaded.

## Layers B and D are never written back

T4 again: the HPS owns canonical B and D and keeps them current from the same
deterministic commands. `modified_BD` is a counter, not a barrier. This block
therefore has exactly one writeback port and it means F.

## Counters count EVENTS. Every one of them. Not one counts cycles

Said explicitly because a sibling terrain block shipped two counters whose names
claimed events and whose bodies counted cycles, and one reported the producer's
patience — 115 offers spread over 1,783 cycles — instead of the refusals it was
named for.

Every counter here increments on exactly one accepted handshake or one consumed
record:

`records_consumed`, `patches_issued`, `prefetch_resident`,
`skipped_not_resident`, `claims_issued`, `claims_refused`, `claims_same`,
`loads_issued`, `loads_deferred`, `writebacks_issued`, `compose_slots_used`,
`pins_issued`, `drained`, `frame_faults`.

A stalled consumer changes how **long** this block takes and changes no number
it reports — which is asserted, not asserted-about: every directed frame is
replayed under four stall patterns and all fourteen counters must be identical
to the always-ready run.

`claims_same` is the odd one and is deliberate. It fires when the lookup said
*miss* and the claim said *already present* — the directory disagreeing with
itself across two transactions. It is counted rather than smoothed over, and the
load still goes out, because a slot that did not answer a lookup is not a slot
this frame may draw from.

## A tripwire nobody reads is decoration

`err_stray_ans_o` latches if a lookup or claim answer arrives in a state that
did not ask for one — the shape of a directory answering out of order, or a
stale answer from the previous record. Every consequence of that is silent: the
wrong slot handle attached to the right patch draws another island's ground in
this island's place.

It is **latched, never self-clearing** inside a set (a tripwire that resets
itself reports the last event rather than whether there was one), it is cleared
by `fr_start_i`, it reaches the bench top, and `A13` fires it on purpose and
requires it to go high. Twelve island signals once reached the top connected to
nothing that read them; this contract's rule is that a counter or error that no
test asserts does not get a port.

## Interface shape

One record in service at a time. The state walk for the cheapest record —
resident, required, static — is `FETCH → LOOKUP → WAIT_LU → PIN → ISSUE`, and
the **measured** cost is **5.38 clocks per record** (43 clocks for an 8-record
frame including its start and done pulses, directory answering at latency 1,
every consumer ready). `seq_rtl_directed` reports that figure on every run and
bounds it, so it cannot go stale in this paragraph. A miss costs more — claim,
answer, load — and a dirty miss more again.

A deeper pipeline is a measurement away, not a guess away, and the measurement
says not yet: a full 1,024-record set at 5.38 clocks is ~5.5 k clocks, about
**0.33% of a 100 MHz frame**. `reports/EARTH60_CAPACITY.md` already establishes
that compose throughput, not this pump, is the world layer's limiter, and
`TERRAIN.LOD`'s own ~784 clocks per patch dwarfs this by two orders of
magnitude. Pipelining this block would be optimising the 0.33%.

Ports, by group: the frame header (`fr_*` + `cfg_load_budget_i`); T5's record
stream (`rec_*`); the residency lookup and claim masters (`lu_*`, `cl_*`);
`pin_*`; the writeback job (`wb_*`); the load job (`ld_*`, field-for-field
`TERRAIN.PAGELOADER`'s `j_*`); the patch issue (`is_*`); the fault
(`frame_fault_o`, `fault_*_o`); and the counters.

**The record is captured at acceptance, every field of it.** The record source
is a DMA'd ring the frame may still be writing behind us, and reading it live
across the four cycles the FSM spends on one record is what
`tools/rtl/check_ingress_capture.py` exists to prevent.

**The pin is asserted; the unpin is not.** T10's "no slot reuse before pin count
zero" means a patch issued to the engine holds its page until the engine is done
with it, so the unpin belongs to job completion downstream. A pump that unpinned
at issue would be promising the page is free while TESS is still reading it.

## Scalar reference function

`zref::terrain::seq::Sequencer`
(`reference/include/zref/zref_terrain_seq.hpp`), with
`zref::terrain::seq::Ledger` as the counter oracle.

**It COMPOSES the record rather than redefining it.** The input type is
`zref::swstream::PatchRecord` and `zref::swstream::PatchFlags` verbatim — T5's
32-byte record, already written down once, already sorted by
`zref::swstream::canonical_less`, already serialised by
`zref::swstream::encode_record`. A second declaration of those fields would be a
second thing to keep in step with T5, and its first divergence would be silent:
a flag bit read at the wrong offset draws the wrong ground in the right place.
The sibling lane deliberately EXTRACTED `zref::island::visible_set` out of
`Streamer::update` so the streamer and the RTL could not diverge; this extends
that discipline rather than repeating the lesson. The only law this file adds is
the sequencing law.

**The oracle takes the directory's ANSWER as an argument rather than modelling a
directory.** That is the same boundary `TERRAIN.COMPCACHE` draws ("the oracle is
the STORE's contents and addressing, not the composition") and it removes a
whole failure class: a model that owned its own set-associative directory would
have to agree with `zhao_terrain_residency_v2` about victim choice, generation
bumps and pin accounting before it could say anything about sequencing at all,
and its first disagreement would be reported as a *sequencing* defect. Feeding
both sides the identical answer stream means the question is only ever "whatever
the directory said, did the two do the same thing with it".

`tests/terrain/seq_rtl_directed.cpp` — **75 checks**:

* **A1–A14, fourteen directed frames**, each replayed under four stall patterns
  and compared **action for action**: the resident-static set, the interleaved
  allocator, resident prefetch records, a clean miss, T4's barrier, T9's refused
  claim, the same-claim disagreement, T7's budget, T6's fault and its recorded
  identity, the prefetch-miss/skip distinction, the frame-scoped allocator run
  twice, the record cursor against a short `patch_count`, the stray-answer
  tripwire, and source-id propagation.
* **A randomised phase of 320 frames / 10,487 records / 24,718 actions**, with
  the stall pattern, the answer latency, the load budget, the flags and every
  directory answer drawn per record.

### What is compared is a LOG, not a value

Counters are compared too — all fourteen, every frame — but **a counter cannot
see order**. The two orderings that matter here are both invisible in a count:
T4's writeback-before-load barrier, and list order itself. So the whole observed
action sequence — kind, position and every payload field — is compared against
the reference's.

### Backpressure is where the bugs are, and full input coverage does not find them

`TERRAIN.ISLAND`'s differential passed 21 checks over a **full 15,625-patch
sweep** plus 3,000 random draws and still missed a dropped answer, because every
phase drove the consumer's ready HIGH on every cycle. So every frame here runs
under four patterns — always-ready, **three-in-four**, one-in-eight and
one-in-two — on all six ready signals independently, plus directory answer
latencies of 1, 2 and 3 cycles. The three-in-four case is there specifically
because the sibling lane's losses happened at *mostly* ready, not at heavily
stalled.

The check with teeth is not that each stalled run matches the oracle; it is that
the four logs and all fourteen counters are **identical to the always-ready
run**. A block that dropped a job when its consumer stalled produces a shorter,
otherwise-correct log.

### The randomised phase MEASURES what it drew

A sibling lane's "randomised" phase over 240 windows turned out to be four
distinct cases, because it drew from the low bits of a linear congruential
generator. Every draw here is taken from **bits 31..16**, and the phase reports
its own distribution on every run and asserts a floor on every bucket:

    320 frames, 10,487 records, 24,718 actions
    dispositions: issued 4,820  prefetch-resident 1,639  skipped 3,236
                  faulted 71  drained 721
    branches: refused 527  same 314  dirty-writeback 661  budget-deferred 647
              cslot 3,193  static-issue 1,627  faulting frames 71
    distinct record shapes drawn: 64 of 64

The floors were **measured first and set afterwards**, at roughly two thirds of
the observed counts. The shape count is asserted **exactly**: 64 is every
combination of the six stimulus bits, so anything less means a shape the block
can meet in the field was never presented to it.

### The acceptance invariant, and why an outside witness was needed

The bench's own tally of accepted `rec_*` handshakes is compared against the
block's `records_consumed`. This was added because a fire test walked straight
past a broken `rec_ready_o`: a block that holds ready high past its declared
`patch_count` takes a record off the ring and does not count it — the producer
considers it delivered, the block considers it never to have arrived, and the
record is gone into the gap between two frames. Every other number was correct
under that mutation, because the block's own counter and its own action log
agreed with each other about a record neither had seen. **Only a witness outside
the block can see that one.**

### Every check has been shown to FIRE

Fourteen mutations were applied to the RTL, built, run and reverted, each aimed
at a distinct check family. **All fourteen fire.**

| | mutation | failures |
|---|---|---|
| M1 | T4 barrier inverted: load before writeback | 20 |
| M2 | allocator made persistent across frames | 24 |
| M3 | T7's budget overflow raised as a T6 frame fault | 17 |
| M4 | a prefetch miss counted as a skip | 2 |
| M5 | static required patches given a composed slot | 19 |
| M6 | the pin handshake dropped | 17 |
| M7 | `claims_refused` counting every answer, not the refusals | 8 |
| M8 | the stray-answer tripwire removed | 1 |
| M9 | `rec_ready_o` outliving `patch_count` | 2 |
| M10 | the issue handshake ignored: a job dropped under backpressure | 17 |
| M11 | the writeback carrying the incoming key, not the victim's | 17 |
| M12 | the composed-cache fault raised for prefetch records too | 15 |
| M13 | the composed slot index perturbed by one | 19 |
| M14 | the slot index held under a low valid bit — the defect this block had | 15 |

### The fire harness itself read low, twice, before it was believed

This is the broken-instrument law arriving inside the tool built to check for
broken instruments, and it is written down because it very nearly shipped a
false "the check did not fire".

1. The first version trusted `BUILD_RC == 0` and ran whatever executable was on
   disk. Four mutants in a row reported their **predecessor's** behaviour — M3
   showed M2's failures, M5 showed M4's, M14 showed M13's. A stale binary and a
   check that does not fire are the same observation.
2. Deleting the executable to force a relink was not enough: ninja relinked the
   **same objects** and satisfied a "did it relink?" guard while the RTL change
   had never been verilated.
3. Deleting the whole verilate directory then broke the build exactly the way
   CLAUDE.md's build note describes — it took `Vtb_terrain_seq.cmake` with it,
   `copy_if_different` of a file nothing writes failed on every build, and ninja
   could not rebuild the graph that would have fixed it. Recovery was a full
   `cmake --preset windows-native` re-verilating every target in the tree.

The harness now deletes the generated sources and objects while sparing the
`.cmake` files, demands a re-verilate, and **fingerprints each mutant's failure
set so an identical consecutive pair cannot pass unnoticed**. The corrected run
is the table above; four of its rows differ from the stale run's, which is the
evidence that the correction mattered.

`tools/rtl/check_ingress_capture.py` was fire-tested on this file too: replacing
`r_flags_q[FLAG_REQUIRED]` with a live `rec_flags_i[FLAG_REQUIRED]` produces
`LATE INGRESS READ ... zhao_terrain_seq.sv:301 rec_flags_i`.

## What is not yet established

* **Not fitted.** No Quartus run has seen this block.
* **Not composed with the real `TERRAIN.RESIDENCY`, `TERRAIN.PAGELOADER` or
  `TERRAIN.COMPCACHE`.** The bench drives the directory's answers so that the
  *sequence* is what is under test, which is this block's actual job; each of
  those has its own differential. Composing them is step 8 of the world layer's
  build sequence and is a separate question.
* **No `TerrainEpoch` port.** T11's `BEGIN` / `END_FLUSH` / `ABORT` is a
  level-teardown protocol spanning residency, the journal and the loader; this
  block consumes the live `resource_epoch` as frame state and stamps it, and
  the teardown sequencer is not built. `fr_busy_o` is what a drain would wait on.
* **No stale-handle re-check between lookup and issue.** Nothing else mutates
  the directory inside a frame, and the block issues immediately after its own
  lookup, so `TERRAIN.RESIDENCY`'s `chk_*` port is not driven. A
  `stale_abandoned` counter was drafted and then **removed rather than shipped
  reading zero** — a counter nothing can increment is the decoration this
  contract's tripwire rule forbids.
* **`view_mask` is forwarded, not acted on.** Splitting a DUAL patch into two
  view-scoped jobs, if that is ever wanted, is a decision for whoever composes
  this with GEOM.PARAMBUF.
* **T7 gives gameplay-required patches no overflow behaviour**, and that gap is
  genuinely open (§7's surviving caution). It lands on `SW.STREAM`, which
  refuses loudly (`unruled_gameplay_starvation`); this block sees only a sealed
  list and cannot tell the case apart.
* **The bench runs `COMPOSE_SLOTS = 16`, not 256**, so T6's fault is reachable
  in a short frame. The law — slot *n* is the *n*-th composing record of this
  frame and the next one faults — is the same at both sizes, and the 256 shape
  is elaborated by `lint_terrain_seq`, which runs the module on its own
  defaults.
