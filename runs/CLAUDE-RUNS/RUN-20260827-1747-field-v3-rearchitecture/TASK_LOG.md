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

### 2026-08-28 (early hours) - DOT, the register file, the arbiter, and three
### failures in my own process

**FIELD.V3.EXEC closed at 31/31** (27 caught, 4 proven equivalent, 0 survived,
0 discarded) with DOT2/DOT3 sequenced against the one-multiplier-per-lane
budget. Barrel re-pinned: 66 clocks for one context, 166 for eight -- the
+32% is the global issue freeze, which stalls seven contexts for one
context's dot product.

**A real RTL defect, found through the sweep.** The ALU KNOWS OP_DOT2/OP_DOT3
-- they are real arms of its decode, not the `default` refusal -- so it left
writes_o HIGH and computed a result from the zero fed to dot2_i. The block
was flagging the op unsupported AND writing the garbage anyway. The file's
own header already claimed the write was refused. A comment is not an
enforcement.

**The executor was built on a register file that implements no semantics.**
`zhao_probe_banked_rf` exists to PRICE the owner's banked shape and says so
in its header. It addresses every bank with the same row, which cannot read
a group crossing a multiple of four. 440 randomized programs passed because
scalar ops never read a+1. The first DOT2 broke 30 of 400, exactly the group
starts 2 and 3 mod 4. `zhao_field_v3_rf.sv` is the functional version, and
the probe's 372 ALM / 93.14 MHz is now a FLOOR for it, not a measurement.

**The multiplier bank and its arbiter now exist** (`zhao_field_v3_mulbank`).
Reading the service probes first showed the bank is FOUR WIDE and is engine
property -- `zhao_probe_curve_svc` contains no multiplier, it drives one --
so one bank has three claimants and nothing arbitrated them. MEASURED: 18,202
accepted, 18,202 delivered, 18,202 grants over 20,000 clocks; one four-wide
request per clock sustained; priority exactly as declared, with the lane
group taking ZERO while a service asks; 8,884 lane stalls, which is the
measured price of fixed priority rather than a worry about it.

---

**THREE FAILURES IN MY OWN PROCESS, recorded because they cost more than the
bugs did.**

**1. I committed a mutant and pushed it.** `wb_reg_o = s4_dst_r + RW'(1)`
rode into a commit whose message said the block was fixed. Three things
lined up: killed tasks left ORPHANED processes still editing files after the
harness reported them dead; the mutant changed only an OBSERVATION port, so
440 programs passed with it in place; and my pre-commit diff was piped
through `head -8`. A glance is not a review.

`tools/sweep_check_clean.py` now asks whether any mutant's replacement text
is present, from OUTSIDE the sweep -- not a diff against HEAD, because HEAD
itself carried the mutant. Every field driver runs it after the final restore
and aborts with exit 14. It has caught two stranded mutants since.

**2. The sweep driver restored one file out of two.** The table, preflight
and apply path had all learned to span a cone; the driver's gold snapshot had
not. X30/X31/X32 touch different lines, so they ACCUMULATED and every
register-file verdict in that run was contaminated. Both guards fired -- exit
4, "the tree is NOT clean" -- and the driver now snapshots every path the
table can reach.

**3. I declared two equivalences predictively, against my own written rule,
and then trusted a run that had already declared itself unclean.** X18 and
X19 were given proofs before any evidence. A contaminated run reported both
CAUGHT so I retracted them; a clean run then showed X19 SURVIVING, so its
proof was right all along -- the contaminated register file had been
returning wrong operands, which made a commutative swap observable.

X18's retraction stands (the staggered start makes it genuinely observable).
X19's proof is restored with the whole history attached.

The lesson is not about commutativity. **A run that fails its own integrity
check has no verdicts, only noise** -- and the guard said so before I read
per-mutant results out of it anyway.

---

**Process change that actually worked:** the probe fits are now launched
DETACHED via Start-Process, so they are not children of an agent task. Four
fits had been lost -- one to a timeout I set too low, one to genuine
contention, and two to the task being killed mid-flight. The detached runner
has since survived several task kills.

