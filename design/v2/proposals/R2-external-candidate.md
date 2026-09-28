# Zhaozhou V2 — candidate R2: timing-first local engines, oracle-led migration

**Revision:** R2, 28 September 2026.  
**Status:** an architectural candidate for criticism. Not the plan, not ratified, not a feature-cut authorization, and not a claim that a bitstream fits.  
**Review target:** R1 at `design/zhaozhou-v2-rfc`, `0e749b8fbe090abdf644268555ea5c757869b878`.  
**Purpose:** turn the agreed design direction into a discriminating programme that tests area, timing and useful work together.  
**Companion:** `R1_REVIEW_AND_R2_RESPONSE.md` gives the evidence corrections and source index.

## 0. What changes from R0 and R1

Keep R0's preservation of the semantic machine and local execution clusters. Keep R1's refusal to treat a chosen resource table as feasibility, its urgency about timing and its focus on the whole workload.

Replace three things:

1. No guessed whole-console ALM forecast, whether constructed top-down from 35k or subtracted from V1 using a universal ALM/register coefficient.
2. No resource-primitive heuristic promoted into a proof of low-rate control, bankable storage, or implementation effort.
3. No global stop on bounded experiments while a genuinely incomplete joint product envelope is being reconciled. Completion cannot be certified without the envelope; useful experiments can proceed on known subprofiles.

The aim remains **the full commissioned capability on the existing intended FPGA**, with V1 involved at every step. An explicitly reduced product is a separate owner decision, not an automatic fallback concealed in a smaller parameter value.

## 1. Preserve the investment and isolate the experiment

V1 remains a reproducible, immutable-by-default executable reference. Pin the C++ semantics, current RTL integration, compiled programme/asset provenance, captures, tests and tool versions. Preserve branch history and unique working files. Oracle fixes are separate, narrow commits with before/after reproducers and explicit semantic authority.

Keep the whole-console V2 namespace distinct from historical Field v2/v3 implementations. `zz2_*` remains a reasonable candidate. The source manifest must ensure the old large implementation is not accidentally included in the V2 shipping build.

Three complementary references participate:

- **Canonical semantics:** exact integer functions and state transitions.
- **Current V1 integration:** transactions, lifetimes, error propagation, ordering and adversarial failures.
- **Content/capture corpus:** the actual capabilities in scenes and long replays.

At a replacement seam, compare accepted semantic requests, results, state publication and externally observable ordering—not arbitrary old pipeline cycle numbers. Never mask a lifetime or error-handling defect merely because final pixels happen to agree.

The existing malformed-index, illegal-PARAMWALK, missing-prepared-constant and mismatched-geometry/material cases seed the test corpus. Adjudicate known V1 bugs rather than requiring V2 to copy them. An oracle-discrepancy ledger is an early deliverable, not a deferred afterthought.

## 2. Working platform and resource policy

Use the existing target `5CSEBA6U23I7` and the intended MiSTer/SuperStation One configuration. Working resource ceilings are 41,910 ALMs, 112 variable-precision DSP blocks and 553 M10Ks. Verify the exact part/package and the tool's available resources in the platform receipt. Use the established single 128 MB FPGA-local SDRAM configuration; extra physical memory is not an assumed baseline rescue. HPS memory remains a distinct resource with ownership and measured bridge traffic.

R0's 35,000 ALM / 96 DSP / 455 M10K row may remain an **aspirational design target for reserve**, never a forecast. Preserve the existing release reserve requirement (the earlier 10% ALM reserve corresponds to 37,719) unless explicitly superseded. Physical fitting, other resource ceilings and corner timing remain independent requirements; passing the ALM line is insufficient.

Maintain two separate accounts:

### 2.1 Forecast from candidate implementations

For each coherent service or cluster, record:

```
replacement_id
old_semantic_capabilities_and_selected_descendants
new_source_closure_and_parameters
oracle_reference_and_test_corpus
area: actual map/fit, or explicit UNMEASURED interval
DSP mapping by operand mode
physical RAM blocks by bank/mode/port layout
registers split into useful pipeline/active state/bulk state/diagnostics
actual service rate, clock and worst request latency
on-chip port demand and external-memory traffic
boundary and scheduler overhead
confidence, provenance, uncovered obligations
```

