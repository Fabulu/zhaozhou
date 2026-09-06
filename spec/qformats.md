# Zhaozhou Fixed-Point Q-Format Specification — v1

**QFMT_VERSION = 3** (see §13 for change control)

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
**C1 (2026-08-17, QFMT_VERSION 1 → 2:** the `quat16` quantised-quaternion
lane — `spec/creature_rules.md` §2.1's 8 B/bone/frame clip format frozen as
S 1.0.14 with hemisphere-canonical quantisation and the 9-product
single-rounding decode of §7.6; evidence: the creature reference lane, run
RUN-20260816-0046, commit `bd1c733`, measurements restated in §7.6 and
asserted as regression tripwires in `tests/geometry/creature_core.cpp`.
No existing type, table, or golden changed — the bump travels so capture
replay can refuse pre-C1 numerics if creature clip pages ever reach a
capture)**. The fog law (§8, same wave) is prose + formula only and adds no
type, so it rides the same bump without a second one.

**C2 (2026-09-02, QFMT_VERSION 2 → 3):** the **particle128 v1 numeric law** of
§10, ruled by the owner (R3 of
`reports/OWNER-RULINGS-BUILDABILITY-20260902.md`). §10 was marked *provisional*
and the layout it carried was not the ruled one — different widths on every
field, `lifetime` stored per record rather than in the species descriptor, and
positions declared S 5.13 where the ruling says S 9.8. It is replaced whole,
and `PARTICLE_FORMAT_VERSION = 1` is introduced alongside. No table or golden
of §6/§7/§12 changed; the bump travels so capture replay can refuse pre-C2
particle numerics.

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
| `quat16` | 4 × s16 | S 1.0.14 per lane (§7.6) | per lane: saturate s16 | round-half-up on quantize | unit quaternion, hemisphere-canonical; the 8 B/bone/frame creature clip lane (C1) |

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
qw(a13) = i == 256 ? T[256] : T[i] + rescale((T[i+1] − T[i]) · t, 6)   // quarter-wave value, s18
fx_sin(a) = q even ? +qw(a13) : −qw(a13)      (sign by quadrant, mirrored index 0x4000 − a13)
fx_sin:  q0: +qw(a13)   q1: +qw(0x4000 − a13)   q2: −qw(a13)   q3: −qw(0x4000 − a13)
```

- **Endpoint guard (spec defect fixed 2026-08-15).** `a13 = 0x4000` — the
  mirrored index of `a13 = 0`, reached by every `fx_cos(0)` / `fx_cos(0x8000)`
  and by `fx_sin(0x4000)` / `fx_sin(0xC000)` — gives `i = 256`, so the
  unguarded slope `T[i+1] − T[i]` requires a **T[257] that does not exist**.
  The `i == 256` case above returns `T[256]` directly. `t` is 0 there, so no
  value changes; the guard removes an out-of-bounds read (a hard error under
  `constexpr`, and an X-propagation hazard in RTL). Implementations MUST carry
  the guard: `reference/include/zref/zref_trig.hpp`,
  `compiler/src/field_ir/numeric.ts`.
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

### 7.6 `quat16` — the quantised-quaternion lane (C1)

The creature clip-bank storage of `spec/creature_rules.md` §2.1 (8 B per
bone per frame) and the GEOM.POSE decode input. One implementation:
`reference/include/zref/zref_creature.hpp` (`quat16_quantize`,
`quat16_axis_angle`, `quat16_to_mat3`); RTL mirrors it at Phase 9 against
the same anchors.

**Lane format:** four s16 lanes `(w, x, y, z)`, each **S 1.0.14** —
`raw = round_half_up(q · 2^14)`, saturate at ±16384. Chosen over
S 1.0.15: the quaternion sum-of-squares is ≤ 1, so every lane fits [−1, 1]
in 1+14 bits with the guard bit absorbing the 0.5-LSB quantisation
overshoot. A saturated lane means the author fed a non-unit quaternion —
a tool-side bug; the quantiser clamps silently (no SatLedger — the clip
compiler is the gate, and the declared bounds below hold for unit
inputs).

**Quantisation is hemisphere-canonical:** negate all four lanes when
`w < 0`, or `w = 0 ∧ x < 0`, or `w = x = 0 ∧ y < 0`, or only `z < 0`.
A quaternion and its negation encode the same rotation and MUST quantise
identically — clip compression must not depend on which hemisphere the
author exported. Deterministic authoring path: `quat16_axis_angle` builds
lanes from a unit axis (fx16) and half-angle sin/cos (the §7.1 tables),
`w = cos`, vector lanes `axis · sin`, each single-rounded per §3.

**Decode — the 9-product formula, ONE rounding per element, NO
renormalisation** (the §3 single-rounding law applied verbatim; with lanes
`Qw..Qz` in 2^14 scale, matrix elements in Q16.16):

```
diag_ijj = 65536 − rescale(Qa² + Qb², 11)          // 1 − 2(qa² + qb²)
off_ij   = rescale(Qa·Qb ± Qc·Qd, 11)              // 2(qa·qb ∓ qc·qd)
```

Products exact in s64 (28-bit lane products), the sums exact, then ONE
`rescale(·, 11)` round-half-up per element and saturate. The shift: a lane
product lives at 2^28 scale and the matrix element at 2^16 — 12 bits down —
but the formula's leading factor 2 hands one bit back, so the net
rescale is 11. Renormalisation is **rejected**: it would insert a second
rounding
stage (isqrt + rcp + 4 muls per bone) into every decode, invalidate every
committed anchor, and buy only the scale correction whose residue is
declared below — the drift is bounded and carried, not repaired.

**Exactness laws (frozen):** identity `(16384, 0, 0, 0)` decodes to the
exact identity matrix; **180° about a principal axis** (one vector lane
±16384, w = 0) decodes exactly. **90° about a principal axis is NOT exact
and cannot be in any power-of-two quaternion lane** — √2/2 is irrational;
in S 1.0.14 it quantises to 11585 (error 0.23 LSB) and the decoded off-axis
elements sit 3 LSB from 65536. The GEOM.POSE contract's directed-test
wording is "identity/180° exact, 90° within the declared bound" (the
original "identity/90° exact" wording was unachievable and is corrected by
this amendment).

**Declared bounds (C1, measurement-cited — commit `bd1c733`,
`tests/geometry/creature_core.cpp` §2, re-observed 2026-08-17):**

| Quantity | Bound | Mechanism |
|---|---|---|
| decode element error vs the quantised-lane oracle | **≤ 0.50 LSB** (fx16 element) | the single-rounding bound — each element rounds exactly once (§4) |
| decoded column-norm drift | **≤ 15.86 LSB** (≤ 2.4·10⁻⁴ relative) | no renormalisation: lane quantisation leaves ‖q‖² = 1 + ε, decode adds ≤ 0.5 LSB/element |
| end-to-end column angle error vs the true rotation | **≤ 0.0156°** | authoring fx-table error + quantisation + decode, composed |

Measurement protocol (frozen with the bounds): a deterministic axis-angle
grid — 5 axes (three principal, (1,1,1)/√3, (1,1,0)/√2) × 720 half-angle
steps = **3,600 rotations** — authored through the integer path
(`fx_sin`/`fx_cos` half-angle tables) and decoded; the oracle is
double precision and belongs to the TEST, not the implementation (the
libm-oracle precedent of §7.1). Two oracles carry two numbers: the
quantised-lane oracle (exact matrix of the quantised quaternion) isolates
the decode formula for the element bound; the true-rotation oracle carries
the end-to-end angle. Committed tripwires assert the looser regression
bounds — element ≤ 1 LSB, norm ≤ 20 LSB, angle ≤ 0.05° — so any lane change
that moves the measured numbers re-runs the sweep before it can pass.

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
E'  = E0 >> 8 (tile-local stepping form; low 8 bits constant per edge)
bias = is_top_left(edge) ? 0 : −1                     // subpixel² units
inside(p) ⟺ E0(p) + bias ≥ 0                          // D3D top-left fill convention
```