**Outstanding:** the mulbank sweep score; place-and-route for probes 4 and 5;
the composition against the 850,000-cycle gate; the scalar bank.

### 2026-08-28 (morning) - the bank, the composition, and a night spent on tooling

**BUILT AND VERIFIED.** `zhao_field_v3_mulbank` -- the four-wide multiplier
bank and its arbiter, the piece nothing arbitrated. Reading the service probes
first is what found it: `zhao_probe_curve_svc` contains no multiplier, it
DRIVES one, and says so. MEASURED: 18,202 accepted, 18,202 delivered, 18,202
grants over 20,000 clocks of contention; one four-wide request per clock
sustained; priority exactly as declared with the lane group taking ZERO while a
service asks; 8,884 lane stalls, which is the measured price of fixed priority.

**`zhao_probe_v3_engine`** composes the executor with that bank. The executor's
private multiplier is gone. 26 directed checks and 400 randomized programs
against `execute_point`, and the barrel lands on exactly its pinned 66 and 166
clocks -- the shared bank uncontended behaves identically to the private
multiplier, so the rewiring is behaviour-preserving.

**PRIORITY (a) WAS ALREADY DONE.** REMAINING_BLOCKERS gated FRAMEBLIT step 8 on
a CMD.DMA redesign described as "NOT yet done". It is done -- incremental
payload CRC, slot_ram in M10K shape -- and both blocks now FIT (cmd_dma ok at
3,607 ALM, frameblit ok at 962). Checking before starting is the only reason
that was not redone. A stale blocker is worse than no blocker.

---

**THE NIGHT'S REAL COST WAS TOOLING, and it is worth an honest tally.**

One multiplier-bank sweep took SIX attempts. Not one failed for the reason it
appeared to, and every guard in the driver reported correctly throughout:

| attempt | apparent cause | actual cause |
| --- | --- | --- |
| 1-2 | contention | ccache: USERPROFILE unset |
| 3 | my concurrent build | same |
| 4 | PATH wrong | same (PATH WAS wrong, separately) |
| 5 | SIGPIPE from `\| head -25` | mine, and it stranded a mutant |
| 6 | -- | fixed by -DOBJCACHE_ENABLED=OFF |

What made the difference was instrumentation, not insight. The answer appeared
in ONE run once the rebuild logged CMAKE_EXIT, NINJA_EXIT and its environment.
Before that I had three wrong diagnoses and had read a log from a `/tmp` that
Git bash and msys bash resolve differently.

**Four mistakes of my own, recorded because they repeat:**

1. I read per-mutant verdicts out of a run that had ABORTED declaring itself
   unclean, and retracted a CORRECT equivalence proof on that basis.
2. I declared two equivalences PREDICTIVELY, against the rule written in the
   file I was editing.
3. I piped a sweep to `head`, which SIGPIPEd it mid-mutation.
4. One fix silently deleted another -- the regex that added CCACHE_DIR removed
   the USERPROFILE export, so the two candidates were never both present and
   each looked independently failed.

