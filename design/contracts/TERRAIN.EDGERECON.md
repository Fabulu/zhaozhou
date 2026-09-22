# Contract — TERRAIN.EDGERECON (Neighbour-edge level producer)

> Ledger: `design/blocks.yml` · owner ZH-050 · phase 6 · maturity UNIT_VERIFIED

## Purpose and exclusions

The producer for `zhao_terrain_lod`'s four `edge_*` inputs — the **adjacent
patches' border levels** — commissioned by owner ruling 2026-09-22 item 6,
ratified in `reports/OWNER-RATIFICATION-20260922-COMPLETION.md`:

> *"For the full-capability target, authorize implementing the **real
> neighbor-edge level producer.** … The coordinator may introduce **bounded
> prepare/reconcile/emit sequencing** over the admitted terrain set for the
> frame … This may buffer decisions and adjacency metadata; it does not
> authorize a second full terrain engine or a duplicate world/geometry payload
> store."*

Implemented as `fpga/rtl/terrain/zhao_terrain_edgerecon.sv`. It holds one
32-bit record of decided subpatch levels per live patch, keyed by the patch's
grid coordinate, and answers a query for one patch with the four 8-bit words
`zhao_terrain_lod` takes on `edge_nz_i`, `edge_pz_i`, `edge_nx_i`, `edge_px_i`
— **port for port, no adapter.**

Excluded: **no ladder** (it never decides a level; it stores the ones
`zhao_terrain_lod` decided), no heights, no lattice, no page bytes, no memory
port of any kind outside its own bank, and **no admitted-set walker** — who
presents the frame's patches during PREPARE is the caller's business. See
"What still has no producer".

## The structural fact this block is built on

**`edge_*` reaches `out_lvl_nz/pz/nx/px` and nothing else.** Measured in
`zhao_terrain_lod.sv` rather than assumed — the four ports appear on four lines
of that file:

```
nb_nz = (e_j != 2'd0) ? lvl[{e_j - 2'd1, e_i}] : edge_lane(edge_nz_i, e_i);
nb_pz = (e_j != 2'd3) ? lvl[{e_j + 2'd1, e_i}] : edge_lane(edge_pz_i, e_i);
nb_nx = (e_i != 2'd0) ? lvl[{e_j, e_i - 2'd1}] : edge_lane(edge_nx_i, e_j);
nb_px = (e_i != 2'd3) ? lvl[{e_j, e_i + 2'd1}] : edge_lane(edge_px_i, e_j);
```

`out_level_o` is `lvl[]`, decided by the projected-error ladder from `sp_*` and
the governor alone. **A patch's own sixteen levels do not depend on its
neighbours**, so the reconciliation has **no fixpoint to iterate**: one pass
decides every admitted patch, and a second, pure lookup fills in the borders.
That is the whole reason "bounded prepare/reconcile/emit" is bounded.

## Why a streaming, serve-order producer is not an option

The cheap build — file each patch as it is served, answer later patches from
whatever has been filed — is **wrong**, and `zhao_terrain_tess` says why in one
line:

```
lv_px = (job_lvl_px_i > job_level_i) ? job_lvl_px_i : job_level_i;
```

The shared edge is tessellated at **max(neighbour, own)** — the coarser of the
two. Take P at level 1 beside Q at level 2. Serve P first; Q is not filed, so P
is answered the fallback and tessellates the seam at `max(0,1) = 1`. Serve Q
second; P *is* filed, so Q is answered 1 and tessellates the same seam at
`max(1,2) = 2`. **One side emits level-1 density and the other level-2 density
on the same edge. That is a crack**, with every counter in the console
balancing.

Repairing it by making the second side fall back too keeps the pair symmetric
and makes **every** edge fall back, because the first-served patch of any pair
never has its neighbour: the scheme degenerates to the constant it replaced.
**There is no partial answer.**

### And the same arithmetic corrects what entry I21 says about the fallback

