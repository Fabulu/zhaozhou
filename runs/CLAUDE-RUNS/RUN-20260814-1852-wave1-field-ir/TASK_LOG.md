# Task Log: RUN-20260814-1852 - [Describe objective here]

**Created:** 2026-08-14 18:52 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260814-1852-wave1-field-ir/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-14 18:52 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260814-1852
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

### 2026-08-14 — P4 Field IR RECON complete (subagent, read-only)

- Read FORM §§3/5/8/9/14/16/17/18/19, Charter §§6A/11.3/19.4/20/23, START_HERE.
- Online survey: AMD GCN VOP3 64-bit encoding, Dalvik verifier, D3DX9 preshaders, SPIR-V, Csmith/CLsmith differential testing, PCG hash (Reed; JCGT 9(3) 2020).
- Environment verified: g++ 16.1.0, Node 20.17.0, CMake 4.0.2; tsc needs local npm install.
- Full findings returned to orchestrator as text (subagent file-writes blocked by harness policy): proposed 64-bit fixed instruction word (op:8|dst:6|a:6|b:6|c:6|imm:32), 64x32-bit register file, full opcode table v1 (0x00-0x29, reserved 0x30+), profiles earth/warp/flow/formation/stamp with concrete I/O records and instruction ceilings 32/48/48/64/32, .zprog/.zvec binary containers with CRC-32C, program hash = CRC32C(code||tables)+count, Q16.16 saturating round-half-up semantics, noise2 = 32-bit PCG RXS-M-XS, evaluator = generic C++ interpreter over serialized bytes + generated typed wrapper (TS interpreter differential, golden vectors owned by C++), vector gen = corners + PCG-seeded uniform over declared bounds, minimize-on-fail (ddmin-lite), acceptance demo = crater_ring end-to-end.
- Orchestrator to persist FINDINGS.md in this run dir and route ratification items (10 decisions) to architect.

### 2026-08-14 — W5 IMPLEMENTER complete (commits 500965d, 681a0b6, e8d652e, 7a21bbd on main, pushed)

- spec/form/field-ir.md v1 (frozen ISA v1 + DCURVE 0x1D + single-rounding MAD) + 4 companion spec stubs (language-semantics, domains-and-effects, deterministic-scheduling, cost-model with the sky/beam budget lines).
- compiler/src/field_ir/ (TS): types, i64 (16-bit limb util), numeric (zref-law mirror), builder, linear-scan allocator with adjacency coalescing, serializer + full-validating reader, .zvec container + PCG vector generator + minimize, TS interpreter, cost, wrapper emitter, crater_ring program, fuzz program generator.
- reference/{include,src}/zfield (C++): full-validating decode + the ONE generic interpreter (Status{sat,rcp0}, all zref:: primitives).
- tests: field_ir.test.ts (18 unit tests incl. hand-computed limb products + BigInt oracles), crater_ring.test.ts (emits + commits wrapper/.zprog), field_ts_differential.test.ts (golden byte-identical replay + failure-artifact minimize replay), field_fuzz_corpus.test.ts (committed corpus), test_field_crater_ring.cpp (8 gates), test_field_fuzz_parity.cpp (nightly TS-vs-C++ over random programs). ctest -L fast 14/14, nightly field tests green, npm 30/30, ledger/abi/tables checks clean.
- crater_ring program hash = 0x484add8d (28 instrs + END, RING at pc 1, source id 0x30010001 line 48).
- Defects found and fixed en route: mulS32 missing two's-complement sign corrections on the high half; JS bitwise ops truncating a 33-bit intermediate (mid >>> 16); mulWideLow64 NaN from a missing 4th limb; fieldRcp non-floored index (T0[0.5] = undefined); TS RING midpoint pre-shifted by <<16 (status divergence — caught by the C++-owned golden differential, exactly as designed); C++ map-section length check anchored at the wrong pointer.

#### Deviations from P4/plan (ratified-interpretation choices, documented in field-ir.md)

1. Opcode table reading of plan 1.B-4 "0x1E-0xFF reserved": P4 §3 numbering frozen verbatim incl. RING 0x21/RIDGE 0x22/ROT2 0x28/ROT3 0x29 (a macro-only RING costs ~15 instrs and overflows the earth ceiling 32); SMOOTHSTEP slot 0x20 stays RESERVED (demoted to a macro-expansion per ops.yml A3e/W2). Reserved = 0x0D-0x0F, 0x1E-0x20, 0x23-0x27, 0x2A-0xFF.
2. CURVE = fx_mad((a - x_i), dy_i, y_i) over the same {x,y,dy} triples as DCURVE (D-1 offline exact-division slopes) instead of P4 §6.5's runtime Δx-reciprocal lerp: zero runtime division, exact at knots.
3. RING pinned v1 = ss(r0,m,d)·(1 - ss(m,r1,d)) with m = rescale(r0+r1,1); P4 §6.6's "rim" formula was undefined.
4. Source map = fixed 8 B/instr records (no delta encoding). Name pool + io map extended with declared bounds (12 B/lane) so the C++ oracle reads bounds from the .zprog itself.
5. LEN2/3 pinned to exact floor isqrt (the qformats §7.2 primitive normalize uses) rather than half-up.
6. P4's "0x0E66" material threshold corrected to 0xE666 (0.9 in Q16.16 — the recon value dropped a digit).
7. Vector count = 3 + in_lanes + N corners/uniform records (P4 listed all-min/all-max/all-zero/each-lane-min + N).
8. fuzz corpus generator lives in compiler/src/field_ir/fuzz_gen.ts, runs from compiler tests (committed-artifact discipline); tests/fuzz/field_corpus_gen.ts documents the wiring; C++ replay registered CTest nightly only (field_fuzz_parity).

#### Notes for W6

- maturity advancement evidence for SW.FIELDIR/SW.ZREF: gate (d) tests = field_crater_ring (fast), field_fuzz_parity (nightly); artifacts = captures/golden/field/crater_ring.zvec, captures/failures/field/fail-484add8d-0x5A17.zvec + .txt, compiler/tests/generated/{crater_ring.hpp,crater_ring.zprog}.
- root package.json gained "field:check" (scripts block only, per instruction).
- captures/{golden,failures}/field/ directories created; the zcap round-trip scratch file is deleted by the test (not evidence).
- The TS differential test SKIPs gracefully when captures/golden/field/crater_ring.zvec is absent (first-run ordering: ctest writes it).
