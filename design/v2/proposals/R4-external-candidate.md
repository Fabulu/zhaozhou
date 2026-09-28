# Zhaozhou V2 — R4 candidate experiment: a complete Field replacement slice

**Date:** 28 September 2026.  
**Status:** a candidate brief for independent criticism and bounded experiments; not a ratified whole-console architecture.  
**Review baseline:** `design/zhaozhou-v2-rfc` at `9ece4a0a978f0b3762545d0162473a4756676d7f`.  
**Companion review:** `R3_EXTERNAL_REVIEW.md`.

## 0. Proposed decision

Accept R3's Field-first recommendation. Stop using either a global register census or an arbitrary clock threshold as the deciding instrument. Compare a complete, representative Field slice in its current form and a candidate with explicit memory organization and timing-first local scheduling.

V1 remains the executable oracle. Preserve its numerical laws, accepted workload/profile commitments, public state and capture meanings. Changes to accidental pipeline organization are permitted subject to an explicit observable-timing audit. Known V1 bugs are adjudicated, reproduced and fixed through the discrepancy ledger; not enshrined as desired behavior.

This experiment does not authorize reducing fields, dropping outputs, approximating math, narrowing arbitrary legal values, switching to friendlier content or removing long-operation services to get a fit.

## 1. The question this experiment answers

Can a more compact organization of programs, prepared values, context state, operands, long-operation ownership and retirement deliver the **same accepted Field work** at lower net physical cost, with timing-clean operation and the required frame reserve?

It is not: “can one standalone memory infer?”
It is not: “can an almost-empty shell run at 51 MHz?”
It is not: “can three spells be hardwired into fewer gates?”
It is not: “does the last frame look plausible?”

## 2. First deliverable: a reproducible baseline boundary

Create a pinned, scoped harness around the existing production selection. Identify exactly which modules and configurations implement:

```
validated prepared request / field association
    -> canonical input generation and input binding
    -> prepared-program/context admission
    -> operand acquisition
    -> short operations and long-operation services
    -> result capture, sticky status and output presence
    -> command-ordered patch reduction
    -> publication to an observable real consumer boundary
```

Record the current LANES, CTX, PLAN, REGS, queue depths, service units, clock constraints, memory model and selected adapters from the composed instantiation—not their file defaults. If the old snapshot and a modern approved production selection differ, keep both identities explicit and do not mix their costs.

Reuse existing generated command/Field types and capture formats. A narrow harness may cut outside the Field slice, but must not replace the expensive inside of the slice with testbench calculation. Inputs and consumer stalls are harness responsibilities; the computed result, retirement and publication are production-candidate responsibilities.

For a first sub-experiment that stops before publication, label the missing costs and do not claim an Earth-slice throughput pass. Return to the full boundary before the principal conclusion.

## 3. Candidate organization to test

### 3.1 Immutable program storage, mutable contexts

Keep a banked program store separate from runnable-context state. Contexts reference a validated immutable program identity and carry a PC; they do not require independent copies of identical instructions merely because several point groups execute that program.

Identity includes canonical program hash, plan ABI, target execution flavor, input/output binding identity and generation as applicable. Prepared uniform values belong to the field instance/version/tick and may differ for two instances using the same instructions. Do not key uniforms solely by program hash.

Support the original accepted number of distinct simultaneously live programs. Sharing equal programs cannot become an undeclared reduction of worst-case distinct-plan capacity. Measure both the common-shared and maximum-distinct cases.

Inspect the actual mapping of `uop_t store[...]` before proposing a RAM conversion saving. Use a typed/elaborated inventory and RAM object reports; the scanner's declaration regex is not sufficient.

### 3.2 Context state partition by access and lifetime

Inventory each object as immutable descriptor, mutable context, operand state, result payload, queue metadata or active pipeline state. For every object record width, depth, reader/writer identities, simultaneous ports, reset/read-during-write semantics, update rate and lifetime.

Use registered memory for bulk state where the access pattern allows it. Small ready/valid bitmaps, active operands and short pipeline tokens can correctly remain in registers. Do not impose “no arrays in flops” as a blanket syntactic rule.