**The comparison is `≥ 0`, not `> 0`, and it is taken on the EXACT `E0`, not
on the floored `E'` (spec defects fixed 2026-08-15).**

With a strict `>`, a pixel centre lying exactly on a shared edge (`E0 = 0`) is
rejected by BOTH triangles — the top-left side fails `0 > 0` and the
non-top-left side fails `−1 > 0` — so the shared edge is a line of holes.

With `≥` but the bias applied to `E' = E0 >> 8`, a second class of hole
survives. Flooring merges *strictly inside by less than one subpixel² unit*
(`E0 ∈ [1,255] ⇒ E' = 0`) with *exactly on the edge* (`E0 = 0 ⇒ E' = 0`). The
two sides of a shared edge see `(E0, −E0)`, i.e. `(E', −E'−1)`, so for
`E0 ∈ [1,255]` the non-top-left owner rejects `E' = 0` on its `−1` bias and
the other side sees `E' = −1` and rejects too — a hole on a pixel that is
strictly inside one of the two triangles. (Observed at a seam fraction of
89/256 with `E0 = ±128`.)

Taken on `E0`, the rule is literally D3D's: accept `E0 > 0` always, and accept
`E0 = 0` only on a top-left edge (the `−1` bias turns `≥ 0` into `≥ 1` for the
other side). Every pixel is then covered exactly once.

