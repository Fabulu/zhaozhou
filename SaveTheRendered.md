Verdict

I found a credible route to 105 MHz. Not a prayer, not “try more seeds,” and not another generic “pipeline harder” speech.

The 99.50 MHz result is not evidence that this renderer has hit a hard wall. The previous campaign stopped for a reasonable reason—critical ownership moved among several blocks across placements—but it stopped before two important things happened:

The design was never actually placed against a 105 MHz constraint. It was placed against 100 MHz, and 99.50 MHz was merely the derived Fmax of that placement.
The current RTL still contains a specific structural tax that can be removed without changing visible latency, fragment throughput, or Early-Z’s same-cycle floor semantics: TilePipe’s column priority encoder feeds the Early-Z presence lookup in the same period.

The nut remains anatomically intact.

What 105 MHz really asks from the current placement

The present SDC constrains gpu_clk to exactly 10.000 ns. The 105 MHz period is 9.524 ns, so the fitter must recover another 0.476 ns relative to the current constraint. The best 99.50 MHz path was already 0.050 ns late, making its total gap approximately 0.526 ns.

Applying the 9.524 ns target arithmetically to the existing best placement—not refitting yet—turns the near-critical owner set into this:

Owner	Slack at 10 ns	Approximate deficit at 105 MHz
Early-Z	−0.050 ns	−0.526 ns
CMD DMA	+0.042 ns	−0.434 ns
Binner	+0.221 ns	−0.255 ns
TileStore	+0.281 ns	−0.195 ns
EdgeWalk	+0.314 ns	−0.162 ns

These are not five equally terrifying architecture failures. They are a finite cluster whose worst member needs about half a nanosecond, followed by four increasingly small gaps.

The bad placements need more: the least fortunate existing seed would need roughly 0.95 ns from TileStore and 0.84 ns from Binner, while another needs roughly 0.93 ns from CMD DMA. That is why one 105 MHz bitstream is easier than robust 105 MHz closure across every seed. The existing measurements also show a placement-only spread of about 0.527 ns, so seed variation is almost exactly the size of the remaining best-placement problem.

But the recurring list is still basically:

Early-Z, TileStore, CMD DMA, Binner—plus a shallow EdgeWalk tail.

This is not every fucking module in the renderer taking turns.

Finding number one: Quartus was never told the real goal

The campaign’s committed SDC says:

create_clock -name gpu_clk -period 10.000 ...

The fitter therefore optimized a 100 MHz timing problem. At the best placement only Early-Z was red; CMD DMA, Binner, TileStore and EdgeWalk were already “passing” and received less urgency from timing-driven placement.

At 9.524 ns, all five become negative. That gives the fitter a materially different optimization problem: it must improve the whole near-critical cluster rather than obsess over the one remaining 50 ps Early-Z endpoint.

That does not mean changing the constraint automatically creates 5.5 MHz. It means the current 99.50 number is the wrong experiment for answering:

Can Quartus place this design at 105 MHz?

The island brief already established the correct methodology: fit named target periods—9.524 ns for 105 MHz—and judge zero WNS and zero TNS, rather than treating a derived Fmax from another target as signoff.

The immediate untouched-netlist experiment

Add a -GpuPeriodNs argument to run_shell_fit.ps1, stage the corresponding SDC into the clean snapshot, then run the exact existing renderer commit at:

9.52381 ns
High Performance Effort
five actual fitter seeds
complete setup-path archives

No RTL changes first.

There is also more legitimate Quartus effort available that the campaign did not exhaust:

PLACEMENT_EFFORT_MULTIPLIER, first 2 and then 4;
PHYSICAL_SYNTHESIS_REGISTER_RETIMING ON;
the more aggressive performance mode exposed by Quartus 17.0, after confirming its exact QSF value using the installed tool and verifying the fitter readback.

Quartus 17.0 documents that placement-effort values above 1 spend more time seeking a better placement and may improve quality; it also supports physical register retiming on Cyclone devices. Quartus 17.0 also exposes a more aggressive performance optimization mode, although Intel warns that aggressive modes can increase area/runtime and can interact badly with other optimizations—so it must be tested, not worshipped.

Two things should not be retried:

OPTIMIZATION_TECHNIQUE=SPEED: measured −3.01 MHz and +147 ALMs.
Explicit physical register duplication: measured exactly zero change, including identical ALM count and worst slack.

The repo has already killed those ideas properly.

Finding number two: the strongest RTL fix is sitting in TilePipe

