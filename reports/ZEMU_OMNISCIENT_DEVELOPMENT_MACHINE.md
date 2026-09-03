# ZEmu: The Omniscient Development Machine

## Design treatise for an agent-native, time-travelling whole-console oracle

**Project:** Zhaozhou / Form / Tribute Upheaval  
**Document status:** Proposal and design treatise; not a ratified specification  
**Repository baseline reviewed:** `Fabulu/zhaozhou`, branch `main`, commit `e78f28e1d9ef6ad23b096808f1c589817b791ca8`, 3 September 2026  
**Intended readers:** Fabian, implementation agents, verification agents, compiler agents, gameplay agents, and future maintainers  
**Working title:** “The Omniscient Machine”

---

## Abstract

ZEmu should not be designed as a desktop program that happens to run Zhaozhou games. It should be designed as the most powerful development environment the Zhaozhou project can manufacture: a whole-machine executable oracle, an always-on flight recorder, a deterministic time machine, a state laboratory, a source-level profiler, a hardware differential harness, a mission test farm, and a fully headless environment that agents can control without touching a graphical interface.

The existing project has already done much of the unusually difficult conceptual work. ZRef is defined as the slow, scalar, exact oracle. ZEmu is required to remain bit-identical to ZRef for implemented features. Frame packets are sealed and versioned. `.zcap` captures already carry controller snapshots, expected framebuffer and tile CRCs, counters, source maps, and a first-divergence record. Form already assigns stable source IDs, separates deterministic truth from degradable presentation, emits exact C++ evaluators for hardware-lowered programs, and treats tests, captures, costs, and source mapping as compiler products. The repository already records exhaustive development provenance through agent runs.

This treatise proposes joining those pieces into one deliberate system. At any chosen cycle, instruction, simulation tick, event, frame, audio sample, or rendered pixel, an agent should be able to ask:

- What is the complete machine state?
- What changed since another point in time?
- Which source declaration owns this byte, entity, command, primitive, pixel, terrain scar, sound, or cost?
- Which instruction or event last wrote it?
- What chain of events led here?
- When did two executions first diverge?
- Can this exact universe be rewound, forked, modified, and replayed?
- Can the failure be reduced to a small portable case?
- Can one thousand variants be run headlessly and compared?
- Which run records explain why the relevant source was built this way?

The answer should be machine-readable first and human-readable second. A graphical debugger can be magnificent, but it must be a client of a headless, deterministic core. An agent should be able to load a cartridge, inject inputs, run until a semantic predicate fires, inspect typed state, request a bounded trace, change a value transactionally, fork the timeline, render a frame, compare two universes, save a forensic bundle, and exit with meaningful status—all through a stable structured protocol.

The fundamental opportunity is not merely that ZEmu can see all RAM. The project owns the hardware, language, compiler, engine, game, models, formats, reference functions, and development records. Therefore it can make the RAM explain itself. Form can emit type layouts, stable identities, ownership information, source spans, fixed-point meanings, pool schemas, invariants, and provenance hooks. The emulator can correlate runtime history with the recorded history of how the runtime was built. It can become a universe with an omniscient flight recorder built into its physics.

---

# Part I — Purpose, footing, and laws

## 1. The thesis

The ordinary purpose of an emulator is compatibility: reproduce a machine well enough that software written for the machine runs elsewhere.

The purpose of ZEmu should be more ambitious:

> **Make every important fact about a running Zhaozhou universe observable, reproducible, addressable, comparable, and manipulable without changing its meaning.**

Compatibility remains mandatory. It is merely the floor.

ZEmu should become the place where the game is normally developed. The physical FPGA proves that the machine exists and meets real timing, bandwidth, electrical, and resource constraints. ZRef proves exact scalar semantics. Verilator proves RTL against those semantics. ZEmu composes the complete machine into a usable world and exposes that world with far more visibility than physical hardware can afford.

This division is important:

- **ZSpec** says what the machine means.
- **ZRef** is the slow scalar executable law for exact operations.
- **ZEmu** is the complete executable world, optimized but exact, with omniscient development facilities.
- **ZRTL/Verilator** proves the hardware implementation.
- **The physical FPGA** proves clocks, bandwidth, interfaces, and actual silicon behavior.

Calling ZEmu the “ultimate oracle” should therefore mean **the ultimate whole-machine development oracle**, not permission for an optimized emulator path to silently redefine a scalar law. When a semantic dispute exists, ZSpec and ZRef win. When a developer needs to understand an entire battle across gameplay, memory, commands, rendering, audio, and hardware pressure, ZEmu is the instrument that makes the answer obtainable.

## 2. Why this project can build an emulator unlike ordinary emulators

Most emulators must infer and reproduce a sealed machine. ZEmu will be built alongside a machine whose blueprints are owned and adjustable.

Most debuggers receive binaries whose types and source structure were partially erased. Form can preserve and emit exactly the metadata ZEmu needs.

Most game engines expose high-level entities while hiding driver, renderer, allocator, asset-import, and scheduling behavior behind third-party boundaries. Zhaozhou owns the causal chain from game declaration to machine command to pixel.

Most projects retain the current source but lose the reasoning that produced it. Zhaozhou’s run system records reconnaissance, assumptions, implementation, failed approaches, review, tests, measured evidence, and unresolved uncertainty.

Most debugging sessions begin after the bug, with inadequate logs. ZEmu can continuously retain a bounded pre-history and can snapshot the entire universe when a predicate, assertion, deadline fault, divergence, crash, or agent request occurs.

The resulting leverage is multiplicative:

1. The emulator can expose complete raw state.
2. Form can tell it what the raw state means.
3. Stable source IDs can link state to code and assets.
4. traces can link state changes to operations and commands.
5. run provenance can link those operations to the development history that created them.
6. deterministic replay can reproduce the event.
7. timeline branching can test counterfactual fixes.
8. agents can perform all of this headlessly and in parallel.

That is not a conventional emulator with extra debug windows. It is a development substrate designed for synthetic engineering.

## 3. Repository footing: what already exists

The proposal is not starting from a blank page. The current repository already supplies strong foundations.

### 3.1 Existing and contracted

| Existing footing | Why it matters to ZEmu |
|---|---|
| ZRef is defined as the slow, scalar, exact oracle. | ZEmu has a semantic authority to remain bit-identical to. |
| ZEmu is defined as the usable desktop console and may parallelize exact work. | Optimization is allowed without permitting semantic substitution. |
| The current `emulator/zemu_main.cpp` is a deliberately empty Phase-1 shell. | There is no large legacy emulator architecture to fight; headless-first design can still be foundational. |
| The stub already consumes sealed frame packets through ZRef and returns meaningful process exit codes. | Structured, automatable replay is already the seed behavior. |
| Frame packets are immutable after sealing, CRC-protected, versioned, and source-ID-bearing. | Inputs to rendering and hardware work are reproducible and attributable. |
| `.zcap` carries resource pages, controller snapshots, framebuffer and tile CRCs, optional depth/stencil CRCs, counters, source maps, and first-divergence records. | The existing evidence format can remain the compact conformance artifact while richer whole-machine formats are added around it. |
| captures already include temporal celestial and environment state so replay can begin at an arbitrary frame boundary. | The project has already accepted the crucial law that consumed temporal state must be captured, not recreated by a warm-up ritual. |
| controller state is an atomic, deterministic `PadFrame` sampled at the frame boundary. | Replays and agent-injected input have one canonical representation. |
| counters are stable, append-only IDs, latched at frame boundaries, and capture-compatible. | Performance and fault observations already have a machine-readable vocabulary. |
| Form separates deterministic truth from degradable presentation. | ZEmu can expose truth and form independently and compare presentation variants without corrupting gameplay state. |
| Form source IDs and `sourceids.zmap` survive through commands, captures, field programs, and hardware traces. | Typed state and hardware events can resolve back to declarations and source spans. |
| Form’s `test` domain is intended to emit deterministic scenarios, captures, vectors, physical-hardware replay packages, and budget assertions. | Headless scenario execution is architecturally native rather than bolted on. |
| the Verilator harness already models the HPS side deterministically and hosts rings and arenas. | ZEmu can share a disciplined machine/host boundary and later substitute RTL blocks. |
| every agent session is a run, logged and archived; evidence and failed interpretations are retained. | Runtime evidence can be linked to development provenance. |

### 3.2 Not yet present

The current ZEmu stub does **not** yet provide:

- cartridge loading and a complete console loop;
- a whole-machine state schema;
- complete RAM/VRAM/register/FIFO/cache serialization;
- deterministic rewind and reverse execution;
- timeline forks;
- typed semantic state inspection;
- a stable headless agent protocol;
- conditional breakpoints over game semantics;
- last-writer or causal provenance;
- whole-run differential lockstep;
- automatic forensic bundles;
- bug-case minimization;
- a simulation farm or balance laboratory;
- integration with the run archive as queryable development provenance.

That is good news. These are easiest to make fundamental before the emulator accumulates hidden host state and GUI-driven assumptions.

## 4. Status vocabulary used in this treatise

To prevent an essay from quietly pretending to be a specification, recommendations are classified as follows:

- **EXISTS** — already implemented or explicitly contracted in the repository.
- **FOUNDATIONAL** — should be ratified before ZEmu grows substantially.
- **RECOMMENDED** — high-value design proposed for normal development.
- **ADVANCED** — powerful facility worth designing toward but not required for the first complete emulator.
- **EXPERIMENTAL** — speculative capability whose value should be proven by a prototype.

No proposed binary format, command name, file extension, or API method in this document is frozen merely because it appears here.

## 5. Non-negotiable laws for ZEmu

### 5.1 Headless is the primary architecture

The emulator core must never depend on a window, GPU context, audio device, desktop event loop, or human clicking a control. The desktop UI, web UI, trace viewer, and interactive debugger are clients.

A fresh agent process must be able to perform every consequential development operation without opening a window.

### 5.2 Exactness is explicit

Every execution mode declares its fidelity. An exact mode may not silently switch to a visually similar host-GPU renderer, approximate fixed-point math, noncanonical scheduler, or lossy state model.

Preview modes may exist, but every result must carry its mode prominently and machine-readably.

### 5.3 Every stateful influence is serializable or declared external

If restoring a snapshot cannot reproduce subsequent execution, some state was hidden. Host wall time, random devices, thread schedules, file timestamps, audio device state, asynchronous I/O, caches with semantic effects, and dynamically generated IDs must either be captured, deterministically virtualized, or prohibited from the canonical path.

### 5.4 Raw truth is always available

Semantic decoding must never replace access to bytes, registers, queues, and packets. A wrong decoder must be diagnosable by looking beneath it.

### 5.5 Semantic truth is first-class

Raw dumps are not a sufficient agent interface. The compiler and runtime should make state queryable as typed values, entities, components, pools, resources, commands, and source declarations.

### 5.6 Observation must not silently alter behavior

Counters, traces, watchpoints, provenance, and UI inspection must be out-of-band wherever possible. When instrumentation changes timing or execution, the mode must declare that fact, and timing conclusions from it must be rejected.

### 5.7 Failure creates evidence automatically

Assertions, crashes, deadline faults, divergent CRCs, illegal memory accesses, deterministic-hash mismatches, and agent-defined predicates should produce portable evidence without relying on a human remembering to press “save.”

### 5.8 Every mutation is transactional and recorded

Agent writes to memory, state, programs, resources, or configuration must occur through an explicit mutation transaction. The before state, requested change, resulting state hash, and rollback point are recorded. Hidden pokes are forbidden.

### 5.9 Host speed and console cost are different measurements

ZEmu may run a scene at 500 frames per second or 2 frames per second on a host. Neither number is the projected FPGA cost. Host elapsed time, modeled machine cycles, measured FPGA cycles, token budgets, and deadlines must be separate fields.

### 5.10 Evidence outranks prose

Run records are invaluable, but an agent-authored report never overrides a capture, test, trace, specification, or measured result. The emulator should link prose to evidence, not turn prose into machine truth.

