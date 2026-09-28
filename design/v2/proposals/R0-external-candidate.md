# Zhaozhou V2 — candidate architecture for review

**Revision:** R0, 28 September 2026  
**Status:** PROPOSAL FOR CRITIQUE. Not ratified; not an implementation order; not a fit certificate.  
**Prepared for:** Fabian Trunz and the Zhaozhou repository agent.  
**Repository snapshot checked:** `Fabulu/zhaozhou`, `claude/ceiling-architecture-20260912`, `814687ae955a3ce4690010ce750f2b7b5e597ec2`.  
**Purpose:** preserve the value and capabilities of V1 while investigating a substantially smaller implementation for the intended MiSTer-class FPGA.  
**Immediate requested action:** review, challenge, improve, save a reasoned R1 in the repository, commit, push, and return its exact branch/commit/paths for another review. Do not treat R0 as the final architecture.

## Read this first

V1 is not disposable scaffolding. It is an executable record of arithmetic, protocols, features, integration failures, test cases, content, and design decisions that would be expensive to discover again. V2 should use that record at every scale: operator, transaction, subsystem, complete frame, long replay, and physical-board validation.

The proposal is **not** to squeeze every existing module into a smaller wrapper. It is to preserve the semantic machine while changing its storage, scheduling, transport, and implementation granularity. Nor is it to turn the whole console into one tiny serialized processor and hope the workload fits.

The preferred candidate is a **small number of locally scheduled engines with banked state**, connected by compact, versioned work references. Geometry and Field retain throughput-oriented arithmetic; the raster keeps its inexpensive per-pixel state and shares expensive preparation; texture has a pipelined hit path; terrain has one authoritative composed state; control and cold preparation use the existing host where justified.

Full capability is the initial target. Neither this document nor an agent may silently remove Gouraud shading, normal detail, fields, giant support, destruction, native modes, or declared performance guarantees. If the full target fails a measured constraint, propose alternatives explicitly. An unknown is not evidence of either feasibility or impossibility.

**Two rules govern the entire programme:**

1. V1 continuously answers **what must happen**. A new schedule and fit continuously answer **whether V2 can afford to do it**.
2. No broad implementation campaign begins merely because an allocation table adds up. The highest-risk service rates, memory paths, and real platform footprint must be tested early.

### R0 in one page

| Decision proposed for review | Rationale | What could overturn it |
|---|---|---|
| Keep the public semantic interfaces and existing content/capture ecosystem | Makes the accumulated work directly reusable | A specific incompatible boundary shown to impose unacceptable cost, with a compatibility alternative |
| Use local geometry, Field, and raster/texture engines rather than a universal processor | Preserve concurrency and predictable data supply without hundreds of independently buffered feature blocks | Measured alternative with better total area/rate/bandwidth |
| Put bulk state in banked memory; keep only active operands in registers | Addresses state and selection-network cost together | A particular port/timing schedule that genuinely requires registers |
| Carry identities, not full immutable descriptions, through most queues | Avoid repeated wide capture and mux networks | Consumer fetch cost exceeds the transport saving for a measured hot path |
| Keep six pixel attributes; share/cache their expensive preparation | Preserve RGB and perspective behavior, reduce redundant setup | Worst-case triangle/tile schedule fails |
| Generate Field coordinates and export results directly | Avoid serial host transport dominating useful computation | Measured adapter cost exceeds a simpler transport design |
| Use HPS for preparation, not assumed-free real-time work | Exploit the existing processor without moving the bottleneck | Actual A9 benchmarks show a better partition |
| Grow a physically fitted V2 from the start | Prevent another late whole-machine size discovery | No exception for release readiness; tiny isolated experiments may precede board access |

---

## 1. Ground truth, uncertainty, and naming

### 1.1 What is established

The retained complete V1 synthesis attribution reports 293,886 combinational ALUTs, 279,210 registers, 3,387,975 block-memory bits and 128 DSP blocks. The separate escalation quotes 222,666 **estimated** ALMs. This is not a placed V1 console or a timing result. The large shell and Field subtrees are particularly important concentrations. These are frozen-source facts, not proof that the functions require those resources. [S1, S2]

The recent completion campaign and its repaired field/material/SDRAM route are valuable integration evidence. A completion register is not an exhaustive behavioral or capacity certificate. In particular, the current tree records malformed-input defects in Field index validation and PARAMWALK rejection. Preserve these as adversarial cases; do not use known wrong behavior to define the new machine. [S8, S9]

The previous area audit identified candidate changes, including exact shorter division and row preparation, wide-table memory layouts, and context-based transport. Its arithmetic-model tests were not RTL or Quartus measurements. They are starting experiments, not savings already earned. [S14]

### 1.2 Do not confuse three meanings of V2

This document's **Zhaozhou V2** is a new whole-console implementation. It does not mean selecting historical modules named `zhao_field_v2_*`, nor reverting the current prepared Field engine to an older fallback. Existing `_v2` and `_v3` suffixes belong to their own subsystem histories. [S6]

Use a distinct implementation namespace, provisionally `zz2_*`, and a separate source manifest. Do not silently replace the meaning of existing filenames. The exact directory structure is for repository review.

### 1.3 The agent must refresh the baseline

R0 was grounded at the SHA above. Before responding, read current heads and the repository's applicable instructions. Record the exact SHA used for R1. Diff relevant changes since R0; do not treat older line numbers, status paragraphs, or parameter defaults as live truth.

The source index distinguishes files read for this proposal, earlier audit material, and items the agent must inventory. It is not a claim that this review re-read every RTL line or ran the whole suite.

---

## 2. Preserve V1 as a usable oracle, not merely an archive

### 2.1 Create a reproducible oracle pack

Keep V1's exact source revision, canonical reference implementation, compiler/planner, asset formats, representative content, toolchain versions, build settings, deterministic seeds, and relevant test invocations. Keep large captures/results in a durable project-controlled location with hashes and retrieval instructions. A path into a temporary worktree is not an archive.

Do not destroy the active V1 checkout, its uncommitted work, or existing branch history. Pin a reference by commit; create an immutable tag only through the repository's normal permitted workflow. Run original and experimental builds in separate build directories/worktrees so source generation and cached outputs cannot cross-contaminate them.

Prefer existing `.zcap`, `.zpak`, generated ABI types, and their version laws. Extend them only where an actual observation is missing. Do not create an incompatible second capture format just for this programme. [S7]

The pack has three executable faces:

- **Semantic oracle:** canonical C++/integer interpretation of commands, fields, geometry, terrain, raster and persistent state.
- **Implementation oracle:** frozen V1 RTL with the accepted transactions, stalls, faults and end-to-end routes that exposed real integration problems.
- **Content oracle:** captures and scenarios that demonstrate the actual intended game capabilities, including ugly workloads rather than only attractive demos.

