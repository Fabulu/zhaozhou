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
