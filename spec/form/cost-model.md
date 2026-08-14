# Form Cost Model — Phase 1 scope

**Status:** stub, content-scoped to Phase 1 (Phase 3 owns the expansion;
Phase 0 hardware lane owns the absolute budgets).

## 1. Field IR costs (frozen v1 mechanics, provisional numbers)

Per-op classes and provisional cycle/DSP estimates live in
`spec/form/field-ir.md` §2/§9 (ALU/MUL/TABLE/NOISE/SPECIAL). The compiler
emits a static cost report per program: instruction count, per-class counts,
estimated cycles, DSP demand, table bytes, register high-water mark. The
numbers are provisional until the RTL profile engine pins real latencies;
the *mechanics* (class assignment, per-program report) are frozen.

Instruction ceilings per profile: field-ir.md §7.3 (earth 32, warp 48,
flow 48, formation 64, stamp 32; global 64 — provisional, R7).

## 2. Budget-line registry (Phase 1 entries)

Declared cost lines already ratified elsewhere; this file becomes their
index. Each line names its owner spec:

- `sky_triangles ≤ 352` total (192 drum + 16 cap + 2 under + 128 cloud +
  2 sun + margin), ×2 Duo views — spec/sky_and_beams.md §sky.
- `sky_fragments ≤ 92,160` (the clear it replaces) + cloud ≤ ~45K blended;
  VRAM ≈ 0.9 MB (~1.9% of the texture pool), shared between Duo views;
  measure-**exempt** with these declared budget lines, fully counted in
  charter §25 counters — spec/sky_and_beams.md §sky.
- God-beam post buffer: 96×60 / 2×64×48; 12 taps; decay 61/64; cost
  69,120 / 73,728 taps ≈ 4% of a 1.67M-cycle frame (100 MHz placeholder,
  Phase 0 freezes the clock); 11.5–12.3 KB M10K POSTBUF; ~0.5–1% ALM inside
  the 6% twod_post group — spec/sky_and_beams.md §beams.

## 3. Phase 3 expands

`costs.zcost` per-program artifact shape; interval-analysis-backed bounds
proofs replacing declared bounds (field-ir.md §9); hardware-validity refusal
rules (a program exceeding its profile envelope never loads); the full
budget-line registry cross-linked to design/budgets/ and the §25 counters.