These faces are complementary. Two implementations that share a faulty decoder do not provide independent evidence for that decoder.

### 2.2 Adjudicate disagreements instead of copying bugs

Create a small oracle-discrepancy ledger. Each discrepancy records input, observed C++ and RTL behavior, applicable current rule, minimal reproducer, proposed resolution, and status. Use current ratified semantics and explicit owner decisions to resolve it—not whichever output is easiest to reproduce.

Known examples for the first ledger:

- A wide Field output index must be checked before narrowing; index 68 must not alias to 4 and become valid.
- A malformed PARAMWALK descriptor must not rasterize the previous triangle merely because the illegal counter increments.
- Constant/prepared values must reach actual execution registers; `runs > 0` is not evidence of correct values.
- Geometry and material state must share the same lifetime; adding unrelated memory traffic must not change their pairing.

Classify each as already resolved, known V1 defect, unclear contract, or V2 regression. Do not suppress all malformed-input comparisons because some V1 cases are wrong.

Corrections to the oracle must be narrow, separately reviewable and versioned. Preserve the original failing case. No bulk re-goldening to accommodate V2.

### 2.3 Define compatibility before designing the transport

Proposed default compatibility envelope:

| Preserve | Usually free to change, subject to explicit review |
|---|---|
| Canonical command meanings and content formats | Internal micro-ops and descriptor layout |
| Fixed-point arithmetic, rounding and saturation | Arithmetic resource sharing and pipeline depth |
| Visibility, blending, depth/tie rules and material results | Cache organization and replacement |
| Terrain collision/render agreement and persistent changes | Location of non-visible working state |
| Published resource/version behavior and malformed-input handling | Number of physical queues and engines |
| Input, audio, presentation and declared frame deadlines | Private diagnostic cycle counts |
| Declared legal capacities and certified program/workload profiles | Internal operation ordering only where independence is proved |

Do not assume every counter is private: counters used by software, Measure policy, capture compatibility, or admission decisions need preserved meaning or an explicit versioned translation. A semantic event count and a microarchitectural stall count are different categories.

Internal latency may change; public timing obligations may not disappear. If a leaf's historical fixed latency is only an implementation choice, update its consumers through a typed boundary. If it is externally observable, model and preserve or explicitly negotiate it.

---

## 3. Build a capability graph, not a replacement module checklist

Inventory the current command schema, production manifest, contracts, actual reachable implementations, reference symbols, tests and owner rulings. Produce one entry per **semantic capability**, with links to its producers and consumers. Do not equate a count of connected module names with preserved capability.

Each entry should contain:

```
capability_id
canonical_inputs_and_outputs
state_read_and_written
observable_order_and_fault_behavior
legal_domain_and_declared_capacity
oracle_reference_symbols_and_tests
known_discrepancies
candidate_zz2_owner_and_dependencies
area_rate_memory_latency_evidence
verification_status_and_remaining_unknowns
```

Generate connectivity from the actual V2 source manifest and types where practical. Keep semantic rulings readable; do not pretend a regex can infer behavioral completeness.

Initial coverage families—not a claim of exhaustive enumeration:

| Family | Must survive the migration |
|---|---|
| Command/control | Decode, validation, frame transactions, resource versions, uploads, fault/abort behavior |
| Terrain | Sparse residency, top/bottom/substance, composition, materials, deformation, stamps, normals, velocity, querying and persistence |
| Field | All ratified profiles and op semantics; canonical/planned forms; prepared values; numeric status and profile certification |
| Geometry | Pose, rigid/two-weight skinning, normal handling, transforms, projection/depth, culling/clipping, warp, lighting, assembly and procedural/Forge routes |
| Visibility/raster | Binning/tile references, edge rules, exact attributes, Gouraud RGB, depth, translucency/ordering, resolve |
| Texture/material | Required formats, palette/version handling, sampling/filtering, recipes, surface/normal detail and refusal behavior |
| Dynamic effects | Particles, collisions, flow/formation/stamp work, beams/sky/stars/flares and the ratified post pipeline |
| Platform | Native modes and Duo, scanout, audio, inputs, reset, memory control, host bridge, debug/capture |
| Tool ecosystem | Existing assets, compilers, captures, replay, validation and development inspection |

Reserve a place and cost for every family at the first budget review, including the unglamorous platform and fault paths. An omitted owner is an unresolved item, not zero area.

---

## 4. Target platform and the first blocking questions

### 4.1 Hardware boundary

The intended part remains `5CSEBA6U23I7`, with a working resource ledger ceiling of **41,910 ALMs, 112 variable-precision DSP blocks, and 553 M10Ks**. Confirm the exact part/package, usable resources and tool report before signoff. The register count should be taken from that tool/device report; do not derive a final packing guarantee by multiplying four registers per ALM. [S1, H1]

Count the **whole configured bitstream**, including MiSTer/SS1 framework, bridges, controllers, video processing, audio/input interfaces, CDC/reset and release diagnostics. Do not reserve an imagined zero-cost shell.

Planning assumption to verify on the owner's actual board: use the existing FPGA-side 128 MB memory arrangement and no more than the intended 512 MB host-side allocation. Do not assume all RAM installed on a generic DE10-Nano is available to this application. Intel's DE10-Nano description lists a dual 800 MHz Cortex-A9 and 1 GB physical DDR3, but that does not certify the SS1 arrangement, usable application allocation, or transfer bandwidth. [H2]

### 4.2 Resolve clock and deadline units explicitly

The workload file budgets approximately 1,666,666 compute clocks at 100 MHz/60 Hz. The frozen video rules also expose Z60's 251,520-cycle deadline/profile period. They describe different timing quantities and historical simulation/board boundaries. V2 must carry an explicit mapping, not pick the convenient number. [S3, S4]

First-round design output:

- physical compute, SDRAM and pixel clocks and ratios;
- simulation clock-enable model and clock-domain crossings;
- public deadline units and their conversion to the physical schedule;
- mode switching, frame-start latching and previous-frame-repeat behavior;
- how an existing capture with a deadline field runs on V2;
- whether a 60 Hz newly rendered-frame requirement applies to every mode/profile.

A repeated previous image can be the correct underrun behavior. It does not count as another completed 60 Hz game frame.

Target 100 MHz as a **hypothesis to fit**, with sensitivity at 80 and 60 MHz. Do not rescue an over-shared design by assuming 150–200 MHz on an unplaced integrated machine. Conversely, if measured higher clocking is practical and physically safe, consider it as a measured alternative rather than a forbidden optimization.

### 4.3 Establish actual memory service

