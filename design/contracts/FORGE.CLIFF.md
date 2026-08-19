# Contract — FORGE.CLIFF (Terrain cliffs and skirts)

> Ledger: `design/blocks.yml` · owner ZH-067 · phase 6 · maturity REFERENCE_COMPLETE

## Purpose and exclusions

Generate cliff/skirt geometry hanging off tessellated terrain edges (hides LOD seams at world edges).

**What landed, and what did not — stated before anything else.** The ledger's
`reference_model` for this block is `zref::forge::rim_plan`, which answers
exactly one question: WHICH RIM EDGES GET A WALL, and with what span.
`zhao_forge_cliff` is that function, complete and bit-for-bit — enumeration,
the merge degrade and the priority degrade.

It does **not** turn a rim edge into wall VERTICES. That second half needs
three things this block's ledger inputs do not give it:

* the composed top lattice "**exactly as emitted by the tessellator's stitched
  edge set** at the owning subpatch's LOD level" (§5) — that is
  `TERRAIN.TESS`'s stitched output;
* the bottom-lattice values of the same two vertices — `TERRAIN.PATCH`'s layer
  C;
* the **accumulated rim length** that drives the strata U, which §6.6 defines
  as a running sum "in lattice scan order … reset per 32×32-cell page".

None of those is a port this block has, and the third is the interesting one:
it is a stateful accumulation across the emitted plan, so it belongs to the
emission stage and not to the planner. **The emission stage is NOT WRITTEN.**
Recorded here rather than left for a reader to infer from a missing test, and
the block's counter is still `triangles_submitted` because the plan is what
decides how many wall triangles there will be (chosen law C3).

## Law FOUND versus law CHOSEN

**FOUND (spec/terrain_rules.md §5, frozen 2026-08-16, reference-tested;
`zref::forge::rim_plan` is the executed answer and this block is
REFERENCE_COMPLETE against it):**

* a rim edge is "a lattice edge between a SOLID cell and a void/OUT neighbour
  (4-neighbourhood, axis-aligned in v1)";
* the budget and the whole degrade are **per 32×32-cell PAGE** — `rim_plan`
  loops `pj`/`pi` in steps of 32 and builds a fresh working set inside each;
* the scan order is `cj` outer, `ci` inner, `side` 0..3, and it is load-bearing
  twice: it decides which edges are adjacent for the run test, and it is the
  tie-break for the priority degrade;
* degrade step 1 merges CONTIGUOUS collinear runs, longest first, ties
  earliest, and **sheds the MINIMUM** — the last merge takes a PREFIX of its
  run, not the whole run;
* degrade step 2 keeps the edges with the greatest max-endpoint `vdist`, ties
  by scan order, and a **null `vdist` means priority 0 everywhere**, which
  under a stable sort is exactly "keep scan order";
* `dropped` counts **BODIES**, not entries: a dropped merged span takes its
  whole span with it, which is what makes `emitted_bodies + dropped ==
  enumerated` an identity;
* the budget is 512 (`zref::forge::kRimBudgetPerPage`) and the TIGHT
  checkerboard worst case is 2,048 edges per page.

**Three things a reimplementation gets wrong**, each carried in the RTL header
with the same numbering:

* **R1 — the minimum shed.** `take = best_len; if (take - 1 > need) take =
  need + 1;`. Two 20-edge runs under a need of 31 give one 20-span and one
  **13**-span.
* **R2 — `dropped` counts bodies.**
* **R3 — a run is CONSECUTIVE IN THE EMITTED ARRAY**, not merely collinear in
  the lattice. `build_runs` compares `es[j]` with `es[j-1]` and breaks on a
  side change, so in the `(cj, ci, side)` scan order a run forms only where
  each cell along the line contributes EXACTLY ONE edge, of the same side — a
  straight wall. It is why the 32×32 checkerboard, whose every solid cell
  contributes four edges, merges **nothing at all**.

**One thing the reference does that the hardware must NOT copy — and the small
theorem that lets it not.** `rim_plan` rebuilds every run from scratch on every
merge iteration; with `need` up to 1,536 and 2,048 entries that is ~3 M steps
per page, twice a 1.67 M-clock frame, for one page. It is also unnecessary:

> **The set of maximal runs does not change when one of them is merged.**
> Merging the run at index `b` with `take` entries kills `b+1 … b+take-1` and
> sets `es[b].span = take`. Those dead entries lie strictly INSIDE a run that
> was already maximal, so they break no adjacency that was not already broken.
> The head gains no new neighbour either: for the entry BEFORE `b` the
> contiguity test reads `prev.ci + prev.span` of the PREVIOUS entry, which is
> unchanged; for the entry after the run's end the test becomes
> `cur.ci == es[b].ci + take`, numerically identical to the pre-merge test
> `cur.ci == es[b+take-1].ci + 1` that maximality already failed. And a PARTIAL
> merge (`take < len`) can only happen on the LAST iteration, because it drives
> `need` to zero.

So the runs are built ONCE and the loop becomes "process runs in descending
length, ties by ascending start" — a counting sort with 31 buckets, `O(31 ×
runs)` instead of `O(need × edges)`, selecting the same run in the same order
every time. The equivalence is the entire basis of the block's control and it
is asserted, not assumed: the 96×96 directed fixture reproduces the reference's
own 20-span + 13-prefix result, and lane L drives 20 more merge-only pages.

**CHOSEN**, each with the alternative that was rejected:

* **C1 — the cell substance arrives as a 34×34 SOLID BITMAP** (the page plus a
  one-cell halo), streamed row-major at the start of a page. `is_rim_edge`
  answers TRUE for an OUT neighbour and TRUE for a non-SOLID neighbour and
  never distinguishes them afterwards, so ONE BIT PER CELL is a **faithful**
  encoding of the predicate's whole input, and a cell outside the lattice
  loading as 0 is exactly right. *Rejected:* a read port into `TERRAIN.PATCH`'s
  layer-D plane, queried four times per cell — 16,384 reads per page against
  1,156, contention with `TERRAIN.PATCH`'s own consumers, and this block's
  timing made dependent on a memory it does not own.
* **C2 — the priority is computed once per edge and stored.** The threshold
  search is 32 counting passes and each must compare every live edge;
  recomputing costs two `vdist` reads per edge per pass (32 × 2,048 × 2 = 131 k
  reads per degraded page) against 4,096 once. The price is a 2,048 × 32-bit
  table, about 7 M10K. *Rejected:* recomputing per pass — it saves the M10Ks
  and spends the cycles in the path that is already the pathological one.
* **C3 — `triangles_submitted` counts TWO per emitted edge.** §5 says "one quad
  per rim edge" and "the governor sees wall quads as ordinary
  triangles_submitted"; a quad is two triangles. *Rejected:* counting edges,
  which would make this block's contribution to a shared counter mean something
  different from every other block's.
* **C4 — `page_merged_o` / `page_dropped_o` are STATUS OUTPUTS, not counters.**
  They are `RimPlan.merged` and `RimPlan.dropped` for the page just emitted,
  held from `page_done_o` until the next page starts. The ledger gives this
  block one counter and the counter catalog is a frozen index space
  (spec/counters.md §2), so minting two more counter ids here would be a ledger
  edit dressed as an RTL edit.
* **C5 — one page in flight.** `cmd_ready_o` is `st == StIdle`. *Rejected:*
  double-buffering the edge table to overlap emission with the next page's
  enumeration — it doubles ~13 M10K to hide a load and an enumeration that
  together are ~5 k cycles.
* **C6 — the priority degrade is a THRESHOLD SEARCH, not a sort.** The
  reference `stable_sort`s up to 2,048 entries by a 32-bit key and keeps a
  prefix; the SET it keeps is determined by the cut key `T` and how many of the
  edges tied at `T` fit. 32 MSB-first counting passes leave `thr` the largest
  key with `count(key >= thr) >= Budget` (that count is monotone), and one pass
  in SCAN ORDER keeps everything above it plus the first
  `Budget − count(key > thr)` ties — exactly a stable descending sort followed
  by a prefix. *Rejected:* a bitonic sorter — 66 stages of 1,024
  compare-exchanges on 32-bit keys, against 33 linear passes over one RAM port
  and one comparator.

## Clock and reset semantics

Single clock `clk` (`clock_domain: gpu`). Asynchronous active-low reset
`rst_n`, synchronously released; every register including the tables' pointers,
the alive bitmap and the counter is initialised in the reset arm.

## Input and output packet layouts

### the page command (in, ready/valid)

| field | width | meaning |
|---|---:|---|
| `cmd_page_ci_i`, `cmd_page_cj_i` | 16 each | absolute cell index of page cell (0,0) |
| `cmd_cw_i`, `cmd_ch_i` | 6 each | cells in this page, 1..32 (a lattice's last page is partial) |
| `cmd_lat_w_i` | 16 | lattice VERTEX width — the `vdist` stride |
| `cmd_vdist_en_i` | 1 | 0 = the reference's null `vdist` |
| `cmd_src_id_i` | 16 | `source_ids: true` |

### the SOLID window load (in, ready/valid)

`ld_solid_i`, one bit per beat, 34×34 = 1,156 beats, row-major, window cell
(0,0) = page cell (−1,−1). Anything off the cell grid is 0 (C1).

### the `vdist` read master (out)

`vd_en_o` + `vd_addr_o[31:0]`; `vd_data_i` is expected the cycle after the
address (a plain synchronous-read memory). Never asserted when
`cmd_vdist_en_i` is low.

### `forge_primitives` (out, ready/valid)

`edge_ci_o[15:0]`, `edge_cj_o[15:0]` (ABSOLUTE, so the packet **is**
`zref::forge::RimEdge`), `edge_side_o[1:0]` (0 = −z, 1 = +z, 2 = −x, 3 = +x),
`edge_span_o[5:0]`, `edge_src_id_o[15:0]`.

### the page status (out)

`page_done_o` is a one-cycle pulse when a page finishes emitting;
`page_merged_o[11:0]` and `page_dropped_o[11:0]` are valid with it and hold
until the next page starts (C4).

## Backpressure rules

Ready/valid on the command, the window load and the edge stream. Hygiene: every
outgoing valid/ready is a function of registers only, never of an incoming
ready. A stalled consumer parks the block in `StEmit` holding the edge; the
plan is unchanged and no edge is dropped, duplicated or reordered.
ENFORCED-BY: tests/forge/forge_cliff_directed.cpp:test_rtl_handshake
(four stall patterns, the plan bit-identical under each).

## Memory ownership

Owned, all block-local, all sized by the §5 worst case:

| table | shape | why that size |
|---|---|---|
| SOLID window | 1,156 flops | the page plus a one-cell halo (C1) |
| edge table | 2,048 × 18 bits | the TIGHT checkerboard worst case; `{cj[4:0], ci[4:0], side[1:0], span[5:0]}` page-local, the origin added at emit |
| alive bitmap | 2,048 flops | one bit per enumerated edge |
| priority table | 2,048 × 32 bits | C2 |
| run table | 1,024 × 17 bits | a run needs ≥ 2 entries, so ≤ MaxEdges/2; `{start[10:0], len[5:0]}` |

Roughly 13 M10K plus ~3.2 k flops. Nothing is shared and nothing persists
across pages except the counter.

## Q formats and rounding

**No rounding anywhere.** The only arithmetic is index construction —
`base = cj·lat_w + ci` and `span·lat_w` for the `vdist` addresses — and a
comparison. `vdist` is Q16.16 1/w and the reference compares it **signed**
(`int64_t p = vdist[va]; if (vdist[vb] > p) …` on two `int32_t`s is a signed
max); the threshold search therefore runs on the key BIASED by 2³¹ so one
unsigned comparator spans the signed range.
ENFORCED-BY: tests/forge/forge_cliff_directed.cpp:test_rtl_priority
(negative priorities, and `INT32_MIN`/`INT32_MAX`).

## Latency (fixed or variable)

`variable`, per the ledger, and the shape is per PAGE rather than per edge.
Measured on the structural worst case — a 32×32 checkerboard page, 2,048 edges
down to 512 through both degrades — **11,946 clocks**, printed by the directed
lane on every run. The breakdown: 1,156 load beats, 4,096 enumeration beats
(one cell-side per clock), one 2,048-entry run pass, 33 threshold passes of
2,048, one keep pass and 512 emit beats.

An undegraded page is ~1,156 + 4,096 + `edges` clocks; the merge and threshold
work only happens when a page exceeds the budget.

## Target throughput

**The ledger's "1 skirt vertex per clock" is not directly comparable to what
this block emits, and that is worth saying plainly rather than claiming a
match.** This block emits a rim-edge PLAN, one edge per clock when the consumer
is ready — and one edge is one wall quad, i.e. four skirt vertices. The
per-vertex rate belongs to the emission stage, which is not written (see
Purpose). What this block does meet is one plan entry per clock on the emit
phase, and the whole-page cost is measured above rather than asserted.

The enumeration is the honest cost line: **one cell-side per clock, 4,096
clocks for a full page**, whether or not any of them is a rim edge. Four sides
per clock would cut it to 1,024 at the price of a four-wide write port into the
edge table and a four-way priority encoder for the write index; that is a
change to the RTL file only and is not made here because the load and the
enumeration together are ~5 k clocks against a page whose walls the rasterizer
will spend far longer drawing.

## Overflow and malformed-input behaviour

* The edge table is bounded at 2,048 and the enumeration stops writing there.
  That bound is not a hope: with `S` solid cells and no solid-solid adjacency
  the edge count is `4S`, and the largest adjacency-free `S` in a 32×32 page is
  512 — the checkerboard — so 2,048 is the exact structural maximum. (§5's
  2,112 counts all cell-adjacency edges, 64 of which have void owners.)
* The run table is bounded at 1,024 because a run needs at least two entries.
* A page with NO solid cells emits nothing; a 1×1 lattice emits four edges.
  Both are directed cases, because no random mask reliably produces either.
* A degenerate lattice (`w < 2` or `h < 2`) never reaches the block: the page
  loop that issues commands is the caller's, and `rim_plan`'s own early return
  is mirrored in `cliff_test::plan_lattice`.
* `merged` ≤ 1,536 and `dropped` ≤ 2,048, both inside their 12-bit fields.

## Counters and traces

`triangles_submitted_o` (32-bit, saturating) increments by **two** per emitted
edge (C3), on the emit handshake, so a stalled consumer never double-counts.
`source_ids: true` is honoured on `edge_src_id_o` and on the outgoing `vdist`
reads. `page_merged_o` / `page_dropped_o` are the per-page plan status (C4).

## Scalar reference function

`zref::forge::rim_plan` (reference/src/zterrain/terrain_core.cpp), with
`is_rim_edge` and `build_runs`. The block is differentiated against it
**whole-lattice**: `tests/forge/forge_cliff_dev.hpp::plan_lattice` walks the
32×32 pages in the reference's own order, runs one page per command, and
concatenates — so what is compared is the same edge list in the same order plus
the summed `merged` and `dropped`, bit for bit.

## Directed tests

`tests/forge/forge_cliff_directed.cpp` keeps its five oracle lanes and adds
five RTL lanes over the same fixtures:

6. **enumeration** — the 4×4 solid block, the centre bite, the 8×8
   checkerboard, plus an all-void lattice (nothing emitted) and a single-cell
   lattice (four edges).
7. **the structural worst case** — the 32×32 checkerboard page clamps to 512
   with 1,536 bodies dropped and NOTHING merged; the worst-page clock count is
   measured and printed.
8. **priority** — the `vdist` master four ways: the oracle lane's single
   nearest spike (one late-scan edge beats 1,536 rivals), a GRADED field with
   ties on both sides of the cut, NEGATIVE priorities, and
   `INT32_MIN`/`INT32_MAX`.
9. **merge under pressure** — the 9-page 96×96 fixture: one run merged whole
   and a 13-edge PREFIX of the second, and the unpressured control where every
   span stays 1.
10. **backpressure and the counter** — four stall patterns, and
    `triangles_submitted == 1024` for 512 emitted edges.

## Randomized differential tests

`tests/forge/forge_cliff_random.cpp` keeps its oracle lane and adds two RTL
lanes:

* **lane G (gameplay-shaped)** — 150 island masks (600 nightly): sparse
  authored voids and breaches on lattices whose sizes straddle the 32-cell page
  grid, with the `vdist` path on for one trial in five and stalls on one in
  three. This is where the enumeration and the page walk are proved.
* **lane L (domain limit)** — 60 masks (240 nightly) BUILT to trip the degrade:
  checkerboard fields dense enough to push a page past 512 with nothing
  mergeable, the same with straight bites cut through them so runs exist under
  pressure, and — every third trial — a **prefix-merge fixture**, a single
  32×32 page tuned to land just past the budget WITH long runs available.

Both lanes count the states they claim to reach and the counts are asserted.
Measured on the committed seeds — lane G: 87 undegraded, 1 merge-only, 62 both,
63 with a surviving merged span, 150 partial pages, max 1,935 edges. Lane L: 16
merge-only, 7 drop-only, 37 both, 49 with a surviving merged span, max 1,088
edges.

**The prefix-merge family exists because a mutation found the hole**, and that
is the most useful thing in this section. Replacing R1's prefix rule with
"merge whole runs" left BOTH random lanes green and was caught only by the
hand-built 96×96 directed fixture: random pressure always overshot the budget
so far that every run was consumed whole and the priority degrade finished the
job, so the last-merge prefix never occurred. The family was added, `merge_only`
went from 0 to 16 in lane L, and the same mutation re-run now dies in lane L as
well as in the directed fixture.

## Formal properties

**None, and here is the honest reason.** The two properties worth stating —
"the emitted plan never exceeds the budget" and "emitted bodies + dropped ==
enumerated" — are both facts about a multi-thousand-cycle sequential run over
2,048-entry tables, so a bounded model check would have to reach a horizon of
~12,000 cycles to see either of them once. That is not a proof, it is a slow
simulation with a solver attached, and it would state less than the
differential lanes already do (which check both identities on every one of 510
random lattices against the executed reference, plus the checkerboard worst
case by hand).

The block has no small combinational arithmetic core of the kind
`zhao_surface_blend` and `zhao_texture_mod255` were factored out to expose: its
widest expression is an index add. If a property is added later, the shape that
would earn its keep is an inductive invariant on the alive-bitmap population —
`alive_count + merged == cnt` through the merge phase — which needs an
auxiliary population counter the block does not otherwise want. Recorded rather
than left as a gap.

## Synthesis / resource ceiling

Written to the conservative synthesizable subset (charter §2): no indexing of a
function call's result, no dynamic part-selects on the left of an assignment
beyond a constant-width slice, every loop bound constant, no arrays on the
boundary. Verilator `-Wall` clean (`lint_forge_cliff`).

Expected cost, by inspection — **this block has NOT been through the Quartus
per-block sweep at the time of writing, so nothing here is a synthesis
claim**: ~13 M10K of tables (see Memory ownership), ~3.2 k flops, two 16×16
multipliers for the `vdist` index (`cj·lat_w` and `span·lat_w`), one 32-bit
comparator, a 1,156-bit dynamic bit-select for the SOLID window and a 2,048-bit
one for the alive bitmap. Those two wide selects are the things most likely to
want attention when the sweep runs; both are ordinary LUT mux trees.

## Integration capture cases

`zref::forge::rim_plan` is the plan `zrender`'s `draw_heightfield` emits
(charter §29-6: "THE one rim law"), and the `terrain-orbit` / `terrain-breach`
reel subjects render the strata walls it produces. Those pin the ORACLE; this
block is differentiated against the same oracle edge-for-edge, so a drift here
turns the differential lanes red before it could reach a capture.

**Not composed with anything yet.** The plan's consumer — the emission stage
that turns an edge into a quad — is not written (see Purpose), so there is no
seam to compose across. Recorded as missing rather than implied.

## Mutation evidence

Three mutations, one per build, each proved to have RELINKED by the SHA-256 of
both test binaries. Clean: directed `fa4ec1e5…`, random `b4fb7b6e…`.

| # | mutation | directed / random SHA-256 | caught by |
|---|---|---|---|
| M7 | `take = min(len, need+1)` → `take = len` (merge WHOLE runs, not a prefix — R1) | `a95f8e5c…` / `ccdb32c6…` | directed lane 9 at the exact 13-prefix span. **The random lanes did NOT catch it** — see below |
| M8 | `prio_key_c > thr_r` → `>=` in the keep pass (the stable-prefix tie accounting) | `8d797ad4…` / `b59c3d9f…` | directed lane 8 (all three priority fixtures) and lane G |
| M9 | `prio_key_c = prio_rd_c ^ 32'h8000_0000` → unbiased (the signed key) | `0608cada…` / `3a13ce16…` | directed lane 8 by name (`negative vdist values sort correctly (signed)` and the `INT32_MIN`/`INT32_MAX` fixture) and lane G |

**M7 is the finding.** It was caught by ONE hand-built fixture and by neither
random lane, because random pressure always overshot the budget far enough that
every run was consumed whole. The prefix-merge family was added to lane L in
response (`merge_only` 0 → 16), and M7 re-run against the strengthened lane now
fails there too: `lane L trial 2 … RTL 447/67/0 vs oracle 512/2/0`. The hole and
its closure are both recorded because the hole is the more instructive half.

## Notes

Skirt depth from terrain LOD delta, not a tunable. World-identity wave: also owns island rim walls and the breach silhouette (spec/terrain_rules.md §5 — 2,112-edge structural bound, clamped emission with span-merge). Deep-keel wave: `zref::forge::rim_plan` reference-complete with the frozen degrade order; the TIGHT checkerboard worst case is 2,048 rim edges (2,112 counts all adjacency edges, 64 with void owners); directed + random tests green; terrain-orbit/terrain-breach render the strata walls.
