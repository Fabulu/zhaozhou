# Form Deterministic Scheduling — L1 v1

**Status:** Phase 1 stub expanded to **L1 v1** (wave 3, plan W3.1, decision
D6; FORM §5; charter §23 Phase 3). Companion law: language-semantics.md
(declarations, FORM-E-50x codes), domains-and-effects.md (read/write
admission). The Phase-1 frozen content below is retained verbatim in §1 —
the Field IR has no scheduler and needs none; this document governs the
*system* level above it.

## 1. What was frozen in Phase 1 (unchanged)

- Evaluation order is the serialized PC order — there is no scheduler freedom
  in the Field IR: the interpreter executes instructions 0..END in order
  (field-ir.md §1.1). Def-before-use is validated (§4 V11), so the order is
  also the data-dependency order.
- Fixed-tick rule: a field program consumes exactly one input record and
  produces exactly one output record per evaluation; no state carries between
  evaluations (field-ir.md §1.4). Ticks are therefore replayable in any order
  with identical results.
- Determinism of artifacts: .zprog/.zvec serialization is timestamp-free and
  byte-stable across runs (field-ir.md §5, §12 check 3); program hashes are
  pure functions of code+tables (§5.4).

## 2. The tick and its phases

The simulation tick is 60 Hz (FORM §5). One tick, in order:

```
phase 0  input latch      one atomic PadFrame[4] snapshot (input_rules §2.1)
phase 1  sim systems      the compile-time schedule (§3), ascending order
phase 2  terrain truth    enqueued @earth applications over their footprints
                          (software: zfield interpreter per column, ascending
                          x then z; hardware lane: TerrainField commands)
phase 3  presentation     present_frame over FormState (pure) → frame packet
phase 4  seal + render    packet sealed (capture_format §3), renderer executes
```

Systems never observe phase 2 mid-application: terrain reads within phase 1
see the terrain state at tick start; systems needing post-application terrain
run in a **later phase** (§3). Present (phase 3) reads the fully-settled
truth state — this ordering is why present is pure and replayable
(domains-and-effects §2.2).

## 3. Compile-time schedule, one writer per component per phase (D6)

The schedule is computed **entirely at compile time** — zero runtime
scheduling decisions. Construction:

1. Each system declares `reads` and `writes` over state components
   (pool fields, globals, `terrain`, `input`).
2. The compiler orders systems into **phases** topologically: a system that
   reads component C after another writes C must land in a strictly later
   phase. Systems with no read/write interaction share a phase.
3. **One-writer law:** within one phase, at most one system may write any
   state component. A violation is a compile error and the diagnostic
   **cites BOTH conflicting declaration spans** (FORM-E-500) — the writer of
   the rule is the error message itself, not a lint.
4. A read-write cycle with no topological order is refused (FORM-E-505):
   the programmer must split a system or introduce an explicit later phase
   by weakening a read.
5. Within a phase, systems run in **declaration order** (source order) —
   declaration order is part of the program's observable semantics and the
   emitted `sim_tick` is a flat phase-ordered call list (D6; W3.3 goldens
   pin it byte-stably).

The first implementation deliberately uses this simple inspectable rule —
one writer per component per phase, explicit later phases for dependent work,
stable iteration order (FORM §5) — not a borrow checker.

## 4. Multi-rate systems: `every N ticks`

A system declared `every N ticks` (N ≥ 1, FORM-E-506) executes iff
`tick % N == 0`. Semantics: the *state* function is defined on executed
ticks only; non-executed ticks leave the system's written components
untouched. Scheduling consequences:

- Rate is part of the schedule: a slow writer and a fast reader of the same
  component interact only through phase order, unchanged.
- The emitted call list is guarded by compile-time constants
  (`if (tick % N == 0) sys_locomotion(...)`); no runtime decision exists.

## 5. Stagger: `stagger over pool`

A slow system iterating a large pool may stagger to avoid workload spikes
(FORM §5): `system update_shards every 4 ticks stagger over shards` executes
per element iff

```
entity_index % N == tick % N        (N = the system's every-rate)
```

Laws: stagger requires exactly one iteration pool (FORM-E-504); the stagger
rate must equal the system rate (FORM-E-507); the per-element predicate is
evaluated in ascending index order; spawn/kill interplay obeys the pool laws
(language-semantics §4.5 — staggered systems see the same dense snapshot at
entry, membership changes resolve at system end via stable compaction).
Stagger never changes the tick predicate (`tick % N == 0` gates the system;
the entity predicate spreads the *elements* inside it).

## 6. Iteration order (always ascending)

Every pool iteration, array walk, terrain-column sweep and hash
serialization walks indices **ascending** — `for i in a..b` ascending-only
(FORM-E-501), pool sugar ascending over `0..count`, terrain applications
ascending x then z, D5 canonical serialization in dense pool index order.
No construct produces descending or unordered iteration in L1; the
compiler-inserted stable compaction (language-semantics §4.5) preserves
relative order.

## 7. Determinism checklist (test obligations, W3.3/W3.7)

1. The schedule is a pure function of declaration order + read/write sets:
   two compiles of the same source yield identical phase lists (golden).
2. Conflicting writes rejected with both spans (one negative test per
   conflict shape: same pool field, same global, terrain).
3. Multi-rate + stagger goldens: 600-tick run hash chain identical across
   desktop/ARM (D5 `H_t` law) — stagger shifts work, never results.
4. `sim_tick` emission is byte-stable (fixed order, no timestamps, LF,
   version banner — D4) and contains no runtime scheduling code.
5. Emitted phase guard constants match the declared rates (no drift between
   schedule and codegen).
