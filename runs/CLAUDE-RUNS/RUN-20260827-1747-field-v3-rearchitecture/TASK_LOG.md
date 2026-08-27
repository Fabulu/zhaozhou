# Task Log: RUN-20260827-1747 - FIELD v3 rearchitecture (phases 1-3)

**Created:** 2026-08-27 17:47 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260827-1747-field-v3-rearchitecture/

---

## Objective

Execute reports/Fieldv3.md phases in strict order:
1. Freeze v2 as exact fallback / differential oracle; amend FIELD.SEQ.CORE,
   FIELD.SEQ.EARTH, TERRAIN.PATCH, the Field cost model and FIELD.PROGCACHE;
   regenerate the cost model with measured numbers (59.22 MHz, transport,
   patch reduction, table loads, real service IIs, active vertex counts).
2. Exact software planner (FPLAN) with table-GENERATED canonical->uop
   translation and shared semantic step functions; full differential vs
   zfield::interpret. NO v3 RTL until green.
3. Five decisive probes: ready-context FIFO scheduler, 4x8x32 vector RF,
   two-bank distance service (II<=20), barrel curve service (II<=14),
   four-bank patch accumulator. Each: fit, Fmax, setup+hold, measured II,
   randomized differential, mutation sweep.

---

## Progress Timeline

### 2026-08-27 17:47 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260827-1747
- Created working directory
- Initial context: brief read in full (reports/Fieldv3.md, 661 lines, e9b6e1f); recon agents dispatched for fit numbers + test infra

---


### 2026-08-27 18:20 - Phase 1 complete: freeze + contract corrections

- v2 freeze ruling written into zhao_field_v2_core.sv / zhao_field_v2_front.sv
  headers (exact fallback / differential reference / not the production path),
  with the measured numbers cited (59.22 MHz, -6.886/-1.938 ns, 27,225
  transport clocks, 439-930%).
- FIELD.SEQ.CORE.md: one-semantic-engine + profile-adapter ruling, three-engine
  status table, FPLAN-is-derived-artifact law.
- FIELD.SEQ.EARTH.md: stub replaced — real E lane bindings (x,z varying at
  R0/R1; age/phase/p0..p7 uniform), walker/export architecture, the real
  deadline (<=6,000/assoc, <=850,000/frame, 80% clock rule), probe gates.
- TERRAIN.PATCH.md: field-major internal reducer amendment; law 1 marked
  superseded for v3 (kept as as-built + differential comparison point).
- FIELD.PROGCACHE.md: FPLAN storage/lookup (hash+planABI+fabric triple,
  performance class travels with the plan, tables resident-with-plan).
- spec/form/cost-model.md: section 5 resource-demand vectors + admission law.
- reports/FIELD_V3_COST_MODEL.md regenerated from committed script
  tools/report/field_v3_cost_model.mjs (hash-pins programs; reproduces the
  brief's arithmetic: splits 16/13, 17/9, 13/14; DIST binds at 5,460/assoc;
  837,888/frame; honest v2 = 1,183-2,012% at 59.22 MHz WITH transport).
- ledger check green. Baseline fast gate: 367/369 green; the 2 red lanes
  (reel_sequence_crc, cmd_dma_directed "Unable to find executable") are the
  CONCURRENT session's in-flight dirty files (tools/reel/*, zhao_cmd_dma.sv;
  their sweep deleted/rebuilt the exe mid-run) - not lanes my files feed.

---


### 2026-08-27 19:10 - Phase 2 complete: exact software planner, differential GREEN, sweep 16/16

Built (no v3 RTL touched, per the gate):
- compiler/src/field_ir/gen_optable.ts: canonical->uop table GENERATOR reading
  OP_INFO (the one canonical table); emits reference/include/zfield/generated/
  zfield_optable.hpp with static_asserts pinning every code to the zfield::Op
  enum. --check lane added to ctest (field_optable_check) so it cannot go stale.