Map-only results are early signals, not placed ALM guarantees. Never subtract changes twice when they remove overlapping V1 logic. For connected designs, record incremental integrated changes as well as stand-alone rows. Leave the remaining footprint explicitly unknown until measured; do not assign it zero.

### 2.2 Constraint allocation

The platform and each accepted implementation consume the physical envelope. A provisional allowance is a rejection/design-pressure criterion, not evidence that an unbuilt engine can achieve it. Reallocate between clusters only against a measured combined trade. Keep routing/timing reserve and whole-device resource availability visible.

The original 222,666 estimated ALM figure remains evidence of V1's implementation. It is not the sum of physically independent, freely subtractable costs in a different architecture.

## 3. Gate 0: recover and reconcile the workload contract

Gate 0 starts immediately and is not a blank product questionnaire.

### 3.1 Distinguish four dimensions

1. **Semantic domain:** which inputs/programmes are valid and which exact answers/faults they require.
2. **Resident/live capacity:** what can exist in world/programme storage.
3. **Active work in a deadline:** which values must be updated, transformed, sampled or published in this frame.
4. **Real-time-certified programme/profile:** which workload receives a frame-rate guarantee rather than only semantic support.

Do not reduce a resident capacity to the number of active physical contexts. Do not label a previously certified real-time input cold merely to make V2 pass. Do not multiply every residency maximum into an active-frame demand without its applicable guarantee.

### 3.2 Start from commitments already in the tree

At the reviewed SHA, the amended Earth contract states the 128-association stress frame, 1,089 samples per full patch, row-tail treatment and the complete Earth-slice deadline. The geometry contract supplies the 120k skinned-vertex tier. The terrain/video/workload contracts supply other capacities, native modes and known texture profiles. Some documents include historical sections and superseded comments, so trace authority and dates rather than blindly copying numbers.

Create one `workload_envelope` source with entries for:

```
profile_id
status: current guarantee / derived / observed / synthetic / unresolved
source authority and supersession
frame period and public deadline
active views, formats and dimensions
resident objects vs dirty/worked objects
programme identities and instruction/service mixes
per-resource counts and time windows
memory locality assumptions and cold cases
allowed concurrency and unknown combinations
terminal behavior on invalid or over-envelope input
```

The remaining owner question must name an actual ambiguity, such as whether a specific certified Earth stress must coincide with a specific maximum creature/giant/Duo profile, and show the current sources and proposed interpretation. Do not ask again whether the intended FPGA is the target or whether a complete console is required.

### 3.3 Preserve demand as a trace and a vector

Capture counts with identities, dependencies, arrival bursts and deadlines. Aggregate totals alone cannot expose head-of-line blocking, service serialization or bursty buffer peaks.

Record p50/p99/observed maximum to characterize content; derive contractual worst-case bounds separately. An observed maximum is not a maximum possible input. Include test-created legal boundary traces. Prove counters notice injected extra work, cache misses and overload rather than recording only completed work after something was dropped.

Capacity analysis uses per-resource service vectors. For an independent engine a first bound is `max(demand_resource / sustained_rate_resource)`, but dependency and contention simulation must account for the rest. Shared clients must sum demand on the shared resource. Global frame latency is not obtained by either summing every independent engine's duration or taking a maximum while ignoring data dependencies.

## 4. Timing-first implementation discipline

### 4.1 Clock target and evidence

Treat 100 MHz as an initial engineering objective, not a granted clock. Keep 80/60 MHz sensitivity as analysis, not automatically accepted product modes. Recompute service rates and latency for the actual placed configuration. Higher clocking is not a paper rescue; lower clocking is not an excuse to silently miss an existing deadline.

Keep the display period, engine clock, SDRAM clock, control clock/enable and physical CDC mapping explicit. Resolve the historical 251,520-video-cycle and ~1,666,666-compute-cycle quantities by their domains. No schedule uses the more convenient unit without a mapping.

### 4.2 Pipeline contract for local services

A representative useful path is:

```
accept compact work reference / reserve output credit
  -> registered context address
  -> RAM response / stage operand selection
  -> registered issue and operand capture
  -> pipelined arithmetic or bounded iterative operation
  -> registered result and numeric status
  -> local commit / response queue
```

