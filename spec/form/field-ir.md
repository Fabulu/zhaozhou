# Zhaozhou Field IR — Frozen ISA v1

**FIELD_IR_VERSION = 1** (see §13 for change control)

This document is the single written law for the exact branchless field-program
IR (charter §6A, §11.3, §19.4; FORM §9). One typed Field IR program produces:
(1) serialized microcode (`.zprog`, §5), (2) a scalar C++ evaluation path (the
single generic interpreter in `reference/src/zfield/` + a generated typed
wrapper, §10), (3) random-vector generators (`.zvec`, §6), (4) a PC→source map
(§8), (5) a static cost report (§9), (6) declared numerical bounds (§9).

**Grep-audit law (charter §29-6):** Field IR *op semantics* exist in exactly
two places — the C++ generic interpreter (`reference/src/zfield/zfield_interpret.cpp`,
`zfield::interpret`) and the TS interpreter (`compiler/src/field_ir/interpret.ts`,
subordinate differential; golden vectors are owned by the C++ oracle). There is
and shall be no third implementation: no hand-written per-program evaluator, no
"faster" fused C++ variant, no RTL-side re-derivation ahead of the profile
engine (which will consume the same serialized bytes). A reviewer greps for the
op-name switch outside those two files and must find none.

Ratified inputs: PLAN.md §1.A/1.B/1.D (run RUN-20260814-1912) over the P4 recon
(RUN-20260814-1852-wave1-field-ir). Where this document and the recon disagree,
this document wins.

Companion numeric law: `spec/qformats.md` (QFMT_VERSION 1). Every fixed-point
rule below cites it. Byte/CRC law: `spec/capture_format.md` (CRC-32C, source-ID
scheme §5 of that document).

---

## 1. Machine model

### 1.1 Word layout

Fixed 64-bit little-endian word, one word per instruction, no variable length
(P4 §2.1, ratified 1.B-1):

```
 63      32 31    26 25    20 19    14 13     8 7      0
+----------+--------+--------+--------+--------+---------+
|  imm32   |  srcC  |  srcB  |  srcA  |  dst   | opcode  |
+----------+--------+--------+--------+--------+---------+
word = opcode | (dst << 8) | (srcA << 14) | (srcB << 20)
      | (srcC << 26) | ((uint64_t)(uint32_t)imm << 32)
```

- `imm32` is a sign-extended-by-consumer immediate / table index / seed / CMP
  mode. It **must be 0 where the op does not use it** (validator V9, §4).
- `PC` of an instruction = its index in `code[]`; byte offset = `8 × PC`
  (the source map, §8, resolves PCs by pure arithmetic).
- Max code size 512 B (64 instructions), global hard ceiling (§7.3).

### 1.2 Register file

64 × 32-bit registers, 6-bit fields (ratified 1.B-2).

- **Inputs** occupy `R0..R(n_in−1)` exactly, in the profile's input-record
  order (§7.1). They are read-only for the program (validator V6).
- **Outputs** are *not* written by an instruction; the header I/O map names the
  registers whose final values become the output record (§7.1). This mirrors
  GCN export binding and removes an OUT opcode from the ISA.
- **Scratch** = everything above the inputs. Programs ≤ 64 instructions;
  register pressure beyond 64 is a compile error, never a spill.

### 1.3 Adjacent-register convention (ratified 1.B-2)

DOT2/3, LEN2/3, DIST2, NORMALIZE2/3, NOISE2, RIDGE, ROT2/3 consume and/or
produce adjacent pairs/triples starting at the encoded register (§2 lists the
exact width per op): e.g. `DOT2` reads `srcA, srcA+1` and `srcB, srcB+1`;
`NORMALIZE2` writes `dst, dst+1`. Groups must lie entirely inside `R0..R63`
(V7). A destination group must not overlap any source register of the *same*
instruction (V8) — sources are conceptually read before the group is written.
This convention halves vector-op instruction count; changing it renumbers
opcodes and invalidates every golden vector (P4 risk 1). The TS allocator
coalesces/copies to satisfy it (§11.2).

### 1.4 Number format, rounding, saturation

All lanes are **Q16.16 (s32)** — qformats.md §2 (decision Q2). Every arithmetic
rule is qformats.md law, cited per op in §3:

- add/sub saturate; mul/mad obey the **single-rounding law** (qformats.md §3,
  A3b): the exact s64 expression `a·b` / `a·b + (c<<16)`, then **one**
  `rescale(·,16)` (qformats.md §4). Double rounding is forbidden.
- Booleans are `0x0000_0000` / `0x0001_0000` (1.0 in Q16.16).
- `angle` inputs to SIN/COS/ROT use the low 16 bits as an angle16 turn
  (qformats.md §2; wraps, never records).
