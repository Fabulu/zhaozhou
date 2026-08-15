# Zhaozhou Fixed-Point Q-Format Specification — v1

**QFMT_VERSION = 1** (see §13 for change control)

This document is the single written law for every number the machine manipulates
(charter §8, §29-7). The C++ reference library `reference/include/zref/*.hpp`,
the table generator `tools/fixgen/`, and every RTL block that touches these
values cite the sections below by number in their header comments. Where this
document and any other text disagree, this document wins.

Ratified amendments baked in (PLAN.md §1.A/1.C, run RUN-20260814-1912):
A2 (sin/cos 257-entry Q1.16 quarter-wave), A3a (both `rcp_u24` and `field_rcp`),
A3b (single-rounding law), A3c (screenXY = S 12.8, 21-bit), A1/A1b (PCG
RXS-M-XS noise constants; bilinear value noise is a macro-expansion, not an
opcode), Q1 (height16 = S 1.7.8), Q2 (Field IR lanes = Q16.16), Q3
(particle128 provisional), Q4 (guard band ±2048 px provisional, widths frozen
now), Q5 (audio mixer adopts this library, fx24 accumulators, Phase 2).

---

## 1. Notation

- Formats are written as explicit triples **`S i . f`**: 1 sign bit, `i`
  integer bits, `f` fraction bits; unsigned as **`U i . f`** (no sign bit).
  `U 0.16` means all 16 bits are fractional.
- Where the shorthand `Qm.n` appears elsewhere in the project it is **TI-style**:
  `Qm.n` = `S m . n`, total = 1+m+n signed bits. Stated once here, nowhere else.
- `LSB` unqualified means one raw unit of the type under discussion
  (2^-16 for fx16, 2^-24 for fx24, etc.).
- Integer ops: `*` `+` `-` on `uN`/`sN` are exact two's-complement wrapping
  ops at the stated width; `<<`/`>>` are shifts at the stated width;
  `rescale(x,k)` is defined in §4.
- `round_half_up(n/d)` (integer division, d > 0) = `floor((n + floor(d/2))/d)`
  — ties round toward +infinity. Exact signed variant defined in §4.

## 2. Type table

| Type | Storage | Format | Overflow | Rounding | Notes |
|---|---|---|---|---|---|
| `fx16` | s32 | S 1.15.16 | saturate + record | round-half-up | core Field IR / language scalar (step 2^-16 ≈ 0.015 mm at m-scale) |
| `fx24` | s64 | S 1.39.24 | saturate + record | round-half-up | sim truth, world truth, wide accumulators (dot3, audio, height cache); **never in RTL field datapaths** (Q2) |
| `angle16` | u16 | U 0.0.16 *turns* | **wraps mod 2^16** | n/a | sin/cos phase; u16 add/sub is free mod-2π |
| `unit8` | u8 | U 0.0.8 | saturate 255 | round-half-up | weights, blend factors, envelopes (value = raw/256) |
| `height16` | s16 | S 1.7.8 (m) | saturate | round-half-up on bake-back | ±128 m, ~3.9 mm step; live math in fx16 (Q1, re-freeze at Phase 6 gate) |
| `world2/3`, `velocity3` | 2–3 × fx24 (truth) / fx16 (field) | — | per component | — | space-typed per Form §5 |
| `colour8` | 4 × u8 | — | saturate | round-half-up | ARGB8888 working; RGB565 at resolve |
| `pixel` / `pixel_error` | fx16 | S 1.15.16 | saturate | — | ABI `fx16` |
| `mat4fx` | 16 × fx16 | S 1.15.16 | saturate/element + record | round-half-up | `SetView` |
| `rectfx` | 4 × fx16 | — | saturate | — | `TerrainField` footprint |
| `transform2fx` | 6 × fx16 (2×3) | — | saturate | — | `SurfaceStamp` |
| `screenXY` | **s32 in ABI/reference; 21 bits in RTL** | **S 12.8 px** | clamp to guard band | round-half-up from fx16 | raster input (§8); ±2048 px guard band provisional (Q4), extent re-ratified at Phase 4/5 |
| `edge` setup | s64 | subpixel² | bound 2^43−2 (§8) | exact pre-step | per-triangle setup |
| `edge` stepped | s32 | px·subpixel | margin | exact | in-tile |
| `invw24` | u24 | U 0.0.24 | saturate 0xFFFFFF | round-half-up | depth, larger = closer (§8) |
| `u/v_over_w` | s32 | S 8.24 | saturate | round-half-up | perspective numerators (§8) |
| `particle128` | u128 | §10 | per field | — | provisional (Q3) |

