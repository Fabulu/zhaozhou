# FINDINGS — P4 Field IR (exact bounded branchless field-program IR) — ZH-010/011 recon

**Agent:** RECON (read-only; no repo modification) — **Date:** 2026-08-14
*Persisted by orchestrator (subagent file-write blocked by harness policy).*
**Inputs:** FORM §§3,5,8,9,14,16,17,18-L2,19.2; Charter §§6A,11.3,19.4,20,23-Phase1; START_HERE; online prior-art survey.

## 1. Requirements distilled from the MDs

### 1.1 What the documents mandate

From FORM §9: one canonical **branchless** Field IR with **exact fixed-point behaviour**; profiles restrict inputs, outputs, instruction count, legal ops. Provisional ops (verbatim): `mov add sub mul mad min max abs clamp select compare dot2 dot3 normalize_approx sin cos curve noise2 length_approx distance_approx smoothstep ring ridge sample_spline sample_curve rotate2 rotate3`. Prohibitions: no loops, recursion, arbitrary pointer reads, texture fetches, dynamic allocation, unbounded control flow. **Exact dual output** — six artifacts per accepted program: (1) serialized microcode, (2) scalar C++ evaluator from the same typed IR, (3) random-vector generators, (4) PC→Form-source maps, (5) static cost report, (6) declared numerical bounds. "Not hardware-valid until C++ evaluator, Verilated profile engine and physical FPGA agree on the generated vectors."

FORM §8: earth programs declare `max_ops` (example: 16), conservative footprint, parameter layout, output bounds, bake-ability, hardware validity — compiler must know or be given all six.

Charter §6A: five profiles (`earth warp flow formation stamp`) lower from the one IR; §19.4: FPGA carries "program hash/version checks", "safe rejection of invalid microcode", "field-program PC tracing", "exact instruction counters".

Charter §11.3 Earth8 rules: fixed-point, branchless, loopless, no arbitrary memory, explicit footprint, fixed instruction maximum, identical C++ and RTL semantics.

Charter §20.1/20.3: ZRef hosts the exact evaluator; every unit test generates input, runs reference, clocks RTL, compares outputs/status/cycle bounds, **saves a minimal failing vector**.

Charter §23 Phase 1 gate (verbatim core): "one typed Field IR program emits a scalar C++ evaluator, serialized program and random vectors; Field IR interpretation is deterministic and covered by golden vectors; source IDs and program hashes survive capture round-trips."

START_HERE: "One typed Field IR program must generate both its C++ evaluator and serialized hardware program. **Never implement those semantics twice by hand.**" Stable source IDs and hashes on every program; save failing vectors.

FORM §16: field programs "automatically receive generated random differential tests over their **declared input bounds**."

### 1.2 Profile I/O records (proposed concrete layouts)

All lanes 32-bit; types: `fx`=Q16.16, `un`=unit Q16.16 in [0,65536], `ang`=angle16, `u32`=raw.

| Profile | Input record (R0..) | Output record |
|---|---|---|
| `earth` | x:fx, z:fx, age:u32(tick), phase:fx, p0..p7:fx (12) | height:fx, velocity:fx, material:u32, nav_cost:fx (4) |
| `warp` | px,py,pz:fx, nx,ny,nz:fx, a0..a3:fx, time:u32, p0..p3:fx (14) | dx,dy,dz:fx, nx′,ny′,nz′:fx (6) |
| `flow` | px,py,pz, vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx (13) | px′,py′,pz′, vx′,vy′,vz′:fx, attr0:fx (7) |
| `formation` | index:u32, time:u32, parent rot2:fx,fx, trans2:fx,fx, p0..p5:fx (11) | tx,ty,tz:fx, rot:ang, scale:fx, mat_phase:fx (6) |
| `stamp` | u,v:un, age:u32, strength:un, p0..p3:fx (8) | tag_op:u32, strength:un, emissive:un (3) |

Instruction ceilings Phase 1 (declared `max_ops` ≤ ceiling): earth 32, warp 48, flow 48, formation 64, stamp 32; global hard ceiling 64 ("hundreds of small resident programs", Charter §19.4).

## 2. Prior-art survey and what to borrow