The RTL keeps its s32 tile-local stepping: because the low 8 bits of `E0` are
constant per edge, with `r = E0 & 255` the two tests are `E0 ≥ 0 ⟺ E' ≥ 0` and
`E0 ≥ 1 ⟺ E' > 0 ∨ (E' = 0 ∧ r ≠ 0)` — one extra constant bit per edge, no
wider datapath.

**Bounding box (spec defect fixed 2026-08-15).** A pixel is a candidate iff
its CENTRE — subpixel `256·p + 128` — can lie in the triangle, so the scan
range on each axis is

```
p_min = ceil((v_min − 128)/256) = (v_min + 127) >> 8
p_max = floor((v_max − 128)/256) = (v_max − 128) >> 8
```

**not** `ceil(v_min/256)`. Using `ceil(v_min/256)` skips one whole column (or
row) whenever `v_min`'s subpixel fraction is in `(0,128]`: that pixel's centre
is inside, but the walker never tests it. Across a shared seam the other side's
edge functions correctly reject the same pixel, so it is covered by NOBODY and
the clear colour shows through — a full-height 1-pixel crack at every drum
column / terrain grid boundary. The bbox is part of the coverage law, not an
optimization, and the exactly-once property below is only true with it.

Adjacent-triangle exactly-once coverage is a SymbiYosys formal property
(charter §20.4; lands with the rasterizer, Phase 4/5). The software raster
asserts it today over a shared-edge quad
(`tests/render/render_directed.cpp`, `test_shared_edge_exactly_once`).

**Depth — `invw24`:** 1/W is linear in screen space; it interpolates with the
same plane-equation machinery:

```
w: fx16 clamped to [wmin, wmax]; r = rcp_u24(normalize(w))
d = rescale(r · scale) round-half-up, saturate 0xFFFFFF   // w == wmin ⇒ d == 0xFFFFFF exactly
test: pass ⟺ d_new > d_old (strict; ties fail; decals use explicit bias); clear value = 0
```

Depth is **defined as the rcp_u24 pipeline output** (§6.1) — that is what
guarantees ZRef ≡ RTL bit-identity.

**The three profiles, and where `scale` comes from.** *(Added 2026-08-31, from
owner ruling 2026-08-31 #1. Until now this section named `wmin`, `wmax` and
`scale` and defined none of them, which is what blocked the depth work for a
whole wave — a formula whose constants are undefined is not a specification.)*

| # | profile | `wmin` | `wmax` | `scale` | `d(wmax)` |
|---|---|---|---|---|---|
| 0 | `WORLD_LONG` | 1.0 m | 16,384 m | 1,099,511,627,776 = 2⁴⁰ | 1024 |
| 1 | `WORLD_STANDARD` | 0.5 m | 8,192 m | 549,755,813,888 = 2³⁹ | 1024 |
| 2 | `CLOSE` | 0.25 m | 2,048 m | 274,877,906,944 = 2³⁸ | 2048 |