- reference/include/zfield/zfield_steps.hpp: the ONE semantic step layer,
  extracted VERBATIM from the interpreter switch; exec_op() + ring_mid() +
  ring_prepared(). interpret(), prepare() and the executor all call it.
- reference/src/zfield/zfield_interpret.cpp: refactored to gather/exec_op/
  scatter driven by the generated shapes. Bit-identical; witnessed by the
  golden .zvec replay and every RTL differential lane staying green.
- reference/{include,src}/zfield/zfield_plan.{hpp,cpp}: the FPLAN planner.
  Forward-taint uniform/varying split (kill on overwrite), SSA-slotted scalar
  bank, prepared-ring lowering (m/rA/rB prepared once, ONE UOP_RING_PREP with
  the nine separately-rounded products), order-preserving vreg compaction
  (adjacency argument in the code), demand vectors per cost-model.md section 5,
  hot/cold admission. prepare() + execute_point() with per-lane ledgers.
- tests/differential/field_fplan_diff.cpp, 3 ctest lanes (bare, --random 40
  fast, --random 600 nightly): committed programs (demand vectors PINNED to
  the cost model: 16/13, 17/9, 13/14 splits reproduced), boundary grids,
  prepared/cold/rcp0 ring, uniform-redef kill, uniform-only saturation,
  occupancy-driven cold, ring_mid law pins, random legal programs.
  MEASURED: directed 22,927 checks green; --random 600 = 324,000 checks green.
- tools/sweep_field_plan.{sh} + _mutants.py + _preflight.py: 16 mutants over
  the brief's software failure classes. MEASURED: 16/16 caught, 0 equivalent,
  0 discarded, SWEEP OK.

THE SWEEP FOUND A REAL HOLE before its closure: P07 (ring midpoint rounding
replaced by saturating-add-then-floor) SURVIVED the whole differential corpus
AND the crater golden .zvec replay - no vector ever sampled an odd or
overflowing radii sum. Closed with direct ring_mid law pins (round-half-up,
33-bit-sum-never-saturates cases) in the directed lane; re-run caught 16/16.

Environment note for the next author: the Bash tool's heredoc collapses
backslash escapes (
 became a literal newline INSIDE a tr argument,
silently turning it into "delete all newlines"). Write scripts with the
Write/Edit tools; the brief's warning extends beyond SystemVerilog.

---


### 2026-08-27 20:30 - Phase 3 in progress: probes 1 and 3 built and measured (sim)

Probe 3 (two-bank exact distance service, zhao_probe_dist_svc.sv):
- 8x the frozen zhao_field_isqrt in two banks of four; service boundary is
  n2 (the mul bank supplies squares, per the brief); accept-order reply FIFO.
- MEASURED (Verilator): lone-reply latency 34 cycles; four-point II over 32
  streamed groups = 17 clocks -- THE GATE (<=20) PASSES in simulation.
  367 directed + 3,600 random checks green.
- The first backpressure section EXPECTED wrong behaviour: the reply register
  is a skid buffer, so the service legally holds THREE requests with replies
  blocked. Test corrected to pin the real capacity + fourth-refusal.
- Mutation sweep: 12 mutants (3 reshaped for -Wall lint, house precedent).

Probe 1 (ready-context FIFO scheduler, zhao_probe_ctx_fifo.sv):
- S0 dequeue+register, S1 REGISTERED plan fetch (true sync RAM), S2 dispatch;
  modeled countdown service; invariants exported for audit.
- THE PROBE'S OWN TEST FOUND A REAL ARCHITECTURAL FLAW in the first draft:
  one FIFO write port with short-requeue priority let eight circulating
  all-short contexts starve service completions and host starts FOREVER
  (measured: issue wedged at 28 slots in a 64-cycle window). Fixed with a
  second write port (service-over-start priority). This is exactly the class
  the brief's formal list names (lost contexts / starvation) and exactly what
  a probe is for -- found before any fit, at zero cost.
- MEASURED (Verilator): 1 instruction/clock sustained over a 64/64 steady
  window with 8 resident contexts; 39 directed checks + 60 random storms
  green (no lost/duplicated instruction, one-in-flight law, restart refusal).