| Prior art | Relevant fact | What P4 borrows |
|---|---|---|
| **AMD GCN/Vega ISA** — 32/64-bit instruction words; VOP3 = 64-bit DEST + 3 operands; inline vs pooled literals; modifier bits | Fixed 64-bit dst+3src is proven silicon practice for our op mix (mad/select/clamp are 3-source) | 64-bit fixed word, dst+3src, small flag bits, embedded imm32 |
| **Dalvik** — register VM, variable-length formats, 4/8/16-bit register fields, a *verifier* (def-before-use, type/register consistency) before execution | Pre-execution validation makes untrusted loadable code safe — matches "safe rejection of invalid microcode" | Register IR + mandatory validator (opcode whitelist per profile, register bounds, def-before-use, ceilings). Variable length rejected: programs ≤64 instrs, fixed 8B is simpler for RTL decoder and PC math |
| **D3DX9 preshaders** — tiny arithmetic programs D3DX interpreted CPU-side before the GPU | Precedent for a micro interpreter for small pure-arithmetic table-using programs beside a big pipeline | The concept itself; we add exactness |
| **SPIR-V** — word-structured serializable IR, dense opcode numbering, normative validation rules, versioned tables | A serialized IR can be a stable cross-tool contract | Opcode numbering discipline, versioned container, normative validator, byte-exact producers |
| **Csmith / CLsmith** — random *program* generation + differential comparison + EMI + delta-debugging | Strongest known methodology for "two implementations of one semantics" | Phase 1 random vectors; Phase 2+ random *programs* (bounded IR fuzz TS vs C++) and shrinking |
| **llvm-reduce / bugpoint** — automatic reduction of failing inputs | "Save failing vectors" is half; shrinking is the other half | Bounded per-lane bisection `minimize` mode |
| **PCG hash** (Reed; Jarzynski & Olano JCGT 9(3) 2020) — RXS-M-XS: mul, variable xorshift, mul, xorshift; integer-only | Bit-identical in TS (`Math.imul`,`>>>`), C++ (`uint32_t`), RTL (DSP + shifts) | `noise2` and vector PRNG |
| **Fixed-function DSP microcode tradition** (early Radeon, audio DSPs) — fixed word, 12–128-reg files, 1–2-cycle ops, table ROMs | Historical sweet spot for sequencer ALUs | 64×32-bit register file |

### 2.1 Encoding decision (recommendation)

**Fixed 64-bit little-endian word, one word per instruction, no variable length.** GCN proves 64-bit dst+3src fits our op mix; fixed length kills a class of RTL decoder risk and makes PC→source pure division (byte_offset/8); 64×8B = 512B max code; ≤~256KB for hundreds of resident programs. Variable length saves ~40% bytes at these sizes — buys nothing. **Register file 64×32-bit, 6-bit fields.** Inputs R0..R(k−1) read-only; outputs bound by header map (no OUT instruction — simpler validation, mirrors GCN export binding); scratch Rk..R63.

## 3. Proposed opcode table v1 (freeze before RTL)

```
 63      32 31    26 25    20 19    14 13     8 7      0
+----------+--------+--------+--------+--------+---------+
|  imm32   |  srcC  |  srcB  |  srcA  |  dst   | opcode  |
+----------+--------+--------+--------+--------+---------+
```
- imm32 sign-extended immediate / table index / flag payload; must be 0 where unused (validator).
- Boolean results `0x0000_0000` / `0x0001_0000` (1.0 in Q16.16); `select` treats nonzero srcC as true.
- All arithmetic saturating; sticky per-evaluation status word (`SAT`, `RCP0`).