- No −0, NaN, Inf, or divide-by-zero *anywhere*: every op is total; the
  determinism argument is "pure function of (serialized bytes, input record)".
- Sticky per-evaluation `Status` (§3.16): `sat` (any saturating clamp in the
  SatLedger counters add/mul/rescale/unit/rcp), `rcp0` (any RCP-zero event).

---

## 2. Opcode table v1 (frozen)

Numbering frozen as v1 (ratified 1.B-4: P4 §3 verbatim **plus** `DCURVE = 0x1D`
in the gap after NOISE2 0x1C). One amendment carried over from the ratified
ledger (design/ops.yml, A3e): **SMOOTHSTEP is a macro-expansion, not an
opcode** — slot 0x20 stays reserved; its mandated expansion is §3.14. All
unassigned slots are reserved and allocate only with a FIELD_IR_VERSION bump.

| Op | Hex | Class | Dst | Srcs | imm | Semantics (§3) |
|---|---|---|---|---|---|---|
| END | 0x00 | — | — | — | 0 | terminator; must be the last instruction |
| MOV | 0x01 | ALU | 1 | A | 0 | copy |
| LDC | 0x02 | ALU | 1 | — | raw | load raw imm32 (fx bits or u32 bits) |
| ADD | 0x03 | ALU | 1 | A,B | 0 | saturating add |
| SUB | 0x04 | ALU | 1 | A,B | 0 | saturating sub |
| MUL | 0x05 | MUL | 1 | A,B | 0 | single-rounded product |
| MAD | 0x06 | MUL | 1 | A,B,C | 0 | single-rounded A·B+C |
| MIN | 0x07 | ALU | 1 | A,B | 0 | |
| MAX | 0x08 | ALU | 1 | A,B | 0 | |
| ABS | 0x09 | ALU | 1 | A | 0 | saturating abs |
| CLAMP | 0x0A | ALU | 1 | A,B,C | 0 | clamp(A, lo=B, hi=C) |
| SELECT | 0x0B | ALU | 1 | A,B,C | 0 | C≠0 ? A : B |
| CMP | 0x0C | ALU | 1 | A,B | mode | imm[2:0]: 0 eq, 1 ne, 2 lt, 3 le, 4 gt, 5 ge |
| *(reserved)* | 0x0D–0x0F | | | | | |
| DOT2 | 0x10 | MUL | 1 | A,A+1,B,B+1 | 0 | exact sum, one rescale |
| DOT3 | 0x11 | MUL | 1 | A..A+2,B..B+2 | 0 | exact sum, one rescale |
| LEN2 | 0x12 | SPECIAL | 1 | A,A+1 | 0 | approx length of (A,A+1) |
| LEN3 | 0x13 | SPECIAL | 1 | A..A+2 | 0 | |
| DIST2 | 0x14 | SPECIAL | 1 | A,A+1,B,B+1 | 0 | len of (A−B pairwise) |
| NORMALIZE2 | 0x15 | SPECIAL | 2 | A,A+1 | 0 | approx unit vector; 0-vector pinned |
| NORMALIZE3 | 0x16 | SPECIAL | 3 | A..A+2 | 0 | |
| RCP | 0x17 | TABLE | 1 | A | 0 | field_rcp; rcp(0) pinned |
| SIN | 0x18 | TABLE | 1 | A | 0 | fx_sin(u16(A)) |
| COS | 0x19 | TABLE | 1 | A | 0 | fx_cos(u16(A)) |
| CURVE | 0x1A | TABLE | 1 | A | tid | piecewise-linear {x,y,dy} table |
| SPLINE | 0x1B | TABLE | 1 | A | tid | uniform Catmull-Rom table |
| NOISE2 | 0x1C | NOISE | 2 | A,A+1 | seed | PCG lattice hash, two unit lanes |
| DCURVE | 0x1D | TABLE | 1 | A | tid | slope of the segment containing A |
| *(reserved)* | 0x1E–0x20 | | | | | (0x20 was P4 SMOOTHSTEP; demoted to macro) |
| RING | 0x21 | SPECIAL | 1 | A,B,C | 0 | annular band-pass, §3.13 |
| RIDGE | 0x22 | NOISE | 1 | A,B | seed | 1−\|2n−1\| over a NOISE2 lane |
| *(reserved)* | 0x23–0x27 | | | | | |
| ROT2 | 0x28 | SPECIAL | 2 | A,A+1,B | 0 | rotate pair by angle B |
| ROT3 | 0x29 | SPECIAL | 3 | A..A+2,B | axis | rotate triple; imm[1:0]: 0=X 1=Y 2=Z |
| *(reserved)* | 0x2A–0xFF | | | | | |

