# FIELD resource model — latency vs initiation interval, and what a barrel engine actually buys

*Owner directive of 2026-08-25 (`reports/PIPELINEINGHINTS`): before implementing
an overlapped seven-stage sequencer, investigate a multi-context/barrel Field
architecture. **First deliverable is not RTL** — it is the pipeline/resource
model and the steady-state throughput for realistic Earth programs.*

---

## The correction this model forces on my own recommendation

`reports/EARTH60_CAPACITY.md` recommended pipelining the sequencer for
throughput and called it **"worth about 7×"**. That is true **only for
all-simple programs**, and the directive named exactly why before I measured it:

> If NORMALIZE accepts only one operation every ~60 clocks and an Earth program
> uses NORMALIZE every eighth instruction, the front-end being II=1 doesn't save
> us.

It doesn't. Measured below: **1.9× for a NORMALIZE-heavy program, not 7×.**

---

## Latency and initiation interval are different numbers, and only one of them matters here

**Every long-operation unit in the Field engine accepts a new request ONLY when
idle.** This is not inferred from behaviour; it is the literal ready expression
in each block:

```
normalize   assign v_ready_o = (state == N_IDLE) && (!r_valid_o || r_ready_i);
ring        assign v_ready_o = (state == G_IDLE);
len         assign v_ready_o = (state == L_IDLE) && (!r_valid_o || r_ready_i);
rcp         assign v_ready_o = (state == S_IDLE) && (!r_valid_o || r_ready_i);
curve       assign v_ready_o = (state == S_IDLE);
noise       assign v_ready_o = (state == S_IDLE);
rot         assign v_ready_o = (state == R_IDLE);
```

So for all of them, **II = latency**. Only two units are pipelined:

| unit | structure | latency | **II** |
| --- | --- | ---: | ---: |
| `zhao_field_mul` | no FSM, registered in/out | 2 | **1** |
| `zhao_field_sin` | no FSM, 2 registered stages (waves 8+10) | 2 | **1** |
| every other unit | FSM + ready-when-idle | see below | **= latency** |

### Per-unit II

Derived as *(measured total instruction latency − 7 front-end clocks)*. The
totals are measured by `tests/differential/field_seq_directed.cpp`; the
subtraction is arithmetic, so these are **derived, not directly measured**, and
a dispatch-to-result counter should confirm them before any RTL is cut.

| op | total | unit II | | op | total | unit II |
| --- | ---: | ---: | --- | --- | ---: | ---: |
| NORMALIZE3 | 68 | **61** | | CURVE | 30 | 23 |
| NORMALIZE2 | 67 | 60 | | ROT3 | 28 | 21 |
| RING | 55 | **48** | | ROT2 | 27 | 20 |
| LEN2/LEN3/DIST2 | 49 | 42 | | DCURVE | 27 | 20 |
| SPLINE | 46 | 39 | | RIDGE | 23 | 16 |
| NOISE2 | 30 | 23 | | RCP | 16 | 9 |

---

## Steady-state throughput: one patch-field pass, 1,089 vertices, 8-instruction program

Barrel model: front end issues one instruction per clock (II=1) given enough
contexts to cover latency. Cost is then `max(front-end, per-resource demand)`.

| program shape | today | barrel II=1 | gain | **binding resource** |
| --- | ---: | ---: | ---: | --- |
| 8 simple | 60,984 | 8,712 | **7.0×** | front end |
| 7 simple + 1 RCP | 70,785 | 9,801 | 7.2× | RCP unit |
| 7 simple + 1 CURVE | 86,031 | 25,047 | 3.4× | CURVE unit |
| 7 simple + 1 RING | 113,256 | 52,272 | 2.2× | RING unit |
| 7 simple + 1 NORMALIZE3 | 127,413 | 66,429 | **1.9×** | NORMALIZE unit |

**The front end stops being the constraint the moment a program contains one
long op per eight instructions.** From RCP upward, the binding resource is the
iterative unit, and the barrel architecture cannot help it.

## Against the spec's 128 live-field patches

At 20% reserve, `1,333,333` clocks per frame:

| program shape | patch-fields/frame today | with barrel | needed | verdict |
| --- | ---: | ---: | ---: | --- |
| 8 simple | 21 | **153** | 128 | **closes, with reserve** |
| 7 simple + 1 CURVE | 15 | 53 | 128 | 2.4× short |
| 7 simple + 1 RING | 11 | 25 | 128 | 5.1× short |
| 7 simple + 1 NORMALIZE3 | 10 | **20** | 128 | **6.4× short** |

So a barrel front end alone closes Earth60 **only if Earth programs avoid long
operations almost entirely** — which is a question about what terrain programs
actually contain, and therefore Fabian's, not mine.

---

## What this says to do, in order

**1. The barrel front end is still worth building.** It is the difference
between 21 and 153 patch-fields for simple programs, it needs no game-behaviour
decision, and it is a prerequisite for everything below: fixing a unit's II is
pointless while the front end serialises at 7 clocks per instruction.

**2. Then fix the units by measured II, not by reputation.** The order the model
gives is NORMALIZE (61), RING (48), LEN (42), SPLINE (39) — and *not* the ROT
and CURVE work the earlier waves happened to touch.

**3. Replication is the expensive answer for NORMALIZE.** Closing 128
patch-fields with one NORMALIZE per eight instructions needs
`128 × 1,089 × 61 = 8,502,912` clocks against a 1,333,333 budget: **seven
NORMALIZE units**. Pipelining the isqrt so it accepts every clock collapses that
to one, and the front end binds again at 153/frame. **Pipelining the long units
is worth more than replicating them, by roughly the ratio of their II.**

**4. The multiplier split may be free timing as well as throughput.** The census
found `u_mul` on 160 of the top 200 worst paths. `MUL_LANES=1/2/3` costs 3/6/9
DSPs against the old engine's 79, and splitting the hub attacks the timing
plateau and the throughput ceiling with one change.

---

## What must be verified before any RTL

* **Confirm the derived IIs directly.** A dispatch-to-accept counter per unit,
  rather than subtracting 7 from a total latency. If a unit overlaps its tail
  with the next accept, its real II is lower and this model is pessimistic.
* **Characterise the banked register file in Quartus, not in arithmetic.** The
  proposal is 4 banks × 3 replicated copies, 16 contexts × 16 registers × 32
  bits = 8,192 bits per bank. The estimate is ~12 M10Ks of 553. The device has
  had hundreds idle while Field starved, and the earlier RF conversion already
  proved that trading ALM mux forests for synchronous memory pays — but that
  conversion was *measured*, and so must this be.
* **The Earth instruction histogram.** Every row above is parameterised on "one
  long op per eight instructions" because nobody has said what an Earth program
  contains. This is the single input that decides whether the barrel front end
  alone is sufficient or merely necessary.

## What this does not cover

`TERRAIN.PATCH`'s ordered-pipeline proposal — one stage per accepted field,
different vertices in different stages, one composed vertex per clock after fill
instead of `1/(1+n)` — is a separate model and a separate deliverable. It
addresses the *other* Earth60 ceiling documented in
`reports/EARTH60_CAPACITY.md`, which no amount of Field-side work can move.
