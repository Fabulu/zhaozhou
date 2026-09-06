# Contract — TERRAIN.VISIBLE (Visible patch builder)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_visible.sv`
> Reference model: `zref::island::visible_set` — `reference/include/zref/zref_island.hpp`
> Test: `tests/terrain/visible_rtl_directed.cpp`

## Purpose

Given a **View** — a centre patch coordinate and a radius in patches — enumerate
every patch coordinate in the square window, ask `TERRAIN.ISLAND` about each, and
emit a stream of the ones that came back `RESIDENT`, each with its page handle,
in a defined order.

**Exclusions.** It does not decide what is on screen, does not stream, does not
evict, does not allocate and does not know what a page contains. It does not
re-implement the extent gate, the pitch gate or the sky mapping — those are
`TERRAIN.ISLAND`'s, this block instantiates that block, and it believes whatever
comes back. `TERRAIN.RESIDENCY` owns the store; `SW.STREAM` owns what gets
published. This block owns one question: **which coordinates are worth asking
about, and in what order.**

## Why it exists at all: the directory answers, it does not ask

`reports/Missingterrain` names the hole in the owner's own words, explaining why
the shipped world is "a little spot" rather than an 8 km island:

> Nothing currently does: camera moved → inspect island directory → determine
> visible patch coordinates → … → issue all visible patches to the terrain
> engine.

`TERRAIN.ISLAND` answers **one** question about **one** patch. Every terrain
block below it processes one patch job. Nothing decided **which** questions to
ask, so the only islands the machine could draw were the ones a test harness
hand-fed it. This block is the missing verb.

It is deliberately the smallest thing that can be that verb: two coordinate
cursors, one comparator against the window bounds, one register holding the
emitted patch, and five counters. Everything it could plausibly duplicate, it
composes instead.

## The window is SQUARE, and that is a decision, not a shortcut

`zref::island::View` is a centre and a radius, and the window is the square
`[cx−r, cx+r] × [cz−r, cz+r]` — not the disc inscribed in it. The reference
states why, and the RTL matches it rather than "improving" on it:

> Square rather than circular on purpose: the visible set is a conservative
> SUPERSET of what is drawn, and a residency policy that is exact about the
> frustum evicts patches the moment the camera turns.

The corners of a radius-6 window are 21% of its cells, and dropping them would be
a real saving in queries. It would also be the wrong trade on this machine. Page
traffic is expensive — a terrain page is 21,376 bytes and has to arrive from HPS
DDR — and comparator area is not. A frustum-exact set evicts a patch as the
camera turns and re-fetches it as the camera turns back, on the frame the player
is most likely to be turning. **Hysteresis belongs in the view, not in the draw
path.** The square is where that hysteresis lives, and rounding its corners off
buys queries and sells page bandwidth at a bad rate.

This is written down because the circle is the obvious "optimisation", it looks
like free work, and the first explanation that absolves it — "the corners are
never on screen anyway" — is true about the frame and false about the frame
after it.

## SKY IS THE COMMON CASE, so the cost is the cost of a REJECTION

An 8 km island at the canonical 2.0 m pitch is 125 × 125 = **15,625 patches**, of
which the shipped island has **793** of ground. **94.9% of the grid is nothing.**

So a window's throughput is dominated by the patches it *rejects*, not by the
ones it emits, and the number this block is sized against is **cycles per
rejected patch**, measured on a window that emits nothing at all:

| window | cells | emitted | cycles | cycles per rejected patch |
|---|---|---|---|---|
| radius 6, entirely in open sky, inside the extent | 169 | 0 | 511 | **3.02** |

Reported by `visible_rtl_directed` on every run, so it cannot go stale.

**That 3.02 is `TERRAIN.ISLAND`'s round trip and nothing else.** The directory
accepts an in-extent query, decides on the captured coordinate one cycle later,
consults the store on the next, and frees its input on the cycle the store
answers — three cycles, and this block adds no bubble on top of it. A query that
fails the extent gate never reaches the store and costs two.

A radius-6 window is therefore ~511 cycles and a radius-16 window (33 × 33 =
1,089 cells, enough to cover a 2.1 km view distance at 64 m patches) is ~3,300
cycles: **about 0.2% of a 100 MHz frame**, for the whole visible-set decision.
The scan is not where the frame goes, which is the point of measuring it.