Field IR lanes are **Q16.16 (s32) everywhere** (Q2); `fx24` exists only as a
ZRef sim-truth accumulator outside Field IR.

**Conversions:**
- `fx16 → unit8`: `u8` from `clamp(x, 0, 0xFFFF)/` rescale 8 → round-half-up.
- `unit8 → fx16`: `raw << 8` (exact).
- `fx16 → height16`: `rescale(x, 8)` then saturate s16 (round-half-up bake-back).
- `height16 → fx16`: `raw << 8` (exact).
- `fx16 → screenXY`: `rescale(x · 256, 16)` = `x` rescaled from 16 to 8
  fraction bits (round-half-up), then clamp to the guard band (§8).

**`mat4fx × vec4`:** four 32×32→64 products summed **exactly in s128** (fx16
lanes) in fixed index order (exact integer sum, no intermediate rounding; the
original v1 wording "summed in s64" is amended — the reference and tests
implement the exact s128 row sum, which is also what an RTL DSP chain must
accumulate before the single rounding), then one `rescale(·,16)` + saturate.
The single-rounding law (§3) applies.

## 3. Core arithmetic pseudocode

Overflow policy (charter §29-11): **saturate and record** in a caller-supplied
`SatLedger` (§5) — explicit parameter, no thread-locals, no globals.
`angle16` wraps (definitional, never records). Saturation is chosen over
wrapping because wrapping silently corrupts truth/geometry; the GTE precedent
keeps the sticky flag but drops the wrap.

```
fx16 fx_add(a,b,L):   r = (s64)a + b;            sat_s32(r, L, ADD)
fx16 fx_sub(a,b,L):   r = (s64)a - b;            sat_s32(r, L, ADD)
fx16 fx_mul(a,b,L):   p = (s64)a * b;            rescale_s32(p, 16, L, MUL)   // ONE rounding
fx16 fx_mad(a,b,c,L): p = (s64)a*b + (s64)c<<16; rescale_s32(p, 16, L, MUL)   // ONE rounding
fx16 fx_div_exact(a,b,L): q = round_half_up_s128((s128)a << 16, b); sat_s32(q, L, MUL)
                      // sim-only exact division; b==0 -> 0x7FFF_FFFF or 0x8000_0000 by sign, +RCP0

fx24 add/sub: s64 lanes, saturate s64, same counters.
fx24 mul/mad: p = (s128)a*b (+ (s128)c<<24); rescale_s64(p, 24, L, MUL).  // s128 in reference only; RTL never does fx24 (Q2)

unit8 unit_mul(a,b):  ((u32)a*b + 128) >> 8      // round-half-up; result ≤ 254, clamp 255 retained defensively
angle16 ang_add(a,b): (u16)(a + b)               // wraps, never records
angle16 ang_sub(a,b): (u16)(a - b)               // wraps
min/max/abs/clamp/select: pure integer, no rounding, no records
```

**Single-rounding law (A3b, ratified):** any multiply-then-add instruction —
`fx_mul`, `fx_mad`, `mat4fx × vec4`, any future fused op — computes the **exact
wide-integer expression** and rounds **exactly once** via `rescale(·,k)` at the
end. Double rounding (round the product, then round the sum) is *rejected*:
it adds bias in mad-chains and diverges from DSP MAC practice. There is no
second rounding primitive anywhere in the machine.

## 4. `rescale()` — the one rounding primitive