Cost classes feed §9. Reserved-slot discipline (P4 §3): any change is a
FIELD_IR_VERSION bump + orchestrator sign-off.

*Interpretation note (plan 1.B-4 wording):* the plan text says "0x1E–0xFF stay
reserved" while simultaneously ratifying P4 §3's numbering as frozen. P4 §3
assigns RING 0x21 / RIDGE 0x22 / ROT2 0x28 / ROT3 0x29; the crater_ring
acceptance program (§12) needs fused RING to fit the earth ceiling (32) — a
macro-only RING costs ~15 instructions and overflows it. This spec therefore
reads the plan as: the *unassigned* slots of the frozen table stay reserved
(here: 0x0D–0x0F, 0x1E–0x20, 0x23–0x27, 0x2A–0xFF). Deviation recorded in the
W5 task log.

---

## 3. Op semantics (pinned, per op)

Common notation: `fx_*`, `rescale`, `field_rcp`, `fx_sin/cos`, `isqrt_u64`,
`rcp_u24_norm`, `noise2_hash`, `smoothstep` are the frozen qformats.md/zref
functions — the C++ interpreter *calls* them; it never re-derives them.
Saturation always records in the interpreter's SatLedger (qformats.md §5);
`Status` (§3.16) is derived from it.

### 3.1 END
Terminates interpretation. No register writes. Validator: must be present,
must be the last instruction (V10).

### 3.2 MOV — `dst ← A`
Bit copy.

### 3.3 LDC — `dst ← imm32`
Raw bits (fx or u32 payload). No saturation, no records.

### 3.4 ADD / SUB — `fx_add(A,B)` / `fx_sub(A,B)`
qformats.md §3: s64 sum, saturate s32, records `add`.

### 3.5 MUL / MAD — single rounding (A3b)
`MUL: rescale_s32((int64)A·B, 16)`; `MAD: rescale_s32((int64)A·B + ((int64)C<<16), 16)`.
Exactly `zref::fx_mul` / `zref::fx_mad` (qformats.md §3). P4's "mul round then
sat add" is **rejected** (plan A3b).

### 3.6 MIN / MAX / CLAMP
`fx_min(A,B)`, `fx_max(A,B)`; `CLAMP = fx_clamp(A, lo=B, hi=C) = fx_max(B, fx_min(C, A))`
(qformats.md §3 — pure integer, no records). Behavior when `lo > hi` is the
composition of the two: `max(B, min(C, A))` — total, pinned by construction.

### 3.7 ABS — saturating
`dst = sat_s32((int64)−A)` if `A < 0` else `A`; `ABS(0x8000_0000) = 0x7FFF_FFFF`
**and sets SAT** (P4 §3; note `zref::fx_abs` itself returns INT32_MIN — the
interpreter applies the saturating wrapper).

### 3.8 SELECT — `dst ← (C ≠ 0) ? A : B`
Any nonzero C is true (P4 §3). No records.

### 3.9 CMP — boolean
Signed raw comparison of A, B; result `0x0000_0000` / `0x0001_0000`. Mode from
`imm[2:0]` (0 eq, 1 ne, 2 lt, 3 le, 4 gt, 5 ge); modes 6–7 are invalid (V9).

### 3.10 DOT2 / DOT3 — exact accumulate, one rescale
```
p = (int128)(int64)A0·B0 + A1·B1 [+ A2·B2]   // exact wide sum
dst = rescale_s32(p, 16)                      // ONE rounding (A3b)
```
The mat4fx×vec4 precedent (qformats.md §2) generalized. |p| ≤ 3·2^62 fits s128
(s64 wrapping arithmetic on the two's-complement pairs is equally exact because
the true sum lies in (−2^64, 2^64); the TS limb util relies on this).

### 3.11 LEN2 / LEN3 / DIST2
```
len2(x,y): n2 = (u64)(int64)x·x + (int64)y·y        // exact, ≤ 2^63
           dst = sat_s32(isqrt_u64(n2))             // exact floor sqrt (qformats §7.2)
len3(x,y,z): n2 = x²+y²+z²  (≤ 3·2^62 < 2^64, exact u64); as len2
dist2(ax,ay,bx,by): len2(fx_sub(ax,bx), fx_sub(ay,by))
```
Floor (not half-up) is pinned to match the exact isqrt primitive of
qformats.md §7.2/§7.4 — the same primitive `normalize3_approx` uses; bias ≤ 1
LSB, declared in ops.yml. Saturation of the isqrt result records `rescale`.

