# Contract — SW.FIELDIR (Field IR evaluator and tooling)

> Ledger: `design/blocks.yml` · owner ZH-010 · phase 1 · maturity REFERENCE_COMPLETE (W6; evidence pinned in the ledger)

## Purpose and exclusions

Exact Field IR builder/serializer/evaluator/vector-generator/validity-checker — the single TS-side semantics (subordinate to the C++ golden interpreter).

Exclusions: no Form parser/type-checker (that is SW.COMPILER.FORM, Phase-3 scope; wave-1 tests drive the builder API per FORM §18-L0), no C++ interpreter (that is `reference/src/zfield/`, a block of SW.ZREF — never hand-implemented twice, charter §29-6), no RTL.

## Input and output packet layouts

In: typed program construction via the builder API (P4 §4 op/builder types). Out: `.zprog` binaries (frozen ISA v1 incl. DCURVE 0x1D; 64-bit fixed word, dst+3src+imm32), `.zvec` vector binaries (CRC-32C'd, program-hash-pinned), emitted typed C++ wrappers. Layouts are law in spec/form/field-ir.md §7–8; the serializer is deterministic (byte-stable across runs).

## Backpressure rules

Backpressure: `none`.

## Memory ownership

Compiler workspace owns all allocations; the emitted wrapper embeds the program bytes (`static_assert` on the program hash). The linear-scan register allocator is a pure function of the op list — no hidden state.

## Q formats and rounding

Q16.16 (s32) lanes everywhere in Field IR (plan Q2); MUL/MAD compute the exact s64 expression and round ONCE via `rescale(·,16)` (plan A3b); saturation + sticky SAT/RCP0 per spec/qformats.md and field-ir.md. TS emulates int64 MUL/MAD via one shared 16-bit-limb util (plan R2) — hand-computed product corpus incl. negative halves is a unit test.

## Latency (fixed or variable)

Latency: `batch`.

## Target throughput

Target throughput: n/a (tool).

## Overflow and malformed-input behaviour

Validator rejects invalid programs before ANY register write (Dalvik model); `decode` re-validates on load and never trusts the bytes. Every op is total: `rcp(0)=0x7FFF_FFFF` + RCP0, booleans 0/0x10000, CURVE/SPLINE clamped with pinned binary search / Catmull-Rom. Failing vectors minimize per-lane (bisection ≤64 steps) and are saved per charter §29-17.

## Directed tests

`compiler/tests/crater_ring.test.ts` (builds/validates/serializes the P4 §10 program, emits the wrapper), `compiler/tests/field_ir.test.ts` (builder/allocator/serializer/validator/limb products), `compiler/tests/generated_conformance.test.ts` (ABI consumer round-trips).

## Randomized differential tests

`compiler/tests/field_ts_differential.test.ts` — TS interpreter replays the C++-owned golden `captures/golden/field/crater_ring.zvec` byte-identically; `compiler/tests/field_fuzz_corpus.test.ts` + `tests/fuzz/field_corpus_gen.ts` (bounded random programs, TS vs C++ parity, nightly).

## Integration capture cases

`captures/golden/field/crater_ring.zvec` (C++ oracle owns it), minimized failing vector `captures/failures/field/fail-484add8d-0x5A17.zvec` + report, committed fuzz corpus `tests/fuzz/corpus/field/*.zprog|.zvec`.

## Notes

Contract filled (Phase-1-active). ISA v1 frozen incl. DCURVE 0x1D (plan 1.B-4); profile ceilings (32/48/48/64/32) explicitly provisional (plan R7).