The V1 memory contract distinguishes a conservative simulation profile from board truth and documents refresh, bank conflicts, multi-burst requests and bounded arbitration. Reuse its negative tests and lessons; do not blindly reuse an old liveness bound after changing the clients. [S5]

Measure sustained sequential, strided and adversarial mixed read/write traffic with scanout, host traffic and refresh active. Record the worst accepted-request delay and actual useful bytes transferred—not only peak bus rate.

Without physical access, simulation and planning can proceed with explicit conservative assumptions. Board qualification remains open; an ideal RAM model cannot close it.

---

## 5. Candidate architecture: locally scheduled engines and authoritative state

### 5.1 Overall organization

```
Existing commands/assets/captures
        |
Compatibility frontend + host preparation
        |
Frame/resource directory + compact work references
        |
   +----+----------------+---------------------+
   |                     |                     |
Geometry engine       Field engine       Terrain/state service
   |                     |                     |
   +-- prepared geometry-+-- composed versions -+
                         |
              Tile lists / attribute preparation
                         |
              Coverage + depth + pixel attributes
                         |
              Texture/material + fragment commit
                         |
                  Resolve / post / frame seal
                         |
                Independent video and audio
```

This is a responsibility diagram, not a proposal for one central crossbar, one global multiported RAM, or one gigantic scheduler. Use local memories and a small number of defined transfers. A high-fanout global ready network can destroy the intended area/timing gains.

Preserve the current semantic scope. All ordinary scene work must execute on the actual target configuration; a desktop oracle may help testing but cannot secretly perform shipping gameplay work.

### 5.2 Host/control partition

Use the existing host for tasks with an amortizable, non-per-fragment cost: validated asset preparation, compiled program plans, immutable draw/material descriptions, residency policy, navigation already assigned there, command batching and non-critical inspection.

Keep the frame-critical geometry/Field/raster work in FPGA unless real Cortex-A9 measurements and a latency budget support a different partition. The OMEN's CPU is not a proxy for the host.

Submission is batched. Do not introduce a CPU/FPGA round trip per vertex, lattice point, texture miss or tile. Separate the control ring from bulk data DMA. Preserve trust-boundary checks even if a host validator has prepared the data: a mutable buffer, stale generation or bad address must not become trusted by assertion.

Specify shared-memory ownership, cache maintenance, barriers, completion and invalidation. A host read of a composed lattice must identify which committed version it received. Never rely on unspecified cache coherence.

Resident draw/material contexts are compiled once per actual version and referenced by work items. A preparation cache key includes the complete semantic dependencies, not just a material number or a non-verified hash.

### 5.3 Geometry engine

Propose a local sequencer with a banked scratchpad, resident palette/view/material references, and a throughput-sized multiply/accumulate bank. Kernel programs cover skinning, world/view transform, projection, clipping, normal/light work and eligible procedural operations. This is a fixed semantic kernel library, not a new arbitrary GPU instruction set.

Use a small number of active contexts to overlap RAM reads, arithmetic and long-operation latency. Survey 2 and 4 active contexts before inventing a large scheduler. The number of resident assets or programs must not force the same number of active execution contexts.

**Preserve exact arithmetic.** Two-weight skinning forms unrounded bone contributions and rounds only after blending; arbitrary signed-32 matrix inputs are part of the current contract. Keep the wide accumulator at the operation that needs it. Use a segmented/multi-cycle exact implementation if that is cheaper; do not round intermediate terms to fit a convenient datapath. [S10]

Do not carry two complete bone matrices down a generic per-vertex queue. Pin their immutable palette/version context, read the required rows locally, and hold only operands/results in flight. Reuse matrix values across a vertex batch only when pose/version/indices are identical.

Skin world-space vertices once where both views actually share that result; project separately for each view. Cache unique terrain vertices rather than recomputing triangle corners. Cache keys must include view, pose/warp, terrain generation, LOD/morph and all other applicable dependencies. Preserve V1's reuse that already exists rather than claiming it as a new saving.

Proposed design experiments compare 3/4/6 effective full-width product lanes and actual operand-port schedules. A mathematical multiply is not a DSP block: map signed widths, accumulators, cascades and any soft partial products on the target part before counting them.

Keep clipping and order-sensitive assembly local to the owning stream. Do not reorder triangles across material/depth/transparency dependencies just to make batches convenient.

### 5.4 Field engine

Retain the canonical Field ISA, validation, hashes, planner semantics and profiles. A prepared plan is an implementation artifact, not a private second ISA. The existing contract already permits profile stream adapters and distinguishes canonical fallback from Earth60-certified work. Verify current production behavior against that contract rather than reverting to its historical frozen engines. [S6]

Candidate implementation:

- banked RAM-backed instruction and operand state;
- a small, explicitly sized vector datapath;
- direct lattice/vertex/particle/instance input generators;
- output capture at the real export point, with credits reserved in advance;
- locally queued normalization, reciprocal, square-root, curve/ring/noise services where warranted;
- a simple ready-context scheduler with bounded queue capacities;
- prepared uniforms stored by immutable version, not aliased between active domains.

Compare one-, two- and four-point execution with matching input/output adapters. Four arithmetic replicas executing point zero four times are not useful SIMD. Cost the complete path, including input generation, all RF reads, long operations, exports and reductions—not just the add/multiply kernel.

Profile restrictions belong in generated validation/planning data. Profile adapters generate/consume streams; they do not reimplement arithmetic.

Preserve saturation and reduction order. Associative real-number algebra is not permission to reorder finite-width saturating composition. Uniform subexpressions may be hoisted only under the exact program's dependency and numeric laws.

Prepared scalar values must have a complete installation path and a reproducible materialization rule. Include the earlier missing-constant and slot-generation failures in every new prepared-program test.

A slow exact path for previously uncertified canonical programs can preserve existing capability distinctions. Creating a new slow path for work previously guaranteed at full rate is a performance concession and must be identified as such.

### 5.5 One terrain state, multiple readers

The strongest terrain invariant is already explicit: render, collision, particle interaction, normals, velocity and navigation consume the same composed lattice under the same triangulation. Keep it. Do not introduce a second approximate CPU/GPU evaluation just because the execution location changes. [S11]

V2 should organize terrain work around a versioned patch transaction:

1. Pin authored input, applicable field/stamp versions and the required time/domain inputs.
2. Evaluate/combine the patch in the prescribed order.
3. Produce lattice, material/substance and required derived-state updates.
4. Publish a complete version with an explicit commit record.
5. Let consumers bind that version and retire their references before storage reuse.

