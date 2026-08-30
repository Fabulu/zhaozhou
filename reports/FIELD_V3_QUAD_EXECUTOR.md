# Field v3 — the four-wide executor, and why the machine stops without it

Written 2026-08-30, from the composed Earth gate
(`tests/differential/field_v3_earth_directed.cpp`). Every number here is
measured on the real RTL, not modelled.

---

> ## CORRECTED 2026-08-30, AFTER BUILDING IT
>
> **The diagnosis in this file was right and the prescribed mechanism was
> wrong.** The machine really was write-port bound at 92%, and widening the
> executor really was the fix. But the section below headed "The register-file
> banking conflict, which must be settled FIRST" prescribes **four write ports
> banked by `ctx[1:0]`**, and that is not what was built and not what worked.
>
> **What worked: a register is a VECTOR, not a number.** The register file word
> widened from 32 bits to `32*LANES`, so one write port carries all four lanes'
> results in a single write. There is still ONE write port. The read/write
> banking conflict the section agonises over **does not exist** once the four
> lanes' values live in the same word -- there is nothing to bank against,
> because the four lanes were never four separate accesses.
>
> The four-port version was actually attempted and reverted: it deadlocked and
> was never measured. The width version shipped, and the whole file went from
> 5.1x over budget to inside it.
>
> Everything above the banking section -- the 92% measurement, the ALU/long
> split, the service initiation intervals, the preload defect -- stands as
> written. The banking resolution and the "expected result" arithmetic at the
> end are superseded by
> `reports/FIELD_V3_EARTH_OPTIMISATION_NOTES.md`, which reports the measured
> outcome: crater_ring 663,936, impact_wave 594,048, wave_pool 559,104, all
> inside 850,000, 12,308 values exact.
>
> Kept rather than deleted because the reasoning is the interesting part: the
> conflict was real, the resolution was elaborate, and the actual answer was to
> make the conflict not arise. A confident architectural prescription derived
> from a correct measurement is still a prescription, and this one cost a
> deadlocked afternoon.

---

## The finding, in one line

**The machine is write-port bound at 92% occupancy, and no service can fix it.**

```
register writes 4096 in 4464 clocks = 92% of the ONE write port
```

`impact_wave` is 16 uops. A four-point group therefore costs 64 register
writes, and one write port means **at least 64 clocks per group** against an
admission budget of 24.3. Across the 128-association stress frame that is
2,236,416 writes: **2.6x over 850,000 from the write port alone**, whatever the
services do and however many contexts are in flight.

This is not a throughput problem in any arithmetic block. Every service was
pipelined and measured this session:

| service | II | | service | II |
|---|---|---|---|---|
| SIN / COS | 4 | | CURVE / DCURVE / SPLINE | 13 |
| RING_PREP | 19 | | LEN2 / LEN3 / DIST2 | 22 |

At II 22, the busiest service supports the whole frame in 768,768 clocks —
inside budget. The arithmetic has been inside budget since the services were
rebuilt. The writeback is what is outside it.

---

## The asymmetry

The service path is **four points wide**. It takes a four-point group, computes
four answers, drains four results.

The executor is **scalar**. One context, one instruction, one ALU result, one
write per clock.

So the machine is a four-wide back end fed by a one-wide front end, and the
front end is where every uop's result has to land.

**Widening only the drain would move almost nothing**, and the split is
measured rather than assumed:

| program | uops | long | ALU | writes per four-point group |
|---|---|---|---|---|
| crater_ring | 13 | 2 | 11 | 52 |
| impact_wave | 16 | 2 | 14 | 64 |
| wave_pool | 17 | 3 | 14 | 68 |

The ALU is **85% of the write traffic**. A four-wide drain with a scalar ALU
addresses about 15% of the problem, which is why the executor and not the drain
is the thing to widen.

**The fix is a quad-wide executor: four contexts advancing in lockstep through
one pipeline, with four ALUs and four register-file write ports.**

Because the four contexts of a quad run the same program at the same pc, the
control logic is shared and only the datapath replicates. It is SIMD across the
quad, which is precisely what the dispatcher already assumes when it gathers
four points into a group.

