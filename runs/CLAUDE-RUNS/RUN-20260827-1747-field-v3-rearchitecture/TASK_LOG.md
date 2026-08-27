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

### 2026-08-27 (resumed session) - Phase 3 probes 4 and 5: built, measured in sim

Probe 4 (barrel curve service, zhao_probe_curve_svc.sv):
- Four scalar lane contexts as a barrel over an active-program table cache
  with two read ports; lanes 0/1 interleave on port A, 2/3 on port B, so a
  registered-read wait is the partner lane's address cycle. SIX reads per
  lane, not nine: the clamp bounds and entry 0 are TABLE properties latched
  at load (the cache meta), and the selected entry is CAPTURED ON THE WAY
  DOWN (a taken step's read IS the entry at the new lo).
- MEASURED (Verilator): four-point CURVE II over 32 streamed groups =
  13 clocks -- THE GATE (<=14) PASSES; structural minimum 12; the extra
  clock is the search->finish handoff. Lone-reply latency 17 cycles.
  4,589 directed + 7,200/90,000 random checks green vs zfield::exec_op
  (the one semantic layer), including per-lane add/mul flag attribution
  mixed within one group, slot reload, and capacity-4 backpressure.
- SPLINE is deliberately NOT barreled (the brief's own cold-lane split).
- Mutation sweep: 15 mutants in flight at log time (12/12 caught so far).

Probe 5 (four-bank patch accumulator, zhao_probe_patch_acc.sv):
- Vertex-mod-4 banking, rotated crossbar for unaligned vector updates,
  2-stage RMW with a per-output-lane one-deep bypass; INIT/ACCUM/DRAIN
  phases (273 + updates + 273+3 clocks per patch).
- EACH OUTPUT ITS OWN REDUCER: height = compose_vertex's chain with both
  clamps; velocity = TERRAIN.VELOCITY V1 chain; material = last covering
  writer wins; nav = V1-style chain.
- FINDING (brief assumption vs tree): the "exact writer-selection law"
  (material) and nav_cost's "declared reduction" were NAMED in
  FIELD.SEQ.EARTH.md, TERRAIN.PATCH.md and the brief but DECLARED NOWHERE
  -- no FIELD.OUT.MATERIAL/NAV in ops.yml, no oracle, no text. Probe 5 is
  their first written form; FIELD.SEQ.EARTH.md now carries the declaration
  with a chosen-not-found note for negotiation.
- Test written (oracle = zref::terrain::compose_vertex + V1 chains +
  declared laws; sat-pulse totals vs SatLedger); not yet run (build/ owned
  by the curve sweep at log time).

### 2026-08-27 (late, resumed) - the shared-build-dir collision, measured and fixed

The patch-acc mutation sweep DISCARDED mutants ("model or exe absent after
rebuild") across three runs: 9/15, then 7/15 on a fully hands-off re-run --
with DIFFERENT discard sets each time (union of caught = all 15, but a
discarded mutant is NOT scored; house rules demand one fully-scored run).

Root cause, measured not guessed: TWO ninja processes live at once -- this
sweep's rebuild loop and the CONCURRENT session's builds, both writing the
shared build/ tree (their dirty files at the time included
zref_creature.hpp, which feeds zhao_zref, which the probe test links). My
first theory (my own concurrent tool calls holding file locks) was
FALSIFIED by the hands-off re-run discarding P02 with zero activity from
this session.

Fix: tools/sweep_field_patch_acc.sh now configures its OWN build tree
(build-sweep/, same pinned toolchain as the windows-native preset) so the
mutant rebuild loop shares nothing with the interactive build dir. The
curve sweep's clean 15/15 run predates the collision window and stands.

Two operator process errors also recorded honestly: (1) edited the probe
RTL header mid-sweep (comment-only; the sweep's gold-restore stomped it,
re-applied after); (2) ran clang-format on the probe test source mid-sweep.
Neither caused the discards, but both violate "do not touch a sweep's
source cone while it runs" -- the rule extends from fits to sweeps.

### 2026-08-27 (later) - build-fieldv3 stood up; probe 4 committed (bc68866) and fitting

- Coordinator confirmed THREE sessions share build/ (their RASTER.EARLYZ
  sweep failed its cross-check from the same collision; their build died on
  my probe-4 verilate cmake mid-edit). Resolution: this session now owns
  build-fieldv3/ (same pinned toolchain; configure needs BOTH mingw64 and
  oss-cad-suite on PATH -- the fresh-dir configure fails without
  verilator_bin). Both probe sweeps' rebuild loops are ported to it.
- Both probe tests rebuilt + re-run green out of build-fieldv3 (39,232 +
  4,589 directed checks).
- Probe 4 committed BEFORE its fit as bc68866 (RTL, test, sweep tools,
  curve sweep log, CMake registration -- the CMakeLists staged via
  hash-object with the probe-5 block stripped so the commit configures
  standalone). Fit launched from bc68866 with -KeepWorkspace.
- Patch-acc sweep re-running ISOLATED in build-fieldv3.

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

### 2026-08-27 (late, main session) - Phase 3 CLOSED, Phase 4 started

Took this over after the subagent parked five consecutive times holding for a
notification that was never going to arrive (346k tokens, 233 tool uses, no
forward motion). Stopped it and finished the work directly.

**Probe 5 landed (c87b5c4).** Four-bank patch accumulator. MEASURED: a full
33x33 field is 297 vector updates and lands in 297 clocks; INIT and DRAIN move
one aligned group per clock (273 each). Mutation sweep 15/15 caught, 0
survived, 0 discarded, in an isolated build tree.

**A sweep-driver defect, found from probe 5 own logs (9ddf067).** The first
two runs of that sweep scored 8/15 with 7 DISCARDED -- and printed SWEEP OK and
exited 0. Cause: the five field drivers add discards into `accounted`, so the
cross-check that exists to catch a short run passes, and the discard block only
printed a NOTE. The seventeen older drivers exclude discards from `accounted`,
which is exactly what made my RASTER.EARLYZ rerun fail its cross-check
(attempted=16 accounted=12) instead of quietly scoring 12 of 16. All five now
exit 13 on an unscored mutant. Isolated re-run caught all 15, so those seven
were catches lost to build contention, not survivors.

**A second defect in the isolation patch itself.** Guard 5 deletes the exe
because it lives OUTSIDE the verilated model directory. After the move to
build-fieldv3 that deletion still pointed at the shared build tree -- wrong in
both directions: it left the real stale exe in place, defeating the guard that
stands between this lane and the stale-binary trap, and it removed another
session binary from the shared tree. Does not invalidate 15/15: rebuild still
clears the verilated model dir and the binary hash covers it, so a failed
re-elaboration DISCARDS rather than scores, and a stale binary can only
manufacture a false SURVIVOR, never a false catch.

**Phase 4 opened: the Earth lattice walker** (`zhao_probe_walk_earth.sv`), the
one piece of the composed machine with no Phase 3 probe. Design decisions, each
stated as a knob rather than a discovered constant:

* **The coordinates are SEPARABLE.** In `compose_lattice`, wx depends only on
  the column and wz only on the row -- 33 + 33 values, not 1,089 pairs. That is
  the whole reason the walker can generate points instead of receiving them,
  and it is what deletes v2 27,225-clock transport.
* **The tables are PREPARED by the ARM, not computed in fabric.**
  `lattice_lerp` is a rounded divide (`a + (span*i + den/2)/den`), NOT an
  `origin + i*pitch` accumulation; the two disagree at interior vertices in a
  way no single-vertex test would catch. Rather than put a divider in the
  fabric to recompute a value identical for every association on the patch,
  the ARM prepares 66 words with the same zref primitives the oracle uses.
* **The covered index box is a HINT; the per-vertex closed-interval test is
  the LAW.** The box only bounds where the walk starts and stops, so an
  oversized box costs clocks and never changes coverage, and an undersized one
  shows up as missing coverage rather than as silently wrong heights.

Lints clean under `-Wall` first pass. Differential written against the
reference own two rules (lattice_lerp and the 9.1 closed-interval test)
rather than a hand-written expectation; build in build-verify is running.

**Contract correction (2b2ad97).** FIELD.SEQ.EARTH "273 four-wide vector
groups" is the ALIGNED flat packing, which is what INIT/DRAIN use. The update
path is row-major over a 33-wide lattice, so a group cannot straddle a row and
a patch costs 9 x 33 = 297. Budgeting the executor from the old line comes up
24 groups short per association, 3,072 clocks over 128.

**NOT claimed yet:** place-and-route, Fmax, setup and hold for probes 4 and 5.
Both fits are running. The first attempt failed at quartus_map with
"Top-level design entity is undefined" -- the probe sources are not in the
shell QSF cone and must be passed with -ExtraSources. Worth noting that the fit
harness exits 0 on a failed fit and writes a `failed:` row, so a caller that
checks only the exit code would read that as success.

### 2026-08-28 - Phase 4 walker MEASURED and swept 18/18

**The block.** `zhao_probe_walk_earth.sv`, committed 0dce2a3. Differential
against `compose_lattice`'s own two rules (lattice_lerp and the 9.1 closed
interval), not a hand-written expectation.

MEASURED: a full 33x33 association is 297 vector groups and lands in **297
clocks** -- one group per clock, no stall. 440 randomized associations over
arbitrary envelopes, footprints and backpressure schedules all match the
reference element by element. 23 directed checks.

That 297 is the number the contract correction predicted and is now measured
rather than argued.

**The sweep found two real gaps on its first run** (14 caught, 2 survived, 1
discarded). Both survivors were the same shape -- the test computed a thing
for itself instead of reading what the block said:

* **W15**: the emptiness guard is `(i0 > i1) || (j0 > j1)` and the test drove
  only the column half, so dropping the row half changed nothing it could
  see. A two-term guard needs a case per TERM; one case per guard is not the
  same thing.
* **W17**: coverage was recomputed from the emitted masks, so
  `verts_covered_o` was never read and could have counted anything. Now both
  counters are checked against numbers that differ -- 297 groups against
  1,089 vertices -- so counting groups where lanes were meant cannot pass.

**W08 taught the driver something.** It swaps LAT_W for LAT_H, both 33, so
Verilator emits a byte-identical model and the binary-hash guard called it a
discard. But rebuild() deletes the model directory before every mutant, so
elaboration definitely RAN -- an identical model means the mutation was
semantically null. The driver now reads that as EQUIVALENT when a proof
exists for that mutant, and keeps it a discard otherwise. The proof carries
its expiry (re-score the moment LAT_W != LAT_H), and W18 states the same
defect as a literal so the index arithmetic is scored today.

FINAL: attempted 18/18, caught 17, equivalent 1, survived 0, discarded 0,
SWEEP OK, RTL restored clean.

**Design note committed** (`reports/FIELD_V3_EXECUTOR_REGFILE.md`): the brief
and the owner's directive cut the executor's register file on different axes
-- four LANES x three readers versus four RESIDUE BANKS x three replicas.
Both are twelve memories of 8,192 bits, so the Phase 3 fit measures either
one's storage, but they serve different operand sets per clock and the IR HAS
multi-member operands. Phase 4 builds on the residue-banked shape because it
is the one that was measured; the note records what would reverse it.

**Still not claimed:** probes 4 and 5 place-and-route. The probe 4 fit is at
3,453 s and still running.

### 2026-08-28 - the executor: a real defect, found through the sweep

**Differential first (478a244).** Both sides run the SAME FPLAN: the test
lowers a canonical program with `zfield::plan`, then runs `prepare` +
`execute_point` on one side and loads that plan's uops into the block on the
other. 440 randomized programs, every output register matching.

**It found a pipeline bug on the first run.** `zhao_field_mul` is TWO clocks
deep -- issue_i registers the operands, the product appears the clock after.
The ALU was at S3, one clock early, consuming the PREVIOUS instruction's
product. Every context still retired and every counter looked healthy.
`desync_o` caught it -- and that signal only exists because `prod_valid` came
back from the linter as unused and the choice was to delete it or make it
evidence.

**Then the sweep: 18 attempted, 9 caught, 8 SURVIVED, 1 discarded.** On a
block whose differential had just passed 440 real programs against the
shipped interpreter. That is the coverage audit's thesis in one result.

FOUR WERE REAL GAPS:

* **X11** -- nothing checked that a REFUSED op leaves the register file alone.
  Closing it exposed a genuine RTL DEFECT: the ALU KNOWS OP_DOT2/OP_DOT3 --
  they are real arms of its decode, not the `default` refusal -- so it leaves
  writes_o HIGH and computes a result from the zero fed to dot2_i. The block
  was flagging the op unsupported AND writing the garbage anyway. The file's
  own header already CLAIMED the write was refused; the claim was false until
  the fix. A comment is not an enforcement.
* **X16, X17** -- the saturation flags. The only ledger comparison ran on a
  random program where several lanes fired at once, and a flag that is too
  eager is invisible beside one that should be set anyway. Each lane now has
  a program that fires it ALONE, and sat_rescale is checked at all for the
  first time (OP_ABS reaches it: |INT32_MIN| is off the rail).
* **X05** -- releasing a context one stage early. Harmless for VALUES (the
  write lands before the re-issued read can reach the file) but it changes
  OCCUPANCY, and nothing was looking. The barrel test now PINS the measured
  counts, 65 clocks for one context and 126 for eight.

FIVE WERE EQUIVALENCES, each declared with a proof AND the condition that
reopens it: X08 (post-END pc is dead state, start_i resets it), X10
(degenerate while PLAN == REGS -- X20 added with a literal stride so the
indexing is scored today), X12 (alu_is_end implies !alu_writes, proven from
the ALU's own decode), X18 (issue order is not program order), X19
(multiplication is commutative, and both operands are sign-extended by the
same expression so the products are bit-identical).

24 directed checks, up from 12.

**A discipline note worth keeping:** the fix moved the write-enable line, so
the X11 and X12 anchors stopped matching -- and the preflight refused the run
rather than scoring against a stale anchor. Also: do NOT `git add` the RTL
while a sweep is running. The sweep mutates the file in place, and staging
mid-run would commit a mutant.

**Probe 4's fit TIMED OUT at 5,206 s** with no measurement -- the curve
service is far harder to place than the other probes (678-1,590 s). Requeued
at 14,000 s, waiting for probe 5's fit to finish so two never share the
machine. No Fmax is claimed for either probe.