Three design choices keep it at the directory's own rate:

* queries are **issued** from one cursor and **retired** against a second cursor
  walking the identical order, so the next query is presented on the same cycle
  the directory frees its input;
* an `OPEN_SKY` answer never touches the emit register, so a fully sky window
  runs at exactly the acceptance rate — the common case is the fast case;
* emission backpressure stalls the retire side, which stalls the directory,
  which stalls issue. A slow consumer costs throughput and never costs a patch.

Measured: at a consumer ready three cycles in four, a fully-resident window is
**not one cycle slower** — the query path absorbs the stall entirely, because it
fills the emit register only once every three cycles. The backpressure check
therefore runs at one-ready-in-eight, where the consumer really is the
bottleneck. The first version of that check stalled at three-in-four and asserted
a slowdown that correctly did not happen.

## It COMPOSES the directory rather than duplicating its gates

`zhao_terrain_visible` instantiates `zhao_terrain_island_dir` and forwards the
residency-store port straight through. It never reads `desc_extent_*` or
`desc_pitch_log2` itself.

The temptation is obvious: the block already has the coordinate, and an extent
compare is four comparators, so clipping the window to the extent before issuing
would save every out-of-extent round trip. **That is a second copy of the rule,**
and its first divergence from `Directory::find` would be silent — a window that
quietly omitted a patch draws slightly less ground, on a grid where drawing
nothing is the correct answer 94.9% of the time. There is no picture in which
that is visible. One definition, one gate, one place to be wrong.

The same argument decides the malformed-descriptor case. On an illegal
`pitch_log2` the directory answers `BAD_PITCH` to every query, and this block
**walks the whole window anyway**, counting each cell, emitting nothing. Aborting
early would mean this block deciding what a legal pitch is. The reference does
the same thing for the same reason, which is why the two agree exactly.

**One query in flight** is the directory's current contract and this block
respects it. Deeper pipelining is future work *in that block*, not a change made
here — and this block is already built for it: the retire cursor needs no
in-flight queue, so a deeper directory changes throughput and nothing else.

## Two cursors, not a FIFO

Something must remember which coordinate each answer belongs to. A queue of
in-flight coordinates works and costs a **depth argument** nobody can discharge
without knowing the directory's pipeline — and getting that depth wrong when the
directory is later pipelined deeper is a silent overflow.

Instead the retire side keeps its own copy of the scan counters, advanced by one
on each answer. Because `TERRAIN.ISLAND` answers strictly in order, the retire
cursor **is** the coordinate of the answer arriving, by construction. No storage
per in-flight query, no depth to get wrong.

The claim is **checked, not assumed**: every query carries a rolling 8-bit tag
and `err_tag_o` latches if an answer ever arrives carrying a tag the retire
cursor did not expect. A silently misaligned cursor attaches the right handle to
the wrong coordinate — a correct patch drawn in the wrong place, which nothing
downstream can detect. The directed test asserts `err_tag_o` never fired across
every window it ran.

## The emission ORDER is contractual

**Row-major: `iz` outer, `ix` inner, both ascending.** The reference guarantees
it and the RTL matches it, and the differential asserts the emitted *sequence*
element by element rather than its membership.

Order matters because the consumer is a stream. A hardware scan that walks a row
at a time keeps the residency's set index sweeping contiguously, and a
downstream stage that assumes monotone `iz` — for row-coherent tessellation
stitching, for instance — is entitled to that assumption or entitled to be told
it does not hold. And a test that compared only the set would pass a block that
emitted every correct patch in a permuted order, which draws exactly the same
ground and shows in no picture.

## Interface shape

**One view in flight.** Accepting a second window while the first is still
scanning would interleave two patch streams on one output with no way to tell
them apart. The caller waits for `v_done_o`.

**Ingress capture.** The window bounds `ix0/ix1/iz0/iz1` and both cursors are
formed from `v_centre_ix_i`, `v_centre_iz_i` and `v_radius_i` at the single
acceptance event and never read live afterwards. A scan runs for thousands of
cycles and the caller is entitled to move the camera the instant the handshake
completes; a late read would silently splice two windows together. This is the
rule `tools/rtl/check_ingress_capture.py` enforces, and the module is under
contract there with prefix `v_`.

