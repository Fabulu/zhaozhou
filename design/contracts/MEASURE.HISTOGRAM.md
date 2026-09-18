# Contract — MEASURE.HISTOGRAM (Error histogram)

> Ledger: `design/blocks.yml` · owner ZH-049 · phase 8 · maturity SPECIFIED

## STATUS — BUILT 2026-09-18. THE REFUSAL BELOW IS KEPT, AND THREE QUARTERS OF IT IS NOW SPENT

**The block exists.** `fpga/rtl/measure/zhao_measure_histogram.sv`, with
`tests/measure/measure_histogram_directed.cpp` (521 checks),
`measure_histogram_sat.cpp` (11, the same RTL at `-GCW=8`) and
`measure_histogram_drain_mutant.cpp` (4, inverted polarity). Verilator
`--lint-only -Wall` is 0/0 after a fired positive control.

The section that follows was written to argue this block should NOT be built,
and it is kept below **unedited**, because it is the reason the block has the
shape it has. What changed is one thing and it is not an argument:

> "I don't want to defer any unfinished blocks now" — Fabian, 2026-09-18,
> `reports/MISSING-ORGAN-REGISTER-20260918.md`

`deferred` is no longer a disposition. The ruling that cut this block was the
owner's to make and was the owner's to revoke.

**Of the four inventions the refusal names, THREE ARE MADE and ONE IS STILL
REFUSED**, and the split is the whole design of the block:

1. **The error metric — made, by parameterising it away.** The block takes an
   unsigned magnitude of `EW` bits and declares nothing about what it measures.
   This is possible *because* of invention 2: with a logarithmic bucketing, a
   change of Q format is a **constant shift of the bin index** and nothing else.
   So no Q format is ratified by omission here, and `spec/qformats.md` needs no
   amendment for this block to exist. The "Q formats and rounding" section below
   still stands.
2. **The bucket geometry — made, and named.** Log2 with `SUB_BITS` mantissa bits
   (default 1, giving 64 half-octave bins over a 32-bit magnitude). Chosen
   because a screen-space error spans orders of magnitude; monotone
   non-decreasing in the input, which is the property a cutoff search would need.
   Every rejected alternative is recorded in the module header.
3. **The interval — made, as a knob.** `snapshot_i` ends an interval; the block
   knows nothing about frames. The widths are derived from a one-frame interval
   (`CW = 24` from 1.67M events/frame at 100 MHz/60 Hz) but the interval itself
   is the host's to choose.
4. **The cutoff rule and the governor port — STILL REFUSED.** The refusal's own
   decisive argument is not weakened by the revocation, and it is not
   paraphrased here because it is exactly right: *"The failure mode this project
   cares most about is two blocks disagreeing about one policy ... Inventing a
   Version-2 cutoff in the same increment that ratified a Version-1 governor
   would manufacture that failure deliberately."* So **this block has no
   `MEASURE.GOVERNOR` port and computes no cutoff.** It is a measurement organ,
   and charter §9 Version 1 already says where the policy lives: *"ARM predicts
   a pixel-error threshold per camera FROM PRIOR COUNTERS."* **This block is the
   prior counters.** That is a complete and useful job, so the refusal costs
   nothing, and `design/contracts/MEASURE.GOVERNOR.md` needs no amendment.

**The declared input is still not wired, and still for the reason below.**
`RASTER.FRAGMENT`'s `fragment_error_o` is still the one-bit tilestore-read
protocol flag that point 2 identified, and this block is still not connected to
it. The event port takes a magnitude from whatever eventually produces one.

**What the owner plan §11.4 added, which the refusal could not have known.**
The refusal calls this "a small, cheap block". At the level of *policy* it is.
At the level of *structure* it is not, and the plan says why:

> "For a RAM-backed histogram, aggregate per-cycle same-bin events and implement
> read-after-write forwarding or scheduled stalls. A one-read/one-write port
> cannot accept arbitrary simultaneous updates to many bins without extra
> hardware. Snapshots/clears need epoch or double-bank semantics so a host read
> sees one complete interval, not a mixture."

That is the real content of the block and it is orthogonal to all four
inventions. The implementation does both halves of each clause: aggregation
**and** scheduled stalls, epoch **and** double bank.