**`scale` is GENERATED, never hand-written.** `tools/fixgen` emits the table to
`reference/include/zref/generated/zref_depth.hpp` and
`compiler/src/generated/depth.ts`; `tests/proofs/depth_profile_law.cpp` derives
the same values independently in C++ and proves the properties. A hand-copied
constant here is how a wrong number becomes an unadjustable one.

The generator solves, per profile:

```
s0     = smallest shift with (Wmin >> s0) < 2^24     // W = w in fx16 raw units
{r0,k0}= rcp_u24(Wmin >> s0)
scale  = round( 0xFFFFFF · 2^(48 + s0 - k0) / r0 )
```

then evaluates any `w` through the pipeline the block diagram above states:

```
s      = smallest shift with (W >> s) < 2^24
{r, k} = rcp_u24(W >> s)
d      = rescale(scale · r, 48 + s - k), round-half-up, saturate 0xFFFFFF
```

**`scale` is solved from the law's own OUTPUT at `wmin`, not from the ideal
reciprocal.** The obvious `scale = 0xFFFFFF · Wmin` yields `0xFFFFFE` — one
short of the pin — because `rcp_u24` carries up to 1 LSB and the pinned input is
exactly where it saturates.

**`scale` is a power of two in all three, and that is a coincidence of these
near planes**, which are themselves powers of two in raw units. The multiply is
therefore a shift *today*; a fourth profile with `wmin = 0.7 m` would not have
that property, and **the generator must not assume it**.

Proved for each profile over ~39,000 geometric samples and 200,000 consecutive
raw units at the near plane: `d(wmin) = 0xFFFFFF` exactly, monotonic
non-increasing, non-zero floor at `wmax`, no intermediate wrap.

**`wmax` is a depth CLAMP, not a far-clip plane.** `GEOM.PROJECT` row 2 stays
inert and the culler stays at five planes, so distant islands still draw — they
simply share the floor depth.

**Which profile a view uses IS decided, and this paragraph was stale.**
*(Corrected 2026-09-06.)* Ruling #1 permitted either two reserved bits of
`SetView.flags` or a small additive view-depth command and left the audit to
decide; the audit chose the flag bits and **D10 step 3 landed them** (`ca7b328`).
`zref::render::render_frame` reads `SetView.flags[1:0]`, refuses the reserved
value 3 with `ZH_ABI_BAD_VALUE`, and stores the accepted profile per view.

**But nothing yet OBSERVES it, and that is the honest state** (external audit
2026-09-05, finding R3). Three places currently disagree and each was written
truthfully at a different time:

| where | what it says |
|---|---|
| this section, before today | nothing selects a profile; `WORLD_STANDARD` only |
| `render_frame.cpp` | `SetView.flags[1:0]` selects; default 0 = `WORLD_LONG` |
| `rast.cpp` | no profile at all — the depth lane is Q16.16 `1/w` |

The raster's lane is Q16.16 `1/w` by plan W3.5 / D7, and its header calls the
change to the `invw24` pipeline a *frozen* one-line switch in `project_vertex`.
That freeze is a PIPELINE-PHASE decision and is not resolved by the ABI ruling,
which settled how a profile is NAMED and not when the Phase-3 raster stops using
its own depth lane. `ProjOut` already carries `w` alongside `1/w` for exactly
this, so the switch stays one line.

**So: parsing and validating a profile is not proof that rendered occlusion
observes it, and no test should be read as claiming otherwise.** What IS
established today is the command ABI and the profile table; the raster's
adoption of `invw24` is Phase-4 work.

Note the two defaults do not agree either: this section said `WORLD_STANDARD`
(profile 1) and the parser defaults to 0 (`WORLD_LONG`), because a zero-filled
capture must keep the profile it was recorded under. When the switch is taken,
that is the first thing to settle -- a default chosen by which file you read is
how a wrong number becomes an unadjustable one.

**Perspective UV:** interpolate `u_over_w`, `v_over_w` (S 8.24) and `invw24`
by plane equation; per pixel `u = rescale((s64)u_over_w · rcp_u24(invw24_interp))`.
Hecker's span-subdivision approximation is deliberately **not** used — the
exact per-pixel rcp avoids his documented span-width-dependent rounding bug.

