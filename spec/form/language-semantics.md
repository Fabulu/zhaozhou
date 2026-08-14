# Form Language Semantics — Phase 1 scope

**Status:** stub, content-scoped to Phase 1 (the charter Phase 3 language item
owns the expansion). Cited from `spec/form/field-ir.md` and the wave-1 tests.

## 1. What exists in Phase 1

There is **no parser yet**. Field programs are authored through the typed TS
builder API (`compiler/src/field_ir/builder.ts`) — FORM §18-L0 explicitly
permits "a small textual or JSON IR may drive initial tests before the full
parser exists". The builder *is* the Phase-1 frontend surface.

## 2. Frozen now (because Field IR v1 depends on them)

- Scalar type: `fx16` (Q16.16, s32) — `spec/qformats.md` §2. Unit, angle16 and
  u32 lanes exist only as I/O-record lane types (`field-ir.md` §7.1).
- Determinism law: evaluation is a pure function of (program bytes, input
  record); no floats, no clocks, no allocation (field-ir.md §1.4).
- Branchlessness: no loops, recursion, pointer reads, texture fetches, dynamic
  allocation, or unbounded control flow (FORM §9; enforced structurally — the
  ISA has no control-flow opcodes at all).
- Source identity: every spell site carries a `source_id`
  (capture_format.md §5; kind 3 = field program) and a program hash; both
  survive capture round-trips (field-ir.md §8, §12 check 5).

## 3. Phase 3 expands

Grammar and lexical rules; the type checker over world2/3, velocity3 and
colour8; profile admission (which spells may run in earth/warp/flow/
formation/stamp); bake-ability analysis (FORM §8); the full spell → Field IR
lowering rules; `max_ops` declarations and their mapping to the §7.3 ceilings.