---

# Part II — The whole machine as inspectable state

## 6. Define one canonical whole-machine state

The first architectural act should be to define what it means to freeze the complete console.

The canonical state should cover every component whose contents can affect future canonical output. At minimum:

### 6.1 Gameplay and HPS state

- Form `sim` state;
- all pools, arenas, free lists, generations, and stable entity identities;
- mission state, objective state, scripted timelines, and victory/failure state;
- AI and behavior state;
- physics, navigation, collision, and terrain-query caches where they affect semantics;
- deterministic random streams and draw positions;
- resource streaming state;
- command-builder state and pending semantic graph;
- audio mixer voices and timestamped events;
- canonical persistent terrain scars and gameplay material/hazard state;
- HPS-visible input snapshot and rumble state;
- virtual filesystem and save data used by the cartridge.

### 6.2 Local VRAM and hardware-visible memory

- all allocated local SDRAM pages;
- framebuffers, depth, stencil, auxiliary, glow, distortion, and history buffers;
- texture, palette, meshlet, clip, pose, terrain, surface-sheet, and microprogram pages;
- resource table and generation/epoch state;
- command, particle, upload, and hot-cache state where semantically relevant;
- dirty flags and residency metadata;
- guard-region state and violations.

### 6.3 Processor and device state

- architectural registers;
- program counters and call/return state;
- interrupts and pending events;
- timers and tick accumulators;
- DMA descriptors and progress;
- command decoder and scheduler state;
- memory-arbiter queues and credits;
- FIFO contents, read/write pointers, and CDC-visible state;
- field-engine contexts and program counters;
- pose, geometry, terrain, particle, raster, texture, compositor, scanout, input, and audio device state;
- counters and high-water marks;
- deadline/repeat state;
- trace trigger and ring state when trace continuity matters.

### 6.4 Host-virtualized state

- deterministic host clock;
- scheduled input events;
- mounted cartridge and resource identities;
- virtual file and persistence contents;
- pending asynchronous host operations represented as deterministic events;
- output sinks’ logical state where feedback can affect the program;
- emulator configuration and fidelity profile.

A useful test for completeness is brutal:

> Save at an arbitrary boundary, terminate the process, restore in a fresh process on another supported host, and produce byte-identical future state hashes, frame packets, displayed frames, audio samples, counters, and faults under the same input journal.

Anything required to satisfy that test belongs in the snapshot contract.

## 7. Give the machine a common time coordinate

Zhaozhou has several clocks and meaningful boundaries:

- Form simulation ticks;
- frame IDs and packet sequences;
- GPU/fabric cycles;
- video cycles and scanlines;
- vblank/frame ticks;
- audio sample positions;
- controller snapshot sequences;
- HPS runtime events;
- field-program instructions;
- command sequence numbers.

ZEmu should not flatten these into one vague “frame number.” It should maintain a **time coordinate** that can correlate them.

A proposed observation point could be represented as:

```text
machine_epoch:       12
sim_tick:            48821
frame_id:            48796
frame_packet_seq:    48796
fabric_cycle:        15549283721
video_scanline:      173
video_dot:           219
input_seq[0]:        48796
audio_sample:        39056800
last_event_id:       981552731
```

Not every fast execution mode must simulate every sub-cycle. It must, however, say which coordinates are exact, derived, estimated, or unavailable.

This shared coordinate enables questions such as:

- Which input snapshot first influenced this creature decision?
- Which simulation tick emitted the command that produced this pixel?
- How many modeled hardware cycles elapsed between input and display?
- Which audio sample corresponds to the foot-contact event at animation key 7?
- Did the terrain query use the scar state before or after the frame’s bake?

## 8. Layered fidelity modes

One emulator should support several intentionally distinct modes instead of forcing every task through the slowest model.

### 8.1 Fast functional mode — RECOMMENDED

Purpose: normal gameplay development, mission iteration, bot simulation, balance sweeps, and rapid agent work.

Characteristics:

- exact gameplay semantics;
- exact command and asset formats;
- bit-identical rendering to ZRef for implemented features;
- deterministic scheduling;
- optimized C++, SIMD, and parallel tiles/patches where exactness is preserved;
- modeled counters and costs;
- no claim of cycle accuracy.

### 8.2 Instrumented semantic mode — FOUNDATIONAL

Purpose: ordinary debugging.

Adds:

- typed state reflection;
- source-level breakpoints;
- event journal;
- last-writer maps;
- semantic diffs;
- assertions and invariant checks;
- bounded pre-history;
- automatic evidence bundles.

### 8.3 Transaction-accurate mode — ADVANCED

Purpose: memory, DMA, queue, command, and contention analysis without full RTL cost.

Adds:

- explicit requests, bursts, arbitration, queue occupancy, and modeled latencies;
- exact ordering at documented transaction boundaries;
- hardware-profile-specific deadline projection;
- deterministic contention injection.

### 8.4 Cycle-aware or cycle-accurate mode — ADVANCED

Purpose: first-divergence analysis and timing-sensitive integration.

This may be implemented by:

- a cycle model of selected blocks;
- Verilated RTL for selected blocks;
- or full Verilator co-simulation.

The mode must report the exact RTL and profile revisions used.

### 8.5 Physical-board shadow mode — ADVANCED

Purpose: compare the running FPGA to the emulator.

The emulator and board receive the same cartridge, frame packets, resources, input snapshots, and reset state. Bounded board traces, counters, CRCs, and memory windows are compared against the expected machine history.

### 8.6 Preview modes — OPTIONAL

A modern-GPU or approximate preview can exist for convenience, but it must be branded as a preview and excluded from conformance evidence. The current charter’s prohibition on silently switching renderers remains absolute.

## 9. Machine components should be substitutable

A high-leverage architecture is a graph of machine components behind exact contracts. For each block, ZEmu can select an implementation:

- optimized software;
- scalar ZRef;
- Verilated RTL;
- remote physical FPGA;
- deliberately mutated test implementation.

A single run might use:

```text
Form sim runtime       optimized native
Field engine           Verilated RTL
Terrain tessellation   ZRef scalar
Rasterizer             optimized exact software
Audio mixer            ZRef scalar
Scanout                 modeled software
```

This makes mixed-fidelity debugging possible. When a composed run diverges, replace half the optimized blocks with ZRef, rerun, and bisect the guilty subsystem. When RTL for one block matures, insert it without requiring the rest of the console to be RTL-ready.

Every substitution must preserve the same input/output contract and state serialization contract. The chosen implementation map is part of the run manifest and state hash identity.

## 10. Raw memory access: full, indexed, and safe

The full contents of all RAM should always be dumpable, but the interface should be richer than `dump 0x00000000 134217728`.

Required raw operations:

- enumerate memory domains and regions;
- read and write ranges;
- dump sparse or dirty pages;
- hash pages and ranges;
- compare ranges between states;
- find byte or word patterns;
- find values that changed during an interval;
- watch reads, writes, or execute accesses;
- report ownership and guard status;
- show last writer and last reader where instrumentation is armed;
- export canonical binary without presentation formatting.

Memory domains should be explicit:

```text
hps.ddr.world
hps.ddr.frame_ring
hps.ddr.trace_arena
local_sdram.textures
local_sdram.terrain
local_sdram.framebuffer.0
m10k.tile_store
m10k.pose_stage
registers.cmd_scheduler
fifo.field_requests
```

Addresses alone are not stable enough. Every region should also carry:

- stable region ID;
- owner block;
- allocation epoch;
- resource or pool identity;
- permissions;
- declared format;
- source/provenance references;
- semantic decoder identity;
- snapshot inclusion policy.

## 11. Semantic memory: make RAM explain itself

The deepest leverage comes from combining raw state with compiler-emitted meaning.

Form should eventually emit a versioned debug/reflection schema—working name `debug.zschema`—containing enough information to decode canonical state without guessing:

- modules and source files;
- type definitions;
- enums and variant tags;
- fixed-point Q formats and units;
- structs, fields, offsets, widths, signedness, and alignment;
- arrays, pools, capacities, live-bitmaps, generations, and free lists;
- structure-of-arrays component layouts;
- stable entity, resource, command, system, scenario, form, material, and program identities;
- state ownership and allowed writers;
- source IDs and spans;
- lifecycle rules;
- declared invariants;
- resource-handle schemas;
- optional display hints;
- program hashes and layout hashes.

Then an agent can ask for:

```text
world.creatures[id=19].brain.mode
world.creatures[id=19].animation.clip
world.creatures[id=19].ground.front_left.height
terrain.patch[id=81].scar_delta[cell=(14,22)]
render.frame[id=48821].view[0].lod_choice[entity=19]
audio.voice[id=7].source_event
```

The response should include both meaning and storage:

```json
{
  "path": "world.creatures[id=19].brain.mode",
  "type": "BrainMode",
  "value": "Pursue",
  "raw": 3,
  "domain": "hps.ddr.world",
  "address": "0x0018A43C",
  "source_id": "0x81230017",
  "owner": "system creature_decision",
  "last_write": {"event": 981552112, "tick": 48820},
  "state_hash": "..."
}
```

The semantic layer must be generated from the same compiler truth that lays out the state. Hand-maintained debugger structs will drift and become worse than raw bytes.

## 12. Stable identities everywhere

Time-travel and semantic diffs become dramatically more useful when identities survive movement and compaction.

Recommended stable identities include:

- entity ID plus generation;
- pool-slot ID plus generation;
- resource handle plus epoch;
- command sequence and source ID;
- spell instance ID and cast sequence;
- terrain patch/cell identity;
- scar/stamp/field operation ID;
- primitive and draw instance ID;
- animation clip/key/event ID;
- audio event/voice ID;
- mission objective and trigger ID;
- timeline branch ID;
- emulator event ID.

A semantic diff should say “creature 19 changed target from 443 to 512,” not “bytes at two unrelated addresses differ.”

## 13. State hashes at several levels

One global hash is useful but insufficient. ZEmu should emit a hierarchy of canonical hashes:

- complete whole-machine hash;
- gameplay-truth hash;
- presentation-state hash;
- HPS state hash;
- VRAM state hash;
- per-device hash;
- per-pool hash;
- per-entity hash;
- per-terrain-patch hash;
- frame-packet hash;
- displayed-frame hash;
- audio-block hash;
- configuration/profile hash.

Hierarchical hashes turn “the run diverged” into “gameplay truth remained identical; presentation first diverged in terrain patch 81; the raster output diverged two stages later.”

Hashes must have canonical iteration order, include schema and version identities, and exclude explicitly nonsemantic host caches.

---

# Part III — Time travel, replay, and branching universes

## 14. Snapshot architecture

A snapshot should be a complete restorable state, not merely a save game.

### 14.1 Full snapshots

A full snapshot contains or content-addresses every state page and device record required for exact continuation. It should be usable as a standalone forensic artifact when configured to embed its dependencies.

### 14.2 Incremental snapshots

Normal development needs frequent snapshots without writing hundreds of megabytes per frame. Use:

- fixed-size pages;
- dirty-page tracking;
- copy-on-write storage;
- content-addressed deduplication;
- periodic full checkpoints;
- intervening deltas;
- compression selected per page type;
- deterministic canonical serialization.

### 14.3 Snapshot boundaries

The system should support snapshots at several legal boundaries:

- reset complete;
- simulation tick boundary;
- frame packet sealed;
- frame boundary/vblank;
- command boundary;
- field-program instruction boundary;
- device-defined quiescent point;
- arbitrary cycle in cycle-aware mode.

A boundary declares which in-flight state must be included. Arbitrary-cycle snapshots are more expensive but invaluable for hardware divergence.

### 14.4 State identity

Every snapshot should include:

- format version;
- machine/spec version;
- emulator build identity;
- cartridge hash;
- Form compiler and schema identity;
- resource manifest;
- hardware profile;
- component implementation map;
- parent snapshot and branch identity;
- exact time coordinate;
- whole and hierarchical hashes;
- instrumentation mode;
- external dependency policy.