Do not combine RAM read, large variable mux, multiply, normalization, wide saturation and unrelated ready fanout onto one cycle. Split wide exact arithmetic at legitimate intermediate stages. Rounding is performed only at the specified semantic point; a pipeline register is not permission for an extra rescale.

Keep ready/credit chains local. Break long combinational backpressure paths with correctly accounted registered credits or elastic buffers, preserving stalled payloads and in-flight capacity. Avoid a global grant network coupling all engines. Explicitly budget the extra in-flight entries needed when ready becomes registered.

Small active accumulator arrays and skid buffers are allowed in registers. Ban **unjustified bulk flop storage**, not the syntax of an array.

### 4.3 Control scheduling must be cheap and bounded

A microsequencer is justified when the same local datapath/state operations serve several genuinely low-demand transitions. Use fixed, small operation formats and local client sets. Read the control store synchronously. Do not introduce a general instruction decoder, huge associative context table and wide operand crossbar merely to replace small FSMs.

Measure instructions/event, events/frame, burst concurrency, response latency, state-bank conflicts and faults. A low mean event rate does not prove that a frame-seal or refill deadline is met. Bulk packet bytes should still stream through inexpensive dedicated logic; do not microcode every byte transfer by default.

### 4.4 Timing experiments need meaningful loading

An empty platform holding a clock says little about the new geometry/Field engine. A witness must include its intended state store, operand delivery, issue logic, arithmetic latency, retirement, counters and worst legal stalls.

Fit on the target with the intended I/O and clocks; inspect internal and boundary paths separately. Include realistic traffic sinks and consumers so synthesis cannot delete the very datapath being measured. Parameterize activity for different corner cases, not fake computation with an unconnected output.

Archive source/configuration/tool hashes, SDC/QSF, clock derivation, setup/hold/recovery/removal/pulse-width checks, CDC analysis, unconstrained-path results and a compact critical-path report. Sample several seeds when the physical margin is small. A statistical Fmax headline alone is not signoff.

Use maps routinely, full fits at topology-changing and named integration gates. The maximum gap between meaningful timing checks is bounded by design risk, not merely by elapsed commits.

## 5. State organization: local backing, active operands, compact references

### 5.1 Three distinct storage roles

- **Backing state:** large or long-lived semantic objects in FPGA-local SDRAM or appropriately owned HPS memory.
- **Local context/scratch banks:** resident programmes, palettes, material descriptions, tiles, patch scratch, queues and frequently reused descriptors in M10K/appropriate smaller storage.
- **Active datapath state:** a few current operands, accumulators, pipeline identities and elastic buffers in registers.

Do not move every context dereference to external SDRAM. The useful default is to bind once, cache/pin locally, and reuse across many vertices/fragments. An identity token is a reference to an ownership discipline, not necessarily an external-memory fetch.

### 5.2 Every state object has a physical layout

For each candidate object, specify element width/depth, read addresses needed each cycle, write pattern, RAM mode, independent bank count, replication, arbitration, read-during-write behavior, reset and validity, version ownership and maximum lifetime. Derive actual blocks using legal modes, not bits divided by 10,240 alone.

Reset validity/epoch metadata rather than all payload bits only when the uninitialized payload cannot leak. Preserve documented zero-initialized behavior using valid/default muxes at an inference-safe point. Check first access, reset mid-transaction, aborted publication and generation wrap.

A full match on a versioned key is required for correctness-critical reuse. Cache hashing may locate an entry but cannot establish its identity by itself. An old entry remains pinned until the last dependent transaction retires.

## 6. Candidate engine boundaries

These boundaries are hypotheses to measure. R1 and R0 agree on several; R2 makes their timing and traffic obligations explicit.

### 6.1 Command and ownership service

Own validation at trust boundaries, immutable descriptor creation, frame/version commits, bulk DMA submission and fault/terminal bookkeeping. Keep thin hardware ingress/guard logic and use the HPS for appropriate preparation/policy. Do not duplicate complete decoding and state-binding machinery downstream after a validated representation exists; retain checks needed for transport corruption, version changes and safety.

Use one semantic mutation authority for each object with explicit replication for read bandwidth. A command completing does not imply all its produced state is dead.

### 6.2 Geometry engine

Use a small locally scheduled arithmetic cluster fed from resident palettes and draw/view contexts. Preserve exact skinning, normals, transform, clipping, lighting and depth rules. Separate instruction/operand supply, product stages, wide accumulation, rounding and retirement.