The current TilePipe holds a 16-bit coverage row in pend_mask_r, then every fragment cycle runs a descending priority loop over all 16 bits to calculate:

wr_col
wr_hot

wr_col immediately becomes:

frag_addr = {pend_row_r, wr_col};

and that late address enters Early-Z’s dynamic 256-bit presence lookup.

The campaign report already identified the resulting structural path as approximately:

column encode → address → 256:1 presence selection

with roughly 2.6 ns tied up in that shape. It concluded that breaking it required a boundary pipeline and therefore changed Early-Z decisions. That conclusion was true for the pipeline attempt it tried—but not for the structure I found.

Register the coverage cursor, not the fragment

There is already a free staging opportunity:

A coverage row is accepted only while pend_mask_r == 0.
A fragment is emitted only while pend_mask_r != 0.
Therefore the cycle that accepts cov_mask is already one clock before that row’s first fragment can issue.

Use that edge to register the first selected column and one-hot bit:

On cov_acc:
    pend_mask  <- cov_mask
    cur_col    <- first_set_column(cov_mask)
    cur_hot    <- onehot(first_set_column(cov_mask))

On frag_acc:
    remaining  = pend_mask & ~cur_hot
    pend_mask  <- remaining
    cur_col    <- first_set_column(remaining)
    cur_hot    <- onehot(first_set_column(remaining))

Then:

frag_addr = {pend_row, cur_col}

The 16-way priority encoder still exists, but it now terminates at local cursor registers. It no longer sits in front of Early-Z’s presence selection, qualification, unique-count decision and floor-promotion logic.

This preserves:

first fragment timing;
one accepted fragment per clock;
lowest-column-first ordering;
arbitrary Early-Z backpressure;
exact fragment addresses;
all pixel CRCs.

No bubble. No delayed Early-Z decision. No change to the public interface timing.

The older architecture contract explicitly said to create a selected-column drain pipeline if that arithmetic became measured critical. It has now become measured critical; this is the compact, zero-throughput-cost version of that prescription.

This is commit one.

Finding number three: Early-Z can lose the 256:1 hot lookup without delaying floor promotion

The previous failed attempt pipelined Early-Z’s floor update. That changed eight decisions because the 256th qualifying unique pixel promotes the floor on its acceptance edge, and the very next fragment must see the new floor. The source carefully documents this and preserves the three depth comparisons in parallel to remain cycle-exact.

So we do not delay the decision or promotion.

Instead, exploit the same coverage-row preparation cycle.

Convert the accumulator into rows and prefetch the active row

Represent the 256-bit mask as sixteen 16-bit rows:

logic [15:0] acc_rows [0:15];

When TilePipe accepts a coverage row, send Early-Z an internal preparation event:

row_prepare_valid
row_prepare_index

Early-Z captures:

active_row_index <- row_prepare_index
active_seen_row  <- acc_rows[row_prepare_index]

During the subsequent fragment stream, TilePipe passes the registered cur_hot one-hot value. The per-fragment lookup becomes:

seen = OR(active_seen_row AND cur_hot)

rather than:

seen = acc_mask_r[frag_addr_i]   // 256:1 dynamic selection

On a qualifying fragment:

active_seen_row |= cur_hot
acc_rows[active_row_index] |= cur_hot

On round_done or tile_begin, clear both the backing rows and active row.

Why this is cycle-exact

The row-preparation edge occurs before the row’s first fragment. The next coverage row cannot be accepted on the same edge as the preceding row’s final fragment, because cov_ready is false while the old pending mask is nonzero. Therefore all writes from the previous row have committed before the next row snapshot is taken.

The 256th unique pixel can still:

observe its current seen state;
assert round_done;
promote floor_r;
clear the accumulator;

all on the original edge. The next fragment still sees the promoted floor. Nothing about the rejected round-12 behavior is reintroduced.

Required nasty tests

This change deserves focused tests for:

repeated pixel in one coverage row;
leaving and later revisiting a row;
the 255th and 256th unique pixels;
immediate post-promotion fragment rejection;
a stall between row preparation and first fragment;
tile_begin precedence;
round_done with more fragments left in the same coverage row;
full renderer differential and pixel CRC.

That is commit two, and it attacks a structural path much larger than the required 0.526 ns. It may not yield the entire 2.6 ns at final placement, but it does not need to.

Finding number four: CMD DMA has one more clean state split in it

CMD DMA is the dominant owner in one placement at −0.454 ns and is only +0.042 ns in the 99.5 placement. It therefore needs approximately 0.434 ns for 105 MHz under that placement.

The current M_HDR_CHK state still performs one ordered ladder containing:

short-header check;
magic;
ABI version;
flags;
command-byte alignment;
command-count bound;
descriptor-length bound;
slot-buffer bound;
frame-slot bound;
header CRC;
epoch.

The code has already moved several unrelated calculations out of this state, but all those predicates and their ordered error selection still occur together.

Split predicate production from error priority

Add:

M_HDR_PRED:
    capture cb, cc, flags
    calculate and register each bad_* predicate independently
    calculate and register need_total

M_HDR_CHK:
    priority-select first registered bad_* bit
    emit the exact existing status
    otherwise commit header success

The expensive compares now run in parallel and terminate at one-bit registers. The following state performs only an eleven-bit priority decision.

This costs one cycle per packet, not per command, fragment, pixel or byte. It does not reduce any steady-state rendering throughput. The existing command path already uses state staging this way for the record walk: RAM read, field decode, then validation.

The exact first-error priority must remain frozen. Add a mutation test that swaps two predicates and prove the directed test catches it.

That is commit three.

TileStore: probably a small fourth edit, not another rearchitecture

TileStore keeps two 256-bit flat present vectors and dynamically selects a bit after first selecting which physical bank serves which role. The RAM data itself is already captured; the remaining problematic shape is presence selection.

First try the noninvasive form:

represent each presence vector as 16 × 16-bit rows;
calculate front-port bank-0 and bank-1 presence candidates in parallel;
calculate resolve-port bank-0 and bank-1 candidates in parallel;
select the already-computed result by front_r;
preserve current clear/write/read/swap precedence exactly.

Do not route the presence vectors through the RAM address-role mux and then perform the selected-bit lookup. Give the fitter the explicitly parallel structure.

If the 9.524 ns full path names the TileStore working port, reuse TilePipe’s row-preparation event and cache the working bank’s active presence row. The resolve port can remain separate until it is actually named.

Its current gap in the best placement is only about 0.195 ns, so this should not begin as a giant redesign.

That is conditional commit four.

Binner: do not operate blind

Binner is the one block where I would not prescribe a definite RTL patch before obtaining the literal 9.524 ns path.

The final seed bundles contain owner summaries but not the complete setup-path report, so we know Binner is near-critical but not which precise register-to-register cone owns its −0.365 ns bad-seed result.

There are plausible candidates around:

S_TILE corner decision
    -> tile RAM read/advance
S_PUSH
    -> need_chunk
    -> arena allocation/full decision
    -> append writes/cursor advance

But Binner already documents a throughput debt: 2.83 clocks per emitted reference, against the nominal one-reference-per-clock ledger target. Adding another serial state merely to buy timing would make an existing problem worse.

The correct escalation is:

Run the 9.524 ns fit and save the exact Binner path.
If it is the allocator return cone, simplify push_ok directly from need_chunk and registered arena_full, while retaining the allocator output as an assertion.
If it is the S_TILE → S_PUSH RAM append path, pipeline tile evaluation and previous-tile append concurrently:
T0 evaluates the next candidate and issues the read.
T1 appends the preceding accepted candidate.
One candidate tile and potentially one reference advance per clock.
Preserve forwarding only for the bounded same-address case.

That larger change could improve both Fmax and Binner throughput, rather than buying timing with another bubble.

Binner is commit five only if the new report earns it.

Leave EdgeWalk the fuck alone initially

EdgeWalk is only approximately 0.162 ns short of 105 in the current best placement. It has already undergone four successful structural fixes, and the repo contains evidence that a generic skid made performance worse.

Do not touch it before:

fitting against 9.524 ns;
registering the TilePipe cursor;
removing Early-Z’s 256:1 hot lookup;
splitting CMD DMA predicates.

Those changes alter placement around the entire raster island. EdgeWalk may pass without another source edit.

If it remains red, inspect the exact new path and make one local change. No more speculative EdgeWalk archaeology.

The apparatus has two bugs to fix first
1. Seed provenance is mislabeled

The folder called perf-seed3 contains both an evidence JSON and manifest claiming seed 1. The script does append a staged seed override, so this is likely a provenance collector reading the committed default rather than the actual fitted assignment. It does not invalidate the 99.50 MHz placement itself; it means the label “seed 3” is not independently proven by the archived artifacts.

The next flow must read back the actual seed from the staged project or Fitter report and reject a run whose requested and reported seeds differ.

2. Preserve the literal paths per seed

The final per-seed bundles retained summaries, manifests and owner tables, but not their complete setup paths. That is why Binner can be identified but not responsibly redesigned from the surviving evidence.

