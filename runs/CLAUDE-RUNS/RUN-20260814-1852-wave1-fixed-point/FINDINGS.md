# FINDINGS — P3: Fixed-Point / Q-Format Specification & ZRef Numeric Library

**Run:** RUN-20260814-1852-wave1-fixed-point — **Date:** 2026-08-14 — **Agent:** RECON (P3)
*Persisted by orchestrator (subagent file-write blocked by harness policy).*
Sources: charter v0.2 (§8, §11, §13, §19, §20.1, §29), FORM codesign (§5, §9), online survey (citations per section, §9 below).

## 1. Notation and type table

The Q-format literature is split: TI counts the sign bit separately (`Q7.8` = 1+7+8 = 16 bits) while ARM counts it inside m (`Q7.8` = 15 bits) — documented on [Wikipedia: Q (number format)](https://en.wikipedia.org/wiki/Q_(number_format)), which also records range formulas and the conventional "mid values are rounded up" multiply rounding (K = 1<<(Q−1)).
**Decision:** `spec/qformats.md` uses explicit `S i . f` triples (sign/integer/fraction bits) and states once: "Qm.n here is TI-style: total = 1+m+n signed."

| Type | Storage | Format | Overflow | Rounding | Notes |
|---|---|---|---|---|---|
| `fx16` | s32 | S 1.15.16 | saturate + record | round-half-up | core Field IR / language scalar (step 2^-16 ≈ 0.015 mm at m-scale) |
| `fx24` | s64 | S 1.39.24 | saturate + record | round-half-up | sim truth, world truth, wide accumulators (dot3, audio, height cache) |
| `angle16` | u16 | U 0.0.16 *turns* | **wraps mod 2^16** | n/a | sin/cos phase; u16 add/sub is free mod-2π |
| `unit8` | u8 | U 0.0.8 | saturate 255 | round-half-up | weights, blend factors, envelopes |
| `height16` | s16 | S 1.7.8 (m) | saturate | round-half-up on bake-back | ±128 m, ~3.9 mm step; live math in fx16 (OPEN Q1) |
| `world2/3`, `velocity3` | 2–3 × fx24 (truth) / fx16 (field) | — | per component | — | space-typed per Form §5 |
| `colour8` | 4 × u8 | — | saturate | round-half-up | ARGB8888 working; RGB565 at resolve |
| `pixel`/`pixel_error` | fx16 | S 1.15.16 | saturate | — | ABI `fx16` |
| `mat4fx` | 16 × fx16 | S 1.15.16 | saturate/element + record | round-half-up | `SetView` |
| `rectfx` | 4 × fx16 | — | saturate | — | `TerrainField` footprint |
| `transform2fx` | 6 × fx16 (2×3) | — | saturate | — | `SurfaceStamp` |
| `screenXY` | s20 | S 12.8 px | clamp to guard band | round-half-up from fx16 | raster input (§3) |
| `edge` setup | s64 | subpixel² | bound 2^41 | exact pre-step | per-triangle setup |
| `edge` stepped | s32 | px·subpixel | margin | exact | in-tile |
| `invw24` | u24 | U 0.0.24 | saturate 0xFFFFFF | round-half-up | depth, larger = closer |
| `u/v_over_w` | s32 | S 8.24 | saturate | round-half-up | perspective numerators |
| `particle128` | u128 | §5.5 proposal | per field | — | provisional |

`mat4fx × vec4`: four 32×32→64 products summed in s64 in fixed index order (exact integer sum, no intermediate rounding), then one round-half-up rescale by 16 + saturate. Matches DSP MAC practice and the CNL/P0037 "products widen exactly" philosophy ([P0037r7](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0037r7.html), [CNL](https://johnmcfarlane.github.io/cnl/), [repo](https://github.com/johnmcfarlane/cnl)).

## 2. Arithmetic semantics

**Overflow (charter §29-11):** add/sub/mul/mad saturate and increment a named counter in a caller-supplied `SatLedger` — explicit parameter, no thread-locals/global state, counter names mirrored in `blocks.yml` and hardware counters. `angle16` wraps (definitional). Saturation chosen over wrapping because wrapping silently corrupts truth/geometry; precedent GTE 1.19.12 wraps with a sticky O-flag — we keep the flag, drop the wrap ([nocash PSX spec](https://problemkaputt.de/psx-spx.htm)).
**Rounding:** one primitive, `rescale(x,k) = (x + (1<<(k-1))) >> k` then saturate — round-half-up. Half-even rejected (conditional-increment cost in RTL, zero benefit to a bit-identical oracle); truncate rejected (systematic downward bias in chains). **Single-rounding rule:** any multiply-then-add instruction computes the exact wide-integer expression and rounds exactly once at the end.

```
fx16 fx_add/fx_sub:  r = a±b (s64); saturate→fx16 (count clamp in L)
fx16 fx_mul:         p = (s64)a*b (exact S 2.30.32); rescale(p,16,L)
fx16 fx_mad:         p = (s64)a*b + (s64)c<<16 (exact); rescale(p,16,L)   // ONE rounding
fx16 fx_div_exact:   (sim-only) round_half_up((s128)a<<16 / b); saturate
unit8 unit_mul:      ((u32)a*b + 128)>>8; clamp 255
angle16 ang_add:     (u16)(a+b)                                        // wraps
min/max/abs/clamp/select: pure integer, no rounding
```

## 3. Raster formats

**Subpixel = 8 bits.** D3D11 mandates exactly 8 bits (1/256 px); D3D10 ≥8; OpenGL `SUBPIXEL_BITS ≥ 4` ([Giesen](https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/); also [Scratchapixel](https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/rasterization-practical-implementation.html), [reddit thread](https://www.reddit.com/r/GraphicsProgramming/comments/avuita/help_with_subpixel_precision_for_software/), [SO overflow question](https://stackoverflow.com/questions/76444945/how-to-avoid-integer-overflow-of-fixed-point-in-rasterization), [16.16 rasterizer example](https://github.com/gustavopezzi/triangle-rasterizer-fix16)). **Prompt verification:** "PS1 GTE subpixel 12.20" is **false** — the GTE is fixed-point (vectors 1.3.12, accumulators 1.19.12) but the GPU takes **integer-only** screen coordinates: zero subpixel bits, which is why PS1 wobbles ([Pikuma](https://pikuma.com/blog/how-to-make-ps1-graphics), [Retrocomputing SE](https://retrocomputing.stackexchange.com/questions/17611/what-precision-did-the-original-playstation-use), [Copetti](https://www.copetti.org/writings/consoles/playstation/)).

**Edge functions** (Giesen bound: p-bit coords ⇒ |Orient2D| ≤ 2^(2p+1)−2; p = 20 for S 12.8 ⇒ s64 setup, s32 stepping inside 16×16 tiles):

```
a' = round_half_up(a.fx16 * 256)                     // S 12.8
E0 = (b.x−a.x)*(c.y−a.y) − (b.y−a.y)*(c.x−a.x)  s64  // subpixel² units
E' = E0 >> 8 (exact at pixel centers, low 8 bits constant per edge)
bias = is_top_left(edge) ? 0 : −1
inside(p) ⟺ E'(p) + bias > 0                         // D3D top-left fill convention
```

Adjacent-triangle exactly-once coverage is a SymbiYosys formal property (charter §20.4). Fill-convention and per-pixel-division grounding: [Chris Hecker, Perspective Texture Mapping 1–5](https://chrishecker.com/Miscellaneous_Technical_Articles).

**Depth — `invw24`:** 1/W is linear in screen space, so it interpolates with the same plane-equation machinery (Hecker; Giesen series).

```
w: fx16 clamped [wmin,wmax]; r = rcp_u24(normalize(w))
d = mul(r, scale) rescale round-half-up, saturate 0xFFFFFF   // w=wmin ⇒ d=0xFFFFFF exactly
test: pass ⟺ d_new > d_old (strict; ties fail; decals use explicit bias); clear = 0
```

Spec defines depth *as the rcp_u24 pipeline output* — that is what guarantees ZRef ≡ RTL bit-identity.

**Perspective UV:** interpolate `u_over_w`,`v_over_w` (S 8.24) and `invw24` by plane equation; per pixel `u = rescale((s64)u_over_w * rcp_u24(invw24_interp), ·, L)`. Hecker's span-subdivision approximation is deliberately not used — the exact per-pixel rcp (2-cycle pipelined) avoids his documented span-width-dependent rounding bug.

## 4. Division / reciprocal

Surveyed: digit-recurrence (exact, slow, tiny area), Goldschmidt (fast, multiply-bound rounding; [arXiv 2508.14611 FPGA divider](https://arxiv.org/html/2508.14611) surveys the family), Newton-Raphson-from-table (quadratic, every step specifiable as exact integer ops).
**Two tiers:** sim truth = exact `fx_div_exact` (128-bit intermediate); hardware/Field = `rcp_u24`:

```
input d in U 0.0.24, normalized so leading 1 at bit 23 (exponent e tracked)
x0 = T[idx], idx = next 8 bits; T generated by frozen formula; |err x0| ≤ 2^-9
x1 = rescale( x0*(2^17 − d30*x0), k, L )     // s32 Q 2.30, round-half-up
x2 = rescale( x1*(2^17 − d30*x1), k, L )
rcp = renormalize(x2)                        // total error ≤ 1 LSB
```

Decisive property: **2^24 input domain is exhaustively checkable offline** — golden table + full-run error assertion proves the ≤1-LSB bound once; RTL differentials replay sampled subsets; full-hash committed. Field IR exposes only `rcp` + `mul` (div = `mul(a, rcp(b))`, declared ≤1 LSB), mirroring GPU reciprocal-approximation shaping.

## 5. Trig, smoothstep, noise, particles

**sin/cos:** `angle16` u16 turns (quadrant = top 2 bits, index = next 8, t = next 6 bits << 2); table 257 entries `T[i] = round_half_up(sin(π/2·i/256)·2^16)` s18 Q1.16; `sin ≈ T[i] + rescale((T[i+1]−T[i])*t, 8)`; output fx16; `cos(a)=sin(a+16384)`. Error: linear-interp bound h²/8·max|f″| ([JOS](https://ccrma.edu/~jos/resample/Linear_Interpolation_Error_Bound.html), [Dannenberg/Bernstein CMU](https://www.cs.cmu.edu/~rbd/papers/tlu98/tlu98.pdf), practitioner threads [SO](https://stackoverflow.com/questions/1164492/sine-table-interpolation)/[s.e.d](https://groups.google.com/g/sci.electronics.design/c/UKwVg29ssRA)) with h=π/512 ⇒ 0.31 LSB(2^-16); + table rounding 0.5 + product rounding 0.5 ⇒ **≤ ~1.3 LSB total**. **Exhaustive 2^16 golden vectors committed.** 257×18b ≈ 4.6 Kbit (charter §7.3 budgets the table). CORDIC rejected: variable latency, per-iteration rounding spec, harder bit-identical mirroring.

```
smoothstep(e0,e1,x,L): t = clamp(fx_mul(x−e0, rcp(e1−e0), L), 0, 1)
                       fx_mul(t, 3·2^16 − fx_mul(2·2^16, t, L), L)   // t²(3−2t), exact const order
isqrt_approx(n): restoring integer sqrt on u32 — EXACT floor(√n), digit-recurrence,
                 ~16 cycles, property r² ≤ n < (r+1)² (exact > approximate sqrt, same rationale as rcp)
normalize_approx(v,L): components × rcp_u24(length); declared ≤2 LSB
noise2: value noise, integer hash frozen in spec
        n = x*0x9E3779B1 ^ (y*0x85EBCA6B ^ SEED); n ^= n>>13; n *= 0xC2B2AE35; n ^= n>>16
        lattice points hashed to U 0.0.16, bilinear with smoothstep weights; output fx16 [−1,1)
        (lattice-noise precedent: [Perlin](https://mrl.nyu.edu/~perlin/noise/); ours is value noise)
```

**Particle 128-bit proposal (OPEN Q3):** pos 3×18 S 5.13 (cell-relative) = 54; vel 3×12 S 1.0.11 = 36; age u7 + lifetime u7 = 14; species u6; size u8 U 0.4.4 px; spin u6 (angle16<<10); seed/flags u10. = 128 bits; alternative 160-bit if seed quality insufficient.

## 6. Library shape and tests

```
reference/include/zref/{zref_fixp.hpp, zref_rcp.hpp, zref_trig.hpp, zref_sat.hpp}
tools/fixgen/  → C++ constexpr tables + SV $readmemh files + golden vector binaries
```

Header-mostly; tables preferably `constexpr`-computed (no .cpp, cannot drift); CI asserts C++ table == SV table byte-for-byte (same philosophy as ABI generation, charter §19). Public API = concrete strong types (`struct fx16 { int32_t raw; }`, no implicit conversions); one internal `detail::fixed<Int,I,F>` template is allowed in C++ (the no-clever-templates rule targets RTL) but public API stays non-template. Operator overloading deferred; oracle code uses explicit `fx_mul(a,b,led)`-style functions only.

**Tests:** (1) exhaustive where feasible — sin/cos all 2^16 angles; rcp_u24 all 2^24 (offline full + committed sample/hash); unit8 pairs 2^16; isqrt properties; (2) committed golden vector binaries consumed identically by C++ and Verilator (charter §20.3); (3) rational oracle (`__int128`) over ≥2^26 random pairs asserting documented error bounds; (4) boundary corpus (±0, ±1, ±MAX, ±MIN, powers of two, 1-LSB-from-saturation, tiny reciprocals); (5) formal properties — shared-edge exactly-once, saturation monotonicity, angle-wrap identities, ledger no-clamp invariance.

## 7. `spec/qformats.md` outline

1. Notation (S i . f triples, TI-style equivalence) 2. Type table (§1 verbatim) 3. Core arithmetic pseudocode (§2) 4. `rescale()` — the one rounding primitive 5. SatLedger + counter names 6. `rcp_u24` spec + LUT formula + exhaustive-proof requirement 7. sin/cos/isqrt/normalize/smoothstep/noise2 exact constants 8. Raster formats (screenXY, edge, bias, invw24, u_over_w) 9. height16 ↔ fx16 conversions (pending Q1) 10. Particle packing (pending Q3) 11. fixgen table-generation contract + byte-identical C++/SV check 12. Golden-vector manifest 13. Change control (QFMT_VERSION in ABI version word).

## 8. Risks / open questions

Risks: 64-bit edge setup needs fixed-latency setup stage (guard-band size freezes widths — Q4); half-up bias accumulates in long integrator chains (audio in fx24); NR initial-table formula must be frozen pre-RTL; fx24 (s64) too expensive for RTL field datapaths — per-profile width policy needed before ops.yml freezes (Q2); particle seed starved at 10 bits (Q3).
Open questions: Q1 height16 split + deformed-cache width; Q2 Field IR internal widths; Q3 particle 128 vs 160 bit; Q4 guard-band extent (proposed ±2048 px ⇒ S 12.8, s64 setup); Q5 audio mixer adoption of this library (recommended, fx24 internal).
