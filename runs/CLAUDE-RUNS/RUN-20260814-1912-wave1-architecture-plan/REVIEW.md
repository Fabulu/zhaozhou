# Wave-1 Adversarial Review — VERDICT: ACCEPTED-WITH-FIXES

*Reviewer agent, 2026-08-15, against main @ 0c2e796. Persisted by orchestrator. All findings verified against actual code (empirically executed where marked).*

## CRITICAL

**C1. SPLINE opcode (0x1B) violates its frozen spec by a factor of 2^16 — both interpreters, identically.**
- `reference/src/zfield/zfield_interpret.cpp:206` — computes `(v·2^16)/2` = v·2^15; law (field-ir.md §3.15) says `dst = fx_add(P1, rescale_s32(v, 1))` = v/2 (the Catmull-Rom ½).
- `compiler/src/field_ir/interpret.ts:178` — same bug mirrored.
- Verified empirically: uniform 5-knot spline 0,0,1,1,1 at segment midpoint (t=0.5, correct 32768) returns **2147483647 (saturated)**. Knots coincidentally exact (v=0) hide it.
- Blind spot: golden crater_ring.zvec uses CURVE only; fuzz corpus has SPLINE programs but TS↔C++ parity compares two implementations sharing the bug. No hand-computed expected-value test exists for any table op.
- Fix: `rescale_s32(v, 1)` both sides; hand-computed vectors (knot, midpoint, near-endpoint); regenerate fuzz corpus .zvec expected values (C++-oracle-owned); cite field-ir §13 change-control. No FIELD_IR_VERSION bump (spec is correct; code is wrong).

**C2. `unit8_from_fx16` wraps to 0 for fx16 raw ∈ [0xFF80, 0xFFFF] (values 0.99805–0.99998).**
- `reference/include/zref/zref_fixp.hpp:195-200`: `(r+128)>>8 == 256` → `uint8_t(256) == 0`. Verified by compiled reproduction: 0xFF7F→255, 0xFF80→**0**, 0xFFFF→**0**.
- Law: qformats §2/§5 saturate 255. A ~1.0 weight silently becomes 0 — the exact silent-corruption class the saturation law exists to prevent.
- Test gap: conversion test only checks exact multiples, 0x10000, −1 — never the 128 one-LSB-from-saturation raws.
- Fix: clamp after shift; add 0xFF7F/0xFF80/0xFFFF (and negatives) to the conversion test.

## MAJOR

**M1. CI lacks charter §27 every-commit "format" and "static analysis" tiers.** No clang-format/clang-tidy/cppcheck anywhere in the repo; `formal_lane` is formal;nightly only (sby IS present in CI — fast formal smoke could run per-commit). Fix: `.clang-format` + dry-run --Werror job; tsc already covers TS; move/extend formal smoke into fast.

## MINOR

- **m1** TS leg of bytes_consumed not pinned (fuzz corpus stores error codes only; frame.ts:63 default 36 only on success path). Fix: per-corpus-case bytesConsumed or a TS unit case.
- **m2** ZH_ABI_TRUNCATED and ZH_ABI_UNIMPLEMENTED_COMMAND exercised by no test. Fix: straddling-record corpus case + shell test with a reserved command. (STALE_HANDLE honestly documented.)
- **m3** qformats §13 promises QFMT_VERSION travels in the ABI version word; commands.zidl has no such constant. Fix: add the const (additive) or amend §13.
- **m4** qformats §2 freezes mat4fx row sum "in s64"; reference sums in s128 (disclosed). Fix: one-sentence spec amendment to s128 exact.
- **m5** fixgen sin table uses host Math.sin (committed tables byte-compared so deterministic in practice). Fix: note in §11 (integer minimax polynomial optional, later).

## Verified-clean
(a) bytes_consumed fix conforms C++/SV/TS (TS pin gap = m1). (b) fixgen-ownership ruling consistent (drift protection real via tables_check). (c) CI mechanically green — gh run 31863716730 all three jobs ✓ (gap = M1). (d) thin session shell satisfies charter "empty ZRef" (one implementation, no second semantics). (e) W2.1 worktree incident fully recovered — branch exactly 3 commits, merge intact.

Charter-law sweep clean: tri-language byte-identity genuinely gated (shared goldens + transitive SV probe); op-semantics in exactly the two sanctioned implementations (no third); failing vectors real and replayed; maturity spot-checks all hold; reserved-bit rejection real in C++ and SV; abi.md LE/16B/records verified; PadFrame 20B ✓; DrawSky 176B ✓; lint waivers all commented. Determinism clean (no host time/rand/float in deterministic paths; ordered iteration). Spec spot-checks: .zprog/.zvec layouts rule-for-rule; A3b single-rounding MUL/MAD conforms; PCG verbatim; CRC-32C everywhere. Docs honest (six spot-checked claims held, incl. remote CI run IDs via gh).

## Required before RTL depends on these paths
1. C1 + corpus regen · 2. C2 + boundary tests · 3. M1 CI tier.
Strongly recommended: m1, m2, m3, m4.
