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
| flops | the two walkers' state (2 + 3), the file latches (16+16+4+2+8), the query latches (16+16+1), the WALK answer (32+4) and the PUBLISHED answer (32+4), the sweep index (8), the frame counter (16), nine 32-bit counters (288) — **~496 with the counters, ~208 without**. The 36 that hold the published answer through the walk were added by the case-12 repair and are counted here rather than left in the earlier ~170. |
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

### RE-MEASURED 2026-09-23 (gz/edgerecon2). THE REMAINDER IS BIGGER, NOT SMALLER

Every previous re-measurement of this walker shrank it. This one grows it, which
is the direction R237 says to check hardest and the direction nobody had found
yet. Two of the three obstacles above are **not** the obstacle, and **three that
appear nowhere above are**.

**The `sp_cx`/`sp_cz` decision this contract deliberately left open is now made,
and it is none of the three candidates.** The section above records the obstacle
as *"getting its PLACED world x/z"*. Measured in `zhao_terrain_place.sv`:

```
:41    wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
:318   vtx_wx_o = place32(units_of(ix_q, vtx_vi_i), pitch_q);
:282   env_ok_c = (org_x_c == hdr_env_x0_i) && (org_z_c == hdr_env_z0_i);
```

**`hdr_env_x0_i`/`hdr_env_z0_i` are a CHECK, not an operand.** No page payload
reaches placement at all: it is a pure function of the patch coordinate, the
vertex index and `pitch_log2`. So a patch's world x/z **is knowable without
composing it**, and candidates (1) and (2) each pay a store to carry a number
that is two shifts away. **The actual missing datum is `pitch_log2` for a
resident patch — two bits**, produced only by the page's own header (spec 2.1
+2, `zhao_terrain_hdrread`) and retained nowhere per slot.

Candidate (3) is dead for a reason this contract does not give: `hdr_ready_o` is
tied high unconditionally (`:287`), and a header acceptance **displaces** the
latched patch (`:361-375`) *and* launches the 66-write `pos_we_o` fill into the
compose cache. You cannot ask PLACE about another patch without destroying the
patch it is placing. The honest shape is a **stateless query port** on PLACE
reusing `place32`/`units_of`, so PREPARE and EMIT are bit-identical *by
construction* rather than by two implementations agreeing.