---

## The register-file banking conflict, which must be settled FIRST

This is the part that will bite whoever starts, and it has a clean answer.

`zhao_field_v3_rf` banks on `register[1:0]` so that a register GROUP
(`a`, `a+1`, `a+2`) can be read for one context in a single clock. Its own
header explains why: registers 3 and 4 are different banks, so a group crossing
a multiple of four needs per-bank addresses. That banking is correct and must
stay.

But four lanes of a drain write **the same register index for four different
contexts**. Same `register[1:0]`, so one bank, four different rows. A
bank-per-write-port scheme therefore gives one write per clock, not four. Reads
want banking by register; writes want banking by context.

**Resolution: bank by `ctx[1:0]` OUTSIDE the existing register banking.**

* Each context lives entirely inside one ctx-bank, so a group read
  (`a`, `a+1`, `a+2` for ONE context) is unchanged — it still resolves inside
  that context's own copy of the existing four-way register banking.
* A quad is contexts `4k .. 4k+3`, which are four DIFFERENT `ctx[1:0]` values,
  so four lanes write four different ctx-banks and proceed in parallel.

The two bankings are orthogonal because one indexes by context and the other by
register. Nothing about the group read changes.

Cost at CTX=32: 4 ctx-banks x 4 register-banks x 3 read copies = 48 memories,
each 8 contexts x 8 registers x 32 bits = 2 Kbit. About 96 Kbit total, which is
modest against the device's M10K budget. Re-fit before quoting a number — the
existing 372 ALM / 12 M10K figure is already a LOWER BOUND for the functional
file rather than a measurement of it.

**The quad invariant this creates:** a dispatch group's four points must come
from four contexts with distinct `ctx[1:0]`. The QUAD drive pattern already
provides exactly that, and making it an invariant of the machine rather than a
property of the driver removes context drift — and therefore partial groups —
at the source rather than by arbitration.

---

## What is already done, so nobody redoes it

* **Services pipelined.** DIST2 146 -> 22, RING_PREP 50 -> 19, SIN/COS 22 -> 4;
  CURVE was already 13. Measured as initiation intervals, streamed, each with
  distinct per-group data.
* **Per-op gather.** The dispatcher holds one gather slot per opcode
  (`GATHERS`, default 4). Partial groups went 138/215 -> 0/138. Before this, full
  groups and overlapped services were in direct conflict.
* **Scaling knobs.** `CTX` and `OUTSTANDING` reach the top of the composition.
  `OUTSTANDING` lives in the dispatcher's parameter PORT list — a parameter
  declared in a module body cannot be overridden by an instance.
* **The preload defect.** The host preload port used to win the register-file
  write port outright and silently discard the machine's own granted write. The
  machine wins now, and `pre_ready_o` tells the host when its write landed. This
  is the one to keep in mind when adding write ports: **a dropped write must
  never be silent.**

## Current standing, CTX=32 / OUTSTANDING=12 / GATHERS=4

| program | clocks/group | frame | against 850,000 |
|---|---|---|---|
| impact_wave | 82 | 2,865,408 | 3.4x over |
| wave_pool | 90 | 3,144,960 | 3.7x over |

Exact: 4,608 values checked against `zfield::execute_point`, zero mismatches,
across all three drive patterns.

`crater_ring` still does not run at all — it needs 7 vector + 29 uniform = 36
registers against a file of 32, and the executor has no scalar-bank read path,
so uniforms reach it only by broadcast. That refusal is a finding, not a skip,
and a wider file changes the arithmetic.

## The expected result

64 writes per group / 4 ports = 16 clocks of writeback per group, under the
24.3 budget, with the busiest service (DIST2, II 22) becoming the limit again
at 768,768 clocks per frame. That is inside 850,000 with margin.

**That projection is arithmetic, not a measurement.** It assumes the four lanes
keep the services as busy as one lane does, and this session has already
produced one confident counter-derived prediction that was wrong by 2x. Run the
gate.