Investigate a mix of dedicated hot pipelines and shared lower-rate services, rather than either a universal processor or a private implementation per feature. Compare 3/4/6 suitable product-service configurations as experiments, but count DSP mapping, actual issue utilization, RAM port supply and all omitted work. A theoretical product count is only one constraint.

Do exact work reduction before declaring rate-side area sharing exhausted:

- reuse identical pose/world transforms across consumers with complete keys;
- cache projections where all identity/view/lattice inputs match;
- test regular-lattice matrix factorization without intermediate rounding;
- retain full-domain fallback for cases outside a proven special structure;
- prepare uniform coefficients once rather than per vertex.

Do not book a gain already present in V1. Document which part of the current selected path is actually replaced.

### 6.3 Field and terrain execution

Keep FPGA-native high-rate Field/terrain execution as the primary candidate. Existing software preparation, canonical simulation and navigation remain appropriate host work; wholesale movement of Field to the CPU is not an assumed feasibility escape. Any material relocation of ownership must be explicit and measured, not called "preparation" after the fact.

Use the existing canonical programme semantics and generated lowering rules. Keep prepared values associated with their real programme/field/tick identity. Generate profile inputs directly, avoid per-point generic host tuple traffic, and export required outputs without repeatedly rereading an entire register file.

Bank programmes, uniform data and execution contexts according to measured read schedules. Separate useful lane width, resident programme capacity and active context count. Long operations use bounded local services; do not add independent private root/curve/normalize units by default.

Evaluate/reduce in canonical command order and preserve output presence; a present material zero differs from no material write. Use a small patch working accumulator and external derived backing. Both views consume the same composed version; the CPU mirror must agree on exact semantics and tick. Persistent damage, eviction and return must not depend on render history.

Measure the complete Earth slice, including init, row tails, programme changes, prepared transport, services, reduction, publication, misses and other memory clients. A vector-issue benchmark is not that slice.

### 6.4 Raster, attributes and texture

Keep the high-rate pixel path simple. Preserve six attributes without assuming six complete expensive divider/preparation engines are necessary. Prototype exact shorter division, triangle/row reuse and prepared row recurrences with the current X-stepping law. Include many slivers, broad triangles, tie rules, saturation, wrap and the giant tile-reference case.

Tile-local depth/colour/tag state belongs in suitable banks with deliberate read/write scheduling. Couple geometry and material identity until final consumption. Early depth rejection is allowed only where it preserves ordered effects and the semantic fragment path.

Provide a pipelined common texture hit path and separately measured filtering, palette and miss resources. Pin material contexts locally; do not reread large immutable recipes per fragment. Count physical texel traffic and memory misses, not only abstract requests. Preserve eight/three-sample recipes where commissioned and the relevant detail-normal behavior; verify against the current selected contract rather than stale alternatives.

### 6.5 Procedural, particle, post and platform work

Map every remaining capability to an owner before adding it. Reuse geometry/Field resources where the combined time windows fit; dedicate small streaming units where needed. Particle terrain queries use the same canonical terrain version. No display renderer is allowed to create a separate gameplay truth.

Do not assume post, geometry, publication and prior-frame scanout are mutually exclusive. Build a dependency/lifetime schedule that explains exactly which resources can be reused across phases.

Scanout, audio and input cannot wait for a free Field/geometry context. Preserve frame latch/repeat, fault and FIFO semantics. Release diagnostics retain required bounds, identity and terminal checks; optional rich traces are measured separately and must not alter functional results.

## 7. Arithmetic/algorithm example to test, not a booked gain

R0's geometry model counted nine independent matrix products per terrain vertex. The projector's documented law has three used rows, exact wide sums and one final rescale. For regular lattice X/Z, factor each row into a prepared affine plane plus one varying-height product. Step **unrounded** plane sums and retain the original output rounding/clamping.

The supplied `review_checks.py` verifies the mathematical identity for 836,352 row results and contains an early-rounding negative control. This is not an RTL or ZRef equivalence certificate. The agent must check coordinate formation, world/camera origins, legal overflow boundaries, warped geometry, matrix changes and actual existing preparation before implementing it.