## 15. Input and event journals

A deterministic replay should consist of:

1. an initial snapshot or reset manifest;
2. a chronological journal of nondeterministic inputs and explicitly external events;
3. the exact executable/configuration identities.

The journal should record more than controllers when relevant:

- canonical `PadFrame` snapshots;
- debug/agent mutations;
- virtual file changes;
- hot-reload events;
- cartridge/resource swaps;
- host service results that are allowed in canonical execution;
- physical-board trace sync events;
- explicit pauses or step commands only when they alter machine-visible time.

Events generated deterministically inside the game do not need to be recorded to reproduce the run; they may still be traced for explanation.

## 16. Reverse execution

Reverse execution can be implemented without requiring every operation to be mathematically reversible:

- retain dense recent checkpoints;
- retain an event journal;
- when stepping backward, restore the nearest prior checkpoint and replay forward;
- optionally maintain inverse deltas for short windows;
- cache frequently visited timeline positions.

Required user/agent operations:

- reverse one event;
- reverse one instruction;
- reverse one simulation tick;
- reverse one frame;
- rewind to the last write of a semantic path;
- rewind to the last time an invariant was true;
- rewind to the first time two states diverged;
- rewind to entity creation, target acquisition, terrain impact, or command emission.

## 17. Timeline branching as a first-class object

A restored snapshot should be forkable. Each fork becomes a branch in a content-addressed history DAG.

Example:

```text
branch A: original creature behavior
branch B: new pursuit law
branch C: old behavior, spell radius -10%
branch D: renderer variant for mission 17
```

Branches share unchanged snapshot pages and resources. Each records:

- parent state hash;
- mutations or alternate inputs;
- component implementation changes;
- outputs and result metrics;
- agent or run that created it;
- human judgement or promotion status.

This transforms debugging and design. A developer no longer remembers that “the old camera felt better.” Both cameras can be replayed over the exact same battle, creature motion, terrain destruction, and input sequence.

## 18. Run-until predicates

Headless development becomes powerful when the machine can run until a semantic condition rather than for an arbitrary number of seconds.

Examples:

```text
run until sim.tick == 1200
run until mission.status != Running
run until any creature.ground_state == Invalid
run until terrain.patch[81].revision changes
run until counter.deadline_faults > 0
run until write(world.creatures[19].brain.target)
run until pixel(view=0,x=133,y=91) differs from baseline
run until zref != zemu
run until audio.underruns > 0
run until no progress for 600 ticks
```

Predicates should be compiled against the semantic schema where possible and rejected if they are ambiguous or refer to stale layouts.

## 19. Binary search for first divergence

Given two deterministic executions with checkpoints, ZEmu should automatically find the first differing point:

1. compare end-state hierarchical hashes;
2. binary-search checkpoints;
3. replay within the smallest differing interval;
4. compare events or instructions;
5. identify the first differing component, field, command, primitive, pixel, or sample;
6. resolve it to source ID and run provenance;
7. save a minimal divergence bundle.

The output should distinguish:

- first **machine-state** divergence;
- first **gameplay-visible** divergence;
- first **command-stream** divergence;
- first **pixel/audio** divergence;
- first **deadline/timing** divergence.

A visible error may occur thousands of cycles after the causal state first changed. Reporting only the first wrong pixel leaves most of the forensic work undone.

## 20. Determinism auditor

ZEmu should be able to run the same job repeatedly while perturbing host conditions that must not affect canonical output:

- thread counts;
- task scheduling orders;
- allocation addresses;
- hash-map seed/order;
- SIMD width or exact backend;
- tile/patch parallel work order;
- host CPU architecture where supported;
- debug logging enabled/disabled;
- snapshot cadence;
- UI attached/detached.

Any canonical hash difference is a determinism defect. The auditor should produce the first-divergence case automatically.

---

# Part IV — Observation, causality, and query

## 21. Observation levels

ZEmu should expose the same event at several levels so agents can move from meaning to bytes without losing the link.

1. **Raw:** bytes, words, registers, queues, addresses.
2. **Typed:** decoded structs, enums, fixed-point values, pools, handles.
3. **Semantic:** creatures, spells, missions, terrain, commands, forms, materials.
4. **Derived:** diffs, invariants, costs, causal chains, last writer, ownership.
5. **Presented:** frames, overlays, audio, contact sheets, plots, reports.

An agent can begin with “Why did creature 19 fall?” and descend into the exact terrain sample, memory write, Form source span, fixed-point value, and hardware transaction.

## 22. A structured query language

The emulator should provide a small structured query language or schema-backed expression system. It should not require agents to parse arbitrary prose output.

Illustrative queries:

```text
select creature where faction == Red and brain.mode == Pursue
show world.creatures[19] depth 3
history world.creatures[19].brain.target from tick 48000 to 48900
writes terrain.patch[81].scar_delta[cell=(14,22)]
explain render.pixel(view=0,x=133,y=91)
diff snapshot A snapshot B scope gameplay
find handles where stale == true
find entities where position.y < terrain.height(position.xz)
```

Each query response should carry:

- schema version;
- state hash;
- time coordinate;
- bounded result count;
- truncation status;
- stable paths/IDs;
- raw references;
- source references;
- artifact references for large data.

## 23. Breakpoints and watchpoints

Breakpoints should be possible on:

- source location or source ID;
- machine instruction or Field IR program counter;
- command opcode, sequence, or source;
- memory read/write/execute;
- typed field change;
- entity event;
- AI decision;
- animation clip, frame, or event tag;
- terrain cell/patch mutation;
- resource load/eviction;
- primitive, tile, pixel, depth, or blend event;
- audio event, voice, sample range, or underrun;
- input snapshot or latency threshold;
- counter threshold or high-water mark;
- assertion/invariant failure;
- deadline repeat;
- ZRef/ZEmu/RTL divergence;
- custom agent predicate.

Triggers should support pre-roll and post-roll capture windows.

## 24. Last-writer tracking

The first useful causal facility is a last-writer map.

For selected memory or semantic state, retain:

- last event ID;
- time coordinate;
- writer component/system;
- source ID and source span;
- previous value;
- new value;
- owning entity/resource/operation;
- timeline branch.

Then `why value?` can immediately answer the operational question “what most recently assigned it?”

This can be always-on for coarse semantic state and dynamically armed for expensive memory ranges.

## 25. Write journals and value history

For a bounded interval or selected state path, record every change rather than only the last one.

Useful operations:

- show all target changes for creature 19;
- show every terrain operation that affected one cell;
- show every resource-generation transition for handle 0x…;
- show the sequence of pose-cache writes for a tuple;
- show all writes contributing to a command packet;
- show every change to an objective’s completion predicate.

The journal should be event-typed and source-resolved, not merely a sequence of addresses.

## 26. Causal provenance modes

Operational causality can be recorded at several costs.

### 26.1 Source/event provenance — RECOMMENDED

Attach stable source IDs and parent event IDs to meaningful events:

- spell cast creates terrain field;
- terrain field creates scar and velocity;
- animation event emits impact;
- impact emits audio and particles;
- AI decision changes target;
- presentation declaration emits command;
- command emits primitives;
- primitive writes pixels.

### 26.2 Last-writer provenance — RECOMMENDED

Track the last writer of selected state and memory.

### 26.3 Dynamic taint/dependency tracking — ADVANCED

For a targeted investigation, tag values and propagate compact provenance through selected operations. This can answer:

- Which input axis influenced this camera coordinate?
- Which spell parameters influenced this terrain height?
- Which Form state values influenced this pixel’s material choice?
- Which entity and light influenced this final color?

This is expensive and need not run globally. It should be dynamically scoped to a state slice, time interval, block, source ID, entity, tile, or pixel.

### 26.4 Provenance honesty

The emulator should distinguish:

- directly recorded parentage;
- data dependency;
- control dependency;
- inferred semantic relationship;
- mere temporal correlation.

“Causal” must not become a confident label for a guess.

## 27. Explain commands

Every major queryable object should support an `explain` operation.

Examples:

```text
explain creature 19 target
explain terrain cell 81:14,22
explain lod entity 19 view 0
explain pixel 0:133,91
explain deadline frame 48821
explain audio voice 7
explain stale handle 0x1200002A
```

An explanation should assemble evidence, not generate free-form mythology. It should include:

- current value/state;
- relevant inputs;
- decision rule or operation;
- source locations;
- recent causal events;
- counters and constraints;
- raw data references;
- uncertainty or missing trace explicitly.

A language model can turn this evidence into prose afterward. The emulator’s job is to make the evidence complete and structured.

## 28. Semantic diffs

A useful diff hierarchy:

- raw page/byte diff;
- typed field diff;
- entity/component diff;
- terrain patch and scar diff;
- mission/objective diff;
- command-graph diff;
- selected LOD/material/representation diff;
- primitive/tile/pixel diff;
- audio event/sample diff;
- counter and deadline diff;
- development-provenance diff.

The diff engine should support filters:

```text
ignore presentation
ignore cosmetic particles
ignore caches
only gameplay truth
only entity 19 and causal dependencies
only frames 48000..48100
only source IDs under spells/upheaval.form
```

## 29. Invariants as executable observability

Invariants should be registerable from Form `test`, ZSpec, block contracts, or emulator-only debug configuration.

Examples:

- no live creature below terrain except declared authored penetration;
- every damaging projectile has a gameplay entity;
- presentation never writes truth;
- one writer per component phase;
- all resource handles match their epochs;
- no command consumes unsealed data;
- no frame is partially displayed;
- a deadline fault repeats the last complete frame;
- no player receives less than its guaranteed Duo budget;
- a pose-cache tuple decodes identically regardless of request order;
- no surface stamp writes outside its patch;
- every mission objective remains reachable or explicitly failed;
- every active slinghook endpoint references a live surface/attachment;
- every terrain-class giant body patch remains rigid plus uniform scale.

When an invariant fails, ZEmu should capture the earliest failure, pre-history, state slice, and reproducer automatically.

---

# Part V — Headless and fully agent-controlled operation

## 30. Core architecture: library first, daemon second, UI third

A recommended product split:

### 30.1 `zemucore`

A deterministic library containing:

- machine composition;
- component contracts;
- clocks and scheduler;
- state serialization;
- snapshot/replay;
- trace and query hooks;
- exact rendering and audio paths;
- cartridge/resource loading;
- no GUI or OS-device assumptions.

### 30.2 `zemu`

A CLI for one-shot jobs and interactive terminal use.

### 30.3 `zemu serve`

A long-running local daemon hosting isolated sessions, accepting structured requests, streaming events, and managing artifacts.

### 30.4 graphical clients

Desktop UI, web trace viewer, timeline debugger, art review viewer, and profiler connect through the same control/query protocol used by agents.

This prevents the UI from acquiring secret powers unavailable to automation.

## 31. Transport and protocol

The simplest robust first transport is newline-delimited JSON over stdin/stdout for one-shot or persistent child-process control. It is easy to launch, sandbox, record, and test.

Later transports can include:

- local domain socket or named pipe;
- HTTP/WebSocket for UI clients;
- gRPC or another binary stream for high-volume traces;
- a tool-protocol adapter for coding agents.

All transports should share one versioned logical API and JSON Schema or equivalent machine-readable definitions.

## 32. Session model

Every emulator session should have:

- session ID;
- immutable launch manifest;
- current state hash;
- current time coordinate;
- branch ID;
- cartridge and resource identities;
- component implementation map;
- fidelity/instrumentation mode;
- resource limits;
- artifact directory;
- event stream;
- audit log of mutations and control commands.

Agents should be able to create, clone, pause, query, fork, and destroy sessions independently.

## 33. Minimum agent control surface

### Lifecycle

- create session;
- load cartridge;
- load/reset snapshot;
- cold reset, warm reset, device reset;
- start, pause, resume, stop;
- destroy session.

### Execution