**BLOCKER A — A PREPARE PASS TEED OFF THE ISSUE PORT CANNOT TERMINATE.** The
correction above says the enumerator *"EXISTS AND IS COMPOSED"*. It does, and it
cannot be used the way that implies. PREPARE must hold the whole admitted set
before EMIT opens; the issue stream is one-shot (`is_valid_o == (st == S_ISSUE)`,
the record overwritten on the next fetch, frame state wiped on `fr_start_i`, and
architecture 2.5 rejecting the persistent cache a replay would need). So a walker
must drink the stream as it flies — but that stream's ready **is the compose
cache's**: `tis_ready = thr_j_ready && tce_can_start`, `tce_can_start =
!tcc_fill_busy`, against a front that holds exactly two lattices. Holding
tessellation back until the last patch is issued stalls the compose, drops
`tce_can_start`, stalls the issue port, and the walk never reaches the last
patch. **The walker must be a second, independent reader of the sealed list that
never touches the compose spine** — a different and larger block.

**BLOCKER B — STEP 2'S COST IS BANDWIDTH, NOT AREA, AND IT IS UNCOSTED.** Owner
ruling **R242 moved `zhao_terrain_devstore` to SDRAM on 2026-09-22 — the same day
this contract was written** — and step 2 above still describes the M10K store.
`zhao_terrain_devstore.sv:48-53`: *"Neither array exists any more; this block now
infers NO MEMORY AT ALL … 320 KiB of local SDRAM."* A read costs two full 64-byte
bursts before the first descriptor and another every four (`:880-883`). A PREPARE
pass over 256 admitted patches is **~1,280 extra 64-byte SDRAM reads per frame,
on top of the identical ~1,280 EMIT already spends.** That, and not the ~3 M10K
bank sized above, is what decides whether this walker is affordable. R242 also
made devstore's `w_ready_o` fall during a burst, which already made one histogram
count a single record 86 times; a walker joining against that ready inherits it.

### BLOCKER B IS NOW COSTED — packet EDGEBAND, 2026-09-23

**The ~1,280 HOLDS, and it is not an estimate — it is exactly 5 × 256.** Derived
twice independently, which is why the suspiciously round number is not a tell
here:

* **By hand off the read FSM.** `R_IDLE → R_HREQ` issues ONE history burst;
  `R_HWAIT → R_DREQ` issues the first record burst; `R_STREAM` re-enters
  `R_DREQ` whenever `r_ptr_q[1:0] == 3`, i.e. once per four of the sixteen
  records. 1 + 4 = **5 reads per patch**. `R_HWR` issues ONE history WRITE
  burst, taken only when the LOD pass emitted a decision (`h_any_q ||
  h_valid_i`). The slot footprint corroborates it from the layout side:
  `zhao_terrain_devstore.sv:129-137`, 256 B of deviations + 64 B of history =
  320 B = 5 × 64.
* **By the committed directed test.** `terrain_lodpath_directed` case 8 already
  asserts `rd1 + 5 == bursts_read_o` — *"a patch read is ONE history burst plus
  FOUR record bursts"*. EDGEBAND built and ran it: **472 checks, 0 failures**,
  case 8 printing *"55 bursts read, 22 written, 77 guard requests; a patch read
  cost 660 clocks of memory wait in total"*. 55 / 5 = 11 patch reads.

**THE CONVERSION THE TREE'S PROSE OMITS, AND IT IS A FACTOR OF FOUR.** Every
bytes-per-frame figure in this repository is written in 64-byte *fabric
requests*. The SDRAM is a **16-bit bus with BURST_LENGTH 8**
(`zhao_sdram_params_pkg.sv:38,72`), so one SDRAM burst moves **16 bytes** and
`zhao_vram_arbiter.sv:167` splits *"a 64-byte request one burst further"* — into
**four**. A "1,280-burst" pass is **5,120 SDRAM bursts**. Converting at the
controller's own cycle-exact grant-to-grant spans (`zhao_sdram_ctrl.sv:30-31`,
read 12/15/18 hit/miss/conflict):

| | bytes/frame | SDRAM grant-clocks | % of a 1,666,666-cycle frame |
|---|---|---|---|
| PREPARE, page-hit (flattering) | 80 KiB | **61,440** | **3.69%** |
| PREPARE, bank-conflict (budget against this) | 80 KiB | **92,160** | **5.53%** |

**So PREPARE's own cost is small, exact and affordable: +80 KiB/frame,
≈ +4.9 MB/s at 60 Hz, +3.7% to +5.5% of the frame's SDRAM cycles.** That is the
number P2 asked for.

**AND THE FRAME STILL MAY NOT HAVE IT, FOR A REASON THAT IS NOT PREPARE'S
FAULT.** The comfortable answer here is "there is plenty of headroom", so it got
the extra five minutes. Two findings survive it:

**(1) The denominator does not exist.** The *"Phase-0 bandwidth matrix"* that
`spec/terrain_rules.md:496-500` and three other documents cost themselves
against is an **empty directory** — `reports/bandwidth/` holds one 0-byte
`.gitkeep` from the 2026-08-14 skeleton commit, and
`reports/digests/LANE2-TERRAIN-8KM.md:22-25` and `reports/DOCKET.md:2655`
already call citing it a phantom citation. **No tool under `tools/` computes
bandwidth**, and `tests/memory/mem_bandwidth_budget.cpp` — named like a budget —
is a *starvation* test that asserts `scanout_preempted == 0` and prints burst
counts as "informational". It never compares a total against a ceiling. It also
**predates R242 by five weeks** (2026-08-18) and models two clients, neither of
them terrain.

`tools/budget/sdram_bandwidth.py` is the missing half, committed by this packet.
It adds up only figures the tree already declares, in the unit the tree already
uses, and its self-test proves it can report a **shortfall** and not merely a
surplus.

**(2) Summing the declared numerators for the first time puts the frame OVER,
and it was over before PREPARE was proposed.** At bank-conflict spans:

```
TOTAL COMMITTED, WITHOUT PREPARE   2,072,128 grant-clocks   124.33% of frame
TOTAL COMMITTED, WITH PREPARE      2,164,288 grant-clocks   129.86% of frame
```

The two rows that dominate are **TERRAIN bake (43.9%)** and **TERRAIN streaming
(41.0%)** — the two independent provisional ~41 MB/s figures that
`LANE2-TERRAIN-8KM.md:385-389` recorded on 2026-09-03 as *"nothing anywhere adds
them"*. Adding them is what puts the frame over. Both are self-flagged *"do not
freeze"* / *"Affordability: NOT COSTED"*, and nothing states that a
worst-case-streaming frame and a worst-case-baking frame co-occur — so this is
a worst-on-worst, not a prediction. At page-hit spans everything fits at 83.86%.
**The honest range is that wide, and closing it needs ZH-004, not more
arithmetic.**

**THE BINDING CONSTRAINT IS NOT BYTES — IT IS THAT DEVSTORE RIDES A BACKGROUND
CLIENT.** `zhao_mem_guard.sv:591-594` passes `devstore_rd_ok` /
`devstore_wr_ok` for `ZHAO_CLIENT_TERRAIN_BUILD` **and no other arm**, and
ruling T3 makes client 6 *"served only when NOTHING else is pending, DEBUG
included"* (`zhao_vram_arbiter.sv:33-41`). Its budget is therefore the **idle
residue**, not a share of the frame, so a row reading "3.69% of frame" invites
exactly the wrong reading:

| | page-hit | bank-conflict |
|---|---|---|
| idle residue left for TERRAIN_BUILD | 1,303,114 | 1,118,794 |
| TERRAIN_BUILD wants, **without** PREPARE | 972,640 (74.6%) | 1,524,256 (136.2%) |
| TERRAIN_BUILD wants, **with** PREPARE | 1,034,080 (79.4%) | 1,616,416 (144.5%) |

**PREPARE moves the background client from 74.6% → 79.4% of its residue in the
flattering case, and from 136.2% → 144.5% in the unflattering one.** It is never
the thing that breaks the frame, and it is never free. `spec/memory_rules.md:396-402`
already predicted the failure mode in its own voice — *"The record READ is
frame-critical … while client 6 is T3's background class … A frame whose page
loads saturate the socket therefore **delays** LOD decisions rather than
corrupting them"* — and named the instrument, `rd_wait_clocks_o`. **PREPARE
doubles the frame-critical read demand on the one client that is ruled first to
starve.**

**VERDICT FOR P2: the bandwidth is affordable and the measurement is no longer
the gate.** P2 may proceed on bandwidth grounds. What it must carry instead:

* **A `rd_wait_clocks_o` budget, not a burst budget.** The bench figure of 660
  clocks for 11 patch reads is **12 clocks per 64-byte burst on a played fabric
  running at 2-cycle read latency** (`design/blocks.yml:2613`). A real request
  is four SDRAM bursts at 12–18 grant-clocks, so the honest per-request figure
  is **48–72** and the bench floor understates by 4–6×. **Do not quote 660 as
  the cost of anything.**
* **PREPARE MUST SUPPRESS THE HISTORY WRITEBACK, and this is new.** A PREPARE
  teed naively off `r_start_i` inherits `R_HWR`, so it writes each patch's
  history row back a second time per frame — and EMIT, which reads history at
  `R_HREQ` *before* its own LOD runs, would then read PREPARE's write as *"the
  previous frame's"* level. That is a hysteresis corruption with every counter
  balancing, in the same family as blocker C, and it is a **correctness** defect
  rather than the 256 extra write bursts it also costs. It belongs beside P3's
  governor freeze.

**ONE READING TRAP TO LEAVE CLOSED.** The "dies on measurement" paragraph below
costs two LOD passes against a 1.67 M-clock frame and is **correct** — that is
the *compute* budget (100 MHz ÷ 60, `design/budgets/workloads.yml:51`). The
bandwidth arithmetic above uses 1,666,666 *SDRAM* cycles, which coincides only
because both clocks are quoted at 100 MHz. `design/budgets/latency.md:50` warns
that the tree carries **two "cycles per frame" numbers differing 6.6×** — the
other being the per-mode video deadline (`zhao_pkg.sv` `frame_gpu_cycles`:
251,520 / 217,984 / **318,592** Duo). Neither figure above is that one, and
ZH-004 could move the two clocks independently.

**BLOCKER C — THE DETERMINISM PREMISE IS NOT SATISFIED BY THE CURRENT
COMPOSITION.** Step 3 rests on *"identical `sp_*` and identical governor targets
give identical `lvl[]`"*. In `zhao_console_core.sv` the held governor targets
`gv_*_q` are captured under an `else if (tld_idle)` arm, and `tld_idle` is
TERRAIN.LOD's own `idle_o` — high **between every patch**. They are therefore
re-sampled ~256 times a frame. MEASURE.GOVERNOR itself decides on `frame_i`, a
frame-boundary pulse, so `mgv_*` is stable within a frame and this is harmless
today; but `veye0_*`/`veye1_*` come from the view block, and a PREPARE pass would
sample them at a different point in the frame from EMIT. **Any motion between the
two passes banks a level the patch does not tessellate at — a crack, with every
counter balancing.** The walker owes a frame-scoped freeze of `gv_*_q`, which is a
behaviour change to composed, working serve-path code.

**AND ONE OBJECTION THAT DIES ON MEASUREMENT**, recorded so it is not raised
again. The ledger sets TERRAIN.LOD at *"1 decision per patch per frame"*, and
`zhao_terrain_lod.sv:52-56` justifies choosing the 32-step isqrt over a
squared-domain multiply **by that rate** — so a second pass reads at first like a
breach. It is not. `:157-159` records ~784 clocks a patch and that 256 live
patches is *"still about 8x the required rate"*. Two passes are 401,408 clocks of
a 1.67 M-clock frame — ~24%, with ~4x margin remaining. **Throughput is not the
blocker.** What step 3 *does* owe is the time-share: TERRAIN.LOD has no mode,
bypass or phase input, so PREPARE-vs-EMIT selection across twelve `sp_*` inputs
and fourteen `out_*` outputs must be a **named block**, never composer wires.

**THE REMAINDER, AS FOUR PACKETS.** None of them may compose this block alone.

| | work | fit |
|---|---|---|
| **P1** | per-slot `pitch_log2` retention from TERRAIN.HDRREAD + a stateless query port on TERRAIN.PLACE. A port change on a composed block: regenerate `gen_prod_top`, `gen_console_board`, `gen_shell_paired_diff`, and connect every bench instantiating PLACE. | none |
| **P2** | the sealed-list PREPARE reader — step 1's real shape. **Bandwidth gate LIFTED 2026-09-23 (EDGEBAND): +80 KiB/frame, +3.7–5.5% of frame SDRAM cycles, affordable.** Now carries instead: suppress the `R_HWR` history writeback (a correctness defect, see blocker B above), and budget `rd_wait_clocks_o` rather than bursts. | none |
| **P3** | the LOD time-share block and the frame-scoped governor freeze. | none |
| **P4** | compose this block, wire `edge_*`, land an **acceptance bench** on `tests/prod/partmat_acceptance.cpp`'s pattern. | **one** |

P4's fit question, named in advance: *does the terrain island still close at NCTX
with the walker, the pitch table and EDGERECON added?*

**The console smoke cannot be P4's evidence.** It fails every terrain page's CRC,
so no page becomes resident, `zhao_terrain_devstore` holds no record, and the
walker is quiescent — the ruling's own *"an otherwise green smoke whose upstream
fixture never reaches the new path does not prove the path."*

**OPEN OWNER QUESTION, and nobody has asked it.** May
`zhao_terrain_island_dir`'s **frame-scoped** `desc_pitch_log2_i` (`:70`) be
ratified as authoritative for every page of its island, rather than each page's
own header field? `zhao_terrain_hdrread.sv:31-35` records that these are two
different fields today. If the island's is authoritative, P1's per-slot table
disappears and P1 halves. If it is not, the table is owed and the walker must
carry it.

### BUILT 2026-09-25 (EDGEPREP). P2 AND P3 EXIST; P1 IS DELETED BY RULING

Owner directive 2026-09-23 section 2 decided the open question this contract
records as *"OPEN OWNER QUESTION, and nobody has asked it"*:

> *"the frame-sealed island descriptor's pitch_log2 is authoritative for EVERY
> page belonging to that island generation. The page-header pitch remains a
> checked redundant value, not an independent source of placement law. This
> resolves D-2 and removes the need for the proposed per-resident-slot pitch
> table."*

**That deletes P1.** The per-slot `pitch_log2` table is not owed. The pitch is
a frame-scoped input (`frz_pitch_log2_i`) and the page-header value becomes a
CHECK to be made at admission/load — see "what P4 still owes", below, because
that check is not built and this packet did not build it.

**P2 is `fpga/rtl/terrain/zhao_terrain_prepwalk.sv`** and **P3 is
`fpga/rtl/terrain/zhao_terrain_lodshare.sv`**. Neither is composed; P4 composes
all three blocks together and spends the plan's one fit.

#### The replay mechanism was already in the tree, and it is the list's address

The directive asks for *"a replayable sealed-list PREPARE reader independent of
compose-cache backpressure"*. **`zhao_terrain_cmd` already reads the sealed list
TWICE, every frame, by design** — *"pass one folds the CRC and emits nothing;
pass two emits and folds nothing"* — because ruling T5 makes the list CAPTURE
DATA at a known address with a known length and CRC. That is the definition of
replayable, and blocker A dissolves against it: `zhao_terrain_prepwalk` is a
THIRD reader of the same bytes whose ready is its OWN consumer's, so the compose
spine is nowhere in its loop.

It is a separate block rather than a third pass inside `zhao_terrain_cmd`
because that block is COMPOSED: new output ports on it would dangle at
`zhao_console_core`'s boundary until P4 wired them and put the completion
register UP, which is the trade R75 endorses refusing.

#### The sp_cx/sp_cz decision, settled by REMOVING the choice

This contract lists three candidates and EDGERECON2 refused all three. The
answer taken is the fourth shape that findings named: **the placement law moved
into `zhao_terrain_place_law_pkg`**, a package holding `place32`, `units_fits`,
`units_of` and `pitch_legal`, moved VERBATIM out of `zhao_terrain_place`, which
now calls them. PREPARE and EMIT are bit-identical **by construction** rather
than by two implementations agreeing, which is the property the symmetry law
needs and the one a second implementation cannot promise.

A package holds no state, has no ports, costs no ALMs and cannot be composed or
left disconnected, so it is not a second PROVIDER: `zhao_terrain_place` still
owns header acceptance, the envelope check, the pitch refusal, the census and
the 66-write compose fill. What moved is the shifter, which was never the
provider. No port changed, so nothing regenerates. Behaviour-neutrality is
measured, not asserted: `terrain_place_directed` (68), `terrain_heighttap_
directed` (2,272) and `surface_dispatch_directed` (1,072 checks, with
`pitch_refused=3`) all pass unchanged.

#### Blocker C survives, and its "harmless today" half does NOT

The re-latch is confirmed at `zhao_console_core.sv:26414`: thirteen `gv_*_q`
captured under `else if (tld_idle)`, re-sampled ~256 times a frame. **The claim
that it is harmless today is false for six of the thirteen.** `veye0_*`/
`veye1_*` come from `zhao_view_eye`, whose registers update on `cfg_we_i` —
*"a pulse the executor owns, and a write lands the cycle it is"* (`:43`,
`:123`). They are HOST-WRITE-SCOPED, not frame-scoped. **The SINGLE pass is
already sampling a moving camera and nothing was watching it.**
`zhao_terrain_lodshare` freezes all thirteen on `frame_i` and `freeze_drift_o`
measures the motion the old arrangement absorbed silently.

#### The history writeback: suppressed, and devstore needed NO change

`zhao_terrain_devstore` enters `R_HWR` on `(h_any_q || h_valid_i)` and
`h_any_q` is set ONLY inside `if (h_valid_i)` (`:859-865`, `:877`); the
producer of `h_valid` is `zhao_terrain_jobissue` (`:343`). The time-share
routes PREPARE's ladder output to the RECONCILER and not to jobissue, so
jobissue is starved for the whole pass and emits no history at all — devstore's
own comment already describes it: *"A patch whose LOD pass emitted NOTHING
skips the write entirely."*

The port is nevertheless routed THROUGH the time-share and gated, because a
correctness property that holds through somebody else's wiring is the kind an
innocent edit breaks in silence — and because the gate gives `hist_leak_o`
somewhere to live.

#### Two defects found in this packet's own new RTL, both recorded

* **`prep_begin_o` was a LEVEL.** This block documents `frame_begin_i` as a
  PULSE and says *"`frame_begin_i` during a sweep RESTARTS it rather than
  queueing"*. A level held while waiting for the phase gate would have
  restarted the 257-clock sweep every cycle — the sweep never completes, the
  gate never rises, and the two blocks hold each other still FOR EVER, with
  every counter reading zero. It is one cycle wide now.
* **`idq_overflow_o` counted the guard CORRECTLY REFUSING**, not an overflow. A
  counter named for a fault that fires on correct behaviour trains its reader
  to ignore it.

#### Evidence

| | |
|---|---|
| `terrain_prepwalk_directed` | **85 checks, 0 failures**, twelve cases |
| `terrain_lodshare_directed` | **96 checks, 0 failures**, nine cases |
| `terrain_lodshare_mutant` | `idq_overflow_o` fired **12x**; positive control `idq_full_stalls_o` = 12; INVERTED polarity |
| `check_quartus17_syntax.py` | clean — it caught a second `import` in a module header that Verilator accepts and Quartus 17.0 rejects |
| `verilator --lint-only -Wall` | 0 diagnostics on both blocks |

**R212 for both new blocks: neither has been through `quartus_map`.** Lint is
one tool's opinion. P4's fit is where that is settled.

**Counters.** `zhao_terrain_prepwalk` has eighteen, ALL fired from the block's
own boundary with legal stimulus, each with a control — so no mutant is owed
there and that is measured rather than omitted. `zhao_terrain_lodshare` has
twelve; eleven fire with stimulus and the twelfth,`idq_overflow_o`, is
unreachable by construction and owns the committed mutant.

**`store_wait_clocks_o` is the budget instrument, not a burst count** — the
thing P2 was told to carry. `spec/memory_rules.md:396-402` named it first.
**Do not quote `terrain_lodpath_directed` case 8's 660 clocks as the cost of
anything**: it is 12 clocks per 64-byte burst on a played fabric at 2-cycle
read latency, and a real request is four SDRAM bursts at 12–18, so that floor
understates by 4–6x.

**Bandwidth.** `tools/budget/sdram_bandwidth.py` gained one row and one
correction. `devstore_prepare()`'s zero write demand is no longer a REQUIREMENT
on a future packet but a built, measured property. `prepare_list_reread()` is
new and is **deliberately not in the total**: the third list pass is 8 KiB a
frame on the **HPS-DDR bridge**, a different socket from the local SDRAM this
ledger adds up, and folding it in would teach the tool that the two are
interchangeable. Totals are unchanged and still reproduce EDGEBAND's:
**124.33% → 129.86% at bank-conflict spans, 83.86% at page-hit.**

### WHAT P4 STILL OWES, NAMED RATHER THAN LEFT TO BE REDISCOVERED

1. **Compose all three blocks together** and wire `edge_*`. Composing any one
   alone dangles ports and puts the register UP.
2. **The admission-time pitch/identity/envelope check the directive requires** —
   *"Check page pitch, island identity and declared origin/envelope consistently
   at admission/load. Refuse and count a mismatch before publishing residency;
   do not silently rescale a page."* **THIS IS NOT BUILT.** `zhao_terrain_place`
   checks the envelope against the PAGE HEADER's pitch; nothing compares the
   header's pitch against the ISLAND descriptor's, which is what the directive
   now makes authoritative. A page whose header disagrees would be placed one
   way in EMIT and another in PREPARE — a crack. The comparator is small and
   belongs in the page-load path.
3. **A producer for `frz_pitch_log2_i`.** `zhao_terrain_island_dir` is **not
   instantiated in `zhao_console_core` at all** — measured, not assumed — so the
   frame-sealed island descriptor has no live producer in the console. P4 must
   supply one rather than tie the port.
4. **A producer for `frz_tok_i`**, the freeze witness: a residency/bake
   generation that changes when either moves. `restart_req_o` is a detected
   restart REQUEST and the caller decides what to do with it.
5. **`prep_gate_i` from the reconciler's `phase_o == 1`**, and `prep_sel_i` from
   the walker's `busy_o`.
6. **The EMIT-side query driver**: the time-share files during PREPARE, but who
   QUERIES `zhao_terrain_edgerecon` during EMIT, with which `q_ix_i`/`q_iz_i`,
   is still the caller's business — this contract's original exclusion stands.
7. **SDRAM scheduling, directive section 7.** Bounded guaranteed service for
   the frame-critical devstore reads. **The reserved client 5 is NOT available
   in the live design** — `zhao_vram_arbiter.sv:353` forces `port_grant[5]` low
   and `:246` records that it appears in no selector arm, while
   `zhao_mem_guard`'s `default` arm grants it nothing. Spending it means
   changing two proven blocks, re-proving the arbitration bound B with all
   clients enabled, and extending `tests/formal/mem_guard_no_escape.sby`. This
   packet measured it and did not spend it, because the route only matters once
   PREPARE actually issues reads in a composed console — doing it now would be
   BUILT, INSTALLED NOWHERE against a path that does not yet exist.
8. **The acceptance bench**, on `tests/prod/partmat_acceptance.cpp`'s pattern,
   covering what the directive names: nondegenerate terrain fixtures, mixed
   neighbouring LODs, both views, changing camera state, deformation, missing
   pages, history and backpressure. **The console smoke cannot be the
   evidence**: its 128 replayed terrain triangles are all degenerate, so it
   cannot evidence a tessellation result, and the directive says to fix the
   fixture rather than repeat the older claim.

P4's fit question is unchanged and already named: *does the terrain island
still close at NCTX with the walker, the time-share and EDGERECON added?*

### LANDED 2026-09-25 (EDGECLOSE). P4 IS DONE; BOTH REGISTER ENTRIES CLOSED

`zhao_terrain_edgerecon` is composed in `zhao_console_core`, entry I21 is
DELETED from the INCOMPLETE block, and the completion register went **11 → 9**.
The four `edge_*` literals are gone from `u_terrain_lod`.

**Six blocks landed together**, because each is the next one's producer and
composing any one alone dangles a port — the trade R75 endorses refusing and
the reason this block sat BUILT AND DISCONNECTED for three days.

| | what it is | which owed item |
|---|---|---|
| `zhao_terrain_islandseal` | the island pitch SEAL and the admission check | 2 **and** 3 |
| `zhao_terrain_prepwalk` | the PREPARE walker (P2, already built) | 1 |
| `zhao_terrain_prepshare` | the two borrowed ports, arbitrated | 7 |
| `zhao_terrain_lodshare` | the time-share and the freeze (P3, already built) | 1 |
| `zhao_terrain_edgerecon` | this block | 1 |
| `zhao_terrain_edgequery` | the EMIT-side query driver | 6 |

Items 4 and 5 are console wiring: `frz_tok_i` is a witness counter moved by a
residency publication, a deformation mark or an island-seal refusal, and
`prep_gate_i`/`prep_sel_i` come from `phase_o` and the drain below.

#### THE ISLAND PITCH HAD NO PRODUCER, AND `zhao_terrain_island_dir` IS NOT ONE

The directive made the island descriptor's `pitch_log2` authoritative. Measured
rather than inherited: `zhao_terrain_island_dir` is not instantiated in the
console **and would not help if it were** — `desc_pitch_log2_i` is an *input* of
that block (`:70`). It CONSUMES a descriptor and produces no pitch, and
TERRAIN.ISLAND is already `superseded by a ruling` in the register besides. The
only ratified carrier of an island table is `TerrainEpoch 0x0220`'s
`island_table_handle`, and that command is `reserved` in `spec/commands.zidl:616`.

`zhao_terrain_islandseal` **seals** the pitch from the first legal page header
of an island generation and thereafter **checks** every header against it —
pitch, island identity, and the envelope origin recomputed through
`zhao_terrain_place_law_pkg`, the same functions `zhao_terrain_place` calls.
Spec 1.5 gives an island one pitch and spec 2.1 +2 says the header's copy "must
match the island table", so the two are the same number by construction and the
COMPARISON was what was missing, not a register. A refusal moves the freeze
witness, so the frame falls back rather than combining a patch placed one way in
PREPARE and another in EMIT. Seven counters say which field disagreed.

#### CLIENT 5 IS STILL NOT SPENT, AND THE SHARED ROUTE COST NOTHING

Re-measured: `zhao_vram_arbiter.sv:353` still forces `port_grant[5]` low.
**PREPARE's devstore reads ARE devstore's own reads** — requester 4 of
`u_build_share` under `ZHAO_CLIENT_TERRAIN_BUILD`, the same socket with the same
scoped permission, because they are literally the same port. No new client, no
arbiter change, no guard change, and `tests/formal/mem_guard_no_escape.sby` does
not move. The sealed list's re-read is HPS client 7 (the arbiter widened 7 → 8),
read-only, at the HIGHEST index on purpose: the arbiter starves high indices
first and PREPARE is the one reader whose failure is a counted fallback.

#### THE FIT QUESTION AS WRITTEN NAMES A TARGET THAT DOES NOT EXIST

This contract named P4's fit in advance as *"does the terrain island still close
at NCTX with the walker, the pitch table and EDGERECON added?"* **Two of those
three nouns are wrong**, and saying so is cheaper than running the wrong fit:

* there is **no `terrain island` fit target**. `design/fit_targets.yml` has
  `zhao_texture_island_top` and `zhao_texture_island_v3_top` and no terrain one;
* **`NCTX` is a texture-island parameter** (`zhao_raster_rcp24_svc`), not a
  terrain one;
* **the pitch table does not exist** — the directive deleted P1.

The question the six blocks actually raise is an AREA and Fmax question about
the composed console, and the target that answers it is `zhao_console_core`,
which has a successful prior row to difference against
(`@console-core-first-light`: 47,582 ALM, 151 DSP, 306 RAM blocks, 56,031
registers, gpu_clk 18.5 MHz, on the 5CEBA9F31C7 SIZING device). **That** is the
one fit this plan spends, as `@edgeclose`, and the five resource categories are
kept apart as `design/budgets` requires.

#### THE ACCEPTANCE BENCH, AND THE THREE DEFECTS IT FOUND

`tests/prod/terrain_edge_acceptance` — 90 checks, 0 failures, seven production
modules, on `tests/prod/partmat_acceptance.cpp`'s pattern. It asks one question:

```
SECTION 1 (control, no PREPARE -- the console as it shipped)
  seam(0,0)+x / (1,0)-x: A=[1 1 1 1] B=[3 3 3 3] DISAGREE