Do not assume the entire hot composed world fits on chip. At 256 patches, one 33×33 height16 plane is **557,568 bytes**; adding a same-sized velocity plane is **1,115,136 bytes**, before other data. That exceeds the entire M10K bit capacity numerically. Use external backing and a small banked on-chip working set, not full-world register or M10K residency. [S11, H1; arithmetic in the planning model]

Sharing the composed result between two views is valuable. Cache reuse across frames is exact only when all dependencies are unchanged. Time-varying fields remain time-varying; a hash of unchanged authored terrain is insufficient.

Invalidate neighboring cells/subpatches when normal, interpolation, LOD or apron dependencies require it. Deformation, undo, reload, eviction and material-only changes all need positive tests.

Separate capacity from residency. External backing may preserve legal patch/program capacity while reducing on-chip working state, but only if the required working set and transfers still meet deadlines. Thrashing cannot be dismissed as impossible unless the legal workload excludes it.

### 5.6 Raster and attribute preparation

Retain the native pixel law, tile edge rules, depth/ties, perspective behavior, Gouraud RGB, normal detail and required transparency order.

Keep the six cheap per-pixel attribute accumulators. Investigate shared, cached preparation for inverse-W, U/W, V/W and RGB:

- exact 98-to-32-position divider transformation after the existing non-saturation test;
- one triangle-gradient preparation reused across its tile references;
- row-seed reuse across horizontal tiles;
- exact quotient/remainder Y advancement with defined handling of wrap and saturation;
- two seed services as an initial candidate, not a predetermined answer.

Preserve the selected rounded-X/global-origin rule. Rational interpolation that looks mathematically nicer is not equivalent to the existing image. The previous audit contains a minimal counterexample and model tests. [S14]

Use a validity-checked cache with an original exact fallback. Include misses, one-pixel slivers, highly fragmented coverage, large reference lists and adversarial ordering in the schedule. A warm-cache broad triangle is not the entire workload.

Early depth/visibility rejection should precede expensive sampling only for cases where the original semantics allow it. Blending, alpha discard, tags, material faults or other required side effects may constrain reordering. Establish the legal fast path rather than silently dropping work that used to report an error.

Keep small tile-local colour/depth/tag state in suitable banks. If two tiles overlap execution, explicitly allocate two independent banks and account for simultaneous resolve/fill. Do not call a single dual-port memory four ports.

### 5.7 Texture/material engine

The goal is a throughput-oriented nearest-sample/cache-hit path and a separately sized filter path—not a universal fully parallel bilinear machine. Size palette reads, cache lanes, filter-channel jobs, output commits and miss buffers individually. [S3]

Use a compact material recipe derived from the existing capability. Bind immutable palette, texture, material and detail versions at the right draw/triangle boundary. Fetch/pin a context locally once when useful; do not replace a wide wire with a new RAM access on every fragment.

Keep bounded outstanding misses and enough hit-under-miss behavior to meet the actual legal traces. Preserve ordered retirement where blending/fault/continuation semantics require it. Do not build a large reorder machine speculatively.

A logical bilinear sample may require four texels and several channel operations. Count those demands, not merely one request token. Format conversion, saturation and generation checks remain part of the reference comparison.

The scalar lookup ROM conversions and shared helper arithmetic remain worthwhile local experiments, but they are not the architectural premise for a fivefold reduction.

### 5.8 Procedural, particle, sky and post work

Map each remaining capability onto an owner before it is implemented. Reuse geometry/Field services for compatible arithmetic; use small dedicated streaming units where their frequency or deadline justifies them.

FORGE and procedural generation are not permission to pre-bake away live geometry changes. Particle collision must use the authoritative terrain version. Beams, stars, skies and post effects must preserve their actual sampling, blend and tag behavior, not just a similar-looking screenshot.

Phase-share expensive arithmetic or DMA staging only when a dependency schedule proves the phases do not overlap. A frame can have composition, rendering and previous-frame scanout active together. 'Post comes later' is not enough to assume all producer state is dead.

### 5.9 Independent presentation and minimal always-on safety

Scanout, audio and input handling must not depend on an available geometry/Field context. Preserve mode latching, frame ownership and audio FIFO behavior. A slow render workload must neither tear a frame nor deadlock reset.

Keep essential bounds/generation checks, timeout/fault retirement, terminal accounting and compact event counters in the release design. Rich traces can be build-selectable or streamed to a host buffer. Never remove semantics or refuse malformed-input testing under the label 'strip debug'.

Measure release and diagnostic configurations separately. A smaller release build counts only after its observable behavior is compared to the diagnostic build and its retained safety surface is documented.

---

## 6. Shared-state and transport rules

### 6.1 Separate semantic capacity, backing capacity, and active execution state

A system can support many resident programs without having a complete independent execution machine for each one. Likewise it can retain many draw contexts externally while keeping a few pinned near a consumer. These are different capacities; name them separately in every parameter and report.

Do not reduce declared capacity by shrinking the backing directory to match an active-context count. Do not overprovision active contexts merely because eight programs can be resident.

### 6.2 Compact work references, local expanded payloads

Use typed work references carrying a sequence/transaction identity, frame epoch, context reference/generation, operation class and small flags. Explore approximately 64–128-bit queue tokens; the exact width must come from required identities and lifetime bounds, not aesthetic preference.

Expand the referenced data into local working registers at the consumer. It is entirely appropriate for a multiply or pixel datapath to carry a wider live operand bundle. The objective is to avoid duplicating immutable descriptions across every queue, not to force all useful data through one narrow serial bus.

Generation wrap is a real protocol issue. Prevent reuse while any reference remains, or use a proven drain/epoch transition. 'The counter is probably wide enough' is not a lifetime argument. Do not use a hash alone as correctness-critical identity without verifying the actual key.

### 6.3 One owner for each state transition

Each state object has an owner, legal readers, mutators, publish boundary, retire rule and invalidation dependencies. State may be replicated for read bandwidth, but there is one semantic mutation authority.

For each queue or memory, document:

- element layout, depth and physical RAM mapping;
- read/write ports and cycle schedule;
- maximum live lifetime and occupancy;
- backpressure, reset, abort and replacement behavior;
- release event and generation handling;
- the observable value or invariant checked by the oracle.

A 32-entry table with a 128-bit payload may warrant multiple width-banked M10Ks. Tiny metadata can remain in registers. Choose by total logic/port/timing cost, not entry count alone.

Reset validity/ownership rather than clearing every bulk data bit when equivalent. Prove no invalid data becomes observable, including read-during-write, aborted publication and reset in flight.

### 6.4 Bounded local scheduling

Do not create a universal all-client arithmetic fabric as the first implementation. Use local schedulers with small fixed client sets and typed requests. A shared square root should not require a global 300-bit router and 32-entry reorder buffer unless measured demand proves that worthwhile.