### 3.12 NORMALIZE2 / NORMALIZE3
`NORMALIZE3` **is** `zref::normalize3_approx` (qformats.md §7.4), verbatim.
`NORMALIZE2` is the same algorithm on two lanes (pinned identically):
```
n2 = (u64)(int64)x² + y²                    // exact
if n2 == 0: {0, 0} and set RCP0             // pinned (P4 §3)
len = isqrt_u64(n2)
m = len; e = 0
while m < 2^23: m <<= 1; e −= 1
while m ≥ 2^24: m >>= 1; e += 1
r = rcp_u24_norm(m)                          // qformats §6.1
out_i = rescale_s32((int64)v_i · r, 31 + e)  // ONE rounding per component
```
Declared bound: 2 LSB per component when n2 ≥ 2^48 (qformats.md §7.4).

### 3.13 RCP — `field_rcp`
qformats.md §6.2 verbatim (256-entry table + one pinned linear correction).
**`rcp(0) = 0x7FFF_FFFF` and sets RCP0** (pinned; plan 1.B-10).

### 3.14 SIN / COS — 257-entry quarter-wave (A2)
`fx_sin` / `fx_cos` (qformats.md §7.1). The angle is `u16(A)` — the low 16
bits of the lane, an angle16 turn. Error ≤ 1.3 LSB, exhaustively proven.

### 3.15 CURVE / DCURVE / SPLINE — the {x,y,dy} triple tables (D-1)

Tables (§5.3) store `{x: i32, y: i32, dy: i32}` triples, `x` strictly
increasing. The table compiler precomputes offline with **exact 128-bit
signed division, round-half-up** (qformats.md §4 `round_half_up_s`):

- `curve` kind: `dy_i = round_half_up(((y[i+1] − y[i]) << 16) / (x[i+1] − x[i]))`
- `spline` kind (uniform spacing, V5): `dy_i = min(round_half_up((1<<32) / Δx), 0xFFFF_FFFF)`
  — the Q16.16 reciprocal of the constant spacing.

Segment selection (shared by all three ops, pinned branchless 6-step
compare/select binary search so RTL cycle counts match):
```
a = clamp(A, x[0], x[n−1])                   // fx_clamp
lo = 0
for k = 5 down to 0:                          // ≤ 64 entries ⇒ 6 steps
    mid = lo + (1 << k)
    if mid ≤ n−1 and x[mid] ≤ a: lo = mid
i = lo                                        // unique: x[i] ≤ a < x[i+1]
```
(The result is unique for strictly increasing x regardless of search method;
the method is pinned for cycle determinism.)

- **CURVE(A, tid)**: `dst = fx_mad(fx_sub(a, x_i), dy_i, y_i)` — one single-
  rounded MAD, zero runtime division. (P4 §6.5 sketched a runtime
  Δx-reciprocal lerp; the ratified D-1 offline-slope form is exact at the
  knots and reuses the DCURVE store — deviation recorded in the task log.)
- **DCURVE(A, tid)**: `dst = dy_i` — the slope of the segment containing A
  (the derivative lane; plan 1.D).
- **SPLINE(A, tid)**: pinned uniform Catmull-Rom, single-rounding ops only:
  ```
  t   = fx_clamp(rescale_s32((int64)(a − x_i) · dy_i, 16), 0, 1<<16)   // unit param
  P0..P3 = y[clamp(i−1,0,n−1)], y[i], y[i+1], y[clamp(i+2,0,n−1)]       // index clamp
  C1 = P2 − P0                    (exact s64, saturate s32, records SAT if clamped)
  C2 = 2·P0 − 5·P1 + 4·P2 − P3    (exact s64, saturate s32)
  C3 = −P0 + 3·P1 − 3·P2 + P3     (exact s64, saturate s32)
  u = fx_mad(t, C3, C2); u = fx_mad(t, u, C1)     // Horner on doubled coefficients
  v = fx_mul(t, u)
  dst = fx_add(P1, rescale_s32(v, 1))             // the ½ of Barry–Goldman/CR
  ```
  Endpoints clamp; no extrapolation; no errors. The doubled-coefficient Horner
  form keeps every step a qformats.md §3 primitive.

### 3.16 NOISE2 / RIDGE — PCG lattice hash (A1/A1b)

**NOISE2(A, A+1; imm = seed)** writes two decorrelated unit lanes:
```
ix = floor_fx(A)   = (int32)A >> 16           // arithmetic: floor
iy = floor_fx(A+1) = (int32)(A+1) >> 16
h0 = noise2_hash((u32)ix, (u32)iy, seed, lane=0)   // qformats §7.5, verbatim
h1 = noise2_hash((u32)ix, (u32)iy, seed, lane=1)
dst = h0 >> 16; dst+1 = h1 >> 16              // U 0.0.16 lanes
```