```
rescale_u(x, k) = k == 0 ? x : (x + (1 << (k-1))) >> k        // unsigned, k in [0,63]
rescale_s(x, k) = k == 0 ? x : (x + (1 << (k-1))) >> k        // signed x, arithmetic shift
```

i.e. round-half-up (ties toward +infinity), followed by saturation to the
destination width (recording in `SatLedger` when the destination records).
Rounding mode rationales, frozen:
- **round-half-up** — chosen: no conditional-increment cost in RTL, zero bias
  asymmetry that matters against a bit-identical oracle.
- **half-even** — rejected: conditional increment costs RTL, zero benefit to a
  bit-identical oracle.
- **truncate** — rejected: systematic downward bias in chains (R12: half-up
  bias accumulation in long integrator chains is recorded as a risk; audio
  adopts fx24 accumulators per Q5, revisit with Phase 2 mixer evidence).

`round_half_up_s(n, d)` for signed exact division (used by `fx_div_exact`,
table generation, and oracles): if `d < 0` negate both; then
`floor_div(n + d/2, d)` with `floor_div` the mathematical floor (C++ `/`
toward zero, corrected by remainder sign).

## 5. SatLedger and counters

```cpp
struct SatLedger {          // reference/include/zref/zref_sat.hpp
    uint32_t add;       // clamps in fx16/fx24 add/sub (incl. mat4fx row sums)
    uint32_t mul;       // clamps in fx_mul/fx_mad/fx_div_exact/fx24 mul/mad
    uint32_t rescale;   // clamps in bare rescale()/conversions (screenXY, height16, unit8)
    uint32_t unit;      // clamps in unit8 conversions (fx16→unit8 clamp at 1.0−ε, saturate 255)
    uint32_t rcp;       // clamps in field_rcp output saturation
    uint32_t rcp0;      // RCP0 events: rcp input == 0 (not a saturation; tracked alongside)
};
```

Rules: every recording operation takes `SatLedger* L` as an **explicit
parameter**; `nullptr` is allowed (count dropped, results unchanged —
"no-clamp invariance", asserted in `tests/unit/test_fixp.cpp`). Counter names
are mirrored by hardware performance counters and in `design/blocks.yml`.
Ledger counts are the *only* overflow observability (charter §29-11/12).

## 6. Reciprocals

Two distinct reciprocals exist (A3a). They serve different consumers and are
**never** conflated:

### 6.1 `rcp_u24` — raster/depth path (U 0.0.24)

Input `d` (u24, any nonzero value). The caller-facing wrapper normalizes;
the core works on the normalized mantissa:

```
rcp_u24(d):                       // d != 0 (d == 0 is a caller bug; asserted)
    m = d << e  until bit 23 set  // m in [2^23, 2^24), e = shifts (u32 wraps)
    r = rcp_u24_norm(m)
    return { r, k = e + 1 }       // reciprocal value ~= (r / 2^24) · 2^k

rcp_u24_norm(m):                  // m in [2^23, 2^24); returns r ~= 2^47 / m
    idx = (m − 2^23) >> 15                       // 8 bits below the leading 1
    x   = T24[idx]                               // T24 below; |rel err| <= 2^-9
    w   = (m · x) >> 24
    x   = rescale_u(x · ((2<<30) − w), 30)       // Newton–Raphson step 1
    w   = (m · x) >> 24
    x   = rescale_u(x · ((2<<30) − w), 30)       // Newton–Raphson step 2
    r   = rescale_u(x, 7)
    return r > 0xFFFFFF ? 0xFFFFFF : r           // pinned: only m == 2^23 hits 2^24
```

**Frozen table** (256 entries, generated by `tools/fixgen`, §11):

```
T24[idx] = round_half_up(2^54 / (2^23 + idx·2^15 + 2^14))     // bucket midpoint
```

