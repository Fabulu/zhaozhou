# Owner direction — hardware rearchitecture priorities, 2026-09-07

`ZHAOZHOU_HARDWARE_REARCHITECTURE_PRIORITIES_2026-09-07.txt`, 2,373 lines,
plus a research/checks bundle. Acknowledged on the hardware branch, beside the
hardware it governs, per the standing rule that a run folder is the wrong home
for anything durable.

## The ruling I am working to

Execution order, verbatim from §3:

> finish the adopted owner authority step; reconcile the RCP specimen and
> workload; take the next small measured control/RCP change; integrate
> replacement return/lifetime paths; consolidate joins and remove old stores;
> close real boundaries; refit the feature-live island. Terrain remains queued.

And: *"Preserve the currently running owner fit."*

## Items the brief lists as open that CLOSED after its 17:16:42 snapshot

The brief is explicit that it reflects a refresh at `5afe76b1`, 17:16:42 CEST,
and that its owner numbers are intermediate. Four of its named obligations have
since landed. Recorded here so the next pass does not redo them:

* **"the report itself leaves actual bank-write observation and the
  claim-to-write lease unfinished"** — both now closed.
  * §8.3's lease holds **by construction**, and became true *because of* the
    §8.2 change: all three bank write enables are registered from `c2*_acc_c`,
    and `c2t_idok_c` now carries current membership. So the RAM write enable is
    "current instance owns this slot and source", registered, with no late
    combinational predicate in front of the RAM — the shape §8.3 permits, not
    the one it warns against. §8.3 therefore needs no edit and by its own
    warning should not get one.
  * Bank-write observation: V05 was strengthened from "no output appeared" to
    "no bank row was written", by reading the row contents after a refused late
    return.
* **"this refresh did not establish new completed owner ALM/Fmax numbers"** —
  established. `zhao_texture_v3own`, commit `d722f085`, digest `96772668c27c`:
  **ALM 5,709 → 3,348 (−41.4%)**, **core→core 91.32 → 98.18**, fitted registers
  4,863 → 3,953, M10K/bits/DSP/pins unchanged.
  `reports/V31-T2-OWNER-FIT-20260907.md`. So **P0-D's "Last fitted 5,709 ALMs"
  is superseded**, and the owner block's remaining payoff should be judged
  against 3,348 — which the brief itself asks for.
  The ALM gate still FAILS at 3,348 against `max_alms: 1800`; not relaxed.
* **"520 passing owner checks"** — now **526**, including §22.8's "return a late
  old packet during local drain" and the T2 ruling's own scenario.
* **"The retrieved archived raw MAP summary still says 4,310"** — the current
  MAP says **3,750**, the exact predicted −560, and the FIT report contains
  **zero** occurrences of the bare `gen_q` table and zero of `ftc_q`.

## P0-A: resolved, and the mismatch was in the specimen

`reports/P0A-RCP-SPECIMEN-RESOLVED-20260907.md`. `scan_cur_q`, `e_busy` and
`e_ra` appear **nowhere in `fpga/rtl/`** and zero times in the island's fitted
report; `c_val` appears 28 times and the worst path is
`rcp24_svc|c_val[5] -> rcp24_svc|c_m.raddr_a[0]` at the brief's own −3.243 ns.

The specimen is `zhao_raster_rcp24_svc.sv`, and the selector→address mapping the
gate asked for is `rr_q` → the NCTX priority scan over `c_val && c_pend` →
`pick_i` → `c_m[pick_i]` as a RAM read address, all in one combinational cycle.
P0-B may proceed against lines 153–179.