Coalesce tables only when their combined port schedule is proven. Some independent single-port memories may qualify for M10K packed mode; other tables may be merged manually into a banked state space. Neither is a free bandwidth multiplier. Preserve validity/generation and initialization semantics, including defined initial zeros.

### 3.3 Local, staged issue rather than a global combinational decision

Candidate stage structure:

```
accept work / reserve destination credits
    -> registered context selection and program address
    -> instruction and context-memory response
    -> registered operand-bank addresses
    -> operand response and selection
    -> registered short/long-operation issue
    -> registered result/status
    -> writeback, export observation and retirement
```

This is an initial shape, not a fixed latency decree. Optimize the number of stages against actual RAM/DSP timing and context count. Account for every new register and buffered in-flight operation. A correct pipeline that cannot be kept fed is not successful.

The current Field operand semantics can require more than three scalar inputs. Preserve the actual bank/replica bandwidth or demonstrate the complete alternative schedule. Do not quietly turn seven operand reads into seven serialized cycles while reporting an unchanged arithmetic issue interval.

Avoid a combinational ready chain crossing the entire slice. Reserve completion capacity before accepting non-stallable work. Backpressure must not lose an already launched result. Long-operation replies carry the context ID, generation, destination and status they belong to; reuse waits for actual last retirement.

### 3.4 Preserve useful parallelism, remove unnecessary repetition

Keep the baseline's useful vector width for the initial comparison unless measurement establishes a better configuration. Do not change lane count and memory layout simultaneously in the first causal experiment.

Prepared uniform work executes at its correct instance/tick frequency, not once per lattice vertex. Cache reuse includes every dependency that can change the result or sticky status. Animated fields cannot remain cached only because no stamp occurred.

The lattice generator and direct export path should avoid generic host write/read transport. Preserve output presence independently of output value; a present zero is a real write. Match complete output values, status, accepted sequence and field command-order reduction.

Keep all required long ops and exact numerical behavior. Do not introduce a private opcode encoding that is only translated in the testbench. Generated lowering and validation remain part of the semantic chain.

### 3.5 Result ownership and publication

One owner commits each register/result event. A result visible on an observation port is not proof it reached the actual register or published lattice. Compare the destination state as well as the streams.

Admission, program upload, execution, reduction, publication and slot reuse must form a coherent generation lifecycle. Check that a new instance occupying an old slot cannot inherit a valid bit, old output, old material tag or stale prepared value. Keep the adversarial lessons from V1's missing constants and metadata swap.

For the first slice, cover relevant Field malformed-input and identity cases directly. The unrelated PARAMWALK consumer defect remains in the whole-machine migration corpus; this packet need not implement the geometry path merely to reproduce that independent test.

## 4. Workload and equivalence matrix

Reconcile current authority/supersession of the existing Earth stress contract, then retain its 128 associations, full lattice shape, program mix and named external conditions. The historically specified update traversal uses 297 four-wide groups per full patch, not the 273 flat INIT/DRAIN groups. Do not substitute the latter in a throughput denominator.

The initial corpus should include:

- The actual canonical impact, wave and crater programs through the existing preparation path.
- A material-writing program and controls demonstrating present-zero, absent output and uncovered/no-field behavior.
- Mixed programs, maximum distinct admitted plans, changes of uniforms with the same program hash, and prepared-constant reachability.
- Random legal programs under the declared profile; exact operation-edge, saturation and zero-divisor/status cases.
- Full patches, coverage masks and row tails; worst command-order overlap; bounded maximum legal downstream stalls, table misses and upload contention.
- Context cancellation/restart/version changes permitted by the protocol, stale-reply rejection and output FIFO pressure.

The V1/reference comparison is by semantic transaction and state, not by arbitrary cycle alignment. Preserve public latency/deadline commitments explicitly. Reject unknown differences rather than re-goldening them.

Instrument the monitor itself: deliberately corrupt one result, drop one present-output flag, stale one generation, suppress one committed write or omit one expensive operation; each must be detected. Never count only issued work when completed work is the requirement.

## 5. Performance acceptance is two inequalities plus integration