This experiment matters because it tests work elimination rather than only serialization. It should be compared against the generic multiply path inside the same service harness, including descriptor setup, operand storage and output rate. If it does not produce a worthwhile net Pareto improvement, discard it without weakening the oracle.

## 8. Discriminating experiments and evidence gates

| Experiment | Deliverable | What fails | What does NOT follow from failure |
|---|---|---|---|
| E0 evidence and contract reconciliation | raw compact attribution, correct units, current guarantees and unresolved combinations | incomplete/misclassified evidence cannot certify anything | that all prototype work must stop |
| E1 state-layout family | exact state semantics, real block/port layout, paired net area and timing | a particular conversion with bad cost or service | that all banked-state architectures fail |
| E2 timing witness in real platform | connected RAM/issue/datapath/retire path, internal/boundary STA | this candidate/constraint set cannot meet its objective | that 18.5 or 60 MHz is an immutable silicon limit |
| E3 validated demand traces | real and adversarial demand with identity/time and counter controls | the instrument misses injected work or contract coverage | that p99 alone defines guaranteed workload |
| E4 coherent cluster replacement | V1 old-set to complete V2 new-set equivalence, area, clock, service and traffic | no useful total trade or missed required profile | permission to silently reduce capability |

E0 and E3 can proceed concurrently with bounded E1/E2 experiments. E4 is the first evidence directly testing the large architectural savings hypothesis. A conversion-rate distribution alone does not test removal of duplicated ownership/transport/control.

Select one coherent initial cluster after E0 confirms actual costs and access patterns. The candidate is **draw/context binding into a geometry consumer**, because it exercises immutable state, multiple users, a local sequencer, a real datapath and retirement. Compare it with a Field prepared-context slice as a competing experiment. Do not start broad replacement of hundreds of source modules.

## 9. Decision rule: compare Pareto fronts, then integrate

For each materially different candidate record:

`(ALM, registers, DSP modes, physical RAM blocks, clock/slack, II, worst latency, external bytes, burst stalls, host work, correctness coverage)`.

Reject variants dominated on the relevant constrained resources. An attractive ALM/register ratio is not a success if it misses the frame; a high Fmax is not a success if operand throughput halves; a small average byte count is not a success if bursts underflow scanout.

A complete design must satisfy the same configuration's functional, capacity and physical obligations. Do not combine timing from one width, area from another and memory traffic from a warm-cache third.

A universal impossibility conclusion requires a resource lower bound that remains true under alternative legal algorithms and architectures, plus the device's attainable service limits. Ordinarily the appropriate statement is narrower: "candidate C fails workload W under configuration P; revise C or identify an explicit product conflict."

## 10. R3 expected from the repository agent

1. Review the corrections against the actual files and prior measurements, not this document's authority.
2. Repair evidence parsing, rounding, attribution labels and unsupported bound language. Preserve historical measured figures with their limitations; do not count already-spent conversions again.
3. Recover current guarantee sources and propose only the unresolved joint choices. Bring an explicit recommended envelope and alternative with consequences, not "all maxima or not?"
4. Provide a timing-first pipeline proposal with top-path classes to measure, memory port schedules and the first connected witness.
5. Propose the first coherent cluster replacement and one serious alternative. Explain why their experiments discriminate between architectures.
6. Keep a per-proposal accept/modify/reject/unresolved table. Remove blanket flop-array bans, arbitrary universal exchange-rate gates and universal-impossibility claims unsupported by a bound.
7. Save an independent R3, its review of R2, source/evidence outputs and updated decisions/questions. Commit only these intended files on the design branch (or a clearly named review subbranch), push non-force, verify the remote SHA and return it.

No V1 production rewrite, changed public guarantee or physical-target substitution is authorized by the mere existence of R2. Small experiments should be isolated and clearly labeled. The purpose of the next iteration is to make the architecture more falsifiable and more buildable, not merely more persuasive.

## Source and test scope

Use the detailed companion review's S1–S12 source index and `evidence/SOURCES.md`. The relevant sources were read at the pinned R1 SHA. Historical contracts require supersession checks before being used as current product law.

No new Quartus, full RTL/ZRef, ARM or physical-board result is claimed. The local Python checks cover arithmetic, a tool-interface mismatch and synthetic counterexamples only. The full uncommitted 26 MB map was unavailable for independent census reconstruction.