**There is no `p_last_o`, and its absence is deliberate.** An emitted patch
cannot know it is the last one until an unknown number of sky cells after it have
been rejected, so a last-flag needs lookahead across the window's tail. A
separate `v_done_o` completion pulse costs one wire and no lookahead, and it
fires only after the emit register has drained — so it cannot arrive while a
patch is still waiting on a slow consumer.

## Counters

`visible_examined`, `visible_emitted`, `visible_sky`, `visible_out_of_extent`,
`visible_bad_pitch` — mirroring `zref::island::WindowTally` field for field.

**`examined` is counted at ISSUE and the other four at ANSWER**, which makes

```
examined == emitted + sky + out_of_extent + bad_pitch
```

a real invariant across an idle boundary rather than one number restated five
ways. A query asked and never answered breaks it, and a list comparison cannot
see that at all — the emitted patches would all still be correct. This is the
same lesson as the material combiner issuing every microjob twice while
producing byte-identical output: *a test that checks what came out cannot see how
many times the machine did it.*

The block also forwards the composed directory's own four counters
(`isl_cnt_*`). They are the *directory's* ledger, not this block's: it may
legitimately gain other clients later, at which point the totals stop agreeing
and the difference is the other client's traffic. Today it has one client, and
the differential asserts they move by exactly the same amounts on every window —
which is how a query invented or an answer swallowed inside the composition
would show.

`err_tag_o` is a sticky fault, not a counter: one is a defect.

## Scalar reference function

`zref::island::visible_set` (`reference/include/zref/zref_island.hpp`), with
`zref::island::WindowTally` as the counter oracle and `zref::island::View` /
`zref::island::Visible` as its argument and element types.

**It was EXTRACTED, not written.** The `want`-set construction inside
`zref::island::Streamer::update` was already this algorithm, and
`zref_island_stream.hpp` was the only place it existed. Writing the RTL against a
transcription of that loop would have created the second definition this tree
keeps paying for — the streamer's copy under test, the hardware's copy shipped.
So the loop moved into `zref_island.hpp` and `Streamer::update` now calls it. The
streamer's own differential (`island_stream_directed`) reports byte-identical
statistics before and after the move — published 1485, evicted 1404, returned
702 — which is the evidence that the extraction moved no behaviour.

`tests/terrain/visible_rtl_directed.cpp` — 27 checks:

* **directed windows** on named cases: one entirely in open sky (the common case,
  and where the per-rejection cost is measured); four straddling the extent edge,
  including one entirely outside the island; one centred on the island's densest
  area where every cell is ground and any permutation fails; four radius-0
  windows — resident, sky, negative and one past the end — where an off-by-one in
  a loop bound shows and at radius 6 would not; and one on a malformed
  descriptor;
* the emission **order** asserted twice — once by element-wise comparison against
  the reference list, once as its own property (strictly increasing in `(iz,
  ix)`) so a future change to the comparison helper cannot quietly stop checking
  it;
* **backpressure**: the dense window replayed with the consumer ready one cycle
  in eight, producing an identical list at more than double the cycles;
* a **randomised phase** over 240 drawn centres and radii, comparing the full
  emitted list, all five counters *per window*, the issue/answer invariant, and
  the composed directory's ledger — with a coverage assertion that the draws
  genuinely produced windows wholly inside, wholly outside, straddling, and
  containing ground.

A quarter of the random draws are aimed at the island on purpose. Drawn
uniformly over the coordinate range, only 12 of 240 windows contained any ground
at all — because the disc is 5% of the grid — so the order comparison, which is
what the phase exists to stress, had almost nothing to disagree about. Drawing
uniformly and then asserting good coverage asserts a property of the island, not
of the test.

## Proof that the test can fail

A detector that has not been shown to fire has not been tested. Nine
perturbations were applied one at a time to a pristine copy of the test, rebuilt,
run, and reverted; each broke **exactly one** check and no other.

