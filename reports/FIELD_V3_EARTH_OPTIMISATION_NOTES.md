# Field v3 Earth — where the clocks are now, and what is left

Written 2026-08-30, from `tests/differential/field_v3_earth_directed.cpp`.
Every number here is measured on the composed RTL against the shipped oracle.
Nothing in this file is modelled or projected unless it says so.

---

## Where it stands

    == VERDICT ==
       3 program(s) ran exact and every one is INSIDE the frame budget.

| program | clocks/group | frame | margin on 850,000 |
|---|---|---|---|
| crater_ring | 20 | 698,880 | 17.8% |
| impact_wave | 23 | 803,712 | 5.4% |
| wave_pool | 24 | 838,656 | **1.3%** |

12,312 values checked against `zfield::execute_point`, zero mismatches, every
lane compared separately. crater_ring had never run at all when the session
started; the other two were at 4,368,000 and 4,507,776.

**The configuration that does it** — all parameters, none of them defaults:

    CTX=32  LANES=4  OUTSTANDING=12  GATHERS=4  REGS=64
    DIST_BANKS=4  RING_UNITS=4  wb_policy=0 (ALU-first)

The shipped defaults are still the narrow ones (LANES=1, DIST_BANKS=2,
RING_UNITS=2, REGS=32) and every block test runs at both settings. A parameter
nobody exercises is a parameter that rots.

---

## What binds it now

Per four-point group, measured on the quad drive:

| | crater_ring | impact_wave | wave_pool |
|---|---|---|---|
| uop issue occupancy | 69% | 73% | 73% |
| write port occupancy | 64% | 69% | 69% |
| contexts alive (of 32) | 30.3 | 29.6 | 28.8 |
| engine idle | 0 | 0 | 0 |
| ready-but-could-not-issue | 0 | 0 | 0 |
| frozen by long-op backpressure | 0 | 0 | 0 |
| partial dispatch groups | 0 | 0 | 0 |

**Nothing is starved and nothing is stalled.** The machine issues about one uop
every 1.4 clocks and writes about one register every 1.5 clocks, and the two
are the same work seen from both ends: every uop produces exactly one register
write.

### The structural ceiling, and why 400,000 is a different kind of problem

    400,000 / (273 x 128) = 11.4 clocks per four-point group

`wave_pool` is **17 uops**. The executor issues at most ONE uop per clock and
every uop lands one register write, so 17 clocks per group is a hard floor for
that program on this machine. 11.4 is below the floor.

400,000 is therefore not reachable by scheduling, more contexts, more banks or
more units. It requires one of:

1. **Dual issue** — two uops per clock, which needs a second register-file write
   port and doubles the read ports. The file is banked four ways by
   `register[1:0]` and two writes to different banks are physically independent,
   so bank-disjoint dual issue is the cheap version of this; it would stall on
   the ~25% of pairs that collide.
2. **Fewer uops** — a compiler concern, not an RTL one. `wave_pool` computes SIN
   and COS of related angles and `impact_wave` uses CURVE where DCURVE shares a
   table and phase. Fused SINCOS and CURVE_DCURVE would each remove a uop, a
   service request and a register write. This is the cheapest real lever and it
   is in the compiler lane.
3. **Hoisting uniforms** — if any per-point uop is actually computing something
   uniform across the association, it belongs in the PREP block the ARM runs
   once. Worth auditing before anything is built.

Realistic near-term target from lever 2 alone: `wave_pool` 17 -> 15 uops is
about 12%, i.e. ~740,000. Useful margin, not 400,000.

---

## Levers already tried, with results

**Worked:**

| change | effect |
|---|---|
| quad-wide executor (LANES 1 -> 4) | 3,878,784 -> 1,083,264 |
| per-op gather slots in the dispatcher | partial groups 64% -> 0% |
| DIST_BANKS 2 -> 4 | DIST2 II 22 -> 12 |
| RING_UNITS 2 -> 4 | RING_PREP II 19 -> 10 |
| writeback policy drain-first -> ALU-first | wave_pool 908,544 -> 838,656 |
| quad-aligned drive (`kCtxPerGroup = 4/LANES`) | 33 -> 28 clocks/group |
| CTX 8 -> 32 | about one clock per group |

**Measured and rejected** — each of these was a reasonable hypothesis that cost
nothing to kill and would have cost a lot to build:

* **A one-entry skid per long service.** The executor-wide freeze on long-op
  backpressure measures **0 clocks**. The per-op gather already gives an offer
  somewhere to go, so S4 is essentially never refused. Do not build this.
* **Removing the global writeback-skid fence.** Tried twice. Naively it
  overflows the skid, whose depth of four is DERIVED from issue stopping while
  it is non-empty. Done properly — per-context scoreboard, issue stopped only
  when full, depth raised to CTX+4 — it is correct (42/42, 183/183, 9,240 values
  exact, blocked 0, drain stalls 29 instead of 1,282) and **slower**, 28 and 26
  against 23 and 24. The fence throttles in a way that helps.
* **OUTSTANDING 16 with GATHERS 8** — indistinguishable from 12 and 4.
* **More contexts beyond 32** — one clock, then nothing.

---

## An inconsistency in the counters, stated rather than explained

Issue occupancy reads 69–73%, and `engine idle`, `ready-but-could-not-issue`
and `frozen` all read **0**. Those cannot all be true: the clocks that are not
issuing should land in one of the three buckets.

Contexts alive averages 28.8–30.3 of 32, so roughly 5% of context-slots are
between retire and reload at any moment — real, but not 27%.

I have not resolved this and am not going to guess at it. The likely candidate
is that `idle_clocks_o` increments inside the upstream block, which is gated by
`!hold_c && !mul_denied_c`, so clocks lost to some other cause never reach the
counter at all. **Anyone optimising further should fix the accounting first** —
an exclusive classification of every non-issuing clock, as the reviewer
suggested, rather than three overlapping counters that can all read zero while a
third of the run is unaccounted for.

---

## Measurement traps this file has already paid for

Written down because every one of them produced a confident wrong number:

* **Counters run from RESET; spans cover a window.** Dividing one by the other
  printed 123% of a single write port and 2,390 uops inside 1,742 clocks. Every
  counter is differenced over the measured window now. If you add one, difference
  it.
* **Latency is not initiation interval.** The first Earth estimate was 9.1x over
  because it summed latencies. Measure II by streaming and dividing elapsed
  clocks by groups RETIRED.
* **Streaming with identical data proves nothing.** Three service tests streamed
  one operand set past every group and compared them all to one answer; a
  cross-group swap would have passed. The tag-order check does not cover it
  either, because the tag rides the order queue and the data rides the banks.
  Every streaming test now carries distinct per-group data.
* **A prediction from a counter is still a guess.** 215 groups against 256 points
  reads as 1.19 points/group only if you forget each point issues two long ops.
  The estimate said 3.4x; the measurement said 1.7x.
* **Pointer wraps must not be narrowed.** `UW'(UNITS)` at UNITS=4 is two bits and
  `UW'(4)` is ZERO, so the wrap became a modulo by zero. `~ptr` is a correct wrap
  for one bit and silently wrong for two. Both bit this file; the dispatcher had
  already paid for the identical thing with `SW'(OUTSTANDING)`.
* **A synthesised uop is not a canonical opcode.** `UOP_RING_PREP` (0xF1) is
  produced by the planner and `zfield::steps::exec_op` has never heard of it;
  feeding it in indexes the op table off the end. It has its own reference,
  `ring_prepared`. crater_ring is the only program that uses it, which is why it
  was the only one that crashed.
* **Rebuild from clean before reporting someone else's gate as red.** A stale
  object made `creature_core` report 4,096/4,096 lit; a clean rebuild gives
  1,305/4,096 and all anchors green.

---

## 2026-08-30, second pass: the gate was measuring the ramp

**The registered gate ran `--points 256` and reported the machine 1.23x OVER.
The same binary at `--points 1024` reports it INSIDE with 22% margin.**

    --points 256    crater 978,432   impact 1,048,320   wave 978,432    OVER
    --points 1024   crater 663,936   impact  594,048    wave 594,048    FITS

Nothing changed but the run length. The admission law is about an INITIATION
INTERVAL, which is a steady-state quantity, and at 256 points a CTX=32 LANES=4
machine spends most of the run filling and draining thirty-two contexts. The
gate is registered at 1024 now, and `tools/field/run_sweep.ps1` refuses to
default below it.

This is the same family as the traps already listed below -- latency mistaken
for initiation interval, a counter divided by the wrong window -- and it is the
most expensive kind, because it was reporting FAILURE. An hour went into
hunting a regression that did not exist.

## The sweep is committed now, and it killed three hypotheses