**And one rule I wrote from a disproved hypothesis** ("no other build
anywhere") and have now corrected in docs/BUILD.md. Separate build trees are
fine; two writers in the SAME tree are not. A rule inferred from a failure
later explained by something else is a superstition with a changelog entry.

**Permanent guards added:** `sweep_check_clean.py` (five strandings caught),
multi-file snapshot/restore in the drivers, rebuild exit-code logging,
`run_sweep_detached.sh`, and the whole sequence written into docs/BUILD.md in
the order the next person will hit it.


---

## 2026-08-28 06:41 -- THE DOT SEQUENCE MOVES TO S4, AND THE OPEN LOOP CLOSES

Five undirected attempts failed (2 -> 5 -> 4 -> 1 -> 9 wrong of 12, table in
reports/REMAINING_BLOCKERS.md). A cycle-by-cycle trace found the contradiction
every one of them was standing on:

> **An instruction cannot be stalled between its multiply ISSUE and its product
> ARRIVAL.**

The product lands two clocks later on a fixed schedule. Advance without it and
the instruction consumes a product that was never issued; hold for it and it
misses one that arrives anyway. The old schedule spread ONE DOT sequence across
S2, S3 and S4 -- three MOVING stages -- so a refusal at any of them hit one horn
or the other. That is why patching oscillated instead of converging: each fix
traded one horn for the other.

**The fix is structural, not another gate.** The whole sequence now issues from
S4, where the operands sit in registers that do not move for its duration. Each
product is issued, retried on refusal, and accumulated when it lands. Nothing
can miss anything, by construction.

Three corrections fell out of the same insight:

* **ISSUE and ARRIVAL are different events and are counted separately**
  (`dot_issue_r` vs `dot_cnt_r`). Conflating them had made the third product
  never get issued at all -- the first arrival advanced the counter, and the
  issue condition keyed on that same counter went false.
* **The accumulator holds every product; the ALU reads the finished total.**
  The old form consumed the last product combinationally on the release clock,
  which is correct only if it arrives exactly then.
* **`desync_o` now checks THE MULTIPLIER'S CONTRACT** -- a product arrives
  exactly two clocks after a granted issue -- instead of stage occupancy. The
  old form assumed one product per instruction and began firing on entirely
  correct behaviour once a DOT had three. A guard that cries wolf is worse than
  no guard, because it gets read as noise.

### MEASURED

    31 directed checks + 400 randomized programs        green
    12 programs under contention, 16 lane stalls        answers unchanged
    the DOT skip                                        REMOVED

    one context     66 -> 69 clocks    (+4.5%)
    eight contexts 166 -> 190 clocks   (+14%)

The old schedule was cheaper because it overlapped a DOT's products with the
instruction's own progress. It was also unfixable. That is what correctness
under contention costs, and the barrel counts are re-pinned rather than hidden.

### THE COMMIT THIS LANDED IN IS NOT THE ONE THAT NAMES IT

A concurrent creature session ran `git commit -a` while this fix was staged and
swallowed `zhao_probe_v3_exec.sv` and `field_v3_exec_directed.cpp` into
**357a702 "run 0326: death organic, flailing alive"**. The content is correct
and committed; only the attribution is wrong, and rewriting another live
session's commit would cost more than it fixes. This entry is the record.

**The rule that follows: `git commit -a` is banned in this tree.** Three
sessions share it. Stage explicitly, by path, always.

### STILL OPEN, and unchanged by this

The curve and distance services have the IDENTICAL defect -- neither has a
`mul_ready` input, so a refused service advances as though its multiply
happened. The same structural answer applies to both.

---

## 2026-08-28 -- STEP 1 FOR ALL NINE REMAINING OPS: the oracle resolves

Checked before writing any RTL, which is the rule. All nine remaining Field IR
ops are fully defined on the reference side, in all three places that matter:

| | where |
| --- | --- |
| shape (dst width, src count, imm use, class) | `reference/include/zfield/generated/zfield_optable.hpp` |
| decode acceptance | `reference/src/zfield/zfield_decode.cpp` |
| semantics | `reference/include/zfield/zfield_steps.hpp` |

So there is nothing to design on the reference side and no oracle to write --
the differential for each op is `zfield::execute_point` against the RTL, the
same shape the executor already uses.

`zfield_plan.cpp` special-cases only RING (UOP_RING_PREP, for the nine
separately-rounded products when its radii are invariant). The rest plan as
ordinary uops.

The costing that came out of this reading is in
`reports/FIELD_V3_REMAINING_OPS.md`, together with the finding that matters
more than any of the counts: **five more v2 units drive the shared lane with no
`mul_ready`**, and they are correct today only because `zhao_field_seq` keeps
one instruction in flight -- a premise the v3 bank exists to retire.

### A git hazard worth writing down

A push failed on a race with the creature session, and `git pull --rebase`
refused because the tree had unstaged changes. **Those changes were a live
mutant**: the exec sweep was mid-run and had a mutation applied to
`zhao_probe_v3_exec.sv` at that moment.

Rebasing or stashing there would have written the working tree underneath a
running sweep -- reverting the mutant mid-score at best, and at worst leaving
the sweep to restore a snapshot over rebased content. The commit is local and
harmless; the push waits for the sweep.

**The rule: no git operation that WRITES the working tree while a sweep is in
flight.** Commit and fetch are fine. Rebase, stash, checkout and reset are not.

---

## 2026-08-28 -- the curve service's refusal loop is VERIFIED

    5,022 directed checks + 7,200 random checks   green
    24 groups under refusal, 11 real refusals     answers unchanged
    four-point CURVE II                           13 clocks, unmoved
    lone-reply latency                            17 cycles, unmoved

The II did not move because a refusal costs a clock only when one happens, and
the gate measures a stream that nothing is contending for. That is worth
stating rather than celebrating: the gate does not exercise the fix, section 7
does.

### It was built WHILE the executor's sweep was running, on purpose

BUILD.md rule 5 says the real condition is one tree, one writer -- not "no
build anywhere", which was an earlier guess later explained by ccache. The two
targets here are provably disjoint: `test_field_curve_svc_directed` elaborates
`zhao_probe_curve_svc.sv` and nothing else, and the exec sweep mutates
`zhao_probe_v3_exec.sv` and `zhao_field_v3_rf.sv`. Checked with
`sweep_consumers.py` rather than assumed, in its own tree, with the object
cache off to match.

Configuring that tree cost 251 seconds and 136 Verilator invocations, because
a fresh tree elaborates every target in the project.

### THE BACKSLASH TRAP, which has now cost four edits

Every scripted edit in this session goes through a shell heredoc, and this
environment COLLAPSES a doubled backslash to a single one inside one. So a
patch script that means to write the two characters backslash-n writes a
line break instead, and it does it silently. It has produced:

* a Python file with an unterminated string literal,
* three anchor searches that matched zero times and looked like drift,
* and two C++ `printf`s split across lines, which is what failed this build.

The fix that works is to build the escape at run time -- `BS = chr(92)` and a
`@N@` placeholder substituted in -- never to type a backslash inside a
heredoc. Every patch script from here does that.

### The other thing not to do while a sweep runs

Do not edit the sweep's own `.sh` files. Bash reads a script INCREMENTALLY
from a file offset, so editing one under a running shell shifts the offsets and
the shell resumes mid-token. Nothing went wrong -- I nearly corrected a stale
comment in `run_sweep_detached.sh` and stopped. It goes in when the sweep ends.

---

## 2026-08-28 -- FIELD.V3.EXEC sweep GREEN after the DOT rework

    31 mutants   27 caught   4 equivalent (proven)   0 survived   0 discarded
    tree checked clean from outside the sweep: 31 mutant texts, none present

Eight of the thirty-one had DEAD ANCHORS before this run -- they pointed at
lines the DOT fix deleted, so they had silently stopped testing anything. They
were re-aimed at the same claims in the new shape rather than dropped, and all
eight are caught. A claim whose mutant stopped applying looks exactly like a
claim nobody thought of.

The four equivalences are unchanged from before and each carries its own proof
and its own re-score trigger (X10 while PLAN == REGS, X12 while no op sets both
is_end and writes, X18 the ready scan, X19 commutativity).

## The same two guards were wrong in the curve driver, and were fixed before use

`tools/sweep_field_curve_svc.sh` was an older generation: hardcoded build dir,
no rebuild log, no `-DOBJCACHE_ENABLED=OFF`, cmake output to /dev/null, and the
TOP_MODULE roster grep. Left alone it would have failed on ccache in a detached
process -- the exact five-abort sequence already in BUILD.md -- and reported
"pristine target did not link" with the cause three layers away. Modernised
first, then run.

## C16 was refused by the preflight, and that was correct

The first mutant for the new refusal loop deleted the only read of
`mul_ready_i`, orphaning the port so Verilator would not build it. A mutant
that cannot build is a DISCARD, not evidence, and the preflight said so before
anything was scored.

Reshaped to send a refused issue straight to F_PUSH: the port stays live and
the claim is the same one from the other side. The pre-fix RTL hung waiting for
a product nobody started; this publishes the finish registers without one, so
it fails by VALUE rather than by timeout. Both are "a refusal is not noticed".

## NOISE2 and RIDGE: unit written, lints clean, test written, NOT YET WIRED

`fpga/rtl/field/zhao_field_v3_noise.sv` -- the v2 state machine step for step,
datapath widened to four points, and every issue state holding until granted.
Lints clean under `-Wall` first time.

    ONE four-wide bank request = the SAME hash step for all four points

so a four-point NOISE2 is SIX bank requests rather than twenty-four products
that need scheduling, and RIDGE is four.

`tests/differential/field_v3_noise_directed.cpp` is written against
`zfield::steps::exec_op` -- the shipped interpreter, never a hash rewritten in
the test, which would only prove two rewrites agree. Seven sections, and two of
them exist because a vector unit can pass a scalar test suite:

* four DIFFERENT points in one group, paired with four IDENTICAL ones, so
  "broadcasts point 0" and "indexes the wrong lane" separate;
* groups whose RXS shift DIFFERS BETWEEN POINTS, hunted for and asserted to
  have been found -- the shift is `(s>>28)+4`, a function of the data, so it is
  a per-point quantity and a unit computing one shift per request would pass
  everything else here.

Neither is wired into CMake yet: `tests/CMakeLists.txt` must not be edited
while a sweep runs, because the driver re-runs `cmake` on every rebuild and
would configure the edit underneath itself.

---

## 2026-08-28 -- FIELD.CURVE.SVC sweep GREEN, and NOISE2/RIDGE runs

    curve service:  18 mutants  18 caught  0 equivalent  0 survived  0 discarded

That includes C16, C17 and C18, the three that attack the new refusal hold.
C16 had to be reshaped first -- see the previous entry -- and the reshaped
form is caught.

### The noise unit works, and every value check passed on the FIRST run

    1,050 directed checks + 7,200 random checks    green
    24 groups under refusal, 118 refusals issued   answers unchanged
    four-point NOISE2  20 clocks                   six bank requests
    four-point RIDGE   14 clocks                   four bank requests

RIDGE being strictly cheaper is the design showing through rather than a
coincidence: it stops after the first hash lane, so it makes four requests
where NOISE2 makes six.

**The one failure was my own test's search budget, not the RTL.** Section 5
wants a group where ALL EIGHT lattice terms have bit 31 set, to put every lane
on the divergent side of the signed/unsigned question at once. That is a
1-in-256 draw and I gave it 400 attempts, which found two. The attempts are
pure arithmetic and only the eight accepted groups cost DUT time, so the budget
is now 40,000 and the reason is written at the loop.

Worth being explicit about: the check FAILED rather than quietly accepting two
groups, because it asserts the count it needs. A search that silently settles
for what it found is how a section ends up proving less than its comment says.

### N01 was refused by the preflight, and the reshape is better than the original

`s_reg[l] <= s_reg[l]` removed the only READ of `s_mix`, orphaning it. Same
class as C16 an hour earlier, same verdict: a mutant that cannot build is a
discard, not evidence.

The replacement replays the NEIGHBOURING point's mix. It keeps the signal live
and it is sharper than what it replaced -- it is wrong PER POINT, so section
3's four-identical-points groups would pass it and only section 2's
four-different-points groups can see it. That pairing is why both sections
exist.

### Two index errors in the table's own docstring, corrected

It credited "N05, N09 and N11" with the vector-specific claims and "N18 and
N19" with the refusal hold. The real answers are N01/N05/N07/N09 and N20/N21.
Nothing depended on it, but a table whose map of itself is wrong is how the
next person aims a mutant at the wrong claim.

---

## 2026-08-28 -- the noise sweep found THREE things my own comments got wrong

    first run:  21 attempted  18 caught  3 SURVIVED  0 discarded

All three survivors are genuine equivalences, and each one disproves a claim I
had written down as fact. That is the sweep doing the job the whole discipline
exists for, and it is worth recording in that order -- the finding first, the
correction second.

### N07: the operand extension cannot matter, and my header said it did

The RTL claimed a sign-extended operand would be "right for small coordinates
and wrong for large ones". **False.** For any 32-bit a:

    A0 = a                      zero-extended
    A1 = a - 2^32*(a>>31)       sign-extended
    A0*B - A1*B = 2^32*(a>>31)*B

The products differ by an exact multiple of 2^32, so their low 32 bits -- the
only bits this unit reads -- are identical for every input. Section 5 of the
differential was written specifically to catch this and was aimed at a
divergence that does not exist. Both comments are corrected.

The zero-extension STAYS, as defence in depth: it states that these are
unsigned hash words and it is the form that survives anything reading above
bit 31, or a lane narrower than 33 bits where the operand would be truncated
rather than extended.

### N10: the final xor-shift is unobservable to these two ops

The hash ends `(w >> 22) ^ w` and both ops take bits [31:16]. A right shift by
S lets the xor term reach bits 31-S and below, so for any S >= 16 it cannot
touch a bit either op reads.

MEASURED rather than only argued: 200,000 random words show zero differences
between S=22 and S=21 in bits [31:16], and sweeping S from 10 to 19 puts the
boundary at exactly 16 -- S <= 15 differs on most words, S >= 16 on none.

**An equivalence must not become an excuse to leave a step untested**, so N22
is the same claim at S = 15, on the observable side of that boundary. The
shift stays 22 because it is the reference's law and `zref::noise2_hash` is
used elsewhere -- creature gibs, the star bake -- where the low bits ARE read.
The equivalence belongs to these two ops, not to the hash.

### N19: RIDGE cannot saturate at all

Swapping one rail for the other changes nothing because BOTH are unreachable.
u is the top 16 bits of a hash word, so its entire domain is 0..65535.
Checked EXHAUSTIVELY over all 65,536 values:

    ridge_t range          -65536 .. 65534
    ridge_t == INT32_MIN   0 times
    ridge_t == INT32_MAX   0 times
    any add saturation     0
    any sub saturation     0

So `sat_add_o` and `sat_rescale_o` are identically zero for every input this
op can be given, and the reference agrees -- its SatLedger never bumps, which
is why the differential passes with both sides reading zero.

The logic is KEPT, because it mirrors the reference's fold and becomes live
the moment the domain widens, but it is now documented in the RTL as
unreachable so it does not read as tested logic. N23 is the reachable
counterpart, so "the flags are zero" is an assertion with teeth rather than a
region nothing exercises.

### The pattern across all three

Every one of them was a place where I had written a confident sentence about
why something mattered, and the sweep showed the sentence was decoration. None
was a bug in the RTL. All three were bugs in the JUSTIFICATION, which is worse
in one specific way: a wrong comment survives refactors that a wrong line
would not.

---

## 2026-08-28 -- FIELD.V3.NOISE closes, and the dispatcher exists

    noise re-run:  23 attempted  20 caught  3 equivalent (proven)  0 survived  0 discarded

N22 and N23 -- the catchable counterparts written beside the two equivalences
that had no reachable form -- are both CAUGHT. That was the point of adding
them: an equivalence must not become an excuse to leave a step untested, and
now there is evidence rather than an intention.

## The long-op dispatcher: 274 checks, and one real hole closed before any test

`fpga/rtl/field/zhao_field_v3_dispatch.sv` gathers four CONTEXTS into one
four-point service request, holds the slot the reply needs, drains results one
register per clock, and releases each context after its last write.

Three shapes came from the shipped RTL rather than from preference:

* **One write port** in `zhao_field_v3_rf`, so a reply drains over
  `used x dst_width` clocks -- 4 for CURVE, 8 for NOISE2, 12 for ROT3. A
  four-point NOISE2 is 20 clocks in its unit and eight more here. That is the
  first real argument for a second write port and it is NOT taken: the
  measurement that should decide it is the composed engine's occupancy, which
  does not exist yet.
* **`dst_width` from the generated op table**, checked row by row, not a
  special case per opcode. An opcode the block does not know is REFUSED, since
  a wrong width writes the wrong NUMBER of registers -- corruption rather than
  an error.
* **`flush_i` is an input, not a timeout.** A timeout turns a deadlock into a
  slow path that still passes, which hides the condition AND costs clocks. The
  executor knows which contexts are active, so it says so.

### The hole, found by writing the code rather than by running it

With `flush_i` high and the group part-full, a context offered on the same
clock would have been ACCEPTED into `g_*`, landed outside the snapshot (which
takes the pre-accept `fill_r`), and then been cleared by D_ISSUE. Handshaked
and gone -- a LOST instruction, not a slow one.

`long_ready_o` now drops while flushing, so the offer stands and joins the next
group. D14 is that exact defect as a mutant, and section 6 of the differential
asserts the offer survives a whole round trip.

### My test was wrong once, and the RTL was right

Section 6 first expected the standing offer to be taken immediately after the
partial group issued. It FAILED, correctly: one slot is outstanding, so ready
must stay low until the reply has come back and drained. The section now
asserts that explicitly and then completes the round trip, which is worth more
than the check that was wrong.

### THE RULE THREE PREFLIGHT REFUSALS TAUGHT, ALL IN ONE DAY

    C16  dropped the only read of mul_ready_i   -> orphaned port
    N01  replaced s_mix[l] with s_reg[l]        -> orphaned signal
    D09  replaced PAD_A with a literal          -> orphaned parameter

Same cause every time: **deleting a USE orphans a thing, and Verilator refuses
it**, so the mutant cannot build -- and a mutant that cannot build is a
discard, not evidence, costing a whole rebuild to discover.

**Prefer mutating a VALUE over deleting a USE.** All three reshapes are better
mutants than the originals: D09 sets the pad to zero rather than bypassing it,
N01 replays a neighbour's mix (wrong PER POINT, so it needs distinct points to
be seen), and C16 sends a refused issue onward so it fails by value instead of
by timeout. The rule is written into the dispatcher's mutant table where the
next table will be copied from.