Reserve output capacity before accepting work that cannot later be cancelled. Every accepted transaction must reach exactly one terminal state: committed, rejected, or cancelled under its stated law. Track this at useful boundaries; forwarding a counter does not require another independent counter bank.

For fan-out, capture once and acknowledge each real consumer. For cancellation, drain already-owned resources and prevent new publication from cancelled work. The release event is the final consumer's retirement, not a binner or upstream producer becoming idle.

These rules directly target the classes of bugs already found in V1. They are not optional decoration around the smaller datapaths.

---

## 7. Workload-driven architecture: the first important deliverable

### 7.1 Replace optimistic averages with an executable demand vector

Extract command and post-visibility work traces from V1/reference runs. A frame's work descriptor should include at least:

| Resource family | Count and distribution needed |
|---|---|
| Geometry | Unique decoded/skinned vertices, rigid/blended mix, matrix products, normal/light operations, per-view projection, clipping expansion, triangle setup |
| Terrain | Visible/live/dirty patches, applicable fields per patch, lattice outputs, derived-state updates, material-only changes, evictions and publications |
| Field | Prepared instruction classes, active points, operand reads, long-op jobs, preload/export traffic, reduction ordering |
| Raster | Tile references, references per tile, covered rows, accepted/rejected fragments, attribute seeds, cache misses and fallback events |
| Texture | Formats/filter classes, texel/cache accesses, palette reads, filter channels, outstanding fills, bytes and address/bank locality |
| Memory | Useful bytes plus burst overhead, conflicts, refresh, read/write turnarounds and competing clients |
| Platform | Scanout/audio/input work, host transfers, reset/fault handling, frame seal and presentation deadlines |

Distinguish **observed traces**, **derived workload estimates**, **ratified guarantees**, and **synthetic stress**. The existing workloads file already makes some of these distinctions and also records unresolved demand. Reuse its discipline; do not promote every estimate into a promise or drop an existing promise because it is inconvenient. [S3]

### 7.2 Resolve simultaneous maxima

Do not silently demand the Cartesian product of every independent maximum; do not silently assume they never coincide either. Build an explicit joint-workload envelope from current rules and intended content.

Example: 256 patches each requiring 1,089 samples across 16 changing fields would mean **4,460,544 point-field evaluations**. At an illustrative 22 vector micro-ops each, that is **98,131,968 lane-micro-ops**. Four perfect lanes would still require 24,532,992 issue clocks before transport or long operations. This is a useful **synthetic falsification case**, not a claim that V1 promises that conjunction at 60 Hz.

Determine whether all those counts are simultaneously required, which fields are changed, whether their supports overlap, what preparation is legal, and which certified profile applies. 'Caching' is not an answer when the cache inputs actually change. Nor is 'full capability is impossible' an answer before identifying the actual guarantee.

### 7.3 Lower bounds first, detailed schedules second

At a target clock `f` and frame rate `r`:

```
raw_cycles = floor(f / r)
usable_cycles = floor(raw_cycles * 0.8)   # initial 20% schedule reserve
```

At 100 MHz and 60 Hz, this conservative two-stage calculation gives 1,666,666 raw and **1,333,332 usable cycles**. The reserve is a proposed engineering target, not a replacement for the existing public deadline law.

For each shared service, sum all users' demand in consistent units. Its resource lower bound is demand divided by effective issue capacity. For independent services, the maximum of the resource lower bounds is only a lower bound on frame length; dependencies, limited queues and memory stalls can make it longer.

A lower bound over the deadline proves that candidate configuration inadequate. A lower bound below it does **not** prove schedulability.

The companion model illustrates why. 120,000 two-weight vertices require 2.16 million matrix products. An additional hypothetical 278,784 unique terrain projections at nine products each brings the counted total to 4,669,056. Three perfect product lanes cannot serve that within the initial reserved schedule at 100 MHz; four leave little room; six leave more. Normals, lights, other geometry, ports and pipeline stalls are omitted, so none of the passing lower bounds is a capacity certificate. [S10, S3; explicit projection-cost assumption]

### 7.4 Include data supply and retirement

Every arithmetic design comes with an operand-port schedule, including RF reads, broadcasts, prepared constants, stores, dependency hazards and queue credits. Do not quote 'six multiplies per cycle' from six multipliers with only two usable operands per cycle.

Every texture design counts cache/texel accesses, palettes, filter jobs, misses and ordered output separately. The existing known profile of 541,640 samples excludes creatures, giants, objects, beams and cache misses; the 1,094,600-sample profile is explicitly synthetic/capability-oriented. Neither is a complete workload certificate. [S3]

A 64-byte transfer is not automatically one SDRAM burst. Respect the actual bus width and burst law. Existing simulation bounds must be re-derived when the arbitration graph changes. [S5]

### 7.5 Trace replay plus adversarial schedules

Construct a small deterministic event-level schedule model before large RTL assembly. Its service costs must eventually come from measured kernels; until then mark them ASSUMED. Replay both actual command-derived traces and adversarial legal orderings.

Model finite queue depths, outstanding misses, refresh, bank conflict patterns, register/memory ports, phase boundaries and output consumers. Preserve functional ordering dependencies while exploring scheduling choices.

Do not make the simulator green by allowing unbounded queues, infinite texture-cache hits, free context fetches, instantaneous DMA or idealized host wakeups. Include cold cache, patch-slot reassignment, both views diverging and near-deadline bursts.

Keep model-versus-RTL trace comparisons. A schedule model that predicts 30% spare time while integrated RTL loses 30% to an omitted service should become an instrument bug with a reproducer, not an unexplained integration tax.

---

## 8. Initial resource allocation — constraints, not predicted sizes

The following is a deliberately challengable starting allocation for the **entire bitstream**. No row has been implemented or fitted by this proposal.

| Owner | ALM allocation | DSP allocation | M10K allocation | Advisory registers |
|---|---:|---:|---:|---:|
| Platform, memory controllers, bridges, video/audio/input | 4,500 | 0 | 70 | 10,000 |
| Command/resource control and shared descriptors | 3,500 | 0 | 45 | 10,000 |
| Geometry engine, local state and arithmetic | 6,500 | 40 | 80 | 22,000 |
| Field engine, adapters and long-op services | 5,500 | 24 | 90 | 24,000 |
| Raster, attribute preparation, depth and tiles | 4,000 | 8 | 65 | 12,000 |
| Texture/material/detail sampling and caches | 6,500 | 16 | 60 | 18,000 |
| Procedural/particle/post assistance not included above | 2,500 | 8 | 35 | 8,000 |
| Explicit integration allowance | 2,000 | 0 | 10 | 6,000 |
| **Total allocation** | **35,000** | **96** | **455** | **110,000** |
| **Nominal unused device resources** | **6,910** | **16** | **98** | Tool/packing dependent |