Every parameter is a Verilator `-G`, so a sweep is a rebuild per point and
cannot be driven from a shipped binary's command line. Earlier sweeps lived in
a shell history and their numbers are unreproducible. The points now live in
`tests/CMakeLists.txt` under `ZHAO_FIELD_SWEEP_POINTS`, off by default:

    cmake -S . -B build -DZHAO_FIELD_SWEEP=ON
    cmake --build build --target field_sweep
    ./tools/field/run_sweep.ps1

Round 1, all exact, QUAD drive, 1024 points:

| point | crater_ring | impact_wave | wave_pool | worst | frozen |
|---|---|---|---|---|---|
| base (shipped) | 663,936 | 594,048 | 594,048 | **663,936** | 36% |
| RING_UNITS 16 | 663,936 | 594,048 | 594,048 | 663,936 | 36% |
| RING_UNITS 32 | 663,936 | 594,048 | 594,048 | 663,936 | 36% |
| GATHERS 8 | 663,936 | 594,048 | 594,048 | 663,936 | 36% |
| OUTSTANDING 24 | 698,880 | 594,048 | **454,272** | 698,880 | 31% |
| CTX 64 + RING 16 | 698,880 | 628,992 | 733,824 | 733,824 | 46% |
| LONGQ 32 | 698,880 | 663,936 | 803,712 | 803,712 | **0%** |

**RING_UNITS was the obvious lever and it does nothing.** Doubling it, then
quadrupling it, moves not one clock on any of the three programs. 36% of clocks
read as "frozen by a long op awaiting the dispatcher" and the ring is still not
the limit. Do not build more ring units.

**The freeze counter is backpressure working, not backpressure hurting.**
LONGQ 16 -> 32 drives the freeze to exactly 0% and makes the machine 21% SLOWER
on wave_pool. A queue deep enough never to refuse lets the front end run ahead
into a resource it cannot use, and the throttle was doing something useful.
This is the same shape as the writeback-skid fence recorded below, which was
also correct to remove and also slower once removed. **Optimising the freeze
counter to zero is optimising the wrong number.**

**OUTSTANDING pulls the two programs in opposite directions.** At 24 it costs
crater_ring 5% and buys wave_pool 24%. That is the only lever in round 1 that
moved anything, and it is not free -- which is what round 2 is sweeping.

Unlocking the sweep needed an RTL fix in its own right: four width ladders
(`UW` in RING_SVC, `BW` in LEN, `GW` and `SW` in DISPATCH) were hand-written
chains of ternaries that stopped at the then-current maximum, so `RING_UNITS=16`
silently produced a three-bit pointer into a sixteen-entry array. They are
`$clog2` now. This is the third time this file has recorded a narrowed pointer
in this engine.

## Round 2, and where the clocks actually are

| point | crater_ring | impact_wave | wave_pool | worst |
|---|---|---|---|---|
| base (OUTSTANDING 12, LONGQ 16) | **663,936** | 594,048 | 594,048 | 663,936 |
| LONGQ 4 | 663,936 | 559,104 | 594,048 | 663,936 |
| LONGQ 8 | 663,936 | 628,992 | 628,992 | 663,936 |
| OUTSTANDING 16 | 698,880 | 594,048 | 524,160 | 698,880 |
| OUTSTANDING 20 | 698,880 | 594,048 | 489,216 | 698,880 |
| OUTSTANDING 24 | 698,880 | 594,048 | **454,272** | 698,880 |
| OUTSTANDING 32 | 698,880 | 594,048 | 454,272 | 698,880 |

**crater_ring is pinned at 663,936 in every configuration that does not make it
worse.** Sixteen builds across two rounds — RING_UNITS, GATHERS, LONGQ,
OUTSTANDING, CTX, DIST_BANKS — and not one of them moves it by a single clock.
OUTSTANDING buys wave_pool 24% and costs crater_ring 5%, and since the verdict
is the WORST program, the shipped configuration keeps OUTSTANDING at 12.

### What the programs actually are

`--histogram` prints the contracted plan, which nobody had looked at:

    crater_ring   7 uops    MUL x4   DIST2 x1   RING_PREP x2
    impact_wave  10 uops    ADD, SUB, MUL x5, DIST2, CURVE, RING_PREP
    wave_pool    11 uops    SUB, MUL x6, DIST2, SIN, COS, RING_PREP

