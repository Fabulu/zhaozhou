# Pass 17 uniform-scale diff review

Reviewed the Wave-1 uniform Q1.15 scale implementation for pose math, identity compatibility, midpoint semantics, validation and test honesty. Two correctness findings survived.

## 1. HIGH — an exact-length generated midpoint track can bypass zero/source validation

**Files:** `reference/src/zcreature/creature_core.cpp:474-488`, `reference/src/zcreature/creature_core.cpp:1300-1322`

`compile_creature` validates the authored `uniform_scale_q15` shape and rejects zero only in that source vector. It does not validate `mid_uniform_scale_q15`. When `bank.bake60` is false, the copied midpoint vector is not regenerated. `decode_pose` then trusts any midpoint vector whose length equals `frame_count * bone_count` and uses it on `sub != 0`.

Concrete failure: a clip has valid nonzero authored scale, `interpolate=true`, `bank.bake60=false`, and a stale/external exact-length midpoint vector containing zero. Compilation succeeds; every affected half-frame decodes scale zero and collapses the bone assembly. A wrong-length midpoint safely falls back, but the dangerous exact-length case does not.

Fix by treating the midpoint as generated-only at compile (clear/regenerate it regardless of `bake60`), or at minimum validate exact shape and reject zero before it can be consumed. Add a compile/decode test with valid source plus exact-length zero midpoint.

## 2. MEDIUM — declared phase seams ignore uniform scale

**File:** `reference/src/zcreature/creature_core.cpp:1207-1243`

The C2 phase-seam gate promises bit-identical poses, but compares quaternions, root and deformation only. It never compares the new uniform-scale channel.

Concrete failure: two clips declare a seam with identical rotation/root/deformation but different eye scale at the referenced keys. `compile_creature` accepts the seam and the state transition visibly pops eye size. Empty and explicit identity must compare as the same identity value; any nonidentity mismatch must fail.

Add normalized per-bone scale comparison to C2 and tests for mismatch rejection plus absent-vs-explicit-identity acceptance.

## Verified sound

- Identity scale skips arithmetic, preserving old matrix bits and saturation-ledger behavior.
- Scaling only the local 3×3 while leaving local translation unchanged correctly scales around the bone pivot; hierarchy composition carries child offsets.
- Unsigned midpoint sum cannot overflow `uint32_t`; rounded linear averaging is monotone and handles wrap/hold-last correctly.
- Q1.15 products use signed `__int128` through the existing documented saturating rescale.
- The zero-initialized full-array identity comparison no longer depends on uninitialized storage.
- Existing narrow tests pass, but neither finding above was initially covered.

## Resolution

Both findings were fixed before commit.

1. `compile_creature` now treats uniform-scale midpoint data as generated-only: it clears every copied companion unconditionally, then regenerates only when `bake60` is enabled; otherwise decode derives the bounded average from validated authored keys. A valid source plus exact-length stale zero midpoint test proves half-frames cannot collapse.
2. C2 now normalizes absent local translation to zero and absent scale to Q1.15 identity, then compares quaternions, root, local translation, uniform scale, primary deform and every extra deform lane. Tests prove empty-vs-explicit identity passes and scale/local/extra-lane mismatches fail.

The header/spec contracts were updated. Fresh `test_creature_core` passes; clean direct rebuild passes; protected Manafold Hit remains 141/141 byte-identical after the fixes. **Review verdict: green for the Wave-1 commit.**