- step cycle;
- step instruction;
- step event;
- step simulation tick;
- step frame;
- run for count/duration in a declared time domain;
- run until predicate, event, breakpoint, fault, or divergence;
- run at maximum speed with presentation/audio optionally captured but not played.

### Input and environment

- inject canonical `PadFrame` snapshots;
- play an input journal;
- schedule future input events;
- mount virtual files/save data;
- change explicitly mutable environment state;
- select hardware timing profile;
- configure controlled fault injection.

### Observation

- query raw memory;
- query typed/semantic state;
- subscribe to events;
- install breakpoints/watchpoints/invariants;
- request counters and timelines;
- request screenshots, buffers, audio, command graphs, and traces;
- request source/provenance resolution;
- request explanation or diff evidence.

### State operations

- save snapshot;
- restore snapshot;
- fork branch;
- export replay;
- compare branches;
- begin/commit/rollback mutation transaction;
- hot-reload compatible code/assets;
- patch state explicitly for an experiment.

### Evidence

- generate `.zcap` from a frame;
- generate whole-machine forensic case;
- minimize failure;
- package relevant sources, schemas, traces, images, and run provenance;
- emit deterministic summary and exit code.

## 34. One-shot job manifests

Agents should be able to submit a complete deterministic job as one file.

Illustrative, not frozen:

```json
{
  "schema": "zemu.job.v1",
  "machine": {
    "fidelity": "instrumented_exact",
    "hardware_profile": "sim-conservative-v2",
    "components": {"FIELD.EXECUTOR": "verilator", "RASTER.FRAGMENT": "zref"}
  },
  "cartridge": {"path": "build/upheaval.zpak", "sha256": "..."},
  "start": {"snapshot": "cases/tower-collapse-before.zstate"},
  "inputs": {"journal": "cases/tower-collapse.zinput"},
  "breakpoints": [
    {"kind": "invariant", "name": "hook_endpoint_live"},
    {"kind": "predicate", "expr": "counter.deadline_faults > 0"}
  ],
  "run": {"until": "breakpoint || sim.tick == 90000"},
  "observe": {
    "semantic": ["world.player[0]", "hooks[*]", "terrain.patch[81]"],
    "buffers": ["framebuffer", "depth"],
    "trace": {"channels": ["physics", "terrain", "render"], "pre_events": 2000}
  },
  "artifacts": {"directory": "out/case-447", "forensic_bundle": true},
  "limits": {"host_seconds": 120, "events": 5000000, "artifact_bytes": 2000000000}
}
```

The manifest itself becomes a receipt and is embedded in the result bundle.

## 35. Deterministic structured responses

Every command response should contain stable fields rather than prose alone:

```json
{
  "request_id": "r-1881",
  "ok": true,
  "session_id": "s-47",
  "state_hash": "zhstate:...",
  "time": {"sim_tick": 48821, "frame": 48796, "fabric_cycle": 15549283721},
  "result": {"reason": "breakpoint", "breakpoint_id": "bp-12"},
  "artifacts": [{"kind": "snapshot", "path": "...", "sha256": "..."}],
  "warnings": [],
  "truncated": false
}
```

Do not mix progress chatter into the protocol stream. Human logs can go to stderr or a separate event channel.

## 36. Exit codes matter

One-shot jobs need predictable exit codes, for example:

- 0 — completed and all requested assertions passed;
- 1 — game/emulator assertion or expected comparison failed;
- 2 — malformed command/job/arguments;
- 3 — cartridge/schema/version incompatibility;
- 4 — emulator internal error;
- 5 — resource limit exceeded;
- 6 — nondeterminism detected;
- 7 — ZRef/ZEmu/RTL/hardware divergence;
- 8 — requested evidence incomplete.

Exact allocation is a later specification decision. The law is that agents and CI must not scrape English to know whether a run succeeded.

## 37. Agent permissions and safety

An agent-controlled emulator is intentionally powerful. Use capabilities:

- read state;
- control execution;
- inject input;
- mutate state;
- hot-reload code;
- access host files;
- attach physical hardware;
- publish artifacts.

Default sessions should be sandboxed, local, networkless, and unable to access arbitrary host files. Mutation and physical-hardware control should require explicit capability grants. Every side effect is audited.

## 38. Bounded output and context-aware extraction

Full RAM may be hundreds of megabytes; complete traces may be gigabytes. Agents should never be forced to ingest all of it blindly.

Every query supports:

- scope;
- time range;
- result limit;
- field selection;
- raw versus semantic depth;
- aggregation;
- stable continuation token;
- artifact spill for large data;
- explicit truncation.

The system should produce three standard evidence sizes:

- **compact** — enough for initial triage;
- **standard** — state slice, recent history, relevant source, and visual evidence;
- **exhaustive** — full snapshot, journals, traces, schemas, and dependencies.

## 39. Agent “slurp packs” / forensic bundles

A portable failure or investigation bundle—working extension `.zcase`—should contain:

- deterministic job/reproduction manifest;
- starting snapshot or content-addressed references;
- input/external-event journal;
- failure snapshot and optional post-roll;
- whole and hierarchical state hashes;
- semantic state slice;
- raw memory ranges relevant to the slice;
- recent event timeline;
- last-writer/provenance records;
- source map and debug schema;
- cartridge/resource/compiler/emulator identities;
- frame packet and `.zcap` where applicable;
- screenshots and selected intermediate buffers;
- audio excerpt where applicable;
- ZRef/ZEmu/RTL comparison;
- invariant or breakpoint definition;
- exact CLI command to reproduce;
- relevant source excerpts or immutable source references;
- links/identities for relevant run records, specifications, reports, and commits;
- machine-readable summary plus a concise human summary.

The compact bundle should be sized so an agent can consume it immediately. The full evidence remains available by content hash.

## 40. Agents should be able to ask for more evidence, not restart blindly

A forensic bundle should expose a session or restorable state from which the investigating agent can issue follow-up requests:

```text
show the previous 50 writes to this field
expand terrain patch 81 to include live fields
trace only source_id 0x9A310022 for 600 events
fork before event 981552112 and patch target selection
compare rendered pixel provenance after the patch
```

This creates an iterative scientific instrument instead of a static crash dump.


---

# Part VI — Domain-specific observatories

## 41. Renderer observability: explain a pixel from declaration to wire

The renderer is where ordinary debugging most often collapses into screenshots and guesses. ZEmu can do much better because commands, programs, resources, primitives, and source IDs are owned.

For any displayed pixel, ZEmu should be able to produce a **pixel genealogy**:

1. displayed frame and repeat decision;
2. viewport and scanout coordinate;
3. compositor/post passes that touched it;
4. winning color/depth/stencil sample;
5. blending history and prior destination values;
6. fragment attributes and material recipe;
7. texture, palette, surface-sheet, light, fog, and effect inputs;
8. primitive and triangle identity;
9. clipping/setup/binner outputs;
10. source meshlet, terrain patch, particle, or procedural primitive;
11. draw/semantic command and command sequence;
12. Form source declaration and source span;
13. gameplay entity, spell, terrain operation, or mission object responsible.

An agent should be able to ask:

```text
explain pixel(view=0, x=133, y=91)
compare pixel 0:133,91 branch A branch B
break when any fragment from source_id X writes tile 48
show every contributor to glow buffer at 0:66,45
```

### 41.1 Stage captures

On demand, capture:

- command graph;
- vertex output;
- clipped primitives;
- tile lists;
- coverage masks;
- interpolants;
- texture addresses/samples;
- depth and stencil decisions;
- blend inputs/outputs;
- fog and lighting terms;
- framebuffer, depth, stencil, glow, distortion, outline/motion, and history buffers;
- scanout and repeated-frame result.

These should be selectable and bounded. Capturing every fragment of every frame is a special mode, not the default.

### 41.2 Tile and primitive minimization

When a frame diverges:

- identify the first different tile;
- identify primitives that can affect that tile;
- replay only the necessary command/resource subset where possible;
- minimize to the smallest primitive/material/resource case preserving the pixel difference;
- emit a conventional unit test and a `.zcap`/`.zcase`.

The existing first-divergence `.zcap` record is an excellent nucleus. ZEmu should extend the analysis backward to the earliest causal state.

### 41.3 LOD explanation

For every selected representation, explain:

- projected error;
- semantic feature weights;
- view-local requirements;
- hysteresis state;
- geometry/fragment tokens;
- shared and per-player budget;
- competing work that consumed the budget;
- previous representation;
- exact rule that selected the new representation;
- source lines declaring the ladder and priorities.

This lets an agent answer “Why did the enemy become a glint in the left view but a mesh in the right?” without reverse-engineering Measure state from screenshots.

### 41.4 Custom renderers and one-mission art laws

ZEmu should actively support bounded alternate rendering paths for specific missions or gameplay islands.

A custom renderer can be treated as a component implementation or presentation profile with:

- explicit scope and activation law;
- exact input contracts;
- its own source IDs and debug schema;
- pinned snapshots and camera states;
- a dedicated capture corpus;
- output and performance comparison against the ordinary path;
- a clean ability to disable or remove it.

The emulator’s branch system makes such experiments cheap: replay the same complete scene through ordinary and custom renderers, compare every intermediate, and keep only the version that earns its existence.

The correct guardrail is not “never build a renderer for one mission.” It is:

> **Make the renderer inspectable, bounded, reproducible, and separable enough that one mission can afford to own it.**

## 42. Terrain and destruction observatory

Zhaozhou’s terrain is layered state, not one height buffer. ZEmu should expose those layers separately and together.

For a selected world point, cell, subpatch, or patch, show:

- authored base height;
- persistent scar delta;
- each active Earth/field contribution;
- final height and vertical velocity;
- normal and slope;
- base material candidates and blend weight;
- surface-sheet tag, strength, age, and material conversion;
- heat, wetness, corruption, hazard, and movement cost;
- void/cliff/wound/topology masks;
- selected tessellation resolution and stitch pattern;
- dirty navigation state;
- resident resource pages and epochs;
- all operations that contributed to the current value.

### 42.1 Terrain history

For any point or region, answer:

- which spell, stamp, creature impact, authored event, or bake changed it;
- when the change began and ended;
- which values were live versus baked;
- what the terrain looked like immediately before and after;
- which units were standing on it;
- whether navigation and collision were invalidated and rebuilt;
- which source declarations emitted each operation.

### 42.2 Counterfactual terrain

From one battle snapshot:

- remove one terrain operation and replay;
- change a field parameter;
- replace a field program implementation;
- delay or accelerate a persistent bake;
- fork with different topology/wound policy;
- compare unit movement, mission reachability, rendering, and hardware cost.

### 42.3 Deformable buildings and terrain-class structures

When skyscrapers or giants are assembled from transformed terrain bodies, ZEmu should expose the **attachment and glue graph**:

- body-patch identity;
- parent transform and local/world conversion;
- seams and constraints;
- load/strain if modeled;
- collision and navigation surfaces;
- deformation operations in local coordinates;
- detached fragments and their new identities;
- surface-sheet continuity;
- rendering/tessellation ownership.

Agents should be able to select a bent tower section and ask exactly which terrain operations, constraints, and transforms produced its current shape.

### 42.4 Terrain visual lenses

Generate overlays for:

- base versus scar versus live deformation;
- height velocity;
- slope;
- hazard/nav cost;
- active field footprints;
- surface-sheet age/strength;
- tessellation error and resolution;
- stitch boundaries;
- dirty navigation cells;
- source ID ownership;
- hardware sample count and cost.

## 43. Creature, animation, and pose observatory

The first creature pipeline already makes rich animation and lighting central. The emulator can make creature development far more exact without letting measurements replace artistic judgement.

For a creature instance, expose:

- stable entity/type identity;
- simulation state and behavior mode;
- target and threat candidates;
- position, orientation, scale, velocity, ground state, and slope tilt;
- active clip, keyframe, clip clock, event tags, and transition reason;
- decoded pose-cache tuple and cache provenance;
- every bone’s local/world transform;
- attachment sockets and reparented children;
- vertex influences and final posed positions on request;
- hitboxes and gameplay collision;
- authored ground-penetration declaration versus measured 3D penetration;
- current LOD representation per view;
- lighting/material inputs and final render source;
- sounds, particles, stamps, and terrain fields emitted by animation events.