For provisional 60 Hz and 20% reserve, using measured complete-slice cycles `C` and a timing-clean frequency `f`:

```
C <= 850000                         # existing Earth-slice cap, if still governing
60*C/f <= 0.80                     # frame-time / reserve requirement
```

`f >= 51 MHz` alone is not the gate. At `C = 850000`, reserve requires `f >= 63.75 MHz`. A lower-clock implementation with sufficiently fewer cycles may satisfy both. These facts do not promise a low-clock candidate will succeed.

The inequalities assume one declared synchronous timebase. For mixed domains or asynchronous memory, measure real elapsed time and the complete critical dependency path, with CDC, arbitration and memory-service constraints. Concurrent separate engines do not simply sum their cycles; shared resources and dependencies do. Whole-console acceptance adds the reconciled combined workload, not just this isolated result.

An unbounded external refusal cannot have a finite completion guarantee. Pin the same bounded environmental assumptions in baseline and candidate, and count the resulting stalls.

## 6. Area, ports and timing receipts

For each configuration retain:

| Field | Required evidence |
|---|---|
| Provenance | source SHA/digest, full dependency closure, top, parameters, tool/version, device, constraints, seed, environment |
| Mapping | estimated ALMs where supplied, combinational ALUTs, registers, DSP modes/count, actual physical memory blocks and per-object mapping |
| Placement | fitted ALMs, RAM/DSP usage, setup/hold and applicable recovery/removal/pulse-width reports, unconstrained-path check |
| Work | exact program/profile/capture hashes, associations, active lanes, useful ops, actual writes, emitted outputs, complete cycles |
| Contention | bank conflicts, read/write stalls, long-service waits, queue maxima, publication/bridge traffic |
| Equivalence | semantic checks and fired negative controls; unexplained mismatches are failures |
| Integration | V1 instances removed/replaced; shared modules retained; adapters and new queues included |

Map-only sweeps are useful for rejecting poor shapes. They do not establish timing. Fit discriminating candidate points rather than every edit. A physical board is required for actual SDRAM/HPS and I/O measurement, but not for generating a correctly constrained compile/placement experiment. Label these stages separately.

Do not subtract overlapping subtree totals or treat ALUT savings as a fixed number of ALMs. Show the integrated delta directly when available; otherwise label the estimate.

## 7. Decision after the first experiment

Return a baseline-versus-candidate Pareto table. Do not multiply one cluster's reduction ratio by the entire console.

- Equivalent, cheaper, and deadline-capable: retain the mechanism and propose the next replacement boundary.
- Equivalent and cheaper but slower: identify the actual binding service; test a targeted lane/port/pipeline change under the area budget.
- Equivalent and faster but too large: show where memory/control/arithmetic dominates and test a specific trade.
- No improvement: reject this organization or choose a different first slice; preserve the measured negative result.
- Incomplete or unmeasured: say which receipt is absent. Do not emit a pass or a new probability of success.

A single experiment cannot prove all full-capability architectures impossible. Conversely, repeated expensive experiments without a justified next discriminator are not progress; every follow-up needs an identified causal change and an expected observable outcome.

## 8. Parallel work: the owner's genuine joint-envelope question

Extract prior numerical commitments and current accepted content traces. Prepare a small number of combined profiles and their visible consequences, preserving existing guarantees. Identify whether Earth stress, maximal geometry, giant coverage, texture workloads and Duo were explicitly promised simultaneously or not.

Bring the owner a proposed product envelope with the true unresolved choices, not a blank request for an expert workload vector. Do not reduce it unilaterally. This work proceeds alongside the slice experiment; it blocks whole-console certification, not all useful measurement.

## 9. Deliverable and iteration

The next response should chiefly contain experiment receipts or a precise, reproducible blocker—not only another architectural essay. If part of R4 is rejected, show why and replace it with an equally discriminating test.

Save R4 and the review without editing the originals. Record the agent's own response and chosen experiment scope, execute bounded authorized work on isolated branches/worktrees, preserve V1 and unrelated working files, then commit and non-force push. Return exact paths, branch, SHA and remote verification. Do not report intended tests as executed or allow a report's date to stand in for its measurement's provenance.