**Error bound (frozen, exhaustively proven):** for all m ∈ [2^23, 2^24),
`|rcp_u24_norm(m) − 2^47/m| ≤ 1` LSB. Measured maximum = exactly 1.0 at the
single pinned input m = 2^23 (result saturates 2^24 → 0xFFFFFF); every other
input is < 1. Full-domain (all 2^24 − 1 nonzero d through the wrapper)
FNV-1a-64 hash over the 3 LE bytes of each `r`:
**`RCP24_FULL_HASH = 0xd624beb8659baf83`**
(FNV-1a-64: offset basis 14695981039346656037, prime 1099511628211, input
order d = 1, 2, …, 0xFFFFFF). The exhaustive sweep runs in CTest label
`nightly` (`test_fixp --rcp-full`); the committed ≥2^20 sample is checked in
`fast` against the golden binary (§12). Depth is *defined* as the output of
this pipeline (§8) — that is what guarantees ZRef ≡ RTL bit-identity.

Derivation (why two NR steps): initial relative error ≤ 2^-9 (bucket half-width
2^14, slope 1/a² ≈ 4 at a ≈ 0.5); NR squares it: 2^-18 after step 1, 2^-36
after step 2; internal roundings add ≈ 2^-30; the final `rescale(·,7)` adds
≤ 0.5. Total ≪ 1 LSB of the u24 result.

### 6.2 `field_rcp` — Field IR RCP opcode (Q16.16)

Input `a` (fx16). Output fx16 + sticky RCP0 in the ledger.

```
field_rcp(a, L):
    if a == 0:   L->rcp0++;  return 0x7FFF_FFFF            // pinned (A3a); positive infinity stand-in
    s = (a < 0);  n = |a|                                   // n in [1, 2^31]
    m = n << e until bit 31 set                             // m in [2^31, 2^32), e = 31 − shifts in [0,31]
    idx = (m − 2^31) >> 23                                  // 8 bits, all 256 entries used
    x   = TF[idx]                                           // x ~= 2^47 / m, |rel err| <= 2^-9
    x   = rescale_u(x · ((1<<48) − m·x), 47)                // ONE pinned linear (NR) correction
    r   = rescale_u(x << 16, e)                             // r ~= 2^32 / n  (exact when e <= 16)
    return saturate_s32(±r, L, RCP)                         // |a| <= 2 saturates (a = +2 -> +0x7FFF_FFFF, error exactly 1 LSB)
```

**Frozen table** (256 entries, generated by `tools/fixgen`, §11):

```
TF[idx] = round_half_up(2^47 / (2^31 + idx·2^23 + 2^22))    // bucket midpoint
```

**Error bound (frozen):** for a ≠ 0,
`|field_rcp(a) − 2^32/a| ≤ |2^32/a| · 2^-14 + 1` (raw units).
Measured: relative error ≤ 2^-15.93 for |result| ≥ 1.0 (dense sweep of
n ∈ [1, 2^20] + strided full domain); absolute error ≤ 0.72 LSB for results
< 1.0. A second software refinement (RCP then MUL) is expressible as a
macro-expansion if a future profile needs tighter; the opcode itself stays
one-correction (A3a: "pinned linear correction").

`fx_div_exact` (§3) remains the sim-truth tier; Field IR division is
`MUL(a, RCP(b))` with the bound above declared on the RCP and the single
rounding of the MUL per §3.

## 7. Trig, sqrt, normalize, smoothstep, noise

### 7.1 sin/cos — 257-entry Q1.16 quarter-wave (A2)

- `angle16` is u16 **turns**. `fx_sin(a)`, `fx_cos(a)`; `fx_cos(a) = fx_sin(a + 0x4000)` (exact by construction).
- Quarter-wave table, 257 entries of Q1.16 in an s18 (stored wider):

```
T[i] = round_half_up(sin(π/2 · i / 256) · 2^16),  i = 0..256      // T[0]=0, T[256]=0x10000
```

- Interpolation (all-integer): with q = a >> 14 (quadrant), a13 = a & 0x3FFF,
  i = a13 >> 6 (8-bit index), t = a13 & 0x3F (6-bit sub-tick):