| Op | Hex | Form | Semantics |
|---|---|---|---|
| END | 0x00 | — | terminator; code after END invalid |
| MOV | 0x01 | dst←A | copy |
| LDC | 0x02 | dst←imm32 | load constant (raw fx or u32 bits) |
| ADD | 0x03 | dst←A+B | sat add |
| SUB | 0x04 | dst←A−B | sat sub |
| MUL | 0x05 | dst←rnd(A·B) | (int64 a·b + 0x8000) ≫ 16, sat |
| MAD | 0x06 | dst←rnd(A·B)+C | mul round then sat add |
| MIN | 0x07 | dst←min(A,B) | |
| MAX | 0x08 | dst←max(A,B) | |
| ABS | 0x09 | dst←abs(A) | abs(0x8000_0000)=0x7FFF_FFFF, sets SAT |
| CLAMP | 0x0A | dst←clamp(A,B,C) | B=lo, C=hi |
| SELECT | 0x0B | dst←C?A:B | C≠0 → A |
| CMP | 0x0C | dst←A cmp B | mode imm[2:0]: 0=eq 1=ne 2=lt 3=le 4=gt 5=ge |
| DOT2 | 0x10 | dst←A·A+1 + B·B+1 | adjacent-register convention (§7.1) |
| DOT3 | 0x11 | dst←A·A+1+A+2 + B·B+1+B+2 | adjacent triple |
| LEN_APPROX2 | 0x12 | dst←approx‖A,A+1‖ | rcp-table based |
| LEN_APPROX3 | 0x13 | dst←approx‖A..A+2‖ | |
| DIST_APPROX2 | 0x14 | dst←len(A−B pairwise adj.) | |
| NORMALIZE_APPROX2 | 0x15 | dst,dst+1←A/‖·‖ approx | ‖·‖=0 → returns 0,0, sets RCP0 |
| NORMALIZE_APPROX3 | 0x16 | dst..dst+2 | |
| RCP | 0x17 | dst←approx 1/A | unit-domain input; rcp(0)=0x7FFF_FFFF, sets RCP0 |
| SIN | 0x18 | dst←sin(A) | A=angle16; shared 1024-entry table + lerp |
| COS | 0x19 | dst←cos(A) | same table |
| CURVE | 0x1A | dst←sample_curve(imm=tid,A) | piecewise-linear table |
| SPLINE | 0x1B | dst←sample_spline(imm=tid,A) | t unit; cubic table |
| NOISE2 | 0x1C | dst,dst+1←noise(A,A+1) | exact PCG lattice hash; two decorrelated unit lanes |
| SMOOTHSTEP | 0x20 | dst←ss(A=e0,B=e1,C=x) | exact integer form §6.6 |
| RING | 0x21 | dst←ring(A=d,B=r0,C=r1) | band-pass via smoothsteps |
| RIDGE | 0x22 | dst←ridge(A,B) | 1−\|2n−1\| composed over NOISE2 lanes |
| ROT2 | 0x28 | dst,dst+1←rot(A,A+1 by B) | sin/cos table driven |
| ROT3 | 0x29 | dst..dst+2←rot(A..A+2 by B) | axis in imm[1:0] (0=X,1=Y,2=Z) |
| — | 0x30–0xFF | reserved | only with a Field IR spec version bump |

Cost classes: `ALU`(1cyc), `MULCLASS`(MUL/MAD/DOT* — 1 DSP), `TABLE`(SIN/COS/CURVE/SPLINE — 1 table port), `NOISE`, `SPECIAL`(ROT3 = table+2).

## 4. IR data structures (TypeScript) — recommendation: typed AST → linear register machine

| Option | Verdict |
|---|---|
| A. Linear SSA internal | no control flow ⇒ phis pointless; degenerates to a topological list |
| B. Tree/DAG direct interpretation | no linear PCs ⇒ no source map, no cost, no RTL mapping — dead on arrival |
| **C. Register machine canonical** | one representation is compiler IR + serialized form + RTL program; native PCs; simple validator |

**Recommendation: A→C pipeline; C canonical.** Compiler builds a typed SSA-ish DAG from Form HIR; a ~100-line linear-scan allocator (trivial without branches) emits the register program. That program is what gets serialized, interpreted, mapped, and costed — one representation crosses TS/C++/RTL, per "never implement semantics twice".

```ts
type FxType = 'fx' | 'unit' | 'angle' | 'u32' | 'bool';
interface FieldOp {
  op: FieldOpCode; dst: Reg; a: Reg; b: Reg; c: Reg; imm: number; // int32
  srcSpan: SourceSpan;                                             // feeds source map
}
interface FieldProgram {
  version: 1; profile: Profile; sourceId: number;                  // charter §17
  code: FieldOp[];                                                 // ≤ ceiling, ends END
  tables: FieldTable[];                                            // ≤4 × ≤64 entries
  inputLayout: IoLane[]; outputLayout: IoLane[];                   // name,type,reg
  declaredBounds: Record<string, {min: number; max: number}>;      // raw fx ints
  cost: CostSummary;                                               // per-class, cycles, DSP, table bytes
}
```

Builder API typed per profile so ops type-check before serialization; serialization Buffer-based, deterministic, no floats in container.

