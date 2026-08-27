# FIELD v3 — the decisive probes (Phase 3 of reports/Fieldv3.md)

Five probes were mandated; a probe that misses its target kills or changes
its topology BEFORE it contaminates the engine. Status per probe, every
number labelled **measured** (with where) or **pending**.

## Probe 1 — ready-context FIFO scheduler (`zhao_probe_ctx_fifo.sv`)

Replaces v2's measured select→index→feedback scheduler loop with explicit
ownership: enqueue on ready, pop to issue, re-enqueue on completion, release
at END. S1 is a TRUE registered plan fetch (sync RAM, address one edge,
word the next).

| what | result |
|---|---|
| issue rate, 8 resident all-short contexts | **measured (Verilator): 64 issue slots in a 64-cycle steady window — 1 instruction/clock** |
| context invariants (loss/dup/one-in-flight/restart-refusal) | measured green: 39 directed checks + 60 random storms (`field_ctx_fifo_directed`, `--random 60`) |
| mutation sweep | `tools/sweep_field_ctx_fifo.sh` — see tally below |
| fit (Fmax, setup AND hold, ALM) | pending / see `reports/synthesis/zhao_block_fit.json` row `zhao_probe_ctx_fifo` |

**The probe already paid for itself before any fit:** the first draft gave
the ready FIFO ONE write port with the short-op requeue preferred; eight
circulating all-short contexts requeue every cycle, so service completions
and host starts starved forever (measured: issue wedged at 28/64 slots).
The production topology therefore requires the TWO-PORT enqueue this probe
now carries (service-over-start priority on port 2). That is a real
architecture change bought for the cost of a directed test, which is the
whole argument for probes.

## Probe 2 — reduced 4×8×32 vector register file (`zhao_probe_banked_rf`)

The probe already exists and measured 375 ALM / 12 M10K / 96.54 MHz at the
16-context × 64-register geometry. The v3-hot geometry is a PARAMETER
variant: `-TopParameters CONTEXTS=8 REGS=32 -RowLabel "@v3hot"`.

| what | result |
|---|---|
| fit at 8×32 | pending / row `zhao_probe_banked_rf@v3hot` |

Per-bank storage at 8×32 is 8 ctx × 8 regs × 32 b = 2,048 bits per replica —
small enough that Quartus may choose MLABs over M10Ks; either answer is the
point of fitting rather than predicting.

## Probe 3 — two-bank exact distance service (`zhao_probe_dist_svc.sv`)

Eight copies of the frozen exact root (`zhao_field_isqrt`), two banks of
four, accept-order reply FIFO. Service boundary is the u64 `n2` (the vector
multiplier bank supplies the squares, per the brief).

| what | result |
|---|---|
| four-point DIST2 II | **measured (Verilator): 17 clocks over 32 streamed groups — GATE ≤ 20 PASSES** |
| lone-reply latency | measured: 34 cycles (32-step root + handshakes) |
| exactness vs `zref::isqrt_u64` + `len_of` saturation | measured green: 367 directed + 3,600 random checks (incl. per-lane sat flags mixed within one group) |
| backpressure capacity | measured: three requests held with replies blocked (reply register is a skid buffer), fourth refused, replies drain in accept order |
| mutation sweep | `tools/sweep_field_dist_svc.sh` — see tally below |
| fit (Fmax, setup AND hold, ALM/DSP/M10K) | pending / row `zhao_probe_dist_svc` |

The demand model makes this probe the binding one: all three committed Earth
programs bind on this service at 273 × II clocks/association
(`reports/FIELD_V3_COST_MODEL.md` §4). At the measured II 17 the binding
occupancy is 4,641 clocks/association — under the 6,000 deadline with more
margin than the ≤ 20 target assumed.

## Probe 4 — barrel curve service (target four-point II ≤ 14)

**Not started.** The structural minimum is 12 lookup clocks (24 table reads
through 2 ports); the probe needs the registered dual-port table RAM, barrel
lane contexts and the shared 6-step search. Next in decisiveness after the
distance service: impact_wave's curve occupancy becomes the binder if the
fitted II lands above 14 while DIST holds 17 (crossover at curve II ≥ 17).

## Probe 5 — four-bank patch accumulator with exact command-order reducers

**Not started.** Requires the TERRAIN.PATCH field-major amendment's reducer
laws (height saturating add per-add, material writer-selection, velocity/nav
declared accumulations) against `zref::terrain::compose_vertex` as the
bit-exact comparison, plus the vertex-major/field-major agreement test.

## Sweep tallies

| sweep | result |
|---|---|
| planner (`sweep_field_plan.sh`, 16 mutants) | measured: 16 caught / 0 equivalent / 0 survived / 0 discarded — after closing the ring_mid rounding hole its first run FOUND (odd/overflowing radii sums were never sampled by any lane) |
| distance service (`sweep_field_dist_svc.sh`, 12 mutants) | see TASK_LOG for the current tally |
| scheduler (`sweep_field_ctx_fifo.sh`, 12 mutants) | see TASK_LOG for the current tally |