**RIDGE(A, B; imm = seed)**: the ridged transform over NOISE2 lane 0 at the
lattice point (A, B):
```
u = (noise2_hash((u32)((int32)A>>16), (u32)((int32)B>>16), seed, 0)) >> 16
dst = fx_sub(1<<16, sat_abs(fx_sub(fx_add(u,u), 1<<16)))
```

### 3.17 RING — annular band-pass (pinned v1)
P4 §6.6's sketch (`1 − |ss(r0,r1,d) − ss(r1,r0+rim,d)|`) left `rim` undefined;
v1 pins the symmetric two-smoothstep form (both edges explicit, no free
parameter — deviation recorded in the task log):
```
m  = rescale_s32((int64)r0 + r1, 1)            // round-half-up midpoint
s0 = smoothstep(r0, m, d)                       // qformats §7.3 verbatim
s1 = smoothstep(m, r1, d)
dst = fx_mul(s0, fx_sub(1<<16, s1))
```
`r0 == r1` degenerates through the pinned `field_rcp(0)` rule (totality).

### 3.18 ROT2 / ROT3 — table-driven rotation
```
ROT2: c = fx_cos(u16(B)); s = fx_sin(u16(B)); x = A, y = A+1
      dst   = fx_sub(fx_mul(c, x), fx_mul(s, y))
      dst+1 = fx_add(fx_mul(s, x), fx_mul(c, y))
ROT3 (axis imm[1:0], c/s as above; x,y,z = A..A+2):
      axis X: y' = fx_sub(fx_mul(c,y), fx_mul(s,z)); z' = fx_add(fx_mul(s,y), fx_mul(c,z)); x' = x
      axis Y: z' = fx_sub(fx_mul(c,z), fx_mul(s,x)); x' = fx_add(fx_mul(s,z), fx_mul(c,x)); y' = y
      axis Z: x' = fx_sub(fx_mul(c,x), fx_mul(s,y)); y' = fx_add(fx_mul(s,x), fx_mul(c,y)); z' = z
```
(3 = axis value 3 is invalid, V9.)

### 3.19 Sticky Status
```
struct Status { bool sat; bool rcp0; };
sat  = (L.add | L.mul | L.rescale | L.unit | L.rcp) != 0    // any clamp
rcp0 = L.rcp0 != 0
```
The SatLedger is per-evaluation (reset each `interpret` call); Status is the
only externally visible part (charter §19.4 "exact instruction counters" land
with the RTL profile engine).

### 3.20 Mandated macro-expansions (NOT opcodes — A1b, ops.yml A3e)

Two language-visible constructs are defined once here as expansions over the
frozen ops; C++/TS/RTL compose them identically and never re-implement:

**smoothstep(e0, e1, x)** — the builder emits (qformats.md §7.3 verbatim;
`zref::smoothstep` is the same law):
```
d = SUB(e1, e0); r = RCP(d); t = MUL(SUB(x, e0), r)
t = CLAMP(t, 0, 1<<16)                       // constants via LDC
t2 = MUL(t, t); w = SUB(3<<16, MUL(2<<16, t))
result = MUL(t2, w)                          // t²(3−2t), the C1 Hermite weight
```

**Bilinear smooth value noise (fx16 ∈ [−1,1))** — NOISE2 + lerp, never an
opcode (A1b). For a sample at (x, y):
```
ix = floor_fx(x); iy = floor_fx(y)                     // arithmetic >> 16
fx = x − (ix<<16); fy = y − (iy<<16)                   // fractional parts
sx = smoothstep_macro(0, 1<<16, fx); sy = smoothstep_macro(0, 1<<16, fy)
(n00, n01) = NOISE2(ix, iy, seed)                      // at lattice (ix, iy)
(n10, n11) = NOISE2(ix+1, iy, seed)                    // x+1 lane
n0 = MAD(sx, n10, ...)                                 // per-lane lerp:
   n0 = n00 + sx·(n10 − n00)   as SUB + MAD, single rounding
   n1 = n01 + sx·(n11 − n01)
v  = n0 + sy·(n1 − n0)                                  // unit-range value
result = 2·v − 1<<16                                    // remap to [−1,1)
```
Gradient noise later composes NOISE2 + lerps the same way; the opcode never
changes (P4 §6.4).

---

## 4. Validator (Dalvik model — reject before any register write)

The same rule set runs in the TS compiler (`compiler/src/field_ir/validate.ts`)
and the C++ loader (`zfield::decode` — never trust, always re-validate); the
RTL loader will run it a third time. **A program is rejected before any
register write** (charter §19.4): validation is a pure pre-pass over the
decoded structure; `interpret` is only reachable after it passes.

Rules (each is a distinct error code, mirrored in both languages):