The advisory register column is a discipline on state growth, not an independent promised mapping. MLAB storage consumes logic resources and must not be counted as free M10K capacity.

This table does not settle a fair internal split. The agent should challenge it. If the measured framework needs 7,000 ALMs, the remaining design budget falls by 2,500; do not hide the difference. Conversely, a cheaper platform should not trigger automatic feature spending before routing/timing is stable.

Area accounting should keep three views:

1. Standalone kernel/map attribution for experiments.
2. Incremental composed changes under the same part, constraints and configuration.
3. Full integrated placement and timing, the release authority.

Do not add parent and child hierarchy totals, compare maps on different parts as savings, or subtract ALUT savings directly from ALMs. Do not treat a debug port exposing every state bit as the real product top.

Each substantial replacement should be evaluated in the integrated skeleton early. Record actual memory modes, DSP decomposition, setup/hold slack, unconstrained paths, CDC/reset and routing pressure—not only total ALMs.

A proposed operating point requires a nonnegative timed design at that point under appropriate constraints. A desired 100 MHz period in an SDC is not a measured 100 MHz machine.

---

## 9. Oracle-backed development at every scale

### 9.1 The comparison ladder

| Stage | Oracle interaction | Acceptance evidence |
|---|---|---|
| Arithmetic | V2 operator vs canonical integer law and V1 operator | Exhaustive reduced-width cases, boundary/random cases, relevant formal invariants |
| Transaction | V1 and V2 consume the same typed requests | Values, identities, terminal status and required order; different internal latency permitted |
| State transition | Run the same command block from the same snapshot | Identical committed semantic state and prescribed faults |
| Subsystem | Insert V2 between V1/reference producer and consumer | No hidden dependence on old timing; backpressure and cancellation cases |
| Complete frame | Whole V2 vs oracle for one snapshot/input batch | Colour, depth/tag surfaces where observable, resource changes and publication |
| Long run | Shared input/replay sequence | State checkpoints, persistence, undo/reload where applicable, divergent views and failure recovery |
| Physical board | Same captures on fitted V2 and oracle host | Actual output/state checks plus timing, memory and sustained operation |

The full V1 oracle normally runs on the workstation/test infrastructure, not beside V2 inside the limited FPGA. Hybrid simulation is a migration instrument; it is not evidence that an FPGA configured with only half the required work is a complete console.

### 9.2 Comparable event traces

Normalize traces around semantic identities and commit points. Compare exact integer payloads, state versions and required event ordering. Do not compare raw internal cycle numbers when the purpose is to change the microarchitecture.

For independent events, compare their identity-keyed content under a documented partial order. For order-sensitive events—saturation, blending, resource mutation, refusal accounting—preserve the actual required order. 'Sort both traces' is not a general equivalence proof.

Internal scratch addresses may differ. Compare logical object/offset/version effects, while separately checking actual V2 guard and address-range behavior. External ABI-visible addresses and layouts require compatibility.

Trace collection itself must not change behavior: show enabled/disabled equivalence and do not clock the DUT once per observation. Preserve raw failing traces and automatically reduce them into small regression cases.

### 9.3 Test corpus

Include real captures and intentional stress for:

- ordinary terrain plus an army and multiple materials;
- the giant/reference-list guarantee and a giant close to the camera;
- many small triangles, slivers, tile seams and clipping expansion;
- maximum admitted concurrent fields under the resolved profile;
- changing fields plus collision/render agreement and persistent destruction;
- material-only changes, equal-signature new patch occupants, eviction and reload;
- Duo cameras sharing some content and then looking in unrelated directions;
- skies/stars/clouds/beams/particles alongside terrain and foreground geometry;
- palette/texture/pose changes while prior work remains in flight;
- reset, abort, deadline miss, malformed descriptor, full queues and stalled consumers;
- adversarial legal texture-cache misses and SDRAM bank conflicts.

The current small successful console smoke is a must-keep regression, not the full corpus.

### 9.4 Prove the checker notices broken behavior

For each important family, keep a deliberate failure that its check must reject. Initial mutations should include:

- old 98/32 divider disagreement through a deliberately broken prefix or rounding rule;
- double-rounded two-weight skinning;
- a missing prepared constant;
- using current rather than pinned material state;
- truncating a raw index before validation;
- forwarding an illegal triangle instead of retiring it as rejected;
- reusing a patch slot with the wrong owner;
- releasing a context on upstream quiet rather than final retirement;
- omitting a changed lattice apron from invalidation;
- accepting an output without the reserved capacity or dropping a terminal event.

Do not quote a passing equivalence run without checking its accepted work count, exercised path coverage, and one applicable negative control. A fixture that feeds zero triangles can match every renderer.

---

## 10. Evidence ledger: capability, performance and readiness stay separate

Give each capability separate states for semantics, integration, performance, area, memory, and board qualification. Suggested vocabulary:

- `NOT_IMPLEMENTED`
- `MODEL_ONLY`
- `RTL_DIFFERENTIAL`
- `INTEGRATED_DIFFERENTIAL`
- `RATE_MEASURED`
- `PLACED_TIMED`
- `BOARD_QUALIFIED`
- `KNOWN_DISCREPANCY` / `UNRESOLVED_CONTRACT`

These are dimensions, not a single linear percentage. A placed implementation with wrong material values is not further along than a correct one solely because it has a bitstream.

Every evidence receipt should include source/configuration/tool hashes, exact top and device, enabled capability profile, oracle revision, input/capture hashes, seeds, test commands, exercised work, positive/negative controls, map/fit/timing results and limitations.

Keep machine-readable outcomes separate from prose summaries. A generated dashboard may summarize them, but it cannot create evidence. Missing measurements remain null/unmeasured—not zero cost.

Release readiness is the conjunction of semantic coverage, complete integration, declared workload capacity, real memory/host assumptions, placed timing, and physical-board evidence. Do not collapse it into another completion register that can hide independent failures.

---

## 11. Development programme and decision gates

### Round R0 → R1: independent architectural review, before broad RTL

The receiving agent reads current evidence, challenges this proposal, tests the arithmetic of its resource arguments, and produces a revised candidate. It should explicitly consider another architecture rather than merely polishing the prose. Commit and push the review package as described in section 14.

Permitted first-round work should remain narrowly scoped: documentation, source inventory, trace extraction, or isolated reversible measurement where already authorized. This document does not authorize rewrites of the active V1 implementation or ratified capability cuts.

### Gate A — freeze oracle inputs and settle contracts