| # | property perturbed | check that fired |
|---|---|---|
| M1 | emission ORDER (swapped two oracle entries) | `dense[0]: rtl (56,56)h206 oracle (57,56)h207` — "emits exactly that list, IN ORDER" |
| M2 | page handle (flipped one oracle handle) | `dense[5]: rtl (61,56)h211 oracle (61,56)h210` |
| M3 | order as its own property (swapped two emitted entries) | "the sequence is strictly increasing in (iz, ix)" |
| M4 | per-window counter vs `WindowTally` | "the sky and emitted counters matching the oracle's own tally" |
| M5 | `examined == emitted+sky+out+bad` | fired on all 240 random windows |
| M6 | composed directory ledger agreement | fired on all 240 random windows |
| M7 | radius-0 asking one question | fired on all four radius-0 windows |
| M8 | whole-window bad-pitch tally | "every one of its 49 cells is counted BAD PITCH" |
| M9 | `err_tag_o` wiring | "the block's own answer-alignment guard never fired" |

M4, M8 and M9 print an identical expected/got pair, because those mutations
change the *predicate* rather than a measured value. That is an artefact of
mutating the expectation, not a diagnostic defect: a genuine divergence prints
differing numbers, as M1, M2 and M7 do and as the two unforced failures below
did.

**And three detectors fired without being asked to**, which is stronger evidence
than any deliberate mutation:

* the differential caught a **real RTL defect in `TERRAIN.ISLAND`** on its first
  complete run — see below;
* the coverage assertion caught a **randomised phase that was not random**: the
  draws took an LCG's low bits, so `aimed` never once fired and `rad` produced
  exactly two values, 0 and 4, 120 times each. The phase reported 240 windows
  and had tested four. The comfortable reading of that failure was "the coverage
  thresholds are too strict";
* the backpressure check caught **its own wrong expectation** — see the
  three-in-four measurement above.

The ingress-capture contract was mutation-verified separately: making the row
restart read `v_centre_ix_i` live instead of `ix0_q` is reported as
`fpga/rtl/terrain/zhao_terrain_visible.sv:285  rad_c,v_centre_ix_i`, and the
unmutated file is clean. Both the port and its combinational alias were caught.

## A defect this block found in TERRAIN.ISLAND

`zhao_terrain_island_dir` **dropped an answer whenever its consumer applied
backpressure**, and the window was one cycle wide. On the cycle the store's
answer is being registered, `ans_full_q` is still low, so `q_ready_o` is high and
a second query is accepted; one cycle later that query's decide overwrites the
first query's answer, which has not been popped.

With `a_ready_i` tied high the overwrite is harmless — the answer is consumed on
that very cycle. **All four phases of `island_dir_rtl_directed` drive
`d.a_ready = 1` on every cycle**, including the full 15,625-patch grid sweep and
the 3,000-draw randomised phase, so a 21-check differential over every input the
block has could not see it. This is the broken-instrument law in its usual
direction: the omission made the block look correct.

Composing it surfaced the defect immediately. Under a consumer ready one cycle in
eight: **911 queries issued, 854 answers returned, 57 lost**, and the composition
deadlocked waiting for answers that no longer existed. The emitted list showed
the signature — correct coordinates (from this block's own retire cursor) paired
with handles drifting one position ahead (from the directory's answer stream).

The fix is one term: the decide is now gated on `ans_free_c`. It closes the
store-answer path with it, because `res_ans_valid_i` can only arrive two cycles
after a decide that required `ans_free_c` and no other query can be accepted in
between. It costs nothing when the consumer is ready, and
`island_dir_rtl_directed` still passes 21/21 with byte-identical numbers.

**`island_dir_rtl_directed` should gain a backpressure phase.** It is the block's
own differential and this is its own defect; that it was found downstream is
luck, not coverage.

## What is not yet established

The block has not been fitted, and it has not been composed with the real
`TERRAIN.RESIDENCY` — the bench models the store, exactly as
`TERRAIN.ISLAND`'s own differential does, so that the *coordinate generation and
filtering* is what is under test. Composing directory, residency and this block
is a separate step with a separate question.

Nothing downstream consumes the stream yet. `TERRAIN.LOD` is the intended
consumer and the union of two players' working sets — the next item in
`Missingterrain`'s list — is not this block: two views produce two streams, and
whoever merges them owns the duplicate suppression.