**Fog — deterministic, per-vertex.** *(Added 2026-08-17, lighting & pose
consolidation wave. Charter §8 names "deterministic fog" a non-negotiable
basic, §16 lists a fog sheet under Twin Horizons, and §20.1 makes ZRef
fog-exact — before this paragraph no spec owned a formula, Q-format, state,
or placement; there was no earlier law to supersede.)*

**State** travels in `SetEnvironment 0x0311` (wire: `spec/commands.zidl`;
semantics: `spec/sky_and_beams.md` §4a): `fog_mode` (u8 enum: 0 off,
1 linear), `fog_near`/`fog_far` (fx16, world metres). `fog_far ≤ fog_near`
disables fog regardless of mode — a deterministic no-op, not an error. The
donor's `envi.d` carried linear AND exponential fog
(`fog {linear|exponential, nearZ/farZ/density, RGB}` — S6 §1); **exponential
is deferred, not refused**: an exact fixed-point exp(−λd) needs a
per-density table rebuild (the ARM palette lane could carry one) or a
multi-step product series, and neither is costed nor consumed today. The
command's `pad[12]` bytes 0–3 are the documented same-bytes site for a
future `fog_density` (the v2→w3 reinterpretation precedent; an opcode's
field set never changes once shipped).

**Linear law (frozen):** per frame, compute once
`k = field_rcp(fog_far − fog_near)` (§6.2 — the denominator is
frame-constant; far ≤ near is the disabled case above). Per vertex, with
`d` = the view-space forward distance — the guarded `w` this section's
depth pipeline already clamps to `[wmin, wmax]`, NOT radial distance
(radial costs a per-vertex `isqrt_u32` (§7.2) and differs from forward
distance only off-axis):

```
f_raw = fx_mul(fx_sub(fog_far, d), k)   // ONE rounding (§3)
f     = clamp(f_raw, 0, 0x10000)        // d ≥ far → 0 (full fog), d ≤ near → 1 (clear)
f8    = fx16 → unit8 (§2, round-half-up)
```

> ### AMENDED BY OWNER RULING D-5 (2026-09-03) — READ BEFORE THE MIX BELOW
>
> **What survives:** the factor computation immediately above. D-5 says the fog
> factor is *"computed once per vertex from the frozen view/fog law"* — that is
> `f_raw` / `f` / `f8`, unchanged, still frozen.
>
> **What is REPLACED:** everything from "Mix (frozen)" to the end of this
> subsection. D-5 rules **"do not carry an already-fogged vertex colour"**.
> Carry **unfogged lit RGB and a fog factor**, transported through clipping and
> `GEOM.PARAMBUF` and interpolated through `ATTRSTEP`. The unified ordering is:
>
> | | ordinary material | cel material |
> |---|---|---|
> | 1 | lighting | lighting |
> | 2 | interpolate lighting + fog factor | interpolate lighting + fog factor |
> | 3 | — | **toon quantisation** |
> | 4 | texture/material combination | texture/material combination |
> | 5 | **fog final source RGB** | **fog final source RGB** |
> | 6 | framebuffer blend | framebuffer blend |
>
> Fog is applied to the **final source colour before alpha or additive
> blending**. Fog-exempt classes — sky family, HUD, deliberately
> emissive/additive effects — take an **explicit bypass**.
>
> **Three sentences below are now false and are kept only as the record of what
> changed:** that fog is a vertex-colour operation in GEOM.PROJECT; that "the
> factor is not a separate interpolant"; and that "there is no per-fragment fog
> anywhere in v1". Under D-5 the factor **is** a separate interpolant and the
> mix **is** per-fragment, at the final source colour.
>
> D-5's stated reason is that the old order produces two visible errors: **fog
> quantised into hard toon bands**, and **texture modulation multiplying the fog
> colour itself**.
>
> `design/contracts/GEOM.LIGHT.md` already carries the new rule. This section
> did not, and it is the numeric law and said *frozen*, so a reader who opened
> it first got the superseded route with the strongest possible warrant.