Entry I21 records of the retained `8'h00`: *"0 is the FINEST level, so a
neighbour read as 0 makes this block clamp its own edge finer than it needs —
more triangles, and NO CRACK."* The first half is right and **the last three
words are not.** `max(0, own) == own`, so a neighbour reported as 0 selects the
patch's own level and `stitch_new` is false: the seam is **not stitched at
all**. Two adjacent patches at different levels then emit different vertex
counts along it.

**The constant is conservative in TRIANGLES, not in CRACKS.** It is crack-free
only where adjacent patches happen to agree. `terrain_edgerecon_directed`
case 2's positive control measures it: over a 3 × 3 block of patches, **24 of
48 x-seam lanes disagree** under the tie-off. The ruling's instruction to
retain `8'h00` until the real producer is validated stands unchanged — it is
still the right fallback, because the *coarsest* constant would seam the ground
visibly everywhere and measure smaller, which is the direction a
resource-pressed campaign is biased to pick. But it is retained as **the least
bad constant**, not as a safe one, and the difference is the argument for this
block.

## THE SYMMETRY LAW — the theorem the crack-free property rests on

Define, for a patch X,

```
ok(X)  ==  bank[idx(X)].valid
       &&  bank[idx(X)].tag == X
       && !bank[idx(X)].poison
       &&  bank[idx(X)].mask == 16'hFFFF        (all sixteen lanes decided)
```

and answer the edge between P and its neighbour N with N's true border row when
`ok(P) && ok(N)`, and with `8'h00` otherwise.

**The predicate is symmetric in P and N**, and the bank is **frozen** for the
whole emit phase, so P's query and N's query evaluate the identical boolean
from the identical bits. Either both sides get each other's true level — and
TESS's `max()` agrees on both — or both get `8'h00` — and `max()` reduces to
each side's own level, which is the console's existing behaviour on that seam.
**No arrangement of records can make the two sides disagree.** ∎

`ok(P)` guards **P's own four edges as well as every query that names P as a
neighbour**, which closes the one asymmetry a direct-mapped bank can produce: a
collision **poisons** the entry, and a poisoned entry fails `ok` for its owner
and for every querier alike. A collision costs triangles on four seams and
cannot cost a crack on any.

`terrain_edgerecon_directed` case 2 checks the theorem by **exhaustion** over a
3 × 3 block — twelve interior seams, forty-eight lanes — with the equality
computed the way TESS computes it, so a failure *is* a crack rather than a
proxy for one.

## The sweep is the "no previous-frame shortcut" enforcement

The ruling's one named prohibition is *"Do not replace current-frame ownership
with an unvalidated previous-frame shortcut."* That is enforced structurally:
`frame_begin_i` walks every entry's valid bit to zero **before** the PREPARE
phase opens, so **every record any query can read was filed in that query's own
frame.** A patch that left the admitted set is not stale data with an old tag —
it is absent, and its neighbours fall back.

An epoch tag was considered instead and rejected. It is cheaper by `ENTRIES`
clocks a frame and it makes staleness a function of the tag width, which is a
correctness property nobody can test at the width that matters. Case 7 tests
the sweep directly: file a frame, prove the answer real, pulse `frame_begin_i`,
file nothing, and every lane falls back; refile and it comes back.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset enters the **sweep** phase and walks `ENTRIES` entries' valid bits to
zero, then settles in EMIT with an empty bank — where every query is answered
`8'h00`, which is the console's existing behaviour and therefore the safe state
to come out of reset into. No clock-domain crossing lives here.

A RAM has no reset, which is why the sweep exists rather than a reset clause on
the array. `busy_o` is the block's own statement that the sweep is done; the
directed suite waits on it rather than counting clocks, because a test that
counted clocks would go green on a sweep that never ran.

## Phases

| phase | `phase_o` | entered by | what is legal |
|---|---:|---|---|
| SWEEP | 0 | reset, and every `frame_begin_i` | nothing; `f_ready_o` and `q_ready_o` are low |
| PREPARE | 1 | the sweep completing after a `frame_begin_i` | filing |
| EMIT | 2 | `prepare_done_i`, and the sweep completing after reset | querying |