## 5. C++ evaluator — recommendation: one generic exact interpreter + generated per-program wrapper

| Option | Verdict |
|---|---|
| (a) per-program emitted straight-line C++ | fastest; but semantics drift risk between emitter and RTL; every program re-tests the emitter — two implementations by the back door |
| **(b) one generic interpreter over serialized bytes + emitted typed wrapper** | semantics single-sourced (charter law); wrapper provides `eval(const EarthIn&, EarthOut&)` (START_HERE satisfied — the compiler genuinely emits C++ from the IR); interpreter reads the same bytes the FPGA will |
| (c) both + CI differential | best; cheap once (b) is golden — Phase 3+ when ZEmu needs speed |

Wrapper shape: `#include "zfield/core.hpp"` + embedded program bytes + typed in/out structs + `static_assert` on program hash. **TS side keeps its own interpreter** of the register program (compiler self-tests, later LSP) — legitimate because golden vectors are *owned by the C++ oracle* and TS must reproduce them byte-exactly in CI (Csmith-style differential, also validates the serializer continuously).

```cpp
namespace zfield {
  struct Status { uint32_t sat:1, rcp0:1; };
  struct Program { /* decoded, validated view of .zprog */ };
  Status interpret(const Program&, const int32_t* in, int32_t* out /*64-reg view*/);
}
```

Determinism argument: every op is total (saturation everywhere, rcp(0) defined, tables finite); the program never touches state outside the register file ⇒ pure function of (bytes, inputs).

## 6. Semantics the spec must pin

**6.1 Number format.** Canonical signed 32-bit Q16.16 (FORM §5 `fx16`); unit values [0,65536] same encoding; angle16 wraps mod 2^16. No −0/NaN/Inf — spec asserts unrepresentable.

**6.2 Rounding/saturation.** ADD/SUB/MAD-add saturate. MUL: `p=(int64)a*b; r=(int32)((p+0x8000)>>16)` then saturate — round-half-up, biased for negatives (−1.5→−1): pin the asymmetry. One adder in RTL; TS emulates via 16-bit hi/lo limbs (exact). SAT events set the sticky bit — §20.6 "first divergent fixed-point values" starts here.

**6.3 Division.** No divide opcode. RCP = 256-entry Q16.16 table + one pinned integer linear correction. **rcp(0)=0x7FFF_FFFF, sets RCP0.** No other op can fail.

**6.4 noise2 — exact hash.** 32-bit PCG RXS-M-XS (Reed; JCGT 9(3) 2020):
```
u32 pcg(u32 v){ v = v*747796405u + 2891336453u;
               u32 w = ((v >> ((v>>28)+4)) ^ v) * 277803737u;
               return (w>>22) ^ w; }
noise2(x,y): ix=floor16(x), iy=floor16(y)
  h0 = pcg(ix*0x9E3779B1u ^ iy*0x85EBCA77u)   // lane 0
  h1 = pcg(h0 ^ 0xE1)                          // lane 1
  unit outputs = h0>>16, h1>>16
```
Mul/xor/shift only; constants copied verbatim into the spec and the generated ABI constant blob. Smoother noise later composes (NOISE2 + lerp ops), never changes the opcode.

**6.5 sample_curve / sample_spline.** ≤4 tables × ≤64 entries `{x:fx,y:fx}`, x strictly increasing (validator). CURVE: clamp to [x0,xn]; pinned branchless 6-step compare/select binary search; integer lerp using a table reciprocal of Δx (unit-bounded). Result unique regardless of search method, but pin the search so RTL cycle counts match. SPLINE: pinned uniform Catmull-Rom, same rounding. Endpoints clamp; no extrapolation; no errors.

**6.6 smoothstep/ring/ridge.** `smoothstep`: t=clamp(rcp_table((x−e0)/(e1−e0))); e0==e1 → t = x≥e0; result t·t·(3−2t) with §6.2 rounding. `ring(d,r0,r1)` = 1 − |ss(r0,r1,d) − ss(r1,r0+rim,d)|. `ridge` = 1−|2·noise−1| composition. Each is 3–6 machine ops; **the spec defines fused ops by their mandatory macro-expansion** so C++ and a fused RTL implementation cannot disagree.