```
qw(a13) = T[i] + rescale((T[i+1] − T[i]) · t, 6)                  // quarter-wave value, s18
fx_sin(a) = q even ? +qw(a13) : −qw(a13)      (sign by quadrant, mirrored index 0x4000 − a13)
fx_sin:  q0: +qw(a13)   q1: +qw(0x4000 − a13)   q2: −qw(a13)   q3: −qw(0x4000 − a13)
```

- **Error bound (frozen, exhaustively proven over all 2^16 angles):**
  `|fx_sin(a)/2^16 − sin(2π·a/2^16)| ≤ 1.3 · 2^-16`.
  Derivation: interpolation error ≤ h²/8·max|f″| = (π/512)²/8 ≈ 0.31 LSB;
  table rounding ≤ 0.5; product rounding ≤ 0.5; total ≤ 1.31 LSB.
  Measured maximum vs libm: **1.1772 LSB**. Exhaustive 2^16 golden vectors are
  committed (§12) and asserted in `fast`; the libm bound is asserted with
  tolerance 1.35 LSB.
- Identities (exact, asserted exhaustively): `fx_sin(−a) = −fx_sin(a)`;
  `fx_sin(0x8000 − a) = fx_sin(a)`; `fx_sin(0) = 0`, `fx_sin(0x4000) = 0x10000`,
  `fx_sin(0x8000) = 0`, `fx_sin(0xC000) = −0x10000`.
- 257 × 18 b ≈ 4.6 Kbit (charter §7.3 M10K budget). CORDIC rejected: variable
  latency, per-iteration rounding, harder bit-identical mirroring.

### 7.2 `isqrt_u32(n)` — exact floor square root

Restoring digit-recurrence on u32, ~16 cycles, RTL-friendly. Property (exact,
asserted): `r² ≤ n < (r+1)²`. Exact is chosen over approximate for the same
reason as exact `rescale`: the oracle is the definition (§7.4 uses a u64
variant internally).

### 7.3 `smoothstep(e0, e1, x, L)`

```
d  = fx_sub(e1, e0, L)
r  = field_rcp(d, L)                       // d == 0 -> pinned r + RCP0
t  = fx_mul(fx_sub(x, e0, L), r, L)        // single-rounded (§3)
t  = clamp(t, 0, 0x10000)
t2 = fx_mul(t, t, L)                       // t²
result = fx_mul(t2, fx_sub(3<<16, fx_mul(2<<16, t, L), L), L)  // t²(3−2t)
```

(NB: the P3 recon sketch wrote `fx_mul(t, 3<<16 − fx_mul(2<<16,t))`, which
computes t·(3−2t), not t²(3−2t) — that polynomial is *decreasing* for
t > 0.75 and was caught by the monotonicity test. The corrected form above
is the C1 Hermite weight; endpoints exact: f(0)=0, f(1/2)=1/2, f(1)=1.)

### 7.4 `normalize_approx(v, L)`

```
n2  = (u128)x² + y² + z²                   // exact; u128 in reference only
len = isqrt_u64(n2)                        // exact floor, r² ≤ n2 < (r+1)²
if len == 0: return {0, 0, 0}              // pinned
m = len normalized to [2^23, 2^24), net shift e (m = len · 2^−e)
r  = rcp_u24_norm(m)
out_i = rescale_s(v_i · r, 31 + e, L)      // ONE rounding per component
```

**Declared bound:** each output component is within **2 LSB (2·2^-16)** of the
exact unit-vector component when `n2 ≥ 2^48`; degenerate vectors return the
deterministic floor-based result above. Measured maximum: 0.51 LSB.
(Consumers: Field IR NORMALIZE2/3 — declared error rides the ops.yml contract.)

### 7.5 noise2 — PCG RXS-M-XS lattice hash (A1/A1b)

The NOISE2 *opcode* returns two decorrelated unit lanes at a lattice point:
`(h0 >> 16, h1 >> 16)`, each U 0.0.16. The bilinear smooth value noise
(fx16 ∈ [−1,1)) is a **mandated macro-expansion** (floor16, MAD lerp,
smoothstep weights), documented once in `spec/form/field-ir.md` (W5) — not an
opcode (charter §29-6).

