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
| mutation sweep | measured: 13 mutants → 12 caught + 1 proven equivalent, 0 survivors (`sweep_field_ctx_fifo.sh`) |
| fit (Fmax, setup AND hold, ALM) | **measured** — row `zhao_probe_ctx_fifo` (sourceCommit cb48f48, clean re-fit after the contaminated first attempt): **257 ALM, 221 regs, 1 M10K (the plan RAM), 0 DSP, restricted Fmax 97.8 MHz**; at 100 MHz setup −0.225 ns with TNS −0.239 (essentially ONE marginal path), **hold +0.445 ns (positive)** |

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
| fit at 8×32 | **measured** — row `zhao_probe_banked_rf@v3hot` (sourceCommit e706f69): **372 ALM, 12 M10K (24,576 bits), 0 DSP, restricted Fmax 93.14 MHz**; at the 100 MHz constraint setup −0.736 ns (TNS −35.5), **hold +0.691 ns (positive)** |

The brief's hypothesis held exactly: one M10K per replica, 4 banks × 3
copies = **12 M10Ks** (Quartus chose M10Ks, not MLABs). Standalone the probe
misses 100 MHz by 0.736 ns with 284 virtual pins in the timing set — the
same family as the 16×64 geometry's 96.54 MHz; the composed number is the
one that binds, and hold is clean.

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
| mutation sweep | measured: 13 mutants → 12 caught + 1 proven equivalent, 0 survivors (`sweep_field_dist_svc.sh`); D06 (single-bank) proves the II gate bites |
| fit (Fmax, setup AND hold, ALM/DSP/M10K) | **measured** — row `zhao_probe_dist_svc` (sourceCommit cb48f48; the first attempt died in quartus_map because Quartus 17 rejects inline `genvar` in a generate-for — fixed): **1,745 ALM, 2,199 regs, 0 M10K, 0 DSP, restricted Fmax 90.6 MHz**; at 100 MHz setup −1.038 ns (TNS −546), **hold +0.268 ns (positive)** |

**Provenance footnote, added on review.** Both this row and
`zhao_probe_banked_rf@v3hot` carry `rtlCleanAtHead: false`, and this row's
`sourceCommit` is `cb48f48` -- the commit BEFORE the `genvar` fix the line
above describes. Those two facts together are the whole story: the fit ran on
the FIXED source while that fix was still uncommitted, so the runner stamped
the then-HEAD and flagged the tree dirty.

Checked rather than assumed: `git diff cb48f48 HEAD` on the probe is 19 lines
and all of it is the genvar hoist -- `for (genvar b = ...)` becoming a
`genvar gb, gl;` declared ahead, with the loop variables renamed in the port
connections. Same generate structure, same connections, no logic. **The
measurement therefore describes the logic HEAD carries**, and the number
stands.

It is written down because a row whose `sourceCommit` predates the code it
measured is exactly the shape of a stale-measurement trap, and the next
reader should not have to re-derive that it is benign here.

The measured 1,745 ALM ≈ 218/root across eight roots — inside the brief's
"~2,000 leaf ALMs, plausible rather than catastrophic". 90.6 MHz standalone
sits above the 80 MHz credibility floor and below the 100 MHz design point;
the pressure is the root's 64-bit compare-subtract recurrence, which is
precisely the radix-4-vs-duplicated-root decision the brief reserved for a
second fitted probe. VERDICT: topology sound (II 17 ≤ 20 with margin, hold
clean, resource cost as priced); the root's inner loop is the named
follow-up before the composed engine, not a reason to change the two-bank
shape.

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
| distance service (`sweep_field_dist_svc.sh`, 13 mutants) | measured: 12 caught / 1 PROVEN equivalent (D01, with re-score trigger) / 0 survived / 0 discarded — SWEEP OK |
| scheduler (`sweep_field_ctx_fifo.sh`, 13 mutants) | measured: 12 caught / 1 PROVEN equivalent (F03, with re-score trigger) / 0 survived / 0 discarded — SWEEP OK |

Both "equivalent" rulings were first SURVIVORS: the sweeps forced either a
machine-readable proof or a reshaped mutant, and each also gained a sharper
sibling (D13 flag-compare, F13 cross-context increment) that IS caught.