### 43.1 Every-frame animation review

Headlessly generate:

- contact sheets containing every frame;
- orthographic and gameplay-camera renders;
- before/after synchronized comparisons;
- tracked point trajectories;
- bone and socket trajectories;
- root motion and velocity plots;
- ground-contact/penetration report from posed 3D vertices;
- event-tag timeline;
- silhouette occupancy over time;
- “badness-selected” frames based on declared probes, not only evenly spaced samples.

The project’s art law remains: the owner judges the art by looking. Measurements and probes belong on the comparison side. ZEmu should make looking at the right evidence effortless.

### 43.2 Pose-to-pixel inspection

Select a bad pixel or silhouette and trace it back to:

- triangle;
- source vertex;
- posed vertex;
- bone influences;
- decoded quaternion/matrix;
- clip frame;
- authored part/ring/mesh source;
- lighting/material decision.

This shortens the path from “the snout looks wrong in frame 17” to the exact transform or geometry that made it wrong.

### 43.3 Creature riding, carrying, grabbing, and reparenting

For reparented objects, show:

- old and new parent;
- keep-world-transform calculation;
- attachment socket;
- generation/epoch checks;
- collision ownership;
- rider/control-transfer state;
- animation and camera coupling;
- exact event that caused reparenting.

This will be especially valuable for mount missions, giant interactions, and gameplay islands whose movement rules change radically.

## 44. AI and behavior observatory

AI debugging should not be limited to reading current state. Record the decision surface.

For each consequential decision, optionally retain:

- candidate actions/targets;
- sensed facts and queries;
- filtered-out candidates and reasons;
- scores/utility values;
- deterministic tie-breaking;
- random stream and draw index where randomness participates;
- chosen action;
- source location of the rule;
- previous decision and hysteresis/cooldown state;
- downstream command or motion consequences.

Then an agent can ask:

```text
why did creature 19 choose target 443?
why did it not defend its wizard?
show every candidate rejected because navigation said unreachable
fork this state and evaluate three targeting laws for 1,000 seeds
```

### 44.1 Behavior alternatives as plug-in branches

Because snapshots are complete, a new behavior implementation can be hot-swapped at a decision boundary and replayed over the exact same battle. Compare:

- survival;
- damage;
- objective contribution;
- time idle;
- path failures;
- player readability;
- hardware/runtime cost;
- qualitative clips selected for human review.

ZEmu should rank and package interesting or pathological runs, not merely average them away.

## 45. Physics, traversal, and slinghook observatory

Gameplay islands involving parkour, flying, riding, hooks, deforming skyscrapers, and moving terrain require unusually good state visibility.

Expose:

- controller/input buffer and exact consumption tick;
- locomotion mode and transition reason;
- desired and actual velocity;
- contact manifolds and surface IDs;
- collision normals and penetration corrections;
- grounded/coyote/jump-buffer state;
- motion root/animation contribution;
- camera-relative transformations;
- slinghook endpoints, constraints, rope length, tension, impulses, and break conditions;
- moving/deforming anchor provenance;
- traversal affordance queries;
- predicted and realized trajectory;
- failure/softlock state.

Useful triggers:

```text
break when hook anchor loses its surface generation
break when player becomes unreachable from any declared checkpoint
break when correction impulse exceeds threshold
break when the same jump input produces divergent grounded state
```

### 45.1 Trajectory laboratory

From a snapshot just before a jump or swing:

- sweep input timing and direction;
- vary one physics parameter;
- run thousands of deterministic branches headlessly;
- map reachable landing regions;
- identify impossible or accidental routes;
- preserve representative success/failure trajectories;
- render only selected branches for human judgement.

This makes spectacular traversal tunable rather than fragile.

## 46. Mission and gameplay-island observatory

A mission should be capturable as a set of meaningful states, not only as a level file.

Recommended mission-development artifacts:

- start-of-mission snapshot;
- named checkpoint snapshots;
- snapshots before every major set piece or rule mutation;
- deterministic input/bot journals for expected paths;
- objective/trigger graph;
- reachability and softlock invariants;
- budget profiles;
- representative visual captures;
- known exploit and chaos cases;
- relevant run provenance.

### 46.1 Micro-scenes

Any interesting instant can become a reusable micro-scene:

- before a tower bends;
- before mounting a creature;
- before entering flight;
- before two terrain waves collide;
- before a boss transforms;
- before a slinghook attaches to moving terrain;
- before a thousand-shard spell;
- before a mission-specific renderer activates.

A micro-scene is a snapshot plus a narrow script of expected interactions. It can serve as:

- mechanic prototype;
- regression fixture;
- performance benchmark;
- animation review scene;
- agent task seed;
- art-style comparison;
- trailer shot reproducer.

### 46.2 Objective and trigger explanation

For an objective, show:

- current state;
- predicates and dependencies;
- last transition;
- events considered;
- source declarations;
- whether completion remains reachable;
- which destruction, entity death, or topology change invalidated a path;
- why a trigger did or did not fire.

### 46.3 Mission fuzzing

Agents can headlessly vary:

- input sequences;
- unit tactics;
- spell order;
- destruction locations;
- entity deaths;
- objective timing;
- resource abundance;
- seeds;
- unusual traversal routes.

Goals:

- find softlocks;
- bypass gates;
- destroy required objects too early;
- strand units through terrain change;
- trigger phases out of order;
- produce unwinnable but unfailed states;
- discover unexpectedly excellent alternate routes.

Not every deviation is a bug. The output should separate violated invariants from emergent possibilities worth preserving.

## 47. Audio observatory

Audio state should be as replayable and attributable as graphics.

Expose:

- timestamped source event;
- Form source ID;
- sample/bank identity;
- voice allocation/stealing decision;
- envelope, gain, pitch, pan, attenuation;
- bus routing;
- reverb/delay state;
- PCM ring state;
- FIFO state and underrun/repeat behavior;
- final sample contribution per voice for selected ranges.

Headless outputs:

- exact WAV/PCM range;
- per-bus stems;
- per-voice stem for selected events;
- event-to-sample latency;
- underrun trace;
- branch comparison.

A selected audio sample should support an explanation analogous to a pixel genealogy.

## 48. Input and end-to-end latency observatory

The canonical `PadFrame` already provides deterministic input. ZEmu should correlate an input edge through the entire machine:

```text
PadFrame sequence
→ Form input read
→ gameplay state transition
→ animation/command emission
→ frame packet
→ rendering
→ displayed frame decision
→ first changed pixel
```

Report latency in:

- simulation ticks;
- frames;
- modeled cycles;
- measured FPGA cycles when available;
- host wall time separately.

For controls, camera, hooks, riding, and combat, this may become one of the most useful game-feel instruments in the project.

## 49. Performance and deadline observatory

The emulator must make the 60 Hz law visible without pretending host execution time equals hardware time.

### 49.1 Counter timeline

Record and correlate:

- frame cycles and deadline faults;
- command counts/bytes;
- geometry/fragment/terrain/particle work;
- tile-list depths and overflows;
- texture/cache behavior;
- VRAM and HPS-DDR traffic by client;
- arbitration waits and queue occupancy;
- field instruction/service utilization;
- pose-cache hits/misses;
- LOD selection and degradation;
- audio/input faults;
- source IDs responsible for costs.

### 49.2 Explain a deadline

For a late or near-late frame, answer:

- which work classes consumed cycles/tokens;
- which source declarations emitted them;
- which view/player consumed guaranteed/shared budgets;
- which queues or memory clients stalled;
- whether the frame repeated;
- which degradation rules fired or failed to fire;
- what the same frame costs under other measured hardware profiles;
- what a proposed optimization changes while preserving output.

### 49.3 Profile families

Run the same capture against:

- conservative simulation profile;
- measured typical board profile;
- measured worst-case board profile;
- intentionally degraded memory profile;
- future reduced-capacity hardware profile.

This makes “the console will probably fit/work” a replayable set of evidence rather than one estimate.

### 49.4 Static and dynamic cost reconciliation

Form’s `costs.zcost` supplies static ceilings and source attribution. ZEmu supplies dynamic observations. The profiler should compare them:

- declared versus observed population;
- declared field max ops versus executed ops;
- estimated versus observed triangles/fragments;
- expected versus observed bandwidth;
- declared versus actual degradation order;
- budget assertions versus runtime traces.

A mismatch may indicate compiler-model error, content unexpectedly reaching a worst case, or emulator/hardware counter error.

---

# Part VII — Verification across software, RTL, and physical hardware

## 50. Lockstep hierarchy

A complete verification chain should support comparison at several granularities.

### 50.1 ZRef versus ZEmu

Compare:

- function/block outputs;
- device state transitions;
- frame packets;
- whole-machine state hashes;
- frames/tiles/pixels;
- audio blocks;
- counters.

Optimized ZEmu paths must remain replaceable with scalar ZRef paths for isolation.

### 50.2 ZEmu versus Verilated blocks

Feed identical transactions and compare at block boundaries. When a block is embedded in the whole machine, compare its serialized state and outputs at defined sync points.

### 50.3 Full Verilator composition versus ZEmu

At checkpoints:

- compare architectural registers;
- RAM/VRAM page hashes;
- FIFOs and descriptors;
- command progress;
- counters;
- frame/tile CRCs;
- selected detailed traces.

### 50.4 Physical FPGA versus ZEmu

Use the same immutable inputs. The FPGA returns:

- completion/error status;
- frame/tile/depth/stencil CRCs;
- counters;
- selected trace ring;
- optional bounded memory windows/checksums;
- hardware profile identity.

ZEmu correlates and identifies the earliest available divergence.

## 51. Trace synchronization

Each trace record should carry enough identity to align streams:

- machine epoch;
- frame ID;
- command sequence;
- source ID;
- component/channel;
- local sequence;
- cycle or defined boundary;
- entity/resource/primitive/tile identity where applicable.

Physical traces are bandwidth-limited. Support:

- trigger predicates;
- pre-trigger ring;
- post-trigger count;
- channel selection;
- address/source/entity filters;
- decimation for noncritical events;
- hash checkpoints to skip equal regions;
- “trace only after first checkpoint mismatch.”

## 52. Differential bisection by component substitution

When ZEmu and RTL disagree, automatically test component maps:

1. all optimized ZEmu;
2. replace half the suspect path with ZRef;
3. replace selected block with Verilator;
4. compare outputs/hashes;
5. bisect until one boundary first differs.

The output is a component-level blame interval, not an assertion that one source file is morally guilty.

## 53. Common-mode error safeguards

The greatest oracle danger is the same wrong assumption implemented in multiple places.

Safeguards:

- independent implementations for high-risk laws where practical;
- generated vectors plus independently authored edge cases;
- mutation/sabotage testing that proves tests can fail;
- formal properties for bounded control laws;
- cross-language and cross-implementation byte comparisons;
- physical hardware captures;
- metamorphic properties that do not depend on one expected answer;
- provenance showing whether two “independent” checks copied the same premise;
- explicit evidence-status labels: proposed, measured, reproduced, ratified, superseded, invalid.

The repository already records examples where tests passed and the model was wrong. ZEmu should preserve that honesty as a machine-readable property of evidence.

## 54. Fault injection

A development emulator should be able to inject controlled faults:

- delayed or dropped memory response;
- queue saturation;
- stale handle generation;
- malformed command;
- resource CRC failure;
- deadline pressure;
- audio underrun;
- input sequence gap;
- selected bit flip;
- device reset;
- cache miss storm;
- reduced memory bandwidth;
- injected RTL mutant;
- missing asset or corrupted page.

Faults are deterministic, manifest-declared, source-attributed, and included in replay identity. This is not random chaos unless a seeded fuzz mode requests it.

## 55. Automatic failure capture

On any configured failure, save:

- nearest prior stable snapshot;
- bounded pre-event trace;
- failure event and first bad state;
- optional bounded post-event trace;
- exact inputs and external events;
- hierarchy of hashes;
- source and run provenance;
- relevant buffers/images/audio;
- reproducible job manifest;
- failure signature for deduplication.

The capture should occur even if the UI is frozen or no human is present.

## 56. Failure minimization

ZEmu should eventually minimize failures across several dimensions:

### Input minimization

Remove or simplify input events while preserving failure.

### Time minimization

Find the latest starting snapshot and shortest replay preserving failure.

### Scene minimization

Remove irrelevant entities, resources, effects, commands, terrain regions, or mission events while preserving failure, subject to declared dependencies.

### Packet/program minimization

Shrink frame packets, command lists, Field programs, constants, or resource pages.

### State-slice minimization

Identify the smallest semantic subgraph whose state differs or whose presence is required.

### Mutant minimization

When a deliberate defect is caught, reduce the test to the smallest vector that still catches it and bank that vector.

The minimizer must always validate the final case against the original predicate. A “small” case that no longer fails is not evidence.

---

# Part VIII — Development, testing, and design at scale

## 57. Snapshot libraries as permanent project assets

Snapshots should be curated, named, and versioned like tests.

Suggested libraries:

- boot and clean reset;
- Wound Lab milestones;
- each creature clip and event;
- terrain operations and topology exceptions;
- render/material/lighting cases;
- near-deadline stress frames;
- memory contention cases;
- controller and latency cases;
- mission checkpoints;
- known bugs and regressions;
- visually excellent trailer moments;
- deliberately pathological states.

Each snapshot carries a purpose, expected laws, provenance, and compatibility identity.

## 58. Scenario generation from Form `test`

Form scenario blocks can compile into:

- initial-state construction or snapshot reference;
- deterministic input/event schedule;
- invariants;
- expected hashes/captures;
- budget assertions;
- headless job manifest;
- Verilator and physical-hardware replay package.

This unifies language-level tests with emulator automation. The same scenario can be run in fast functional mode during development and promoted through ZRef, Verilator, and hardware as maturity increases.

## 59. Test classes ZEmu can generate or host

- directed examples;
- golden captures;
- randomized differential tests;
- property tests;
- metamorphic tests;
- fuzzing of commands/resources/state transitions;
- long-horizon soak tests;
- determinism perturbation tests;
- save/restore round-trip tests;
- cross-version compatibility tests;
- mutation/sabotage sweeps;
- performance profile sweeps;
- mission softlock and exploit searches;
- visual and animation regression cases;
- physical-board replay cases.

## 60. Metamorphic tests

When exact expected output is expensive to author, test transformations with known relationships:

- translating a whole isolated scene translates outputs consistently;
- swapping two identical players swaps views but not shared truth;
- reordering pure presentation declarations does not change output;
- varying tile/patch parallel execution order does not change canonical hashes;
- snapshot/restore at a legal boundary does not change future execution;
- replacing an optimized block with ZRef does not change output;
- removing cosmetic particles does not change gameplay-truth hash;
- a zero-strength terrain field changes nothing;
- a full-turn angle offset preserves orientation;
- replaying the same `PadFrame` journal is identical regardless of UI attachment.

## 61. Balance laboratory

From one canonical battle snapshot, ZEmu can execute many branches:

- creature health/damage/speed variations;
- targeting laws;
- spell radius/cooldown/cost;
- terrain resistance and scar persistence;
- formation spacing;
- AI aggression and retreat thresholds;
- mission resource timing;
- player skill-policy bots;
- hardware presentation tiers.

Outputs can include:

- outcome distributions;
- time-to-objective;
- deaths/damage/resource use;
- spatial control;
- terrain change metrics;
- decision/path failures;
- selected representative replays;
- outliers and degenerate strategies;
- cost and deadline behavior.

Numbers do not decide fun. They find states worth playing and looking at.

## 62. Adversarial gameplay agents

Give agents or scripted bots full control through the same canonical input path available to a player. They may:

- seek exploits;
- destroy mission assumptions;
- search traversal routes;
- maximize terrain damage;
- provoke queue/deadline extremes;
- attempt softlocks;
- test unit-command edge cases;
- discover emergent tactics.

The emulator must not grant a gameplay agent hidden mutation powers when the test claims to model a legal player. Observation privileges and control privileges are separate.

## 63. Interestingness mining

Headless farms can generate too much data. Add selectors that preserve unusual or valuable runs:

- first occurrence of a new state signature;
- rare objective order;
- extreme but legal terrain topology;
- unusually close outcome;
- new traversal route;
- novel interaction chain;
- large visual motion or composition change;
- high disagreement among behavior variants;
- human-tagged aesthetic criteria.

An agent can review summaries and render only the best candidates.

## 64. Mechanic prototyping by world surgery

A complete snapshot plus transactional state mutation makes rapid mechanic experiments possible:

- attach a slinghook component to the player;
- transfer control to a creature;
- enable flight state;
- turn a tower into body-patch terrain;
- replace a renderer component;
- inject a new Form module;
- alter a mission rule;
- promote an effect from presentation to gameplay in an experimental branch.

Every surgery is a recorded branch. It can be replayed, compared, thrown away, or promoted into source with evidence.

## 65. Hot reload without lying

Hot reload is valuable only when state compatibility is explicit.

For a changed module/resource:

- compare program and layout hashes;
- identify compatible, migratable, and incompatible state;
- preserve state only when the schema proves compatibility;
- require a deterministic migration function where layouts change;
- otherwise restart from a named snapshot or reset point;
- record the reload as an external event in the branch journal;
- never treat a hot-reloaded run as equivalent to a clean boot without proving it.

## 66. Reproducing trailer and art-review shots

A shot manifest should include:

- snapshot/state hash;
- camera and input journal;
- rendering profile;
- asset/program versions;
- output resolution and frame range;
- exact color/presentation settings;
- expected frame hashes;
- optional narration/audio timing.

Then a great shot is never lost to “I cannot get that battle to happen again.”

---

# Part IX — Joining runtime provenance to development provenance

## 67. Two black boxes

ZEmu can preserve two histories:

1. **Runtime history:** what happened inside the universe.
2. **Development history:** why the universe was built this way.

A serious investigation often needs both.

Runtime asks:

- which field wrote this terrain height?
- which decision changed this target?
- which command produced this primitive?

Development asks:

- why is the field’s rounding law this way?
- which alternatives were rejected?
- what evidence ratified this command layout?
- did a prior agent warn that the test was circular?

## 68. Machine-readable run manifests

The existing run archive is enormous and valuable. To make it reliably queryable, every run should eventually have a compact machine-readable manifest alongside prose:

```text
run_id
start/end time
owner request
agents/roles
repos and starting commits
files inspected
files changed
spec clauses touched
block/op/source IDs touched
assumptions
claims with status
commands/tests executed
artifacts and hashes
commits produced
reviews and verdicts
superseded claims
open questions
```

This does not replace the prose. It indexes it.

## 69. Evidence graph

Build a queryable graph connecting:

```text
source span
↔ source ID
↔ compiler artifact
↔ command/program/resource
↔ runtime event
↔ snapshot/capture/trace
↔ test/invariant
↔ block/operation ledger entry
↔ report/measurement
↔ commit
↔ run record
↔ owner direction
```

Then an agent can ask:

```text
why is this register 24 bits?
show every run that investigated this terrain seam
which evidence advanced FIELD.EXECUTOR to its current maturity?
which tests depend on the old rounding assumption?
what owner direction governs Zixxtrixx ground contact?
```

## 70. Claim lineage and supersession

Documentation saturation creates a risk: ten obsolete reports may appear to outweigh one newer correction.

Every indexed claim should carry:

- claim ID;
- status: proposed / observed / measured / reproduced / ratified / rejected / superseded / invalid;
- subject IDs;
- evidence references;
- authoring run;
- supersedes/superseded-by links;
- scope and known limits;
- timestamp and commit.

Search results should prefer current ratified claims while still showing the historical path when requested.

## 71. Runtime-to-history query

An agent selecting a runtime event should be able to request:

- source declaration;
- current specification clauses;
- block/operation ledger entry;
- implementation commits;
- runs that created or materially changed it;
- tests/captures that accepted it;
- known open issues;
- owner direction.

This turns the repository’s impossible quantity of documentation into context delivered at the exact point of need.

## 72. History-to-runtime query

The reverse should also work. From a run or design claim:

- launch the cited capture;
- restore the cited snapshot;
- reproduce the benchmark;
- display the before/after states;
- rerun the mutation or deliberate defect;
- verify whether the claim still holds on current code.

A report stops being dead prose and becomes an executable portal into its evidence.

---

# Part X — Formats and persistent artifacts

## 73. Do not overload `.zcap`

The existing `.zcap` is a compact, versioned frame/capture evidence container. It should remain stable and useful for conformance.

Stuffing complete RAM histories, branch DAGs, arbitrary indexes, and huge traces into every `.zcap` would destroy that simplicity.

Recommended separation, with working names:

| Artifact | Purpose |
|---|---|
| `.zcap` | Compact frame/capture evidence; existing contract. |
| `.zstate` | Complete restorable whole-machine snapshot. |
| `.zinput` | Canonical external input/event journal. |
| `.ztrace` | Bounded typed trace stream. |
| `.zrun` | Deterministic execution manifest plus result summary. |
| `.zcase` | Portable forensic bundle referencing or embedding the above. |
| `.zschema` | Compiler-emitted typed state/debug reflection schema. |

These may be separate files or sections in a common outer container later. Their logical responsibilities should remain distinct.

## 74. Content addressing

Large artifacts should be content-addressed:

- immutable pages/blobs by cryptographic hash;
- manifests reference hashes;
- branches deduplicate unchanged pages;
- evidence can be copied shallowly or made self-contained;
- CI and agents can verify nothing silently changed;
- old cases can pin exact emulator/compiler/cartridge identities.

A local artifact store can maintain human names as aliases without making names the identity.

## 75. Whole-machine snapshot schema

A `.zstate` manifest might contain sections for:

- identity and version;
- time coordinate;
- component implementation map;
- HPS address spaces;
- local SDRAM;
- on-chip memories;
- processor/device records;
- FIFOs and queues;
- host-virtualized services;
- resource table;
- debug/provenance state if continuation needs it;
- hierarchical hashes;
- parent snapshot and branch;
- schema/source-map references;
- compression/page table;
- integrity checks.

Each device owns a versioned state serializer. The whole-machine loader refuses unknown required versions instead of guessing.

## 76. Trace schema

A typed trace event should include a common envelope:

```text
event_id
parent_event_id(s)
time coordinate
component/channel
event type
source_id
entity/resource/command identity
payload schema/version
```

Payloads are channel-specific. The trace format must support:

- streaming;
- chunking;
- indexing by time, channel, source, entity, address, tile, and event type;
- compression;
- bounded truncation with explicit loss markers;
- partial recovery;
- deterministic ordering of simultaneous events.

## 77. Forensic bundle index

A `.zcase` should begin with a small readable manifest so an agent can decide what to load. Include:

- case summary and failure signature;
- reproduction command;
- identities;
- artifact table with sizes/hashes;
- semantic subjects;
- time range;
- evidence completeness;
- known uncertainty;
- links to development provenance.

## 78. Cross-version survival

A bug case should not become unreadable because the emulator evolved.

Options:

- maintain readers/migrators for stable artifact versions;
- retain a compatibility runner or container identity;
- embed sufficient old schemas and source maps;
- distinguish “migrated for inspection” from “executed under original semantics”;
- never rewrite original evidence in place;
- permit current tools to launch an older emulator build for exact replay.

## 79. Storage management

Omniscience can eat disks. Provide policies:

- ring-buffer recent history;
- automatic deduplication;
- compression by data type;
- retention tiers;
- promotion of important cases to permanent storage;
- garbage collection of unreferenced branches/blobs;
- artifact size budgets in jobs;
- summary/index retention even after bulk trace eviction;
- redaction/export tools for public bug reports.

