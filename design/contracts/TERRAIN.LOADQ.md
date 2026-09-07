# Contract — TERRAIN.LOADQ (the load queue between the sequencer and the loader)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_loadq.sv`
> Reference model: `zref::terrain::LoadQueue` — `reference/include/zref/zref_terrain_loadq.hpp`
> Tests: `tests/terrain/loadq_rtl_directed.cpp`, and phases L/L2 of
> `tests/terrain/world_composed_directed.cpp`

## Purpose

Take a page-load job off `TERRAIN.SEQ` immediately and hand it to
`TERRAIN.PAGELOADER` when that block is free, so that **the sequencer's walk is
never blocked by a load's ACCEPTANCE**.

## The defect it exists to remove, in numbers

`TERRAIN.SEQ`'s law is that a miss is *skipped and counted, not stalled on — the
frame must never wait on an 80 microsecond page load mid-walk*. That law held
for load **completion** and was defeated by load **acceptance**:
`zhao_terrain_pageloader.sv` drives `j_ready_o = (state == S_IDLE)`, so it takes
one job and holds ready low for the whole 21,376-byte transfer, and the
sequencer sat in `S_LOAD` for all of it — with every patch that *was* resident
waiting behind the one that was not.

The composed bench measured it, then measured the fix, then measured the fix at
the wrong depth:

| frame | queue | cycles | per miss | note |
|---|---|---|---|---|
| 8 misses | bypassed | 53,806 | 6,726 | the original wiring |
| 8 misses | depth 8 | 67 | 8 | 803× |
| 32 misses | depth 8 | 176,768 | 5,524 | 10.6% of a frame; 176,509 cycles on a full queue |
| 32 misses | depth 32 | 259 | 8 | |

## Why the depth is 32 and not 8

The first version of the block was eight deep and carried a confident paragraph
arguing that T7's 32-page budget was *"a budget for a different question"* — the
loader still moves one page at a time, so a deeper queue supposedly bought
nothing but registers. The table above is what happened when the bench was
pointed at a **legal full frame** instead of a quiet one.

The depth is not covering "the acceptance stall". It is covering **the whole
frame's miss list**, and T7's 32 is the number precisely because it is how many
pages a frame may ask for. Eight looked sufficient only because the frame that
measured it asked for eight.

This is the house failure mode in its usual costume — a plausible argument
sitting where a measurement belongs — and it survived one green test run,
because that run asked the easy question.

## Where it sits, which is the design decision

```
  TERRAIN.SEQ  ->  [ TERRAIN.LOADQ ]  ->  (T4 writeback barrier)  ->  TERRAIN.PAGELOADER
```

**Before the barrier, not after.** The queue exists to stop the sequencer
waiting on acceptance; the barrier exists to stop a load *starting* before the
writeback it displaces has landed. Put the queue after the gate and the
sequencer still blocks whenever the barrier is closed — the exact stall the
queue was built to remove. Put it before, and the job is recorded immediately
while the load itself still waits for the barrier: both laws hold at once, and
neither block had to learn about the other.

## Exclusions

It does not reorder, merge, drop, retry, prioritise or inspect a job. It does
not know what a page is. It does not decide residency, does not publish, and
does not enforce T4 — **that barrier lives in `TERRAIN.SEQ` and must stay
there.** A structure here that could reorder a load relative to a writeback
would break a law it cannot see, which is why this is a FIFO: the cheapest
structure that *cannot* invent an ordering.

## Storage, and why it is one M10K

A job is 243 bits. Thirty-two of them in flops is 7,776 registers, which would
blow the composed island's 9,000-register redline on a FIFO alone. So the record
is serialised:

| | |
|---|---|
| word width | 40 bits — M10K's widest supported configuration |
| words per job | 8 → 320 bits of room for a 243-bit record |
| store | 32 × 8 = 256 words = 10,240 bits = **exactly one M10K** |
| address | `{job_index[4:0], word[2:0]}` — a concatenation, no adder, no modulo |

Serialising costs 8 cycles in and 9 out. The loader spends 6,726 cycles per
page, so that is free — stated plainly rather than hoped about.

The array is read with a **registered address straight into a register**, with
nothing combinational between (`QUARTUS_GOTCHAS.md` §14). A "FIFO" that infers
7,776 flops instead of one M10K is the same budget disaster wearing the fix's
name.

## Fitted, and the storage claim is no longer a claim

Nothing in simulation can tell one M10K from 7,776 flip-flops — a Verilator run
is identical either way — so the whole storage argument above was **unverified**
until a fitter said otherwise. Quartus 17.0.2, 5CSEBA6U23I7, 2,018 s:

| | measured | designed for |
|---|---:|---|
| block memory bits | **10,240** | 32 × 8 × 40 = 10,240, exactly |
| M10K | **1** | one |
| registers | **692** | the serialiser, pointers and counters — *not* 7,776 |
| ALM | 734 | |
| DSP | 0 | |
| Fmax | **107.33 MHz** | 100 MHz product clock |

The store inferred as memory **to the bit**, which is what the 40-bit × 8-word ×
32-job packing was arranged for, and the register count is the other half of the
same evidence: a fit that inferred the M10K *and* kept a shadow copy in flops
would satisfy `min_memory_bits` and still be the disaster the design avoids.

## Capacity is DEPTH + 1

`DEPTH` is the size of the **store**. One further job sits deserialised at the
output port, waiting for the loader to take it, and that job has left the store.
So the block can be *holding* 33 while it can *store* 32.

This is written down because the bench found it rather than the design stating
it: the reference model capped at 32, the RTL accepted a 33rd, and every job
after that compared one position out of step. A model that had quietly used
`depth + 1` with no explanation would have hidden a real seam fact behind an
off-by-one that looked like arithmetic.

## `level_o` and `inflight_o` are different numbers

`level_o` is the store. `inflight_o` is everything the block is holding: the
store, plus the job in the write serialiser, plus the one in the output
register.

Anything asking *"is this queue done?"* — a settle, a drain's tally, a frame
boundary — wants `inflight_o`. `level_o` is for judging DEPTH and nothing else.
The composed suite's first drain check compared against `level_o` and was wrong
by exactly those two.

## The drain, and the ruling it is waiting for

`drain_i` throws away everything in flight and counts all of it. It is the
mechanism for T6's frame fault abandoning the rest of the list.

**Whether a fault SHOULD abandon queued loads is an owner ruling that has not
been made.** Nothing in the tree asserts `drain_i` today; the composed suite
fires it directly, because a port that has never been pulsed is a port nobody
has tested. When the ruling lands, the wire is already there and already
measured.

## Counters

| counter | port | meaning |
|---|---|---|
| `loadq_accepted` | `accepted_o` | jobs taken off the sequencer |
| `loadq_issued` | `issued_o` | jobs handed on to the loader |
| `loadq_drained` | `drained_o` | jobs thrown away on a drain, from all three places |
| `loadq_refused` | `refused_o` | **cycles** offered while it could not take one |
| `loadq_level` | `level_o` | jobs committed to the store right now |
| `loadq_inflight` | `inflight_o` | jobs held anywhere in the block |
| `loadq_high_water` | `high_water_o` | the deepest the store ever got |

`loadq_refused` counts **cycles, not jobs**, and only the counter's own
documentation can say so. A blocked sequencer holds `j_valid_i` up for as long
as it is blocked, so the number *is* the stall — which is the thing worth
reading. It should be zero on any legal frame; a non-zero value means either the
depth is wrong for the frame or a caller ignored `j_ready_o`.

## Scalar reference function

`zref::terrain::LoadQueue` (`reference/include/zref/zref_terrain_loadq.hpp`),
with `zref::terrain::LoadJob` as the payload record.

**The oracle is the ORDER and the PAYLOAD — not the timing.** The model owns
three laws and only three: jobs leave in the order they arrived; at most
`DEPTH` live in the store with one more at the port; and
`accepted == issued + drained + held`. It deliberately does not model the
eight-cycle serialiser, because that is a storage decision no observer outside
the block can distinguish from a wide register file except by counting cycles.
A model that mirrored it would be a second copy of an implementation detail,
and its first divergence would be a red test that is not a fault.

## Evidence

`accepted == issued + drained + inflight`, always, checked every phase.

`tests/terrain/loadq_rtl_directed.cpp` compares the **sequence and every field**
against `zref::terrain::LoadQueue` under the same four stall patterns the
sequencer's differential uses, with both sides busy on the same cycle at every
occupancy from empty to full, 200 jobs per pattern (6.25 wraps of the store).
Every field of every job is distinct — a fixture whose upper words are zero
cannot tell a working serialiser from one that stops after word 5, and words 6
and 7 are where the CRC and the source id live. `ix` and `iz` take both signs,
because a queue that zero-extended them would pass every test drawn from
positive coordinates.

`world_composed_directed.cpp` phase L runs the same cold frame with the queue
bypassed and with it enabled, because a fix whose absence was never re-measured
is a claim rather than evidence; phase L2 does the same for a legal full frame,
which is what chose the depth.