Deliver the oracle pack manifest, capability graph, discrepancy ledger, clock/deadline interpretation, platform identity and joint-workload envelope. Extract representative traces. Reproduce selected V1 outputs with the pinned toolchain. Open uncertainties must identify the next discriminating experiment.

Do not spend weeks making a perfect inventory before measuring a known hard bottleneck. Gate A's minimum is a trustworthy boundary and enough trace data to test the candidate; extend the corpus continuously.

### Gate B — fit the real platform skeleton

Bring up command ingress, RAM/SDRAM access, native output, frame ownership, minimal input/audio and fault/reset on the actual framework. Fit it and record the physical footprint. Run stress memory patterns with presentation active.

A temporary test-pattern or diagnostic-only configuration is explicitly a bring-up milestone. It is not called full Zhaozhou.

### Gate C — attack the three highest-risk engines in parallel

Run bounded architecture trials on separate branches/worktrees:

1. **Geometry/state trial:** exact skin/projection kernels, resident palette/context memory, actual operand supply, 3/4/6-lane schedule comparisons.
2. **Field/terrain trial:** prepared program correctness, complete point ingress/egress, long-op mix, two domains, multiple dirty patches, backing-state transfers.
3. **Raster/texture trial:** exact shorter division, shared seeds, sliver/giant tile traces, texture format mix and cache misses.

Every trial reports total local cost, not just arithmetic-unit cost. Compare to the reserved whole-system budget. Exercise competing memory traffic. Keep a second candidate where the first is uncertain.

The hard question at Gate C is whether these measured clusters, plus the measured platform and bounded remaining work, plausibly compose. If not, revise architecture now. Do not implement fifty more feature adapters to postpone the decision.

### Gate D — one complete V2 frame with compact ownership

Compose a small but real path from existing command/asset input through context binding, geometry, tile work, texture/material, resolve and frame seal. Use actual memory and the public version/guard behavior. Demonstrate the metadata-lifetime and malformed-input cases.

Differentially compare this frame to V1 and fit the integrated configuration. Keep explicit coverage labels for capabilities not integrated yet.

### Gate E — complete geometry and terrain/Field coverage

Add remaining pose/warp/light/procedural routes and terrain composition/persistence using the established kernels and state ownership. Every added capability must either use an existing engine or justify its incremental resource cost.

Keep 'physics equals pixels', material-at-consumer and two-view sharing tests in the composed path. Add full-domain numeric boundaries, not only normal content.

### Gate F — complete effects and platform semantics

Integrate the remaining particles, collision, beams/sky/stars/flares, post, audio/input/debug and fault behavior. Price them from the beginning; this gate spends their reserved allocation rather than discovering them as an unbudgeted tail.

Run combination tests rather than only separate showcases. A sky-only test and a terrain-only test do not establish their shared-sampler frame.

### Gate G — whole-profile scheduling and physical signoff

Run original declared profiles and representative game traces under the resolved board timing. Report the worst observed frame, distributions, and analytic/formal bounds where available; do not call observations a universal proof.

Require the real shipping configuration to fit with timing and explicit reserves. Compare board state/output to the oracle; run repeated destruction/persistence, both views, resets and memory stress. Confirm host work and data-transfer budgets on the actual HPS.

Only now can a full-capability MiSTer implementation be called demonstrated. If it fails, preserve the negative evidence and the working implementation; the next iteration starts from knowledge, not from a new claim that a small netlist must be possible.

### Stop/rethink triggers throughout

Revise immediately when a service cannot feed its arithmetic, when a local memory needs unbudgeted replication, when integration loses the assumed clock, when queues must grow without a bound, when cache misses defeat the legal workload, or when host deadlines depend on unmeasured desktop-speed assumptions.

Do not solve these by silent quality/capacity cuts. Try a different partition, dedicated hotspot hardware, layout or preparation plan. Only a deliberate owner-approved profile change turns a failed full-profile candidate into a different product tier.

---

## 12. Alternatives the agent must evaluate

R0 prefers clustered scheduled engines, but the review should compare at least one materially different alternative.

| Candidate | Strength | Principal risk | Discriminating evidence |
|---|---|---|---|
| **A. Clustered scheduled engines** — R0 preference | Local data supply and concurrency; substantial reuse without one global scheduler | Still too much total control/state or difficult arbitration | Integrated area/rate trials with real memory traces |
| **B. More unified vector/microcoded compute engine** | Potentially lower duplicated arithmetic/control | RF ports, context traffic, instruction scheduling and long-op contention recreate a costly processor | Same mixed geometry+Field workload, including stalls and exact arithmetic widths |
| **C. Host-heavy preparation and selected execution** | Exploits hard processor and external memory | A9 throughput, OS jitter, coherence and round trips | Actual target-host benchmarks and end-to-end latency, not OMEN measurements |
| **D. Incrementally refactor V1 clusters** | Maximum direct implementation reuse and lower initial semantic risk | Keeps broad transport/state architecture that caused the size problem | Composed before/after map plus a projection of remaining disjoint subtrees |

Hybrid decisions are welcome: a deliberately dedicated skinner inside an otherwise shared geometry cluster can beat both extremes. The goal is the best measured system, not ideological purity about sharing.

Do not confuse exact caching, memory packing or scheduling changes with capability reductions. Conversely, do not label a lower worst-case rate 'only architectural' when software was promised the original rate.

---

## 13. Open questions R1 must answer or turn into experiments

1. Which public timing quantities are fixed, and how are they mapped onto physical clocks in each mode?
2. Which independent maximum workloads are guaranteed simultaneously? Which rows are merely synthetic or derived?
3. What is the actual framework/controller footprint on the intended board/configuration?
4. Which host-side capacity and bandwidth are genuinely available alongside the game and operating system?
5. Which original counters/Measure outputs are semantic software inputs rather than replaceable diagnostics?
6. Where must V1 faults be corrected before it serves as an oracle, and what evidence establishes the correction?
7. Can geometry keep its exact widths and full required rate using local banked contexts within its allocation?
8. What complete Field ingress/execute/export architecture meets the certified workload, and how many useful lanes does it need?
9. How much SRAM replication is unavoidable for the actual operand, texture and tile-port schedules?
10. Is a two-service exact attribute seed engine sufficient on worst legal tile/reference distributions?
11. What texture miss and format mix is required beyond the current derived profiles?
12. Which terrain changes require full patch work, and which dependency-proven subregions can be updated exactly?
13. Can contexts be compacted without a per-fragment memory bottleneck or a large reference-count/control fabric?
14. What reserve should be required in the integrated release design, and what minimum board evidence is necessary?
15. Which alternative architecture has the strongest chance of beating this candidate, and what experiment compares them fairly?

These questions are not a request to hand routine technical decisions back to the owner. Decide reversible engineering details, document reasoning, and test them. Escalate actual changes to promised capability or unresolved product intent.