crater_ring is **seven uops and nineteen clocks per group**. It is not
issue-bound and it is not write-port bound (36%). 58% of its clocks are
`longop-hold`, and DIST2 is the one long op every program has.

### DIST2 front-end pipelining: built, measured, REJECTED

The front end walked ISSUE → WAIT → ISSUE → WAIT, paying the multiplier's whole
latency once per component, even though `zhao_field_v3_mulbank` accepts an issue
every clock and replies in issue order two clocks later. Issuing the components
back to back and accumulating products wherever they land is obviously right,
the block's 49 directed checks all pass, and **crater_ring got slower**:
663,936 → 698,880. Reverted.

The mechanism is almost certainly contention: LEN now holds the shared bank on
consecutive clocks and the ring units behind it start later. A local
improvement to a service made the machine worse because the service was never
the constraint.

### The shared multiplier bank, brought out and measured

Every service in the path — every ring unit, every root bank, trig, curve —
issues into ONE four-wide multiplier. Its grant counter existed inside
`zhao_field_v3_svcpath` and stopped there; it reaches the top now.

| program | mul bank grants | occupancy |
|---|---|---|
| crater_ring | 2,785 in 3,723 | **75%** |
| impact_wave | 1,337 in 3,230 | 41% |
| wave_pool | 1,130 in 3,219 | 35% |

**That is the answer to why RING_UNITS and DIST_BANKS both did nothing.** They
add consumers of a resource that was already the busiest thing in the machine.
crater_ring issues 10.9 multiplies per four-point group — two smooth rings at
four products each, plus DIST2's two squares — into a bank that can start one
four-wide multiply per clock.

75% is not saturation, so the bank is not a hard floor; it is the largest single
consumer and the only structure with measured evidence FOR widening it. Ring
units and root banks have measured evidence AGAINST. **If anyone builds one more
thing in this engine, build a second service multiplier bank — and measure it,
because this file has now recorded three confident architectural prescriptions
that were wrong.**

### Standing

    crater_ring   19 clocks/group   663,936   21.9% margin
    impact_wave   17 clocks/group   594,048   30.1% margin
    wave_pool     17 clocks/group   594,048   30.1% margin
    24,624 values against the oracle, zero failures

All three inside 850,000. The remaining roadmap items are diminishing returns
against a 22% margin on the worst program.

## Round 3: the frame figure was quantised, and two "5% regressions" were 1%

`group_clocks` was rounded to a whole clock per four-point group and the frame
cost was then `group_clocks x 273 x 128`. **One clock of rounding is 34,944
clocks of frame -- 4.1% of the entire admission budget.** Both changes this
session reported as ~5% regressions had moved the measured span by about 1%,
and crossed a rounding boundary.

`Result::group_exact` keeps the interval as measured and the frame comes from
it. Every figure below is on the exact arithmetic at `--points 2048`, and they
are not comparable with the integer figures above.

### The ring's two `x2` products are exact shifts. Proven, built, and it does not help crater_ring.

`ring_prepared` computes `fx_mul(F(2 << 16), t)` twice, and `t` has just been
clamped to [0, 1.0]. So `2*t` is in [0, 2.0], 2.0 is exactly representable, and
`resc16(131072 * t)` never carries its rounding term. It is `t << 1` with no
residue and no ledger event.

**Proved exhaustively, not argued:** all 65,537 values of `t` in [0, 1.0] give
an identical raw and an untouched `SatLedger`. The full ring goes from nine
products to seven; smooth mode from four to three. The two steps also stop
costing a bank transaction and a wait state, so the walk is shorter as well as
cheaper. All 23 ring-service checks pass; 49,184 Earth values exact.

    A/B at --points 2048, exact arithmetic

    |             | nine products      | shifts             |        |
    | crater_ring | 19.32   675,193    | 19.57   683,960    | +1.3%  |
    | impact_wave | 16.69   583,236    | 16.39   572,879    | -1.8%  |
    | wave_pool   | 17.05   595,776    | 16.67   582,609    | -2.2%  |
    | crater bank | 6,654 grants, 77%  | 5,379 grants, 62%  | -19%   |

**Nineteen percent of crater_ring's multiplier traffic was deleted and it got
1.3% SLOWER.** Together with RING_UNITS 8/16/32 and DIST_BANKS 4/8 doing
nothing, that settles it: **crater_ring is not multiplier-bank bound.** 77% was
the highest occupancy in the machine and it was still not the constraint. The
remaining cost is dependency latency and phase, not arithmetic throughput.