**6.7 sin/cos.** One shared 1024-entry Q16.16 quarter-wave table, generated by the ABI generator, byte-identical TS/C++/SV (Phase 1 gate). Lerp with §6.2 rounding. Part of console ABI, not per-program.

**6.8 PC→source map.** Per-instruction srcSpan → delta-encoded `{pcΔ:u8, sourceId:u32, lineΔ:i16, colΔ:i16}` (RLE-collapsed). Resolves to FORM §17 shape (`spells/upheaval.form:48`). Map lives inside .zprog so a trace with a program hash always resolves PCs.

## 7. Serialized microcode (.zprog)

Little-endian, no floats, CRC-32C:
```
0 : magic "ZFIP" u32        12: instr_count u16 (≤ceiling)
4 : version u16 (1)         14: table_count u8 (≤4)
6 : profile u8 (0..4)       15: io_lane_count u8 (≤32)
7 : flags u8                16: table_section_bytes u16
8 : source_id u32           18: map_section_bytes u16
                            20: code_crc32c u32  ← program hash
                            24: body_crc32c u32  ← whole file
then: code (n×8B) | io map {reg:u8,type:u8,name_id:u16} | tables {n:u16, n×(x:u32,y:u32)}
    | source map deltas | name pool (debug only)
```
**Program hash = CRC-32C(code‖tables) + instr_count.** Stable across runs (deterministic serialization, no timestamps/floats). Flows into commands, traces, .zcap.