---

## 2026-08-28 -- the dispatcher sweep found FOUR gaps, and none of them was an
## equivalence

    first run:  25 attempted  21 caught  4 SURVIVED  0 discarded

Unlike the noise block, where all three survivors were provable equivalences,
every one of these is a real hole in the test. The suite passed 274 checks and
was weaker than that number suggests.

| mutant | why it survived |
| --- | --- |
| D03 `fill_r <= 4` | a FIFTH context is accepted on the clock the full group is still in D_GATHER, and 4[1:0] is 0, so it overwrites LANE 0. Nothing offered a fifth context. |
| D07 drop `fill_r != 0` | an EMPTY group is issued on flush. Nothing asserted flush with an empty gather. |
| D21 release on member 0 | for a width-1 op that IS the last member, so CURVE and RIDGE cannot see it. For widths 2 and 3 the release comes early -- but the release COUNT and ORDER are unchanged, and those were all the test checked. |
| D24 tag never increments | every group carries the same tag. One group per instance is exactly the shape that cannot notice. |

**D21 is the instructive one.** The test recorded every release and checked
how many there were and in what order, and both were right under the mutant.
What it never recorded was WHEN a release happened relative to that context's
own writes. `rel_after` does now: it counts the registers that had landed for
that context at the moment it was released, and the assertion is that the
number equals the op's destination width. Under D21 a width-2 op releases
after one write instead of two.

The other three are plain omissions -- a fifth context, an empty flush, and a
second group -- each of which is one section.

    after:  327 checks, and the four sections name the mutant they exist for

### The heredoc escape trap, now in BUILD.md because it has cost six edits

Writing those sections split four `printf` literals across two lines, which is
the sixth time today. A heredoc in this environment collapses a doubled
backslash to a single one, so a patch meaning to write backslash-n writes a
REAL LINE BREAK, silently. It has produced an unterminated Python string,
three anchor searches that matched zero times and read exactly like stale
anchors, and six broken C++ literals.

The rule is in docs/BUILD.md now: never type a backslash inside a heredoc.
Build it at run time -- `BS = chr(92)` -- and substitute a placeholder.