The change is kept: it improves two of three programs, deletes real DSP
transactions and a wait state, and a shift is not a multiply. But it is kept as
an honesty and area change, not as a speed one, and **the projection that it
would reach 559,104 did not happen** -- the same class of error as every other
prediction in this file. The arithmetic count was right; the assumption that
the machine was arithmetic-bound was not.

### Three architectural prescriptions, all measured, all wrong

| prescription | outcome |
|---|---|
| four ctx-banked register write ports | deadlocked; widening the WORD was the answer |
| more ring units / more root banks | not one clock, three times |
| DIST2 front end at II 8 | 1.3% slower |
| delete 19% of the multiplier traffic | 1.3% slower on the program it was aimed at |

Every one followed from a correct measurement. **A correct measurement does not
make a prescription correct**, and this engine has now paid for that four times.

## Round 4: both multiplier banks, per clock, exclusively

There are TWO four-wide multiplier banks: the executor's, for ordinary MUL and
MAD, and the service path's, shared by every ring unit, root bank, trig and
curve service. They are physically independent, so a program's arithmetic floor
is `max(service grants, engine grants)` per group and **not their sum**. Whether
they ever run together had never been measured. Four exclusive buckets that sum
to the clock count, at `--points 2048`:

| program | both | engine only | service only | neither | sum |
|---|---|---|---|---|---|
| crater_ring | 2,371 (27%) | 1,199 (14%) | 3,008 (35%) | **2,132 (24%)** | 8,710 of 8,710 |
| impact_wave | 1,807 (25%) | 3,109 (42%) | 866 (12%) | 1,579 (21%) | 7,361 of 7,361 |
| wave_pool | 1,617 (22%) | 3,726 (50%) | 621 (8%) | 1,472 (20%) | 7,436 of 7,436 |

**The banks do overlap — 27%, 25%, 22% of clocks — so the machine is not
phase-serialised.** That hypothesis is dead. Per four-point group, crater_ring:

    service grants   10.5      both              4.63 clocks
    engine grants     6.97     engine only       2.34
                               service only      5.88
                               NEITHER           4.16
                               ------------------------
                               engine time      17.01   (+2.1 harness preload)

Total grants per group is 17.5 and engine time per group is 17.0. **The machine
issues almost exactly one multiplier grant per clock, across the two banks
combined.** With perfect overlap the floor would be max(10.5, 6.97) = 10.5
clocks per group, or about 367,000 clocks per frame.

The gap is 6.5 clocks per group, and the largest single slice of it is the
**4.16 clocks per group where NEITHER bank is doing anything at all.** That,
and not bank throughput, is what is left to attack. It is consistent with
everything else this file records: deleting 19% of the service traffic did not
help because the bubbles simply grew, and adding units behind either bank
cannot fill a bubble.

`contexts alive` is 30.6 of 32. The machine has run out of independent work, not
out of multipliers.

## Round 5: the descriptor cache, and the law it exposed

### The ring service was FEED-bound. That was the whole thing.

Its accept path was `F_IDLE (1) -> F_FETCH (6 serial scalar reads) -> F_HAND (1)`
= **eight clocks per request**, so crater_ring's two ring requests per
four-point group sat under a sixteen-clock floor whatever the arithmetic did.
It measured 19.57.

A ring request names four prepared scalars by bank index and the ARM writes them
once per association, so the service was re-reading the same four values
273 x 2 x 4 = 2,184 times per association over TWO distinct descriptors. A
two-entry cache keyed on the immediate's packed indices turns a hit into two
clocks instead of eight.

    --points 2048                before          after
      crater_ring             19.57  683,960   13.43  469,237    -31%
      WORST of the three             683,960          584,918    -14.5%
      crater mul bank occupancy         62%              90%
      crater "neither bank"       2,132 (24%)       388 (6%)
      crater ring front end            --        896 requests, 896 hits,
                                                 0 misses, 0 fetch clocks

**That single fact retroactively explains every result this engine refused to
give up**: RING_UNITS 8/16/32 doing nothing three separate times, DIST_BANKS 4/8
doing nothing, deleting 19% of the multiplier traffic doing nothing, and 24% of
clocks with neither multiplier bank busy. None of them were the feeder.

Invalidation is on any scalar-bank write, and the test for it is real: removing
the invalidation line kills exactly the two new checks and nothing else. A key
alone is not enough -- section 2 covers "different slots must refetch", and this
covers the case a key cannot, the SAME slots holding a new association's values.