---

## 14. Instructions to the receiving agent: critique, revise, save, commit, push

The owner explicitly wants a design conversation between independent reviewers. Treat this document as an external candidate, not higher authority than the repository's current source, measurements, or owner decisions.

### Required first response

1. **Read the relevant V1 source and evidence yourself.** Record the live baseline and what changed since R0.
2. **Write your independent pre-mortem before merely editing this RFC.** Name the most likely reasons this architecture would miss area, rate or correctness.
3. **Review the proposal claim by claim.** Mark accept, modify, reject, or unresolved; cite specific evidence or a discriminating experiment.
4. **Develop at least one serious alternative.** It may be better than R0. Compare full-system costs, not just its favorite kernel.
5. **Produce R1 as a coherent candidate architecture.** Keep useful R0 ideas, remove weak ones, and state remaining uncertainty. Recompute allocations and schedules; do not copy them because they look precise.
6. **Save the documents and machine-readable planning inputs in the repository.** Preserve R0 as the received proposal so the disagreement is reviewable.
7. **Commit and push through the normal authenticated repository workflow.** Verify the remote SHA matches the local commit. Do not stop after writing a chat response or a file in a temporary directory.
8. **Return the exact branch, commit, paths, verdict and next experiments.** That is the material the next review will inspect.

### Suggested repository layout

Use the established design area if there is one. Otherwise:

```
design/v2/
  proposals/R0-external-candidate.md
  reviews/R0-agent-review.md
  proposals/R1-agent-candidate.md
  DECISIONS.md
  OPEN_QUESTIONS.md
  inputs/planning_inputs.json
  evidence/README.md
```

Use an isolated design branch, provisionally `design/zhaozhou-v2-rfc`, based on the correct current branch. If that branch exists, inspect it rather than overwriting it. Preserve unrelated local work. Stage exact files rather than `git add .`; use a normal non-force push. Do not open a public issue or merge into production as a side effect of this document review.

If push fails, report the exact failure and the local commit; do not claim the review is published. Do not request a credential be pasted into a chat. Existing repository authentication is the intended route.

### Handoff content to return

- Live baseline and final pushed SHA.
- File paths for review and R1.
- Your preferred architecture and why.
- The strongest objections to R0, including any arithmetic or unsupported claims.
- What V1 work is retained unchanged, wrapped, rescheduled, or replaced.
- A revised whole-system allocation, clearly separated from measurements.
- A joint-workload/rate argument with unresolved dependencies visible.
- The first three experiments, acceptance criteria and who/what they need.
- Any genuine owner decisions, separated from routine engineering choices.

This author will review the returned committed material in a subsequent user-requested turn. There is no automatic monitoring or background review implied.

---

## 15. Source index and provenance

Repository links are pinned to `814687ae955a3ce4690010ce750f2b7b5e597ec2` unless stated otherwise. Descriptions identify the evidence used; historical commentary inside these files must still be checked for supersession.

- **S1:** `reports/synthesis/console_entity_attrib_shipping_20260928.md` — current retained hierarchy/resource attribution; map-only caveats.
- **S2:** `reports/OWNER-ESCALATION-20260928-THE-CONSOLE-DOES-NOT-FIT-ANY-CYCLONE-V.md` and `reports/PHASE3-CONCLUSION-20260928-THERE-IS-NO-LARGE-LEVER.md` — estimated ALMs, current local-optimization conclusions, not a proof about an unbuilt architecture.
- **S3:** `design/budgets/workloads.yml` — compute budget, sampler resource-vector model, derived/synthetic/unmeasured distinctions and terrain-projection caveats.
- **S4:** `spec/video_rules.md` — native modes, stored layout, mode latching and timing/profile distinctions.
- **S5:** `spec/memory_rules.md` — simulation-vs-board boundary, burst/refresh/arbitration lessons.
- **S6:** `design/contracts/FIELD.SEQ.CORE.md` — canonical semantics, prepared plans, stream adapters, historical realizations and certification distinctions. Parts of this document retain superseded implementation descriptions.
- **S7:** `spec/capture_format.md` and the listed `spec/commands.zidl` schema — reuse the existing language/capture boundary; inventory exact current versions during Gate A.
- **S8:** `reports/FINDING-20260928-TRUNCATE-THEN-CHECK-IN-THE-FIELD-HOST.md` — index validation discrepancy.
- **S9:** `reports/FINDING-20260928-PARAMWALK-ILLEGAL-FLAG-HAS-NO-CONSUMER.md` — rejected-and-counted discrepancy.
- **S10:** `design/contracts/GEOM.SKIN.md` — exact single-rounding two-weight law, full signed widths and the documented rate/parallelism trade.
- **S11:** `spec/terrain_rules.md`, especially the lattice law — one composed state, same triangulation, 33×33 lattice and the 256-patch cache budget.
- **S12:** `reports/MEASURED-20260928-A-REGISTERED-READ-TURNS-THE-CASE-INTO-A-ROM.md` — measured standalone ROM inference experiment, not a completed integrated saving.
- **S13:** `reports/BOARD-LINT-TRIAGE-20260928.md` — remaining implementation defects and unresolved warnings.
- **S14:** Previous external audit, `zhaozhou_audit/README.md` and `zhaozhou_v2_audit.zip`, audited at `50cfd2c1671112149eb2c6ea347f9cb9715c92e7` — selected RTL inspection and locally executed integer models. Treat its experimental patch as untested RTL until independently verified.

Pinned repository base: https://github.com/Fabulu/zhaozhou/tree/814687ae955a3ce4690010ce750f2b7b5e597ec2

Official hardware references checked for this proposal:

- **H1:** Altera, *Embedded Memory Capacity in Cyclone V Devices*: A6 has 553 M10K blocks; MLAB is distinct. https://docs.altera.com/r/docs/683694/current/cyclone-v-device-overview/embedded-memory-capacity-in-cyclone-v-devices
- **H2:** Intel, *Terasic DE10-Nano*: dual 800 MHz Cortex-A9, physical DDR3 and platform description. This is not an SS1 board qualification. https://www.intel.com/content/www/us/en/developer/topic-technology/edge-5g/hardware/fpga-de10-nano.html

### Scope of this contribution

This is a proposed architecture and review workflow grounded in live repository reads and the previous audit. No V2 RTL, Quartus placement, timing, HPS benchmark, physical-board experiment, or complete oracle regression was executed here. The included Python model checks allocation arithmetic and illustrates lower bounds; it deliberately cannot certify feasibility.

**The point is to carry V1's knowledge forward while making V2 earn its size and speed—not to declare the old effort a failure, nor to declare this proposal the answer before it is tested.**