---

# Part XI — User interface as a client of the oracle

## 80. Timeline debugger

A graphical client can show:

- frame/tick/cycle timeline;
- input, simulation, command, render, audio, fault, and trace lanes;
- checkpoints and branches;
- scrub/reverse/step controls;
- first-divergence markers;
- selected event details;
- link to source and run provenance.

## 81. Semantic state explorer

A tree/table view of:

- worlds and missions;
- entities and components;
- terrain patches;
- resources;
- command queues;
- devices and memories;
- counters;
- invariants;
- changed values since a selected point.

Every semantic value can reveal raw storage and history.

## 82. Battlefield and terrain viewer

Overlays for AI, paths, fields, scars, hazards, LOD, hardware cost, source IDs, collision, and mission triggers, synchronized with the timeline.

## 83. Render pipeline viewer

Select a frame, view, tile, primitive, or pixel and navigate every stage, including alternate branches/renderers.

## 84. Creature/art viewer

Synchronized animation playback, every-frame contact sheet, orthographic cameras, concept/reference overlays, pose probes, ground-contact report, lighting/material controls, and before/after branch comparison.

## 85. Evidence/history viewer

Show the specification, source, tests, commits, run records, and ratification lineage connected to the selected runtime fact.

## 86. The UI must have no secret operations

Every action performed by the UI should be expressible through the public control/query API, and every view should be reproducible from exported artifacts. This keeps agents and CI first-class citizens.

---

# Part XII — Proposed implementation path

## 87. Phase E0 — Ratify the emulator laws

Before large implementation, settle:

- ZRef/ZEmu authority relationship;
- headless-first requirement;
- canonical whole-machine state boundary;
- time-coordinate model;
- determinism and external-event law;
- component state serialization contract;
- raw and semantic observation layers;
- artifact separation (`.zcap` versus whole-machine artifacts);
- instrumentation transparency law;
- component substitution contract;
- agent capability model.

Deliverable: a compact ratified ZEmu specification extracted from this treatise, not this entire essay made law.

## 88. Phase E1 — Turn the stub into a deterministic headless console

Build:

- `zemucore` with no GUI dependencies;
- cartridge loading;
- deterministic host services;
- complete console loop for the currently implemented machine;
- canonical controller journal injection;
- structured CLI/NDJSON responses;
- exact reset and run-to-frame/tick;
- whole-state hash;
- first complete snapshot/restore at a frame boundary;
- existing `.zcap` production/replay;
- clear exit codes.

Gate:

- a fresh process loads a cartridge/snapshot, runs headlessly, and reproduces identical frames, audio, counters, and state hashes;
- attaching/detaching a UI changes nothing canonical;
- the stub’s existing frame-packet replay remains supported.

## 89. Phase E2 — Semantic state and query

Build:

- compiler-emitted `.zschema` prototype;
- memory-region registry;
- typed state decoder;
- stable semantic paths;
- source resolution;
- raw/typed/semantic query API;
- breakpoints/watchpoints over semantic state;
- invariant registration;
- automatic failure snapshot.

Gate:

- an agent can identify a creature, terrain point, command, and counter entirely through structured queries and descend to raw bytes/source;
- stale schemas are rejected by hash/version.

## 90. Phase E3 — Time travel and branching

Build:

- input/external-event journal;
- incremental snapshots;
- recent-history ring;
- reverse step via restore/replay;
- timeline branches and copy-on-write state;
- semantic diff;
- first-divergence binary search.

Gate:

- arbitrary named states can be rewound, forked, mutated, and replayed deterministically;
- branches share storage safely and never contaminate each other.

## 91. Phase E4 — Domain observatories

Build in the order demanded by active development:

- render/tile/pixel inspector;
- terrain layer/history inspector;
- creature pose/animation inspector;
- AI decision trace;
- physics/traversal inspector;
- audio/input-latency inspector;
- performance/deadline explanation.

Gate:

- each selected domain can produce a compact agent-ready evidence pack from one command.

## 92. Phase E5 — Verilator and board lockstep

Build:

- per-component implementation substitution;
- state adapters for Verilated blocks;
- synchronized trace envelopes;
- whole-machine checkpoint comparisons;
- physical trace/counter/CRC import;
- automatic component bisection.

Gate:

- a deliberately injected RTL defect is localized to its first differing boundary and saved as a minimized portable case.

## 93. Phase E6 — Agent laboratory and farm

Build:

- long-running session daemon;
- isolated concurrent sessions;
- job queue and resource limits;
- `.zcase` forensic bundles;
- input/scene/program minimizers;
- batch branch sweeps;
- interestingness selection;
- mission fuzzing and balance reports;
- adapters for coding agents.

Gate:

- an agent can receive one case, investigate it, request more evidence, propose a patch, replay the exact failure, and return proof without a human operating ZEmu.

## 94. Phase E7 — Development provenance integration

Build:

- machine-readable run manifests;
- evidence graph/index;
- claim lineage;
- runtime-to-run and run-to-runtime queries;
- executable report links.

Gate:

- selecting a runtime value or source ID returns current law, evidence, superseded claims, and relevant run history without manual repository archaeology.

---

# Part XIII — Risks and safeguards

## 95. Emulator drift

**Risk:** optimized ZEmu behavior gradually differs from ZRef/hardware.

**Safeguards:** component substitution, continuous differential tests, capture corpus, exact modes, no silent fallbacks, hierarchical hashes, physical replay.

## 96. Hidden host state

**Risk:** snapshots appear complete but depend on wall time, thread order, files, or device state.

**Safeguards:** deterministic host virtualization, fresh-process restore gates, determinism perturbation, external-event journal, explicit noncanonical modes.

## 97. Instrumentation changes behavior

**Risk:** detailed tracing alters scheduling/timing and “fixes” the bug.

**Safeguards:** out-of-band semantic tracing, fidelity labels, hardware-cycle conclusions only from timing-faithful modes, triggerable bounded traces, compare instrumented and uninstrumented canonical hashes.

## 98. Data deluge

**Risk:** full state and traces overwhelm storage and agent context.

**Safeguards:** typed queries, hierarchical hashes, scopes, indexes, content addressing, compact/standard/exhaustive packs, automatic minimization, retention policies.

## 99. Wrong semantic decoder

**Risk:** a pretty typed view lies about the raw machine.

**Safeguards:** compiler-generated schema, raw-address disclosure, schema hashes, round-trip tests, independent spot checks, refusal on mismatch.

## 100. Common-mode oracle bugs

**Risk:** ZRef and ZEmu share the same wrong premise.

**Safeguards:** independent tests, metamorphic properties, mutation/sabotage, formal verification, physical evidence, claim provenance, explicit unknowns.

## 101. Documentation poisoning

**Risk:** stale or copied run prose dominates search.

**Safeguards:** machine-readable claim status, supersession edges, evidence ranking, current-spec priority, provenance showing copied premises, no prose-as-oracle.

## 102. Agent mutation chaos

**Risk:** agents silently alter state, confuse branches, or make nonreproducible conclusions.

**Safeguards:** capabilities, transactions, audit log, immutable baseline, branch IDs, exact manifests, rollback, sandboxing.

## 103. Overbuilding observability before the game

**Risk:** the emulator becomes another infinite infrastructure project.

**Safeguards:** build observability in response to real active lanes; keep the headless/state foundations early; require every advanced lens to retire a demonstrated debugging/design cost; use Wound Lab and Tribute Upheaval cases as gates.

The design is expansive, but implementation should be pulled by actual leverage.

## 104. Treating metrics as art judgement

**Risk:** automated likeness or motion metrics override the owner’s eye.

**Safeguards:** preserve the project’s art law. Use probes to reveal, compare, and select frames; do not let them author the value by default. Make A/B replay effortless and keep the final visual decision human-owned.

## 105. Mistaking host acceleration for hardware evidence

**Risk:** an optimized emulator is fast, therefore the FPGA is assumed fast.

**Safeguards:** separate host timing, modeled cycles, profile projections, Verilator cycles, and measured hardware counters in every report.

---

# Part XIV — Hard acceptance criteria

## 106. First complete emulator acceptance

ZEmu is no longer a shell when it can:

1. load a real cartridge and all required resources;
2. run the complete currently specified console headlessly;
3. accept canonical input journals;
4. produce exact frames/audio/counters;
5. save and restore complete state at a frame boundary;
6. reproduce future state hashes after fresh-process restore;
7. emit and replay existing `.zcap` evidence;
8. return structured status and useful exit codes;
9. operate with no graphical or audio device;
10. refuse unsupported semantics rather than approximating them.

## 107. Omniscience foundation acceptance

The development substrate is real when:

1. all canonical memory regions and devices have versioned serialization;
2. semantic queries resolve through compiler-generated metadata;
3. every semantic response can reveal raw storage and source IDs;
4. breakpoints can trigger on typed state and invariants;
5. failures automatically produce reproducible bundles;
6. snapshots and branches have canonical hashes;
7. UI attachment cannot change canonical output;
8. agents can perform the complete workflow through the protocol.

## 108. Time-machine acceptance

1. reverse one tick/frame reliably;
2. rewind to a prior semantic event;
3. fork from a snapshot;
4. apply a transactional mutation;
5. replay both branches;
6. produce a semantic and output diff;
7. locate the first divergence;
8. package the result.

## 109. Hardware-oracle acceptance

1. run identical evidence through ZRef, ZEmu, Verilator, and FPGA where supported;
2. compare at documented boundaries;
3. identify first available divergence;
4. map it to component, source ID, command/program counter, and evidence;
5. reproduce an injected defect;
6. minimize and bank the failing case.

## 110. Agent-native acceptance

From a clean environment, an agent with no GUI can:

1. discover protocol/schema capabilities;
2. create a session;
3. load a case;
4. inspect semantic state;
5. ask for history and source provenance;
6. install a breakpoint;
7. run/rewind/fork;
8. mutate one state or implementation transactionally;
9. compare the branches;
10. save a portable evidence package;
11. exit with a machine-readable verdict.

---

# Part XV — Worked development workflows

## 111. Creature walks into a crater instead of attacking

### Trigger

An invariant detects a creature entering an invalid ground state, or a tester bookmarks the visible event.

### Automatic evidence

- snapshot before target decision;
- input journal;
- creature semantic state;
- AI candidate/score trace;
- navigation query and dirty-cell history;
- terrain layers under each foot;
- active clip/frame and pose;
- last writer of target and ground state;
- relevant source/run provenance;
- short video/contact sheet.

### Agent investigation

1. query `explain creature 19 target`;
2. rewind to the last target write;
3. inspect whether navigation used a stale patch revision;
4. fork branch A with current code;
5. branch B forces nav refresh;
6. replay identical inputs;
7. compare truth hash, path, animation, and outcome;
8. promote a minimized case to regression.

## 112. One wrong pixel after a renderer optimization

1. ZEmu detects displayed-frame hash difference.
2. hierarchical comparison says gameplay and command stream are identical.
3. tile CRC identifies first different tile.
4. stage capture identifies first different fragment.
5. pixel genealogy resolves material, texture sample, interpolants, primitive, command, and source.
6. component substitution shows scalar ZRef raster path is correct and optimized path diverges.
7. minimizer removes unrelated commands/resources.
8. final `.zcase` contains one triangle/material/pixel reproducer plus source and run history.

## 113. FPGA disagrees only after twenty minutes

1. periodic checkpoint hashes are equal through tick N.
2. next checkpoint differs in Field service state.
3. emulator binary-searches the interval using the same input journal.
4. board trace is armed only around the narrowed cycle range.
5. first differing service response identifies source program PC and fixed-point operands.
6. ZRef, optimized ZEmu, and RTL mutant comparison isolates the block.
7. failure is banked as a long-horizon regression with a late-start snapshot so future runs need seconds, not twenty minutes.

## 114. Design a slinghook mission