### THE LAW: in this engine, throughput comes from ACCEPTING faster, not from FINISHING faster

Four latency reductions have now been built and measured. Every one of them made
the worst program slower or did nothing:

| change | effect on the worst program |
|---|---|
| DIST2 front end II 12 -> 8 | +1.3% |
| ring `x2` products -> exact shifts (nine grants to seven) | +1.3% on crater_ring |
| ring P3/P7 shift STATES removed (latency 32 -> 30, II 14 -> 13) | **+6.4%** |
| LONGQ 16 -> 32, which removes all visible backpressure | +21% |

And the one change that increased ACCEPTANCE rate was worth 31%.

The mechanism is the same every time. A fixed number of contexts feeds the
machine. Returning a context sooner does not create new independent work -- it
bunches the next requests, and the bubbles grow to swallow the saving. The
long-op queue depth behaves identically: driving its "frozen" counter to zero
costs 21%, because the throttle was spacing the traffic.

**Before building anything else here, ask whether it makes the machine accept
sooner or merely finish sooner.** Only the first kind has ever paid.

The P3/P7 state removal is therefore REVERTED, measured at 4,096 points:

    impact_wave  16.90  590,642   (states kept)
                 17.99  628,517   (states removed)
    crater_ring  13.42  468,789 / 13.41  468,665   (no difference)

The arithmetic change from commit 01eb557 stays -- it deletes real bank
transactions -- but the schedule change on top of it does not.

### Standing

    crater_ring  13.42  468,789   44.8% margin
    impact_wave  16.90  590,642   30.5% margin   <- now the worst
    wave_pool    16.51  576,771   32.1% margin

impact_wave is engine-bank dominated: 6,272 engine-only clocks against 1,501
service-only, and 4,116 (26%) with neither bank busy. It is an executor problem,
not a service one, and the ring work above barely touches it.

## Round 6: a rejected change becomes a winner when the bottleneck moves

### The parameter space re-optimised itself, and the shipped defaults won

Every point in rounds 1-5 was measured against a machine bound by the ring
service's eight-clock accept path. With that gone, re-swept at 2,048 points:

| point | crater_ring | impact_wave | wave_pool | worst |
|---|---|---|---|---|
| **base (shipped)** | 469,237 | 584,918 | 576,771 | **584,918** |
| LONGQ 4 | 484,447 | 626,886 | 621,644 | 626,886 |
| CTX 64 + LONGQ 4 | 499,160 | 645,568 | 602,006 | 645,568 |
| CTX 64 + OUTSTANDING 24 | 608,608 | 710,787 | 631,859 | 710,787 |
| CTX 64 + DIST_BANKS 8 | 518,764 | 716,014 | 569,275 | 716,014 |
| CTX 48 | 625,314 | 696,442 | 766,314 | 766,314 |
| CTX 64 | 621,783 | 802,325 | 653,150 | 802,325 |

**`LONGQ=4` was the best point in round 3 and is among the worst now.** A
parameter optimum does not survive an architectural change, and shipping the
round-3 winner would have cost 7%. Re-sweep after every structural change or do
not quote the sweep.

### DIST2 front-end pipelining, rejected in round 3, is worth 2.7% here

Identical change: `F_ISSUE -> F_COLLECT` instead of `ISSUE -> WAIT -> ISSUE ->
WAIT`, so a DIST2's two squarings go out on consecutive clocks and the products
are accumulated wherever they land. It cost the worst program 1.3% when the ring
feeder was the wall. Measured again at 4,096 points on this engine:

    impact_wave  16.90  590,642  ->  16.44  574,444   -2.7%
    crater_ring  13.42  468,789  ->  13.22  461,989   -1.5%
    wave_pool    16.51  576,771  ->  15.67  547,602   -5.1%

98,324 values against the oracle, zero failures. All three improved.

**This is not a contradiction of the acceptance law, it is a demonstration of
it.** A service's initiation interval IS its acceptance rate; what the four
rejected changes lowered was LATENCY. And it is the sharpest possible statement
of the other rule this session has been paying for: **a change is only ever
measured against the bottleneck that exists at the time.** A rejected change
deserves re-testing every time the bottleneck moves, and this file's "measured
and rejected" table needs reading with a date against every row.