A file beat offered outside PREPARE is **refused** (`f_ready_o` low) and counted
on `file_out_of_phase_o`; a query offered outside EMIT is refused (`q_ready_o`
low) and counted on `query_out_of_phase_o`. **The refusal is what makes the
symmetry law's "identical bits" true** — a record that changed between P's query
and N's query would break it.

`frame_begin_i` during a sweep **restarts** it rather than queueing: two begins
with no work between them is one frame's worth of sweeping, and the later one
wins.

## Input and output packet layouts

**FILE port** (PREPARE), ready/valid, one subpatch decision per beat:

| field | width | meaning |
|---|---|---|
| `f_ix_i` `f_iz_i` | 16 each | the PATCH's grid coordinate, held across the patch |
| `f_ox_i` `f_oz_i` | 6 each | `zhao_terrain_lod`'s `out_ox_o`/`out_oz_o`; `n = {oz[4:3], ox[4:3]}` |
| `f_level_i` | 2 | `out_level_o` |
| `f_surface_i` | 1 | `out_surface_o`. 1 = the underside replay: **consumed, not filed** |

The four LOD fields are that block's emit port field for field, so the two wire
with no adapter. `f_ix_i`/`f_iz_i` are not LOD's — that block has no patch
coordinate — and are held across the patch exactly as the governor's targets
are. The underside replay carries the top's level (TERRAIN.LOD law 7) and would
write the identical bit into the identical lane; it is dropped so that
`lanes_filed_o` counts **decisions** and not beats.

`f_ready_o` is high only on the address-presenting cycle of the bank's
read-modify-write, so the port runs at one lane every other clock — 64 clocks a
patch, against a ladder that spends ~784 on the same patch.

**QUERY port** (EMIT), `q_valid_i` / `q_ready_o` with `q_ix_i`, `q_iz_i`.
`q_done_o` pulses when the four words below are new. Seven clocks per query:
accept, five sequential bank reads (own, −z, +z, −x, +x), publish.

**The answer** is `zhao_terrain_lod`'s `edge_*_i`, port for port, plus

| field | width | meaning |
|---|---|---|
| `edge_real_o` | 4 | bit 0 = −z, 1 = +z, 2 = −x, 3 = +x. HIGH = a real neighbour decision; LOW = the conservative fallback |

The answer is **registered and held until the next walk PUBLISHES** — not until
the next query is accepted — which is `TERRAIN.LOD.md`'s *"must be held stable
across a patch job"*. The walk accumulates into a separate set of registers
(`w_*`) and the published set (`q_*`) moves on one edge, the same edge as
`q_done_o`.

**That split is a repair, and the way it was missed is the point.** The first
version had one set of registers, cleared on the query ACCEPT, so `edge_*`
dropped to `8'h00` for the seven clocks of the walk. **All eleven directed
cases passed**, because each queries and then reads — none of them looks at the
port while a walk is in flight. A caller that started patch N+1's query while
TERRAIN.LOD was still emitting patch N's descriptors would have fed that block
the fallback **mid-patch**: a crack whose cause is a handshake, with every
counter agreeing. Case 9 holds the answer BETWEEN queries; **case 12 holds it
DURING the next one**, which is the different claim and the one that was wrong.

### The border-row packing

Lane *k* occupies bits `[2k+1:2k]`, which is `edge_lane()`'s own indexing.
Subpatch *n* = `{j, i}`.

| the querying patch's edge | the neighbour's shared cells | lanes |
|---|---|---|
| −z | the neighbour's `j = 3` row | `i = 0..3` |
| +z | the neighbour's `j = 0` row | `i = 0..3` |
| −x | the neighbour's `i = 3` column | `j = 0..3` |
| +x | the neighbour's `i = 0` column | `j = 0..3` |

Directed case 1 gives all sixteen subpatches of both patches **different**
levels, so a lookup that read the wrong row, or the right row backwards, cannot
accidentally be right — the same defect `terrain_lod_tess` caught inside one
patch, one level up.

## Memory ownership

**One bank**, `ENTRIES = 1 << (IXW + IZW)` entries of `BW = 82` bits: a 32-bit
tag (the full `{ix, iz}`), sixteen 2-bit levels, a 16-bit fill mask, `valid` and
`poison`. At the defaults that is **256 × 82 = 20,992 bits**.