| # | Rule |
|---|---|
| V1 | magic `'ZFIP'`, FIELD_IR_VERSION = 1, exact file length (§5) |
| V2 | body CRC-32C matches; program hash field matches recomputed hash (§5.4) |
| V3 | profile ∈ 0..4; flags == 0; lane_bits/format fields legal |
| V4 | 1 ≤ instr_count ≤ global ceiling 64 and ≤ profile ceiling (§7.3) |
| V5 | table_count ≤ 4; each table: kind legal; 2 ≤ entries ≤ 64; x strictly increasing; spline kind additionally: uniform spacing (x[i+1]−x[i] constant); dy/entries present |
| V6 | I/O map: input lanes are exactly R0..R(n_in−1), each once, kind=in; output lanes: distinct regs, each ≥ n_in (so never an input); each has min ≤ max bounds (inputs); lane count ≤ 32; types legal |
| V7 | every register field < 64; every group (§1.3) fits entirely below 64 |
| V8 | destination group never intersects an input register, and never intersects the same instruction's source registers |
| V9 | opcode known (§2); imm discipline: zero where unused; CMP mode ≤ 5; CURVE/SPLINE/DCURVE imm < table_count; ROT3 imm[1:0] ≤ 2 and imm bits ≥ 2 zero |
| V10 | END present, last, unique |
| V11 | def-before-use: every source register is an input or the dst-group member of an earlier instruction |
| V12 | every output-map register is defined by some instruction |

## 5. Serialized microcode — `.zprog`

Little-endian, no floats, no timestamps (ratified 1.B-7). Layout:

```
off  size  field
 0    4    magic "ZFIP" (u32 LE 0x5049465A)
 4    2    version u16 = 1 (FIELD_IR_VERSION)
 6    1    profile u8 (0 earth, 1 warp, 2 flow, 3 formation, 4 stamp)
 7    1    flags u8 (reserved, must be 0)
 8    4    source_id u32 (capture_format.md §5; kind 3 = field program)
12    2    instr_count u16 (incl. END; ≤ ceiling)
14    1    table_count u8 (≤ 4)
15    1    io_lane_count u8 (inputs + outputs, ≤ 32)
16    2    table_section_bytes u16
18    2    map_section_bytes u16 (io map + source map + name pool)
20    4    program_hash u32 (§5.4)
24    4    body_crc32c u32 (§5.5)
28   ...   body: code (instr_count × 8 B) ‖ tables ‖ io map ‖ source map ‖ name pool
```

`28 + 8·instr_count + table_section_bytes + map_section_bytes == file size` (V1).

### 5.1 Code section
`instr_count` words of §1.1, in PC order.

### 5.2 Table section
Per table (in index order; index = the `tid` immediates use):
```
kind u8 (0 = curve, 1 = spline) | rsvd u8 (0) | entry_count u16
entry_count × { x: i32, y: i32, dy: i32 }
```

### 5.3 I/O map + name pool (inside map_section_bytes)
Per lane, in declaration order (inputs first):
```
reg u8 | kind u8 (0 in, 1 out) | type u8 (0 fx, 1 unit, 2 angle, 3 u32) | name_id u8
min i32 | max i32        // declared bounds; inputs only (outputs carry 0,0)
```
Name pool: the io_lane_count NUL-terminated UTF-8 names concatenated, in lane
order; `name_id` must equal the lane's ordinal (V6; kept as a field for future
deduplication). Names are debug metadata — never semantically load-bearing.

### 5.4 Program hash (frozen)
```
program_hash = zhao_crc32c(zhao_crc32c(0, code_bytes) over table_bytes)
             + instr_count            (u32 wrap-around addition)
```
i.e. CRC-32C over `code ‖ tables` (capture_format.md CRC law; the generator
constant table lives in the generated ABI headers), then `+ instr_count` mod
2^32. Deterministic, timestamp-free. The hash flows into commands, traces,
`.zvec` headers and `.zcap` resource pages.

### 5.5 Body CRC
`body_crc32c` = CRC-32C over the complete file image with bytes 24..27 (the
field itself) taken as zero. The loader recomputes and compares (V2).

## 6. Random vectors — `.zvec` — and the minimize policy

### 6.1 Layout (ratified 1.B-7/8)

```
off  size  field
 0    4    magic "ZFIV" (u32 LE 0x5649465A)
 4    2    version u16 = 1
 6    2    lane_bits u16 = 32
 8    4    program_hash u32
12    8    seed u64
20    4    vector_count u32
24    1    in_lanes u8
25    1    out_lanes u8
26    2    rsvd u16 (0)
28    4    crc32c u32 (over the file with bytes 28..31 zeroed)
32   ...   vector_count × { inputs: in_lanes × i32, expected: out_lanes × i32, status: u32 }
```
`status`: bit0 = sat, bit1 = rcp0 (§3.19). Binary primary (JSON float parsing
is a determinism trap; lanes are raw int32 anyway).