- Harness lesson recorded: an audit that starts after start_ctx calls
  miscounts (contexts issue during their siblings' starts) -- run_storm now
  performs starts INSIDE the audited loop.

Phase 2 addendum: V20 caught a claim without ENFORCED-BY in the dist probe
header (fixed); format tier caught unformatted new C++ (clang-formatted);
the optable ctest lane was REMOVED in favour of compiler/tests/optable.test.ts
because compiler/dist is gitignored and CI does not build the compiler -- a
SKIP-if-absent ctest lane would be the drift-hiding failure MEMORY warns of.

---


### 2026-08-27 (late) - sweeps final, fits in flight, one process error caught by the tooling

Sweep tallies, all MEASURED:
- planner sweep:   16/16 caught, 0 equivalent, 0 survivors, 0 discards. SWEEP OK.
- dist svc sweep:  13 mutants -> 12 caught + 1 PROVEN equivalent (D01: sat_len's
  > vs >= coincide at the single differing input; flag compare is separate and
  its own mutant D13 is caught). SWEEP OK.
- ctx fifo sweep:  13 mutants -> 12 caught + 1 PROVEN equivalent (F03: pc
  increment at S0-exit vs S1-exit is unobservable because a context is never
  in S0 and S1 simultaneously; the real cross-context increment F13 is
  caught). SWEEP OK.
Both equivalence proofs carry RE-SCORE triggers in the machine-readable table.

PROCESS ERROR, honestly recorded: the ctx-fifo mutation sweep was launched
while the ctx-fifo Quartus fit was still running -- the sweep mutates the very
file in the fit's source cone. The fit runner's provenance enforcement CAUGHT
it: the row records status "contaminated:source-changed-during-fit" with the
contaminated file named, and no numbers were recorded. The fit is re-run
clean after the sweeps. The rule ("do not edit any file in a fit's source
cone while one runs") failed at the operator, not the tooling.

Environment note: the sweep rebuild() needed BOTH the VERILATOR_ROOT export
AND the winlibs PATH pin -- a bare cmake in the sweep's bash env resolves to
the msys2 cmake, fails the preset, and leaves the OLD build.ninja, exactly as
BUILD.md warns. Both are pinned inside the sweep scripts now.

---


### 2026-08-28 - all three fit probes MEASURED; Phase 3 probes 1-3 closed

| probe | fit (5CSEBA6U23I7, Quartus 17.0.2) | at the 100 MHz constraint |
|---|---|---|
| ctx FIFO scheduler | 257 ALM, 221 reg, 1 M10K, 0 DSP, restricted Fmax 97.8 MHz (678 s, cb48f48, clean re-fit) | setup -0.225 (TNS -0.239, ~one path), HOLD +0.445 |
| banked RF @ 8x32 (v3hot) | 372 ALM, 12 M10K, 0 DSP, 93.14 MHz (1,366 s, e706f69) | setup -0.736, HOLD +0.691 |
| two-bank dist service | 1,745 ALM, 2,199 reg, 0 M10K, 0 DSP, 90.6 MHz (1,590 s, cb48f48) | setup -1.038 (TNS -546), HOLD +0.268 |

Read against v2's leaf (59.22 MHz, setup -6.886, HOLD -1.938): every v3
probe is 31-38 MHz faster than the v2 plateau and every one has POSITIVE
hold. The brief's structural claims survive contact: the FIFO killed the
scheduler loop; the reduced RF is exactly 12 M10K; eight roots price at
~218 ALM each. The one finding AGAINST the naive envelope: none of the
three probes reaches 100.0 MHz standalone -- 90.6-97.8 -- which supports the
brief's own 80-90 MHz-credible / design-for-100 framing rather than the
100-MHz-for-free reading.

Quartus 17 gotcha recorded: inline genvar declarations in a generate-for
are a map-time syntax error (10170); declare genvars ahead. Cost one fit
cycle (35 s failure + refit).

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*