No other memory. **It is not a world store**: thirty-two bits of decision per
patch, against `zhao_terrain_devstore`'s 185 M10K of page deviations.

### Why direct-mapped and not a directory

`zhao_terrain_island_dir` does this arithmetic for the page store and its answer
is worth repeating: an 8 km island is 125 × 125 patches, so a dense table over
the grid is 16,384 entries for an occupancy of a few hundred — tens of M10Ks to
hold mostly nothing.

The residency solves that with a set-associative directory, and this block
deliberately does **not** reuse it: the reuse would be a second reader on a port
the paging spine owns, and a `(ix,iz) → slot` lookup per neighbour per patch.
The bank is direct-mapped on `idx = {iz[IZW-1:0], ix[IXW-1:0]}` with the full
coordinate as a tag. At the default 4 + 4 that is a **16 × 16 patch window** —
1,024 m square at the canonical 2.0 m pitch — larger than any view the visible
radius produces, so two live patches collide only when the camera set spans
more than sixteen patches in one axis. **A collision is not a fault and not a
crack: it is the conservative fallback on four seams**, counted on
`collisions_o` so that the day a view outgrows the window the machine says so.

An off-island neighbour needs no special case: `ix - 1` at `ix == 0` wraps to
`16'hFFFF`, no patch can carry that coordinate (the island extent is 125), the
tag compare fails, and the lane falls back. **The wrap is the answer, not a
hole in it.**

## Backpressure rules

Ready/valid on both ports, and neither can be entered out of phase. `f_ready_o`
is low during the write half of every read-modify-write, so the file port
self-throttles; `q_ready_o` is low for the whole five-record walk, so a second
query cannot be injected into one in flight (case 9 measures both).

## Latency

| path | clocks |
|---|---:|
| sweep (reset, and every frame) | `ENTRIES` + 1 = 257 |
| one filed lane | 2 |
| one patch filed | 32 |
| one query | 7 |

At the visible set's 256 patches a frame: 257 + 256×32 + 256×7 = **10,241
clocks** of a 1.67 M-clock frame (100 MHz at 60 Hz) — **0.6%**.

## Target throughput

One query per patch per frame, matching `TERRAIN.LOD`'s one decision per patch
per frame. The margin is the latency table above.

## Overflow and malformed-input behaviour

- **The whole word on every coordinate field.** Coordinates are not range
  checked: an out-of-extent coordinate simply fails to match any tag and falls
  back, which is the correct answer for it. Range checking here would be a
  second copy of `zhao_terrain_island_dir`'s extent gate.
- **A partly filed record is unusable, from both directions.** `mask == FFFF`
  is in `ok()`, so a patch whose sixteenth lane never arrived is refused to its
  neighbours *and* falls back on all four of its own edges. Case 5.
- **Two live patches on one index poison it**, and the poison is permanent for
  the frame. Case 6 checks all three directions: the owner, the collider, and a
  healthy neighbour asking about either.
- **Every counter saturates** rather than wrapping (`spec/counters.md` §4).

## Counters and traces

| port | what it counts |
|---|---|
| `records_filed_o` | patches whose sixteenth lane landed |
| `lanes_filed_o` | subpatch levels written (the underside replay does not add to it) |
| `collisions_o` | entries poisoned by two live patches — **once per entry, not once per lane** |
| `queries_o` | queries answered |
| `edges_real_o` | lanes answered from a decision |
| `edges_fallback_o` | lanes answered `8'h00` |
| `query_own_missing_o` | the **queried** patch's own record was unusable |
| `file_out_of_phase_o` | a file beat offered outside PREPARE |
| `query_out_of_phase_o` | a query offered outside EMIT |

**And the invariant that makes the two edge counters worth reading:**
`edges_real_o + edges_fallback_o == 4 × queries_o`. They are incremented on one
event from **complementary predicates over four bits written by four different
states from four different bank words**, so a fault in the predicate moves one
sum and not the other. This repository has shipped a mismatch counter whose two
operands were loaded by the same enable and which therefore read zero for ever;
case 11 asserts the invariant and case 4 proves the fallback side moves, so
neither counter's silence is quoted alone.