1. capture a tower-and-player micro-scene.
2. add experimental hook component in a branch.
3. agents run trajectory sweeps from the exact state.
4. preserve representative arcs, failures, exploits, and spectacular outcomes.
5. inspect moving/deforming anchor provenance.
6. tune parameters through branch comparisons.
7. run reachability/softlock invariants.
8. promote the mechanic and mission snapshots when the human playtest says it sings.

## 115. Custom art style for one mission

1. pin several representative mission snapshots and cameras.
2. implement renderer variant as a component/profile.
3. replay the same frames through ordinary and custom paths.
4. produce side-by-side stills, motion clips, intermediate buffers, and hardware-cost reports.
5. use metrics only to reveal differences/regressions.
6. judge by eye at final resolution.
7. keep the custom renderer with a dedicated capture corpus—or delete it cleanly.

## 116. Balance one creature without homogenizing it

1. take battle snapshots representing attack, defense, pursuit, retreat, terrain chaos, and wizard protection.
2. branch behavior and stats across a matrix.
3. run deterministic seeds with several player/bot policies.
4. identify outliers, degenerate strategies, and characteristic moments.
5. render a small curated set.
6. owner chooses the version with the desired character, not merely the highest win rate.
7. retain the rejected branches’ evidence so future changes do not rediscover the same failures.

## 117. Mission softlock after terrain destruction

1. mission invariant reports no legal route to required objective while mission remains Running.
2. snapshot and terrain operation history are saved.
3. query identifies the field/stamp/topology changes that closed all routes.
4. minimizer finds the smallest destruction/input sequence preserving the softlock.
5. branches test possible laws: alternate objective, escape spell, path guarantee, destructible gate, or explicit failure.
6. replay validates the chosen design without changing unrelated battle history.

## 118. Why is the tower bent this way?

1. select section in battlefield viewer.
2. inspect body-patch attachment/glue graph.
3. list every deformation operation contributing to its vertices.
4. follow each operation to spell/entity/source ID.
5. replay the sequence with one operation removed.
6. compare geometry, collision, navigation, and rendering.
7. link to the runs that defined the seam and bending law.

---

# Part XVI — Recommended command sketches

## 119. CLI examples

Illustrative commands, not frozen syntax:

```bash
# Headless deterministic run
zemu run build/upheaval.zpak --headless --frames 600 --json

# Restore and run to a semantic predicate
zemu run --state cases/tower-before.zstate \
  --input cases/tower.zinput \
  --until 'mission.status != Running || invariant.failed' \
  --bundle out/tower-case.zcase

# Dump complete raw RAM plus semantic index
zemu inspect --state out/failure.zstate --all-memory --schema --out out/dump

# Query typed state
zemu query --state out/failure.zstate \
  'world.creatures[id=19].{brain,animation,ground}' --json

# Rewind to the last write
zemu rewind --session s-47 \
  --last-write 'world.creatures[id=19].brain.target'

# Fork and patch transactionally
zemu fork --session s-47 --name nav-refresh
zemu mutate --session nav-refresh \
  --set 'navigation.patch[81].revision_seen=terrain.patch[81].revision' \
  --commit

# Compare two universes
zemu diff --a branch:original --b branch:nav-refresh \
  --scope gameplay,terrain,creature:19,render --out out/diff

# Explain a pixel
zemu explain pixel --session s-47 --view 0 --x 133 --y 91 \
  --trace-back-to-source --json

# Compare one block through ZRef and Verilator
zemu compare --capture cases/field.zcap \
  --component FIELD.EXECUTOR=zref,verilator \
  --first-divergence --minimize

# Run a parameter farm
zemu farm --state cases/battle.zstate \
  --matrix tuning/creature19.yml \
  --seeds 1000 --headless --select-interesting 24
```

## 120. Persistent protocol sketch

```json
{"id":1,"method":"session.create","params":{"fidelity":"instrumented_exact"}}
{"id":2,"method":"machine.load_cartridge","params":{"session":"s1","path":"build/upheaval.zpak"}}
{"id":3,"method":"breakpoint.add","params":{"session":"s1","predicate":"any creature.ground_state == Invalid","pre_events":2000}}
{"id":4,"method":"machine.run","params":{"session":"s1","until":"breakpoint || sim.tick == 60000"}}
{"id":5,"method":"state.query","params":{"session":"s1","select":"creature[id=19]","depth":4}}
{"id":6,"method":"history.last_write","params":{"session":"s1","path":"world.creatures[id=19].brain.target"}}
{"id":7,"method":"timeline.fork","params":{"session":"s1","name":"candidate-fix"}}
{"id":8,"method":"case.export","params":{"session":"candidate-fix","profile":"standard","path":"out/candidate.zcase"}}
```

## 121. Query result sketch

```json
{
  "subject": {"kind":"creature","id":19,"generation":4},
  "time": {"tick":48821,"frame":48796},
  "state_hash":"zhstate:...",
  "values": {
    "brain.mode":{"type":"BrainMode","value":"Pursue"},
    "brain.target":{"type":"EntityRef","value":{"id":443,"generation":2}},
    "animation.clip":{"value":"LeapAttack"},
    "animation.key":{"value":11},
    "ground.front_left":{"value":"InvalidSurface"}
  },
  "source": {"id":"0x81230017","file":"creatures/zixxtrixx.form","span":[1844,2012]},
  "last_events":[981552112,981552119],
  "raw_refs":[{"domain":"hps.ddr.world","address":"0x0018A400","bytes":128}],
  "truncated":false
}
```

---

# Part XVII — Ratification candidates and open questions

## 122. Decisions worth freezing early

1. Is ZEmu’s headless core officially the primary product and every UI a client?
2. What exact boundaries define a complete whole-machine snapshot?
3. Which state is canonical, derived, cache-only, or host-only?
4. What is the shared time-coordinate model?
5. What host services are allowed in canonical execution?
6. What are the first fidelity modes and their truth claims?
7. What component interface permits ZRef/ZEmu/Verilator/hardware substitution?
8. What compiler metadata is required for typed state decoding?
9. How are semantic paths and stable entity identities represented?
10. What is the minimum always-on event/provenance data?
11. How are instrumentation levels declared and verified nonsemantic?
12. What artifact families remain separate from `.zcap`?
13. What content-addressing and integrity scheme is used?
14. What transport and schema serve headless agents first?
15. What capabilities govern mutations and physical hardware?
16. What failure events automatically produce evidence?
17. What snapshot cadence and storage budgets are practical?
18. How are old snapshots/cases preserved across schema changes?
19. How are run records indexed and linked without treating prose as law?
20. What first real Tribute Upheaval case proves the value of each advanced feature?

## 123. Questions that should remain open until prototypes

- How much dynamic provenance can remain always-on before it damages throughput?
- Is NDJSON sufficient for trace-heavy work, or should only control use it while traces use a binary stream?
- How far can scene/state minimization safely remove entities and resources automatically?
- Which counterfactual balance searches produce useful design evidence rather than noise?
- How much cycle modeling belongs in optimized ZEmu versus Verilator composition?
- How should physical board memory windows be exposed without consuming too much fabric/bandwidth?
- Which graphical views actually reduce owner and agent time enough to justify maintaining them?
- What representation makes the run-provenance graph most useful to agents?

---

# Conclusion

The most important emulator feature is not a better shader debugger, save states, or a RAM window in isolation.

It is the decision to make **the complete running universe addressable as evidence**.

Zhaozhou already has the rare prerequisites:

- exact specifications and reference functions;
- a deliberate ZRef/ZEmu/RTL hierarchy;
- immutable frame packets;
- deterministic input;
- stable source IDs;
- versioned captures;
- counters and first-divergence concepts;
- compiler-owned source maps and cost metadata;
- deterministic Form truth;
- fully owned formats and assets;
- exhaustive run provenance;
- and a project culture willing to record that a green test can still be wrong.

ZEmu can join them.

A normal bug report says:

> “The creature acted strangely after the tower fell.”

The finished system should be able to answer:

> At simulation tick 48,821, creature 19 changed from GuardWizard to Pursue because candidate target 443 scored 17 units above the defense action after navigation read patch 81 at revision 1406. A terrain field emitted by spell instance 722 had already advanced that patch to revision 1407, but the navigation reader had not consumed the dirty-cell event. The first divergent state write was event 981,552,112 at this Form source span. Here is the complete pre-state, every relevant write, the terrain operation history, the resulting pose and path, the first wrong displayed frame, the run that introduced the reader contract, and three forked fixes replayed over the identical universe.

That is the standard to aim at.

Not because every bug deserves an essay, but because the project can manufacture conditions in which agents do not have to guess. They can inspect, replay, fork, and prove.

The physical console is the machine players encounter.

ZEmu can be the machine that lets its creators see everything.

> **The source tells us what exists. The run archive tells us why it exists. The emulator tells us exactly what it did.**

---

# Appendix A — Repository basis reviewed

This treatise was grounded in the following current project artifacts rather than written as a generic emulator wishlist:

- `README.md` — five-product layout and current ZEmu/ZRef role.
- `emulator/zemu_main.cpp` — current intentionally empty Phase-1 replay shell and exit-code behavior.
- `ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md` — machine laws, ZRef/ZEmu/Verilator hierarchy, source-level traces, capture stack, memory split, input/audio, phases, and Wound Lab.
- `FORM_LANGUAGE_HARDWARE_CODESIGN.md` — truth/form split, deterministic simulation, explicit domains, source IDs, tests, captures, costs, source-level hardware debugging, and ZEmu hot reload.
- `spec/capture_format.md` — sealed frame packets, `.zcap`, source maps, CRCs, controller/counter/framebuffer/tile/depth/trace/state sections, and replay-exact temporal state.
- `spec/input_rules.md` — canonical atomic `PadFrame` and deterministic snapshot sequence.
- `spec/counters.md` — stable counter catalog and frame-boundary snapshot protocol.
- `spec/memory_rules.md` — deterministic HPS harness, memory/ring ownership, VRAM regions, and modeled profiles.
- `spec/creature_rules.md` — creature/clip/pose/attachment/reparent/body-patch contracts and art/gameplay seams.
- `CLAUDE.md` — run discipline, documentation saturation, art-law constraints, and the requirement to retain reproducible probes.
- `STATUS.md` — examples of mutation testing, saved evidence, multi-context deadlock discovery, stale-test detection, and explicit separation of measured truth from attractive but meaningless numbers.
- `design/blocks.yml`, `design/ops.yml`, `design/formal_runs.yml` — machine-readable architecture, operation, and evidence ledgers.
- `reference/` and `compiler/` directory structures — substantial exact reference and Form compiler surfaces already in place.
- `captures/golden/` and `captures/failures/` — existing committed evidence families.

## Appendix B — One-page capability checklist

### Foundational

- headless `zemucore`;
- cartridge loading;
- deterministic host services;
- complete state serialization;
- canonical state hashes;
- snapshot/restore;
- structured CLI/protocol;
- raw memory access;
- compiler-emitted semantic schema;
- source resolution;
- agent-controlled run/step/pause;
- automatic failure evidence.

### High-value development

- incremental snapshots;
- rewind/reverse;
- branches and transactional mutations;
- semantic diffs;
- run-until predicates;
- last-writer history;
- hierarchical hashes;
- render/terrain/creature/AI/physics/audio inspectors;
- latency tracing;
- cost/deadline explanation;
- `.zcase` forensic bundles;
- scenario and mission micro-scenes.

### Verification

- ZRef substitution;
- Verilator block substitution;
- component bisection;
- first-divergence search;
- FPGA CRC/counter/trace comparison;
- fault injection;
- mutation/sabotage;
- deterministic perturbation;
- automatic minimization.

### Scale and revolution

- isolated concurrent sessions;
- headless simulation farm;
- counterfactual branch sweeps;
- adversarial gameplay agents;
- interestingness mining;
- mission fuzzing;
- design parameter search;
- custom one-mission renderer experiments;
- runtime/development provenance graph;
- executable run records;
- agent context packs assembled from exact evidence.