**Ledger maturity stays `SPECIFIED`**, deliberately. The ladder is a claim about
evidence against a ratified oracle, not about test coverage, and V17 is explicit
that a block past `SPECIFIED` may not cite a phantom `reference_model`.
`zref::MeasureHistogram` is still the tenth phantom (see "Scalar reference
function"), and writing an oracle in this same increment, from these same
freshly-invented laws, by the same author, would be a second statement of one
intent dressed as independent evidence. `npm run ledger:check` reports **zero
errors** against this block.

---

## STATUS — the original refusal, kept verbatim

Phase 8's three MEASURE blocks were opened together. `MEASURE.TOKENS` and
`MEASURE.GOVERNOR` landed. **This block was deliberately not started**, and the
reasons are recorded here rather than left as an unexplained gap — the same way
`FIELD.SEQ.EARTH` and `FIELD.SEQ.FORM` were refused in phase 7.

**1. The charter sequences it AFTER the two blocks that did land, explicitly.**
Charter §9's "Practical implementation path" is in two numbered stages:

> **Version 1:** ARM predicts a pixel-error threshold per camera from prior
> counters; FPGA performs local hierarchy traversal against that threshold; a
> global token guard rejects only low-priority refinement when the budget is
> nearly exhausted.
>
> **Version 2:** FPGA builds a small histogram of candidate error buckets; a
> cutoff bucket is selected; eligible refinements above the cutoff are emitted.

Every word of Version 1 is now built: the ARM's threshold arrives on
`SetView.pixel_error`, `TERRAIN.LOD` is the local hierarchy traversal,
`MEASURE.GOVERNOR` converts the threshold, and `MEASURE.TOKENS` is the global
token guard. This block is the whole of Version 2. The ledger's own note says
the same thing: *"Charter §12 calls this Version 2: registered now, built
late."*

**2. Its only declared input already exists in RTL, and it is a different
thing.** The ledger gives this block `inputs: [fragment_error]`, `upstream:
[RASTER.FRAGMENT]`. RASTER.FRAGMENT shipped in phase 4 and it *does* have a
port called `fragment_error_o` — but it is:

```systemverilog
assign fragment_error_o = s1_v_r && !rd_valid_i;
```

a **one-bit tilestore-read protocol flag**, whose own comment says *"It should
never fire"*. It is not a screen-space error magnitude, it carries no value, and
it has no bucket index. So the ledger edge resolves to a signal that exists and
means something else entirely. Building this block against it would either
require inventing a second, real `fragment_error` port on a landed, tested block
— or quietly using the flag and producing a histogram of protocol faults labelled
as a quality metric. **Neither is acceptable, and picking one silently would be
worse than not building the block.**

**3. Building it now would require four stacked inventions, none of which has a
law anywhere in this tree.** In the order they would have to be made:

- **the error metric** — what number goes in a bucket. Charter says "candidate
  error buckets" and stops. A projected screen error? A per-fragment residual? A
  per-meshlet deviation? No spec names one, no Q format is declared for it, and
  no block produces one.
- **the bucket boundaries** — how many, linear or logarithmic, over what range.
  Nothing anywhere states this.
- **the cutoff rule** — "a cutoff bucket is selected" is the entire
  specification. Selected how? Against what budget? With what hysteresis?
- **a Version-2 governor input** — the cutoff has to reach
  `MEASURE.GOVERNOR`, which was just built to charter Version 1 and by
  definition consumes no cutoff. Adding one would mean either a second policy
  path inside a block whose contract has just been written, or contradicting it
  in the same increment.

That last one is the decisive argument. **The failure mode this project cares
most about is two blocks disagreeing about one policy** — it is exactly what
happened when `TERRAIN.LOD` had to guess this file's sibling and why the
governor's contract now carries a whole section reconciling it. Inventing a
Version-2 cutoff in the same increment that ratified a Version-1 governor would
manufacture that failure deliberately.

**4. What is NOT the reason.** It is not effort, and it is not that the block is
hard. A bucket histogram with a cutoff scan is a small, cheap block — smaller
than either block that landed. It is that a small block built on four invented
laws is worse than no block, because the inventions become ratified by being
implemented, and the next wave inherits them as though they were found.

## What would unblock it

In order, and none of them is this block's work:

1. A ratified **error metric** with a Q format and a producing block — most
   naturally an amendment to `spec/qformats.md` plus a real value port on
   RASTER.FRAGMENT (distinct from the existing protocol flag, which should
   probably be renamed at the same time, since its current name is now known to
   collide with a ledger edge).
2. **Bucket geometry** ratified in a spec: count, boundaries, and whether the
   scale is linear or logarithmic.
3. A **cutoff rule** in the charter or a spec, with the budget it is selected
   against.
4. An amendment to `design/contracts/MEASURE.GOVERNOR.md` defining how a cutoff
   composes with the Version-1 per-camera ratio — *before* either block is
   changed, so the two cannot drift.

## Purpose and exclusions

Error-bucket histogram and cutoff feedback ("Version 2" quality loop) feeding
the governor.

## Clock and reset semantics

`gpu` domain per the ledger. One clock, `clk`. Reset is **asynchronous
assertion, negedge `rst_n`**, and it resets registers ONLY — the bin memory is
never reset, because a reset over every cell is not expressible as a memory
reset and is how storage silently becomes flip-flops (`zhao_vertex_arena`
paid for that lesson).

Instead the block performs a ONE-TIME POST-RESET SCRUB: it walks all
`2^(BINW+1)` = 128 addresses, one per cycle, writing `{epoch 0, count 0}`, with
both `ev_ready_o` and `rd_ready_o` held low. This is not tidiness. The epoch
scheme compares a STORED bit against a live one, and after reset the stored bits
are whatever the memory powers up holding; an accidental match would admit an
arbitrary count into a live bin. It is the only clear that costs cycles — every
interval clear afterwards is the epoch flip and costs none.

## Input and output packet layouts

**IN — one beat of up to `LANES` events, `ready_valid`:**
`ev_valid_i`, `ev_lane_valid_i[LANES-1:0]`, `ev_err_i[LANES*EW-1:0]` (lane `l`
occupies `[l*EW +: EW]`, an UNSIGNED magnitude), `ev_src_id_i[15:0]`,
`ev_ready_o`.

The magnitude's meaning is deliberately undeclared — see STATUS invention 1. The
declared ledger input `fragment_error` is NOT wired: `RASTER.FRAGMENT`'s
`fragment_error_o` is still the one-bit protocol flag identified below.

**CONTROL:** `snapshot_i`, a one-cycle pulse. Ends the interval and begins the
next.

**OUT — host read of the FROZEN bank, fixed two-clock registered read:**
`rd_valid_i`, `rd_bin_i[BINW-1:0]`, `rd_ready_o`, `rd_data_valid_o`,
`rd_count_o[CW-1:0]`.

**OUT — frozen-interval summary:** `snap_valid_o`, `snap_total_o[CW-1:0]`
(events ACCEPTED into the interval), `snap_src_id_o[15:0]`,
`snap_index_o[CW-1:0]`.

**BUCKET GEOMETRY** (invention 2), `SUB_BITS = 1` by default:

    v < 2^SUB_BITS  ->  bin = v                                       (exact)
    otherwise, e = index of the highest set bit:
                    ->  bin = ((e - SUB_BITS + 1) << SUB_BITS)
                              + ((v >> (e - SUB_BITS)) & (2^SUB_BITS - 1))

Monotone non-decreasing in `v`. `NBINS = (EW - SUB_BITS + 1) << SUB_BITS` = 64
at the defaults; `BINW = 6`, so 64 of 64 addressable bins are reachable and the
top magnitude `0xFFFFFFFF` lands in bin 63 without wrapping. Multiplying every
input by `2^k` adds `k * 2^SUB_BITS` to every bin and changes nothing else —
which is precisely what keeps "Q formats and rounding" below undecided.

## Backpressure rules

`ready_valid`, and it is real backpressure rather than a drop.

A beat costs **one cycle per DISTINCT bin it contains**: 1 in the best case,
`LANES` in the worst. `ev_ready_o` is high exactly when the beat currently held
will be empty at the next edge, so a beat whose events all share a bin is
accepted with no stall at all and the next beat can follow on the very next
cycle. Nothing is ever dropped.

The host read port has **priority**. While `rd_valid_i` is high no group can
retire, so a host that holds it high indefinitely starves the accumulator and
blocks the snapshot drain. That is the host's fault, not the block's, and
`host_conflict_o` is where it is visible.

## Memory ownership

ONE `zhao_dc_sdp_ram` instance (the ratified inferable shape, both clocks tied
to `clk`), `2^(BINW+1)` words of `CW+1` bits: `{epoch, count}`, addressed
`{bank, bin}`. **3,200 logical bits at the defaults.**

That figure is **SHAPE ARITHMETIC AND NOT A MEASUREMENT.** Whether Quartus
infers an M10K at this depth, uses MLABs, or spreads it into logic is unknown
and unclaimed — no fit has seen this block, and
`reports/OWNER-RULING-M10K-CEILINGS-20260918.md` says in terms that "logical
bits are still not physical M10Ks ... only a fit reports what it cost."

What the memory replaces is also an estimate: the flip-flop form is 2 banks ×
64 bins × 24 bits = **3,072 registers**, about **768 ALM at the Cyclone V
four-registers-per-ALM packing limit**, before any of the 64-way read mux or the
`LANES`-way update crossbar. Against a campaign ~15× over its ALM budget that is
the entire reason for the shape.

**THE 1R/1W PORT IS THE DESIGN PROBLEM**, and three mechanisms answer it:

* **H1 aggregate** — every pending lane sharing the selected bin folds into ONE
  read-modify-write carrying an increment of up to `LANES`.
* **H2 schedule a stall** — lanes with other bins retire on later cycles.
* **H3 forward the write in flight** — the memory is read in cycle T and written
  in T+1, so a group issued in T+1 reads before its predecessor's write lands.
  The hazard is **exactly one cycle deep** (a read issued in cycle C sees every
  write committed at the end of C-1), so one comparator and one substitution
  close it.

The `zhao_dc_sdp_ram` header requires its user to make a same-address
read-during-write "UNREACHABLE by ownership". Here it is reachable — it IS the
H3 hazard — and the obligation is discharged the other way: on exactly those
cycles the colliding read's RESULT IS NEVER CONSUMED, the forwarded write data
being used instead. Stored data is never at risk; only the read output is, and
nothing reads it.

## Q formats and rounding

**STILL UNDECIDED, and still deliberately so.** The error metric has no ratified
Q format anywhere in this tree, and this block does not create one. It can
afford not to: the bucketing is logarithmic, so a rescale of the metric
translates the histogram and does nothing else. There is no rounding anywhere in
the block — the bucket law is a shift and a mask, and the accumulate is an
integer add with a saturate.

## Latency (fixed or variable)

`variable` per the ledger, and it is variable in the event path only.

* **event accept → bin updated:** 2 clocks after the group issues; the group
  issues 1..`LANES` cycles after the beat is accepted, which is the variable
  part.
* **host read:** FIXED at exactly 2 clocks, request to `rd_data_valid_o`.
* **`snapshot_i` → swap:** 1 clock when the pipeline is idle (the pulse is
  registered into a request before it can act), at most 3 more while it drains.

## Target throughput

`1 bucket update per fragment batch` per the ledger, and the block exceeds it:
one memory update per clock sustained, with up to `LANES` events folded into
each when they share a bin. `updates_o` and `events_o` are both exposed so the
aggregation ratio is MEASURED rather than assumed — a block that serialised
unconditionally would produce identical bin contents and four times the memory
traffic, which no result-checking test can see.

## Overflow and malformed-input behaviour

`spec/counters.md` §4: **saturate, never wrap**, and a saturation is itself
visible. Every bin and every counter is `CW` = 24 bits, derived from the
documented interval (one 60 Hz frame at 100 MHz ≤ 1,666,666 events → 21 bits,
rounded to a byte boundary). Not 32, which would buy 8 unused bits per bin
across every bin in both banks.

A clipped bin sets `bin_sat_o`. Note the deliberate consequence: after a
saturation `snap_total_o` no longer equals the sum of the bins, because the
total counts events ACCEPTED and the bins count events STORED. `bin_sat_o`
non-zero is how a host knows to stop trusting the sum.

There is no malformed input: any `EW`-bit value is a legal magnitude, `bin_of`
is total, and a `rd_bin_i` beyond `NBINS` addresses a scrubbed word and reads
zero.

## Counters and traces

**LEDGER DEVIATION: `lod_representation_counts` IS NOT DRIVEN.** This section's
own note offered two options — "a reading of its own that is honestly a
*representation* count, or ... record a ledger deviation instead of driving it"
— and the second is taken. This block counts error magnitudes, which is not a
representation count under either existing owner's reading (`TERRAIN.LOD`'s
subpatch levels, `MEASURE.TOKENS`' charter §9 ladder rungs). Inventing a third
reading to justify driving the entry would be the naming equivalent of the
policy collision invention 4 refuses.

Seven counters of its own instead, all `CW` wide and all saturating:
`events_o`, `updates_o`, `stall_cycles_o`, `bin_sat_o`, `fwd_hits_o`,
`host_conflict_o`, `snapshots_o` — and `frozen_write_o`.

**`frozen_write_o` MUST READ ZERO**, and a zero is a claim. It counts memory
updates aimed at the bank the host owns, a state unreachable while the drain is
correct, so it gets a COMMITTED MUTANT rather than an argument:
`tests/mutants/zhao_measure_histogram_drain_mutant.sv` deletes the drain, and
`tests/measure/measure_histogram_drain_mutant.cpp` PASSES WHEN THE COUNTER
FIRES — carrying its own negative control, the shipped module under identical
stimulus, required to stay at zero.

It is also built to survive CLAUDE.md's "a detector wired to two operands that
move together cannot fire": its operands are `b_bank_q`, loaded by the ISSUE
enable, and `active_q`, loaded by the SWAP enable. Different enables, different
events, so the comparison is sensitive to a TIMING fault and not only a value
fault.

## Scalar reference function

**`zref::MeasureHistogram` STILL DOES NOT RESOLVE** — it names nothing in this
tree and never has. It is the tenth phantom, after `zref::CmdDma`,
`zref::SurfaceStamp`, `zref::SurfaceSheet`, `zref::AuxSource`,
`zref::TerrainBake`, `zref::TerrainVelocity`, `zref::ProgCache`,
`zref::MeasureTokens` and `zref::MeasureGovernor`. It is STILL NOT amended in
`design/blocks.yml`, and building the block did not change that: three of the
four inventions are now made, so an oracle COULD be written — but writing one in
the same increment, from the same laws, by the same author, would be a second
statement of one intent dressed as independent evidence.

**This is why ledger maturity stays `SPECIFIED`.** V17 permits a `SPECIFIED`
block to cite a not-yet-written oracle and forbids an advanced one from doing
so, which is exactly the right rule. The advance is unblocked by one thing: a
`zref::measure` oracle written by someone who did not write the RTL, against the
laws in the module header.

## Directed tests

`tests/measure/measure_histogram_directed.cpp`, registered in
`tests/CMakeLists.txt` as `measure_histogram_directed`. **521 checks, passing.**
Twelve lanes, each with its "could have been red" statement in the file header:
the one-time scrub; an eighteen-row hand-computed bucket table; scale invariance
on the DUT; same-bin aggregation in one cycle; distinct bins serialising with
exactly `LANES-1` refused cycles; read-after-write forwarding (8 back-to-back
updates of one bin total 8, with 7 forwards — without the forward the bin would
read 1); an event hitting a bin being read back; a snapshot taken mid-interval;
the epoch clear proven by recycling a bank; the snapshot summary; a 400-beat
mixed stream through real backpressure; and counter liveness.

`tests/measure/measure_histogram_sat.cpp` (`measure_histogram_sat`), **11
checks**, is the SAME RTL built with `-GCW=8`, because reaching the shipped
24-bit ceiling needs 16.7M events. It is the saturate-never-wrap lane.

`tests/measure/measure_histogram_drain_mutant.cpp`
(`measure_histogram_drain_mutant`), **4 checks**, inverted polarity — see
"Counters and traces".

**NONE OF THESE IS A DIFFERENTIAL LANE**, and the file headers say so rather
than implying otherwise. With no oracle, every expectation is hand-computed or
structural; the in-file `Model` is bookkeeping across long stimulus and
agreement with it is not evidence.

## Randomized differential tests

**Still none, and it cannot be differential.** `measure_histogram_random.cpp`
does not exist; `design/blocks.yml` still names it, and that is recorded as an
open ledger deviation rather than satisfied by pointing the entry at the
directed file. The randomised coverage that would have lived there is lane 11 of
the directed file: 400 beats of mixed masks and magnitudes through real
ready/valid backpressure with all 64 bins compared.

## Formal properties

None. The ledger names no formal lane for this block.

## Synthesis / resource ceiling

**NOT MEASURED. THE BLOCK HAS NEVER BEEN THROUGH `quartus_map`.**

`verilator --lint-only -Wall` is 0 errors / 0 warnings, established after a
POSITIVE CONTROL (a planted width fault on a scratchpad copy fired `WIDTHTRUNC`,
proving the gate fires). That settles one tool's opinion about syntax and width
and nothing else: CLAUDE.md records two SystemVerilog forms that linted at 0
diagnostics and failed Quartus 17.0 outright. Both are avoided deliberately —
the elaboration checks are inside `initial begin ... end`, and there is no
implicit generate — but avoiding two known traps is not evidence of
synthesizability.

The area claim is the shape arithmetic under "Memory ownership" and carries the
same label. `fpga/rtl/measure/` now contains `zhao_measure_tokens.sv`,
`zhao_measure_governor.sv` and `zhao_measure_histogram.sv`.

## Integration capture cases

None yet. The block is standalone and is in no composed fit; per CLAUDE.md's
"fit at SUBSYSTEM BOUNDARIES", it should be wired in with a subsystem's worth of
change rather than fitted on its own.

## Notes

Charter §12 calls this Version 2: registered now, built late. Phase 8's
increment of 2026-08-19 built Version 1 in full and stopped here on purpose;
2026-09-18 built the measurement half of Version 2 and stopped at the policy,
which is where the original refusal put the line and where it still belongs.