**Constants frozen verbatim** (P4 §6.4, ratified A1; identical text lives in
field-ir.md):

```
noise2_hash(x, y, seed, lane):                    // all u32, wrapping ops
    s = (x · 0x9E3779B1) ^ ((y · 0x85EBCA77) ^ seed)   // lattice mix
    s = s + lane · 0xE1                                 // lane salt; lane ∈ {0, 1}
    s = s · 747796405 + 2891336453                      // PCG LCG advance (M, inc)
    w = ((s >> ((s >> 28) + 4)) ^ s) · 277803737        // RXS-M-XS output permutation
    return (w >> 22) ^ w
lane value = noise2_hash(...) >> 16                    // U 0.0.16
```

Known-answer vectors are committed (§12, `noise2_kat.bin`) and asserted in
C++ and TS — the reference implementation lives in
`reference/include/zref/zref_fixp.hpp` (`noise2_hash`), the Field IR
interpreter (W5) calls it; there is no third implementation.

## 8. Raster formats

**Subpixel = 8 bits** (D3D11 mandates exactly 8; D3D10 ≥ 8; OpenGL requires
`SUBPIXEL_BITS ≥ 4` — we freeze 8, like D3D11).

**screenXY (A3c, ratified):** S 12.8 px = **21-bit value**; stored s32 in the
ABI and ZRef; RTL carries 21 bits. Guard band **±2048 px provisional** (Q4:
widths frozen now, extent re-ratified at Phase 4/5 against raster evidence).
Conversion from fx16: `rescale(x · 256, 16)` = drop 8 fraction bits
round-half-up, then clamp to the guard band.

**Edge functions** (Giesen bound: p-bit coords ⇒ |Orient2D| ≤ 2^(2p+1) − 2;
p = 21 for S 12.8 ⇒ **s64 setup**, bound 2^43 − 2; s32 stepping inside
16×16 tiles):

```
a'  = rescale(a.fx16 · 256, 16)                      // to S 12.8
E0  = (b.x−a.x)·(c.y−a.y) − (b.y−a.y)·(c.x−a.x)  s64 // subpixel² units, exact
E'  = E0 >> 8 (exact at pixel centers; low 8 bits constant per edge)
bias = is_top_left(edge) ? 0 : −1
inside(p) ⟺ E'(p) + bias > 0                          // D3D top-left fill convention
```

Adjacent-triangle exactly-once coverage is a SymbiYosys formal property
(charter §20.4; lands with the rasterizer, Phase 4/5).

**Depth — `invw24`:** 1/W is linear in screen space; it interpolates with the
same plane-equation machinery:

```
w: fx16 clamped to [wmin, wmax]; r = rcp_u24(normalize(w))
d = rescale(r · scale) round-half-up, saturate 0xFFFFFF   // w == wmin ⇒ d == 0xFFFFFF exactly
test: pass ⟺ d_new > d_old (strict; ties fail; decals use explicit bias); clear value = 0
```

Depth is **defined as the rcp_u24 pipeline output** (§6.1) — that is what
guarantees ZRef ≡ RTL bit-identity.

**Perspective UV:** interpolate `u_over_w`, `v_over_w` (S 8.24) and `invw24`
by plane equation; per pixel `u = rescale((s64)u_over_w · rcp_u24(invw24_interp))`.
Hecker's span-subdivision approximation is deliberately **not** used — the
exact per-pixel rcp avoids his documented span-width-dependent rounding bug.

## 9. height16 ↔ fx16 (Q1, decided)

`height16` = **S 1.7.8** (s16): ±128 m, step 2^-8 m ≈ 3.9 mm. Live terrain
math runs in fx16; height16 is the baked-cache/ABI storage. Conversions in
§2. Re-freeze only at the Phase 6 gate with rendering evidence.

## 10. particle128 (Q3, provisional)