### Where impact_wave's clocks are now

    issue 10,579 (67%)   longop-hold 4,135 (26%)   idle 1,051 (7%)

Three long ops per group -- DIST2, CURVE, RING_PREP -- against initiation
intervals of 12, 13 and 13. CURVE's 13 is structural and documented at its own
head: six dependent binary-search steps x 2 lanes x 2 cycles = 12 address slots
per port plus one handoff clock. That is the next wall, and it is a table-search
problem, not a multiplier or scheduling one.

## Round 7 — where it finished, and the one rule that produced most of the gain

    2026-08-30, --points 4096, exact interval, CTX=32 OUTSTANDING=16 LANES=4
    LONGQ=16 DIST_BANKS=8 RING_UNITS=8 REGS=64

    crater_ring  12.07  421,887   50.4% margin
    impact_wave  14.37  502,006   41.0% margin   <- worst
    wave_pool    13.12  458,387   46.1% margin
    98,540 values against the oracle, zero failures

    worst program over the session:  683,960 -> 502,006   -26.6%
    worst margin:                        19.5% -> 41.0%

### What actually moved it

| change | worst program |
|---|---|
| ring prepared-descriptor cache (accept path 8 clocks -> 2) | -14.5% |
| DIST2 front end, ISSUE/COLLECT instead of ISSUE/WAIT | -2.7% |
| curve search starts at the first step that can succeed (II 13 -> 8) | -5.3% |
| DIST_BANKS 4 -> 8 with OUTSTANDING 16 | -9.1% |

### THE RULE: a rejection is only valid against the bottleneck it was measured on

Three of those four had already been rejected on this engine.

* **DIST_BANKS 8** was rejected TWICE, once written up as "double the roots for
  no change". It is worth 9.1% now.
* **DIST2 front-end pipelining** cost 1.3% in round 3 and buys 2.7% now.
* **`LONGQ=4`** was the best sweep point in round 3 and is among the worst now.

Nothing about any of them changed. The wall moved out from under them: ring
feeder, then curve search, then the root banks. **Re-sweep and re-test after
every structural change**, and read a date against every "measured and rejected"
row in this file.

The counterpart still holds and is not in tension with it: **four LATENCY
reductions each cost the worst program time, and every acceptance-rate increase
paid.** A service's initiation interval is an acceptance rate. Its end-to-end
latency is not.

### The measurement that ended six rounds of guessing

Per-service accept/refuse counters. Six sweep rounds failed to find the wall;
two counters named it immediately, and then named the next two as they moved:

    impact_wave, before      curve refused 4,704   len 3,664   ring     0
    after the curve fix      curve refused 1,957   len 4,266   ring     0
    after DIST_BANKS 8       curve refused 2,902   len 1,832   ring    14

**Measure which unit REFUSES, not which unit looks busy.** Occupancy said the
multiplier bank was at 77% and it was never the constraint; the refusal counter
said CURVE and it was.

### Still open on this engine

* **CURVE is the top refuser again** on impact_wave at 2,902 clocks. Its II is 8
  now and the floor for a 9-entry table is 2x4+1 = 9 with two lanes per port.
  Below that needs four table read ports and two groups in flight so a port
  never idles on a dependent read.
* **RING refuses 4,370 clocks on crater_ring**, which issues two ring ops per
  group. It refuses nothing at all on the other two.
* **Area is unmeasured and now matters.** Eight banks of four lanes is
  thirty-two floor-exact roots, and the LEN header prices eight roots at roughly
  2,000 ALMs. `RING_UNITS=8`, `LANES=4` and `REGS=64` are all above the shipped
  narrow defaults. **Re-fit in Quartus before quoting any resource number.**
* Timing closure at 100 MHz is unverified for any of this.

## Open items

* **`wave_pool` has 1.3% margin.** That is thin enough that a timing or workload
  change could cross it. It is the program to watch.
* **The 2.0 clocks/group of harness preload** are the probe's serial port, not
  the machine. The finished design streams point data from memory. They are
  counted in the headline anyway, because subtracting a cost the real machine
  will also pay would be flattering.
* **Area is unmeasured.** DIST_BANKS=4 is sixteen floor-exact roots and
  RING_UNITS=4 is four ring units; LANES=4 quadruples the ALU and the register
  file word. None of it has been through Quartus. **Re-fit before quoting any
  resource number** — the existing census is an upper bound built from per-block
  fits that do not share.
* **Timing closure at 100 MHz is unverified** for any of this.