SECTION 2 (the composed producer)
  seam(0,0)+x / (1,0)-x: A=[3 3 3 3] B=[3 3 3 3] AGREE
  control=[1 1 1 1] -> producer=[3 3 3 3]
```

That is `max(neighbour, own)` computed as `zhao_terrain_tess` computes it, so a
failure IS a crack. The "a level actually moved" assertion is separate and is
what stops the bench going green on a fixture that could not have failed.

**Composing against the REAL ladder for the first time found three defects, all
silent, none visible to any existing gate:**

1. **`IDQ_DEPTH = 4` WAS A DEADLOCK.** This block's own header claimed
   `zhao_terrain_lod` is *"a sequential ladder with ONE descriptor in flight"*.
   It is not: `zhao_terrain_lod.sv:671-684` accepts **all sixteen** subpatches
   before emitting any, because a subpatch's four interior neighbours need the
   whole `lvl[]` array. The queue filled on the fourth descriptor, the producer
   stalled, the ladder never reached its sixteenth, and the two blocks held each
   other still **with every counter reading zero**. `terrain_lodshare_directed`
   passed 96 checks against this because it drives a ladder MODEL that emits per
   descriptor — *"a gate that cannot reach the state is not evidence about the
   state"*.
2. **THE IDENTITY QUEUE POPPED TWICE PER SUBPATCH ON A DUAL PAGE.** `StEmit` is
   interleaved per subpatch (`:715-723`) — top, underside, advance — so two
   beats carry one identity. The queue emptied halfway through a dual patch,
   `ident_mismatch_o` fired, the file was refused, and every seam that patch
   touched fell back. Defect 1 was masking it.
3. **`prep_sel_i(busy_o)` AND `prepare_done_i(prep_done_o)` LOSE THE LAST PATCH
   OF EVERY FRAME.** `busy_o` is `(state != P_IDLE)` and the walker reaches idle
   when its sixteenth descriptor is ACCEPTED, while the ladder still holds all
   sixteen. Ownership flipped mid-patch and the bank froze before the last
   sixteen lanes were filed. Both are now gated on the time-share being
   **drained** (`idle_o`), which is the block's own statement and not a timeout.

#### ONE THING THE FALLBACK STILL DOES NOT BUY, asserted rather than assumed

Section 3 drives a **missing page** and the seam across it **still disagrees**:
both sides get `8'h00`, and `max(0, own) == own`, so the two patches' own levels
differ. **The symmetry law holds and crack freedom does not** — which is the
directive's own sentence about a shared sentinel, now measured on the live path.
In the console a non-resident patch is not composed either, so it presents no
geometry for the seam to crack against; the bench emits it anyway, which is why
the assertion is about the sentinel and the counters rather than about
agreement.

#### WHAT THIS PACKET DID NOT DO

* **`zhao_terrain_island_dir` is still not composed**, and is not owed: the
  register records TERRAIN.ISLAND as superseded, and the block consumes a
  descriptor rather than producing one.
* **The console smoke still replays 128 degenerate triangles.** Measured this
  session, the cause is NOT the one the smoke's own comment gives (*"a flat zero
  height field"*): a flat but PLACED lattice has a non-zero cross product,
  because `zhao_terrain_place` supplies distinct world x/z from the patch
  coordinate with no page payload reaching placement. The real cause is that the
  bench writes only the 64-byte header, so the compose cache never fills and
  `zhao_terrain_compcache_front` returns POISON on all three lanes. Fixing that
  fixture is the smoke's packet, not this one; the acceptance bench does not go
  through the compose cache at all.
* **`zhao_terrain_edgequery` owes no committed mutant**, and that is measured:
  every one of its twelve counters is fired by the acceptance bench or by a
  named fallback path with legal stimulus. `door_src_unknown_o` is the one that
  is unreachable in a well-ordered frame and it is reachable by construction —
  a door whose identity matches neither held record — rather than by a mutation.

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
