# P0-B — the RCP selection/execution boundary, registered

`fpga/rtl/raster/zhao_raster_rcp24_svc.sv`. Implementation of the rearchitecture
brief's first-priority scheduler change, on the specimen P0-A resolved.

## What was wrong

The fitted island's worst internal path, −3.243 ns:

    zhao_raster_rcp24_svc:u_rcp|c_val[5]~DUPLICATE
      -> zhao_raster_rcp24_svc:u_rcp|c_m.raddr_a[0]~0_OTERM1091

`c_m` is `logic [23:0] c_m [NCTX]`, inferred as RAM, so `c_m.raddr_a` is its read
address. The chain in one clock was:

    rr_q -> NCTX-way priority scan over (c_val[idx] && c_pend[idx]) -> pick_i
         -> c_m[pick_i] / c_x[pick_i] / c_w[pick_i] / c_ph[pick_i]
         -> operand mux -> 64'(mul_a_c) * mul_b_c -> m1_p_q

Selection, context read, operand preparation and a 32×64 multiply, all sharing
one clock — which is the structure §5.2 asks to be separated.

## What changed

One register: **S1, the issue record**. `{context, phase}` is captured when the
arbiter's choice is accepted; the operand read and the multiply address storage
from that flop instead of from the live scan.

**The arithmetic did not change.** §5.4 is explicit that the exact law — ROM
seed, two Newton iterations, the uint64 wrap, rescale-by-30 round-half-up and
uint32 truncation, the rescale-by-7 and 24-bit clamp — is not to be touched to
buy a fit. Only the index feeding the operands moved.

**Eligibility is surrendered at SELECTION.** `c_pend[pick_i]` clears on the
selecting edge, so a context with a job in S1 is already invisible to the next
cycle's scan. §5.3 and the brief's C01 model both name the alternative —
clearing on execution — as the duplicate-select window this exact change opens
if done carelessly.

Completion also had to learn about S1: a context whose last job is selected but
not yet multiplied has `c_pend` low and parks at `PH_MX1`, so without the extra
term a reciprocal would be reported done a cycle before its final multiply
launched.

## Measured

| | before | after |
|---|---|---|
| `raster_rcp24_svc_directed` | 5 pass | 5 pass |
| multiplier launches / reciprocal | 1628 / 407 = **4.00** | 1628 / 407 = **4.00** |
| clocks per reciprocal | 4.01 (1631) | 4.01 (1632) |
| `island_composed_directed` | — | **119 pass** |

One extra clock across a 407-reciprocal run: the added per-context latency,
hidden by the other contexts in flight. The shared multiplier still takes a
launch every clock, which is what §16.1's four-clock rate actually depends on.
The launch ratio is exactly 4.00, so nothing was issued twice.

## The detectors fire

Four assertions live in the RTL, not in a bench. Fire-tested by building the C01
failure mode deliberately — eligibility surrendered at execution instead of
selection:

    %Error: zhao_raster_rcp24_svc.sv:378: Assertion failed in
      TOP.tb_rcp24_pair.u_svc.a_svc_s1_not_eligible: 'assert' failed.

Immediate, on the restored-then-remutated source, with the source restored after.

**The first attempt at that fire test read a stale binary** and printed numbers
identical to the baseline — the documented tell. The executable was timestamped
17:57:09 against a source of 17:58:57. Comparing mtimes before believing the
result is what separated "the detector does not fire" from "the detector was
never compiled".

## NOT established

**No timing claim.** This is a source change with a simulation result. The brief
is explicit that an independent dispatch→FRAGROB family around −2 ns also exists
and that removing one family does not deliver the clock. Whether registering
this boundary moves the island's Fmax is a question for the next fit, and the
answer may be "the limiter moved somewhere else" — which is still information,
and is the reason the change is measured rather than assumed.