### 6.2 Generation — pure function of (program_hash, seed, N)

PRNG (PCG, the qformats.md §7.5 constants):
```
state = (u32)(seed & 0xFFFFFFFF) ^ program_hash
draw(): state = state · 747796405 + 2891336453 (u32 wrap)
        r = RXS-M-XS(state)   // ((s >> ((s>>28)+4)) ^ s) · 277803737; (w>>22)^w
```
Per input lane i with declared bounds [min_i, max_i] (raw s32 as u32):
```
count = (u32)(max − min) + 1        // careful: full s32 range wraps to 0
value = count == 0 ? r : min + (r mod count)
```

Record order (corners mandatory, then uniform):
1. all lanes at min;
2. all lanes at max;
3. all lanes at 0 clamped into [min, max];
4. for each lane i: lane i at min, every other lane drawn uniform;
5. N uniform records (every lane drawn; default N = 256).

Total `vector_count = 3 + in_lanes + N`. The demo seed is `0x5A17`
(FORM §16 echo). `expected`/`status` are filled by the **C++ oracle** (the
golden vectors are C++-owned; TS must replay them byte-identically —
Csmith-style subordinate differential).

### 6.3 Failing vectors and minimize (ratified 1.B-8)

On divergence, save the single failing record + report as
`captures/failures/field/fail-<program_hash>-<seed>.zvec` (single-record
`.zvec`, same layout) plus a divergence report
`{vector_index, first_divergent_lane, expected, actual, status_diff}`.

Minimize (bounded ddmin-lite, deterministic): starting from the failing input
record, per lane in order, bisect the lane value toward its **nearer declared
bound** (round-half-up midpoint), ≤ 64 steps per lane, keeping the input only
while the failure persists; on a lost failure, restore and move to the next
lane. The minimized record + report are the committed artifact; replaying the
artifact must reproduce the divergence exactly (it doubles as a test input).

## 7. Profiles (v1, ratified 1.B-9)

### 7.1 I/O records

All lanes 32-bit, Q16.16 unless noted (P4 §1.2 verbatim). Input lanes map to
R0.. in the listed order; output lanes take their values from the named
registers at END.

| Profile | id | Input record (R0..) | Output record |
|---|---|---|---|
| earth | 0 | x:fx, z:fx, age:u32, phase:fx, p0..p7:fx (12) | height:fx, velocity:fx, material:u32, nav_cost:fx (4) |
| warp | 1 | px,py,pz:fx, nx,ny,nz:fx, a0..a3:fx, time:u32, p0..p3:fx (14) | dx,dy,dz:fx, nx′,ny′,nz′:fx (6) |
| flow | 2 | px,py,pz, vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx (13) | px′,py′,pz′, vx′,vy′,vz′:fx, attr0:fx (7) |
| formation | 3 | index:u32, time:u32, parent rot2:fx,fx, trans2:fx,fx, p0..p5:fx (11) | tx,ty,tz:fx, rot:angle, scale:fx, mat_phase:fx (6) |
| stamp | 4 | u,v:unit, age:u32, strength:unit, p0..p3:fx (8) | tag_op:u32, strength:unit, emissive:unit (3) |

### 7.2 Op whitelists

v1: all five profiles admit the full opcode table (§2). The whitelist *hook*
exists in the validator (per-profile bitmap) so per-profile restrictions are a
table edit, not a structural change.

### 7.3 Instruction ceilings (provisional — R7)

earth 32, warp 48, flow 48, formation 64, stamp 32; **global hard ceiling 64**
(instr_count field). Explicitly provisional: tune against Wound Lab programs;
a ceiling change is a spec-constant edit, never an encoding change — golden
vectors are unaffected.

## 8. PC→source map

Per-instruction source spans, stored in the map section (§5.3), in PC order:
```
instr_count × { source_id: u32, line: u16, col: u16 }   // 8 B each
```
Simplified from P4 §6.8's delta encoding (fixed records are trivially
resolvable and deterministic; deviation recorded in the task log). A trace
carrying a program hash always resolves PCs: PC → byte offset 8·PC → record →
`{source_id, line, col}` → capture_format.md §5 resolution (embedded SOURCE_MAP
→ sidecar → raw hex). The crater_ring test asserts the RING op's PC resolves
to its builder span (§12, check 4).

## 9. Cost metadata and declared bounds

`FieldProgram.cost` (emitted into the wrapper as comments and `costs.zcost`
when the Form backend exists — Phase 3):
- instruction count; per-class counts (ALU/MUL/TABLE/NOISE/SPECIAL, §2);
- estimated sequencer cycles = Σ per-op provisional cycle costs
  (ALU 1, MUL 2, TABLE 3, NOISE 2, SPECIAL 5; provisional until the RTL
  profile engine pins them);