**Mix (SUPERSEDED by D-5, retained as the record):** fog is a vertex-colour operation in GEOM.PROJECT,
ordered AFTER lighting and AFTER the global tint (sky_and_beams §4a — the
tint darkens the object; fog then pulls it toward the sky's own colour, so
the fog colour is never itself tinted), per channel of the colour8 lanes:

```
c' = sat_u8( c + rescale_s((fog_c − c) · f8, 8) )
```

ONE rounding per channel. The fogged colour rides the ordinary Gouraud
path — the factor is not a separate interpolant and there is no
per-fragment fog anywhere in v1 (a per-pixel form would be a
RASTER.FRAGMENT recipe change: not costed, not built). Because the mix is
per-vertex, a triangle whose vertices shade differently interpolates
*mixed* colours rather than *mixing interpolated* colours — the accepted
cost of zero fragment work (the donor's own terrain was Gouraud-lit; the
modern Dagon reimplementation's per-pixel fog is explicitly not donor
truth — S6 §1).

**Colour (frozen binding — the Giants depth-cue):** `fog_c` is NOT a
command field. It is the active sky set's horizon colour — the
`band_lower_horizon == under` join value that `spec/sky_and_beams.md`
§1.2 law 1 already forces equal — so fog and sky cannot disagree at the
seam where they meet. Frames without DrawSky use the bound set's
background (the §1 fallback value); during a §1.3 crossfade the horizon
lerps with `w` like every other ramp entry. Lane expansion by the frozen
replication law (§2; stars_and_flares §2).

**Exempt list (frozen):** the sky family — pass-1 prefill drum and cap
(they ARE the far field), the under-plane (`fog-exempt` already,
sky_and_beams §1.1 layer table), the sun quad and cloud sheet (pass 6,
beyond-far content; `sky_cloud_fade` has no lawful fog input); additive
emissive — beams, flares, glints, souls (fog toward a colour on an
additive layer has no lawful form; the donor drew these unlit with
energy, S6 §1); the HUD/overlay planes (charter §16, 2D). Fogged:
terrain (every layer), creatures, forms, decals and projected shadows (a
decal must fog with its host surface or it pops against it), polygon
particles (their fog application rides the particle tint path — not
costed).

**Not costed (per the no-invented-costs law):** the per-vertex ALU — one
sub, one mul, one clamp per vertex plus one `field_rcp` per frame and
three channel mixes per vertex in GEOM.PROJECT, against a vertex cost
model that does not exist beyond "1 projected vertex/clock"; and the
charter §16 fog-sheet 2D plane mode (listed by the charter, owned by no
spec yet — a 2D plane mode is orthogonal to this vertex path, and
nothing here depends on that plane existing).

## 9. height16 ↔ fx16 (Q1, decided)

`height16` = **S 1.7.8** (s16): ±128 m, step 2^-8 m ≈ 3.9 mm. Live terrain
math runs in fx16; height16 is the baked-cache/ABI storage. Conversions in
§2. Re-freeze only at the Phase 6 gate with rendering evidence.

## 10. particle128 — v1 numeric law (C2, ruled)

`PARTICLE_FORMAT_VERSION = 1`. **No longer provisional.** The layout below is
the owner ruling R3 and is binding; the earlier §10 table was a sketch that
summed to 128 and agreed with the ruling on nothing else.

### Bit layout — 128 bits, exactly

| bits | field | format |
|---|---|---|
| 0..17 | position X | **s18**, S 9.8 |
| 18..35 | position Y | s18, S 9.8 |
| 36..53 | position Z | s18, S 9.8 |
| 54..64 | velocity X | **s11**, S 2.8 |
| 65..75 | velocity Y | s11, S 2.8 |
| 76..86 | velocity Z | s11, S 2.8 |
| 87..96 | age | **u10** |
| 97..103 | species | **u7** |
| 104..109 | size | **u6**, U 2.4 |
| 110..115 | spin | **u6**, U 0.6 turns |
| 116..119 | flags | 4 bits |
| 120..127 | variation | **u8** |

* **position s18** — S 9.8 m **relative to the population origin**; LSB
  1/256 m; range −512.000 .. +511.99609375 m.