No counter-catalog id is bound: minting one is a `spec/counters.md` amendment,
not an RTL decision.

## Scalar reference function

**None, and that is deliberate.** This block stores and returns numbers another
block decided; there is no arithmetic for an oracle to be an independent
implementation *of*. The property that matters is a **relation between two
queries** (the symmetry law), which a per-call reference function cannot
express. `terrain_edgerecon_directed` case 2 checks it by exhaustion instead,
with `tess_edge()` — three lines reproducing `zhao_terrain_tess`'s own
`max(neighbour, own)` — standing in for the geometry.

## Directed tests

`tests/terrain/terrain_edgerecon_directed.cpp` — **1,057 checks**, twelve
cases plus a generator self-check:

1. **The border rows come back**, with every subpatch of both patches at a
   different level so a transposed lookup cannot be accidentally right.
2. **THE SYMMETRY THEOREM by exhaustion** over a 3 × 3 block: twelve seams,
   forty-eight lanes, `max(own, given)` equal from both sides. Plus the
   **positive control** on the checker — the same comparison under `8'h00`
   disagrees on 24 of 48 lanes.
3. **Job-order permutations**: four file orders with non-uniform gaps × two
   query orders, byte-identical answers.
4. **An absent neighbour falls back**, `edges_fallback_o` fires and
   `edges_real_o` is shown flat beside it.
5. **An incomplete record is unusable from both directions**, and
   `query_own_missing_o` fires for its owner with the complete patch flat
   beside it.
6. **A collision poisons symmetrically** — owner, collider and healthy
   neighbour all refused; `collisions_o` counted once per entry.
7. **No previous-frame shortcut**: the sweep erases last frame's record, and
   refiling restores it.
8. **Phase gating both ways**, each with an in-phase control.
9. **The answer is held** for 900 clocks, and `q_ready_o` is low for the whole
   walk.
10. **The underside replay is consumed, not filed** — thirty-two beats, sixteen
    decisions, and the record still completes.
11. **The counter invariant**, over a mixed set.
12. **The published answer is HELD FOR THE WHOLE NEXT WALK**, walked cycle by
    cycle against a second query whose answer differs in every field, with a
    control asserting the two answers really do differ.

**And case 0, the generator self-check, earned its place immediately.** The
level grid was `n * 7 + …`; `n` steps by 4 between rows and 7 × 4 = 28 is 0 mod
4, so **every COLUMN of every patch came out uniform** — `col_i0()` and
`col_i3()` were four copies of one level. Every case still passed, because they
compare exact values, but a transposed or reversed COLUMN lookup would have
been accidentally right, which is the one defect case 1 exists to catch. It was
found by case 12's control failing because one of the two answers it compares
was all zeroes. The grid is now `i + 3j + …`, which walks 0,3,2,1 down a column
and 0,1,2,3 along a row, and the self-check asserts that rather than trusting
the comment.

**R95: all nine counters are fired on purpose from the block's own boundary
with legal stimulus, each with a control beside it, so no committed mutant is
owed here.**

## Randomized differential tests

None. See "Scalar reference function": there is nothing to difference against.
The order-permutation case is the randomization this block would benefit from
and it is directed rather than random because the space is small enough to
enumerate.

## Formal properties

None. The symmetry law is a two-line consequence of `ok()` being one expression
evaluated against a frozen store — a solver would restate it. What makes it
true is that there is exactly **one** `rec_ok_c` in the RTL and the walk cannot
run while the bank is writable; both are structural and both are tested.

## Synthesis / resource ceiling

**ESTIMATED, NOT MEASURED — no fit has been run on this block** (the owner's
instruction for this campaign is not to fit, and this packet did not). Counted
by hand, in the unflattering direction:

| | estimate |
| --- | ---: |
| bank | 256 × 82 = 20,992 bits → **3 M10K** (quantised: 3 × 256 × 32-ish slices = 24,576 bits) |
| flops | ~170: the two walkers' state (2 + 3), the file latches (16+16+4+2+8), the query latches (16+16+1+32+4), the sweep index (8), the frame counter (16), nine 32-bit counters (288) — **~460 with the counters, ~170 without** |
| ALMs | **~400–550**, dominated by the nine saturating 32-bit counters and the read-modify-write's variable part-select |
| DSP | **0** |

The counters are more than half of it. They are kept because R95 says a counter
whose silence will be quoted must be able to fire, and all nine can.

**R212: this block has NOT been through `quartus_map`.** `verilator --lint-only
-Wall` reports 0 diagnostics, and that settles one tool's opinion.

## Integration capture cases

None yet — see below.

## What still has no producer

**The admitted-set PREPARE walker.** This block owns the store, the law and the
sequencing gate; it does not enumerate the frame's patches or drive the ladder
over them. Composing it in the console needs a driver that, at the frame
boundary:

1. enumerates the admitted patch set — **this exists and is composed**; see
   the correction below, which replaces the claim this line first made;
2. for each, assembles `sp_*` from `zhao_terrain_devstore` (deviations, centre
   height, history) and `zhao_terrain_place` (the centre's x and z), both of
   which **are** composed, keyed by the page slot — which needs `(ix,iz) →
   slot` from `zhao_terrain_residency_v2`, also composed, and a `slot → (ix,iz)`
   direction the residency has no port for;
3. runs `zhao_terrain_lod` over them with its output routed **here** instead of
   to `zhao_terrain_jobissue`, discarding `out_hold_o` so the PREPARE pass does
   not advance the history the EMIT pass will read;
4. pulses `prepare_done_i` and hands the serve loop back.

Step 3's determinism is the property that makes the two passes agree: identical
`sp_*` and identical governor targets give identical `lvl[]`, so the levels this
block banks in PREPARE are the levels TESS receives in EMIT. **A deformation
bake landing between the two passes would break it**, and the driver owes the
interlock.

### CORRECTED 2026-09-22, in this packet, before the claim was left standing

The four steps above were written from `zhao_terrain_jobissue`'s and
`zhao_terrain_spdesc`'s accounts of what terrain lacks. **Two of the three
obstacles they name do not exist**, and the difference was found by reading
`zhao_terrain_seq`'s port list rather than the entries about it — R237/R240,
a stated blocker that dies on first contact.

* **The admitted-set enumerator EXISTS AND IS COMPOSED.** It is not
  `zhao_terrain_visible` — that block is **superseded by owner ruling R16**,
  because ruling T5 puts the visibility walk on the HPS and the hardware
  consumes a **sealed list**. The list arrives as `SubmitTerrainSet @ 0x0230`
  through TERRAIN.CMD and `zhao_terrain_seq` pumps it. `u_terrain_seq`'s
  **issue port** is live in `zhao_console_core.sv` right now:
  `is_valid_o`/`is_ready_i` with `is_slot_o`, `is_ix_o`, `is_iz_o`, `is_gen_o`,
  `is_cslot_o`, `is_flags_o`, `is_view_mask_o` and `is_src_id_o` — **one beat
  per admitted patch, in T5's canonical order.**
* **The `slot → (ix,iz)` direction is NOT the residency's to provide**, and the
  walker does not need one. **The pair is in hand at the issue port**, on the
  same beat, exactly as `{slot, src_id}` is in hand at the compose door for
  `zhao_terrain_spdesc`. That is this subsystem's established answer to
  "two keyings, no map", and it applies unchanged here.
* **`zhao_terrain_devstore` is composed and is keyed by that slot**, and it
  already returns the three deviations, the centre height and the history.

**SO THE REMAINDER IS ONE THING, AND IT IS SMALLER AND SHARPER THAN "NO
PRODUCER".** The prepare pass needs `sp_cx`/`sp_cz` — the subpatch centre's
world x and z — **for a patch that is not the one currently being placed or
composed**:

* `zhao_terrain_spdesc` gets them from `zhao_terrain_compcache_front`'s
  `lat_wx_o`/`lat_wz_o`, which describes **the patch the cache is serving**.
* `zhao_terrain_place` has a random-access `vtx_vi_i`/`vtx_vj_i →
  vtx_wx_o`/`vtx_wz_o` port, but it answers for **the patch whose header it
  last took**, and the fill path owns that.
* Recomputing placement from an origin and a pitch is the available shortcut
  and `zhao_terrain_spdesc` refuses it by name — it would be a second
  implementation of TERRAIN.PLACE's ratified law.

**Whoever builds the walker owes that one decision.** Three candidates, and
**this contract deliberately does not pick one**, because picking one was
attempted here and the reason offered for it turned out to be wrong — see the
correction below the list.

1. **A second `zhao_terrain_place` instance** fed from the issue stream. Small
   — TERRAIN.PLACE is shifts and an adder, no multiplier — but it is a
   **duplicate provider**, and R16 (which retired `zhao_terrain_visible` for
   exactly that) is the precedent against them.
2. **A `cx`/`cz` column on `zhao_terrain_devstore`**, written at page load
   beside the deviations.
3. **Time-multiplexing the existing PLACE query port** against the fill path,
   the way `zhao_terrain_spdesc` splices into the compose cache's lattice chain
   and injects only on cycles the upstream client leaves.

**CORRECTION, made before this recommendation was left standing.** This section
first named (2) as the recommendation, *"because the deviations are already
written at that moment and by that block"*. **The second half of that sentence
is false.** `zhao_terrain_lodfeed` observes the **mip pass** — `f_start_i`,
`f_slot_i`, `f_gen_i`, `f_epoch_i`, `f_src_id_i` and `f_h_i`, which are
`mg_fine_*` heights. **It never sees a placement and never sees a patch
coordinate.** Option (2) therefore needs the placed x/z routed to it as well,
which is most of option (1) or (3) wearing option (2)'s clothes.

What *is* measured, and is what the next packet should start from:

* **The patch coordinate is available at both ends of the page's life.**
  `zhao_terrain_hdrread` publishes `h_patch_ix_o`/`h_patch_iz_o` at page load
  (spec 2.1 +8/+10, checked against the record TERRAIN.SEQ issued the job
  from), and `zhao_terrain_seq` publishes `is_ix_o`/`is_iz_o` per admitted
  patch. **Naming the patch is not the problem.**
* **Getting its PLACED world x/z is**, because `zhao_terrain_place` answers for
  the patch whose header it last took and the compose cache holds only the
  patch it is serving.

**Recomputing `wx = (patch_ix·32 + i) << (16 + pitch_log2)` inside the walker is
the available shortcut and it is the one to refuse** — TERRAIN.PLACE exists
precisely because I27 refused to inline that arithmetic into a composer, and
`zhao_terrain_spdesc` refuses it again by name.

Plus the interlock named above: the PREPARE pass must **discard `out_hold_o`**,
and a deformation bake landing between the two passes breaks their determinism.

Until that walker exists the console remains in **conservative edge mode** and
`u_terrain_lod`'s `edge_*` still read the literal `8'h00`.

## Notes

1. **The bank stores decisions, not geometry.** The ruling permits buffering
   "decisions and adjacency metadata" and forbids "a duplicate world/geometry
   payload store". 32 bits a patch is the former.
2. **The fallback is per-lane-group, not per-query.** An edge with a filed
   neighbour is real even when the other three are not; case 1 asserts
   `edge_real_o == 4'hA` on a patch with two filed neighbours. A per-query
   all-or-nothing would have been simpler and would have thrown away real
   decisions the ruling asks to use *"where available"*.
3. **`ok(P)` is in the predicate for P's own edges.** Without it a poisoned or
   incomplete P would still be *given* its neighbours' truth while its
   neighbours were denied P's — the one asymmetry a direct-mapped bank can
   produce. It costs one bank read per query and it is the difference between
   a theorem and a hope.
4. **The underside is dropped rather than filed idempotently.** Filing it would
   also be correct (same lane, same value) and would make `lanes_filed_o` count
   beats. A counter that means "beats" cannot be compared against sixteen,
   which is the check that catches a lost lane.