- DSP demand = MUL-class count (+ muls inside SPECIAL ops, provisional);
- table bytes = Σ (4 + 12·entries);
- register high-water mark = max live physical register + 1.

Declared bounds: every input lane carries {min, max} raw s32 (§5.3); the
validator enforces min ≤ max (V6). Vector generation (§6.2) draws uniformly
over them. Phase 2+ interval analysis *proves* bounds; Phase 1 = declared +
vector spot-check.

## 10. Evaluation architecture (ratified 1.B-5)

**(b) One generic interpreter over the serialized bytes + generated typed
wrapper.** The interpreter (`zfield::interpret`) is the only C++ semantics;
the wrapper embeds the program bytes, typed in/out structs and a
`static_assert` on the program hash — the compiler genuinely emits a C++ API
from the IR. Per-program straight-line codegen is deferred until ZEmu needs
speed (then (c) both + CI differential). The TS interpreter
(`compiler/src/field_ir/interpret.ts`) is the compiler-side mirror; it is
*subordinate*: the C++ oracle owns every golden vector and TS must replay
them byte-identically every commit.

TS int64 discipline: MUL/MAD/DOT products use one shared 16-bit-limb
arithmetic util (`compiler/src/field_ir/i64.ts`); hand-computed product tests
including negative halves are the unit test (plan risk R2). No BigInt in the
interpreter runtime path.

## 11. Compiler-side structure (TS)

### 11.1 Builder (`compiler/src/field_ir/builder.ts`)
Typed per-profile builder (§7.1 layouts): op methods return `Val` handles
(virtual registers); `smoothstep()` emits the §3.20 macro; tables are declared
with `{x,y}` pairs and the compiler precomputes `dy` offline by exact
128-bit division (BigInt in the *tool*, never in the interpreter).

### 11.2 Register allocator (`compiler/src/field_ir/alloc.ts`)
Linear scan over live intervals (def → last use), inputs pinned to R0..,
free-list of scratch registers, consecutive-run allocation for dst groups,
and MOV-insertion to coalesce non-adjacent source groups for §1.3 ops. No
spilling: > 64 live registers is a compile error.

### 11.3 Serializer / wrapper emitter
Deterministic Buffer serialization of §5; the C++ wrapper emitter writes the
embedded bytes + typed structs + hash `static_assert`. Serialization is
byte-stable: two runs over the same program are byte-identical (§12, check 3;
also asserted in TS tests).

## 12. Acceptance program — `crater_ring` (earth)

```
d        = DIST2(p − centre)            // centre = (p0, p1)
band     = RING(d, r_in = p2, r_out = p3)
walls    = smoothstep(p3, p2, d)        // §3.20 macro (11 base ops)
depth    = CURVE(age_curve, phase)      // 8-entry attack/decay {x,y} table
velocity = DCURVE(age_curve, phase)     // derivative lane (D-1)
height   = band · walls · depth · p4    // saturating single-rounded chain
material = SELECT(CMP(depth, 0x0E66, gt), MAT_CHARRED, MAT_SOIL)
nav_cost = MUL(height, 0x4000)          // height / 4, round-half-up
```
Outputs {height, velocity, material, nav_cost}; 29 instructions + END ≤ 32
(earth ceiling). Scripted checks (tests/differential/test_field_crater_ring.cpp
+ compiler/tests/{crater_ring,field_ts_differential}.ts):
(1) TS build → validate → serialize → emit wrapper; C++ compiles it, loads,
re-validates, prints the hash; (2) `.zvec` (seed 0x5A17, 256 + corners)
generated **from the C++ oracle**, committed at
`captures/golden/field/crater_ring.zvec`; (3) replay twice → identical bytes;
(4) RING's PC resolves through the source map to the builder span;
(5) synthetic `.zcap` round-trip preserves source ID + program hash;
(6) injected flipped lane → minimized failing `.zvec` + divergence report
committed under `captures/failures/field/`; (7) TS replays the golden
byte-identically; (8) same vectors later replay in Verilator unchanged
(Phase 2+, profile engine).

## 13. Change control

`FIELD_IR_VERSION` (currently **1**) bumps on any change to: the word layout
(§1.1), the opcode numbering or any op's pinned semantics (§2–§3), the
validator rule set (§4), the `.zprog`/`.zvec` layouts (§5–§6), or the profile
I/O records (§7.1). A bump requires: spec amendment, regeneration +
recommitment of every golden vector and failure artifact, and orchestrator
sign-off. Editorial changes (prose, citations, ceiling retunes per §7.3,
whitelist restriction per §7.2) do not bump.