The next target-period run should archive at least:

1,000–2,000 setup paths;
exact startpoint and endpoint;
logic versus routing delay;
clock skew;
physical location;
fitter seed and actual period;
zero-TNS result at 9.524 ns.
The exact campaign I would hand Hardware Agent
MISSION: close the existing reduced renderer at 105 MHz without changing
pixels, ordering, throughput contracts, or hiding paths.

C0 — FIT PROVENANCE
Read back actual staged GPU period, actual fitter seed and optimization
settings. Fail the run if requested != reported. Preserve full setup paths.

C1 — UNMODIFIED 105 MHz BASELINE
Fit exact renderer snapshot at 9.52381 ns, High Performance Effort,
seeds 1..5. Do not infer 105 capability from a 10 ns fit.

C2 — TILEPIPE REGISTERED COVERAGE CURSOR
Register current column and one-hot on cov_acc and after frag_acc.
Remove the 16-way selected-column encoder from frag_addr's hot path.
No bubble, no ordering change, one fragment/clock.

C3 — EARLYZ ACTIVE-ROW PRESENCE
Store accumulator mask as 16x16 rows. Prefetch the active row on cov_acc.
Use registered one-hot AND row for seen. Preserve same-edge round_done
and floor promotion. Differential against old Early-Z and full pixel CRC.

C4 — CMD DMA HEADER PREDICATES
Split independent predicate calculation from ordered error selection.
One packet-level cycle only. Preserve exact error priority.

REFIT C2-C4:
fixed seed paired before/after, then seeds 1..5 at 9.52381 ns.

C5 — TILESTORE ONLY IF NAMED
Row-bank presence and parallel role lookups. Reuse row prepare for the
front working port only if its exact path remains red.

C6 — BINNER ONLY IF NAMED
Use literal 9.524 ns path. Do not add a serial bubble. Pipeline candidate
evaluation and append if a structural split is needed.

C7 — TOOL MATRIX
For the best structurally clean RTL:
- High Performance Effort baseline
- placement_effort_multiplier 2
- placement_effort_multiplier 4
- physical_synthesis_register_retiming ON
- installed Quartus 17 aggressive performance mode, exact value from qhelp

One variable at a time. Verify every setting in the Fitter report.
Do not retry SPEED synthesis or explicit register duplication.

PASS:
WNS >= 0 and TNS == 0 at 9.52381 ns;
all differential, formal and pixel-CRC gates green;
no initiation-rate regression;
resource delta recorded;
actual seed and period proven.
My actual odds after reading the source

This is engineering judgment, not a fabricated Quartus result:

At least one genuine 105 MHz passing renderer placement: about 80–90% after the TilePipe/Early-Z/CMD changes and target-aware fitting.
Three out of five ordinary seeds passing 105 MHz: about 60–75%.
Current netlist passing 105 solely through a better target-aware fit/tool setting: perhaps 35–50%.
110 MHz reduced renderer: still uncertain and should not be smuggled into the 105 claim.

Why I’m substantially more optimistic now:

The fitter has never been asked to solve the actual 105 MHz problem.
The worst path needs about 0.526 ns, not several nanoseconds.
Early-Z still contains an explicitly documented ~2.6 ns encode-plus-presence structure.
TilePipe offers a staging edge that removes the encode from the hot path without adding a cycle.
Early-Z can row-prefetch the presence state without delaying its same-cycle floor law.
CMD DMA has a clean packet-level state split left.
The remaining best-placement Binner, TileStore and EdgeWalk gaps are all under 0.3 ns.
This reduced fit is not area-starved: the campaign reports approximately 13.1k ALMs on a 41.9k-ALM device, so a few cursor, row-cache and predicate registers are affordable here.

So I am no longer looking at 99.5 → 105 as “find five percent of invisible FPGA magic.”

I’m looking at it as:

Run the correct constraint, remove one late address encoder, replace one 256:1 hot lookup with a prepared 16-bit row, split one validation ladder, and let the resulting placement tell us whether TileStore or Binner still deserves a knife.

That is a finite closure campaign. It may still take several obnoxious Quartus batches, but it is not a hard-wall problem.

One boundary remains: this would prove the reduced renderer at 105 MHz on the provisional virtual-pin Cyclone V fit. It does not pre-prove the texture-survivor composition, physical board I/O, SDRAM timing or final full machine. The existing SDC explicitly says those are not yet characterized.

But the specific worry you handed me—the seemingly immovable 99.5 MHz renderer—is crackable without cracking the nut.