128-bit particle record, **provisional** (charter §13 "provisionally 128";
Phase 10 revisits seed starvation; 160-bit alternative if seed quality
insufficient):

| Field | Bits | Format |
|---|---|---|
| pos (cell-relative) | 3 × 18 = 54 | S 5.13 |
| vel | 3 × 11 = 33 | S 1.0.10 |
| age | u7 | ticks |
| lifetime | u7 | ticks |
| species | u6 | — |
| size | u8 | U 0.4.4 px |
| spin | u6 | angle16 >> 10 |
| seed/flags | u7 | — |

(Sums to exactly 128; the P3 recon sketch's field list summed to 134 —
corrected here. Nothing in wave 1 consumes particle128.)

## 11. fixgen contract

`tools/fixgen` (TypeScript, zero runtime deps) is the **single generator** for
all tables and golden vectors in this spec. One source of truth emits:

| Output | Consumer | Format |
|---|---|---|
| `reference/include/zref/generated/zref_tables.hpp` | ZRef C++ | `constexpr` arrays + `QFMT_VERSION`, hex literals |
| `fpga/rtl/generated/tables/sin_q16.mem` | SV `$readmemh` | one hex word per line, width per §7.1 |
| `fpga/rtl/generated/tables/rcp24_t0.mem` | SV `$readmemh` | 256 words, 31-bit |
| `fpga/rtl/generated/tables/field_rcp_t0.mem` | SV `$readmemh` | 256 words, 17-bit |
| `compiler/src/generated/tables.ts` | TS | `export const` arrays, hex literals |
| `tests/golden/fixp/*.bin` + `manifest.json` | tests | exact little-endian byte dumps (§12) |

Determinism rules: no timestamps, no host paths, LF line endings, fixed hex
widths, trailing newline; identical hex digit strings across all three
languages. `npm run tables:gen` regenerates; `npm run tables:check`
(= `npm run -w tools/fixgen check`) regenerates in memory and byte-compares
against the committed files — drift fails CI and the local CTest `fast` label
(test `tables_check`). Generated outputs are **committed**; they are evidence,
not build artifacts.

## 12. Golden-vector manifest

`tests/golden/fixp/manifest.json` — integer/string fields only, **no floats
anywhere**; every file carries a SHA-256 (`node:crypto`, i.e. the platform's)
and record count:

| File | Records | Layout (little-endian) |
|---|---|---|
| `sin_cos_u16.bin` | 65536 | per angle a (ascending): s32 fx_sin(a).raw, s32 fx_cos(a).raw |
| `unit8_mul_u8.bin` | 65536 | out[a·256 + b] = unit_mul(a, b), u8 |
| `rcp24_sample.bin` | 1048576 | per i (ascending): u32 d = (i·16 + (i & 15)) \| 1 — d = 0 excluded by construction — then u32 r = rcp_u24(d).r |
| `noise2_kat.bin` | 1024 | per record: u32 x, y, seed, h0, h1 (x = i·2654435761 mod 2^32, y = i·40503, seed = i·0x9E3779B1 mod 2^32 + 1) |

The C++ tests consume these binaries **as the oracle** (charter §19 byte
discipline): the C++ implementations must reproduce them exactly (sin/cos,
unit8, noise2) or within the frozen ≤1 LSB bound (rcp_u24 sample — the TS
generator implements the same integer algorithm in BigInt, so exact equality
is expected and asserted).

## 13. Change control

`QFMT_VERSION` (currently **1**) bumps on **any** change to: a type's width or
format, the rounding/saturation laws (§3–§5), any frozen constant or table
formula (§6–§7), a golden-vector layout (§12), or the fixgen output set (§11).
A bump requires: spec amendment, full regeneration + recomittance of tables
and goldens, re-derivation of every error bound, and orchestrator sign-off.
The constant travels in the ABI (`spec/commands.zidl`, `const u16
QFMT_VERSION` — emitted into all three generated languages) so capture replay
can refuse mismatched numerics. Editorial changes (prose, citations,
provisional extents re-ratified per Q4) do not bump.
