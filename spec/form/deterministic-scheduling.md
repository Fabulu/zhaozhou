# Form Deterministic Scheduling — Phase 1 scope

**Status:** stub, content-scoped to Phase 1 (Phase 3 and the sequencer blocks
own the expansion).

## 1. What is frozen now

- Evaluation order is the serialized PC order — there is no scheduler freedom
  in Phase 1: the interpreter executes instructions 0..END in order
  (field-ir.md §1.1). Def-before-use is validated (§4 V11), so the order is
  also the data-dependency order.
- Fixed-tick rule: a field program consumes exactly one input record and
  produces exactly one output record per evaluation; no state carries between
  evaluations (field-ir.md §1.4). Ticks are therefore replayable in any order
  with identical results.
- Determinism of artifacts: .zprog/.zvec serialization is timestamp-free and
  byte-stable across runs (field-ir.md §5, §12 check 3); program hashes are
  pure functions of code+tables (§5.4).

## 2. Phase 3 expands

The sequencer's per-profile issue rules (FIELD.SEQ.* blocks in
design/blocks.yml); instruction counters and budget enforcement per
charter §19.4; multi-program residency and the program-cache invalidation
protocol; capture/replay scheduling rules (which tick a capture replays).