* **velocity s11** — S 2.8 m/tick; LSB 1/256; −4.000 .. +3.99609375 m/tick,
  which is about −240 .. +239.77 m/s at 60 Hz.
* **age u10** — whole 60 Hz ticks, 0..1023. **Lifetime is not in the record**:
  it lives in the species descriptor, is 1..1023, and `lifetime == 0` refuses
  the descriptor.
* **species u7** — table index 0..127.
* **size u6** — U 2.4 relative radius multiplier;
  `radius = base_radius_fx16 * size / 16` with **one** final round-half-up.
  **World scale, never camera-space pixels.**

  > **This field changed meaning, and something already believed the old one.**
  > `fpga/rtl/particles/zhao_part_expand.sv` and `zref::part::expand_polygon`
  > were built against the pre-C2 §10, where size was **U 0.4.4 pixels** — an
  > 8-bit byte of sixteenths of a pixel, turned into subpixels by `size << 4`.
  > Under C2 it is six bits, a multiplier rather than a size, and a **world**
  > length rather than a screen one, so `size << 4` is not a projection of it.
  >
  > Both files now carry a banner saying so. Neither is fixed, deliberately:
  > turning a world radius into a screen half-side is a projection, and
  > inventing one to make the amendment fit is how a plausible wrong number
  > gets shipped. It needs a decision first.
* **spin u6** — U 0.6 turns; `angle16 = spin << 10`. The species carries a
  signed spin rate in angle16/tick; the stored phase wraps mod 64.
* **flags** — bit 0 `STUCK`, bit 1 `COLLIDED_THIS_TICK`, bit 2 `BORN_THIS_TICK`,
  bit 3 RESERVED (zero in, preserved zero).
* **variation u8** — a **stateless deterministic code** from frozen hash
  inputs. **Not a mutable PRNG state**, which is the difference between a
  record that replays and one that does not.

### Population descriptor

`population_id`; origin x/y/z as fx16 **on a 1/256-m grid**; `active_count` and
`capacity`; species-table handle; tick index; capture source id.

World position = origin + local. **HPS may rebase only between complete ticks**,
by subtracting the same exact 1/256-m offset from every live record and adding
it to the origin; **the rebase is captured**. If a population cannot stay inside
the ±512 m cube, **split it** — do not silently saturate.

### The tick — exactly 60 Hz, semi-implicit, ordered

    validate
      -> derive variation
      -> evaluate frozen recipe
      -> accumulate in wide lanes
      -> round ONCE into S 2.8, saturate and count
      -> position_next = sat_s18(position + velocity_next)
      -> increment age
      -> emit bounded events
      -> collision response
      -> survivors compact, THEN children append

**One rounding**, into velocity, and the position update is exact-then-saturate.
The order is part of the law, not an implementation choice: survivors compact
before children append is what makes the ceiling behaviour deterministic.

Twelve recipe IDs remain the vocabulary. **Hardware does not infer forces from
colour, size or speed.**

### Collision, v1

FPGA sources are **live deformed terrain height/normal** and **explicit analytic
planes**. Creature and unit gameplay collision remains HPS-authoritative.
Responses: `IGNORE`, `DIE`, `STICK`, `SLIDE`, `BOUNCE`, selected by species.

### Ceiling behaviour

**Survivors are never evicted for children.** Earlier parents outrank later
ones; later child groups are refused deterministically; there is **no same-tick
recursive spawn.**

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

`QFMT_VERSION` (currently **3**; 1 → 2 with amendment C1, §7.6; 2 → 3 with
amendment C2, §10) bumps on
**any** change to: a type's width or
format, the rounding/saturation laws (§3–§5), any frozen constant or table
formula (§6–§7), a golden-vector layout (§12), or the fixgen output set (§11).
A bump requires: spec amendment, full regeneration + recomittance of tables
and goldens, re-derivation of every error bound, and orchestrator sign-off.
The constant travels in the ABI (`spec/commands.zidl`, `const u16
QFMT_VERSION` — emitted into all three generated languages) so capture replay
can refuse mismatched numerics. Editorial changes (prose, citations,
provisional extents re-ratified per Q4) do not bump.