Validator (runs in TS compiler, C++ loader, RTL loader — Dalvik-verifier model): magic/version; profile known; instr_count ≤ ceiling; opcodes in profile whitelist; registers <64; dst not in input range; def-before-use (inputs excepted); imm=0 where unused; END present and last; adjacency constraints (dst+1/src+1 within file and within def'd groups); table x strictly increasing; declaredBounds sane. **Invalid microcode rejected before any register write** (§19.4).

**7.1 Adjacent-register convention** — DOT2/3, LEN*, NORMALIZE*, NOISE2, ROT2/3 consume/produce adjacent pairs/triples. Halves vector-op instruction count; matches a vector ALU sequencer step; forces TS allocator coalescing (easy, no control flow). Alternative (explicit per-lane DOT) costs 2–3× instructions for warp/flow. Recommend adjacency — ratify early; changing it renumbers opcodes and invalidates all golden vectors.

## 8. Random vectors (.zvec)

**Binary primary, `--dump-json` for debugging** (charter capture family is binary+CRC; vectors replayed by C++/TS/Verilator; JSON float parsing is a determinism trap — lanes are raw int32 anyway):
```
0 : magic "ZFIV" u32   12: seed u64      24: in_lanes u8
4 : version u16        20: vector_count  25: out_lanes u8
6 : lane_bits u16(32)  u32               26: rsvd u16
8 : program_hash u32                     28: crc32c u32
records ×N: {inputs: in_lanes×i32, expected: out_lanes×i32, status: u32}
```
**Generation** = pure function of (program_hash, seed, N): PCG32/splitmix64 PRNG; each input lane uniform over its declared bound; **corners mandatory**: all-min, all-max, all-zero, each-lane-at-min-with-others-random, plus N uniform (default 256; demo seed 0x5A17 echoing FORM §16). **Failing vectors**: save the single failing record + `{vector_index, first_divergent_lane, expected, actual, status_diff}` as `fail-<hash>-<seed>.zvec`; then deterministic minimize: per-lane bisection toward the nearer bound, keep any still-failing input, ≤64 steps (bounded ddmin-lite).

## 9. Cost metadata and bounds

Into `costs.zcost` per program: instruction count + per-class counts; estimated sequencer cycles; DSP demand (MULCLASS count); table bytes; register high-water mark; conservative footprint (earth: author-declared rect/circle Phase 1); declared output bounds per lane. Phase 1: declared + vector spot-check. Phase 2+: interval analysis proves bounds; hardware-validity checker refuses programs exceeding profile envelope.

## 10. Acceptance test: `crater_ring` (earth profile)

```
d      = DIST_APPROX2(p − centre)          // centre = params p0,p1
band   = RING(d, r_in=p2, r_out=p3)
walls  = SMOOTHSTEP(p3, p2, d)
depth  = CURVE(age_curve, phase)           // attack/decay table, 8 entries
height = band * walls * depth * p4         // saturating mad chain
material = SELECT(depth > 0x0E66 /*0.9*/, MAT_CHARRED, MAT_SOIL)
nav    = height >> 2
outputs {height, velocity (CURVE derivative lane), material, nav}
```
Gate, all scripted into `runs/…/artifacts/`: (1) TS build → validate → serialize `.zprog` → emit C++ wrapper; (2) g++ compile; interpreter loads, re-validates, prints hash; (3) generate `.zvec` (seed 0x5A17, 256+corners) **from the C++ oracle**, freeze as golden vectors; (4) C++ replay passes; run twice → identical hashes; (5) TS interpreter replays same `.zvec` byte-identically (compiler-side differential); (6) inject a flipped lane → minimal failing `.zvec` saved with divergence report; (7) source-map: RING's PC resolves to the builder span; round-trip through a synthetic `.zcap` keeps source ID + hash; (8) later, same vectors replay in Verilator unchanged.

## 11. Risks

1. Adjacency convention leaks allocator structure into the ISA — ratify now; later change renumbers opcodes, invalidates all golden vectors.
2. TS int64 mul rounding needs 16-bit limb emulation — implement once, hand-computed product table incl. negative halves.
3. Fused ops defined by macro-expansion keep C++/RTL honest, but a hand-written "faster" C++ version will tempt someone — lint-forbid; only the interpreter exists.
4. Opcode freeze: renumbering after RTL starts invalidates decoders; version field + reserved range mitigates — ratify §3 now.
5. Sine/reciprocal tables are a generated-ABI dependency (owned by the abi-capture wave) — sequence that wave first.
6. Ceilings 32/48/48/64/32 are guesses; Wound Lab programs may force revision — cheap now, expensive after RTL.
7. Noise exactness first, quality second: gradient noise composes from NOISE2 + lerps; never opcode churn.

## 12. Architect ratification list

1. 64-bit fixed word, dst+3src+imm32 (§3). 2. 64×32-bit reg file; header output map; adjacency convention. 3. Q16.16, round-half-up mul, saturating all, sticky SAT/RCP0. 4. Opcode numbering §3 frozen as v1. 5. Evaluator = generic interpreter + generated wrapper (§5b); codegen deferred; TS differential subordinate to C++ golden vectors. 6. noise2 = PCG RXS-M-XS with §6.4 constants. 7. `.zprog`/`.zvec` binary, CRC-32C; program hash = CRC32C(code‖tables)+count. 8. Vector gen = corners + PCG-seeded uniform over declared bounds, N=256; minimize-on-fail. 9. Profile I/O records and ceilings (§1.2). 10. rcp(0)=0x7FFF_FFFF; boolean 0/0x10000; CURVE/SPLINE semantics §6.5.

## 13. Sources

- Local: FORM §§3,5,8,9,14,16,17,18,19; Charter §§6A,11.3,19.4,20,23; START_HERE.
- [GDC 2017 Advanced Shader Programming on GCN (GPUOpen)](https://gpuopen.com/download/GDC2017-Advanced-Shader-Programming-On-GCN.pdf) · [Vega ISA (TechPowerUp mirror)](https://www.techpowerup.com/gpu-specs/docs/amd-vega-isa.pdf) · [LLVM AMDGPU Usage](https://llvm.org/docs/AMDGPUUsage.html)
- [Android — Dalvik bytecode](https://source.android.com/docs/core/runtime/dalvik-bytecode)
- [ShaderX2 — Introduction to the DirectX 9 HLSL (Lund PDF; preshader-era D3DX)](https://fileadmin.cs.lth.se/cs/education/EDA101/assignments/assignment3/ShaderX2_IntroductionToHLSL.pdf)
- [Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIR-V.html)
- [csmith-project/csmith](https://github.com/csmith-project/csmith) · [mc-imperial/clsmith](https://github.com/mc-imperial/clsmith) · [ICSE'19 randomized differential testing survey](https://wcventure.github.io/FuzzingPaper/Paper/ICSE19_Difftest.pdf)
- [Nathan Reed — Hash Functions for GPU Rendering](https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/) · [Jarzynski & Olano — JCGT 9(3) 2020](https://jcgt.org/published/0009/03/02/)
- Environment verified 2026-08-14: g++ 16.1.0 (MinGW-W64 ucrt), Node v20.17.0, CMake 4.0.2; `tsc` requires local `npm install typescript`.
