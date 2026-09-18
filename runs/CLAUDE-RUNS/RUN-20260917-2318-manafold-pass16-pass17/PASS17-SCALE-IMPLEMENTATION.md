# Manafold Pass 17 Wave 1 — uniform scale implementation

**Date:** 2026-09-18
**Scope:** reference/oracle scale primitive, core tests and creature-format specification only
**Status:** Complete; narrow test green

## Implemented

- `reference/include/zref/zref_creature.hpp`
  - added optional frame-major `Clip::uniform_scale_q15`;
  - added generated `Clip::mid_uniform_scale_q15`;
  - identity is Q1.15 `32768`; zero is invalid;
  - clarified `clip_frame_bytes()` is the base rigid-pose payload, excluding optional hero sidecars.
- `reference/src/zcreature/creature_core.cpp`
  - validates optional track shape and rejects authored zero;
  - clears stale generated midpoint data when the source shape is absent/malformed;
  - generates one rounded linear half-sample `(a+b+1)/2`, respecting wrap and `hold_last`;
  - decodes authored/generated scale and uniformly scales only the local 3x3 basis before hierarchy composition;
  - skips all scale arithmetic for identity, preserving the exact old matrix path and saturation ledger;
  - leaves the bone's own bind/local/root translation unscaled while parent composition structurally carries child offsets.
- `tests/geometry/creature_core.cpp`
  - absent vs explicit identity byte equality at keys and half-frames;
  - exact 1.5× authored basis and 1.25× midpoint;
  - own pivot remains fixed, Eye scale carries Pupil offset, sibling/root remain unchanged;
  - test-local broken-parent control demonstrably fires the child-registration gate;
  - odd midpoint round-half-up and in-segment monotonicity;
  - `hold_last` clamp and looping final→first midpoint;
  - malformed source clears stale mids and fails compile validation;
  - zero authored scale fails with a specific reason.
- `spec/creature_rules.md`
  - documented optional uniform sidecar semantics, identity, presentation midpoint and base-payload accounting.

No Manafold art/model/clip/renderer/site file was changed. No RTL or Quartus fit was run.

## Verification

Configured a fresh Windows-native tree with `zhao-env.ps1` sourced:

```powershell
cmake --preset windows-native -B C:\programmieren\zencrifice\manafold-p16\build-p17-core
```

Result: configure/generate RC 0.

Built only the requested target:

```powershell
cmake --build C:\programmieren\zencrifice\manafold-p16\build-p17-core --target test_creature_core
```

Result: build RC 0. The fresh target required its full `zhao_zref` dependency closure (459 Ninja steps); no unrelated test executable was run.

Ran exactly one test process, again with `zhao-env.ps1` sourced:

```powershell
C:\programmieren\zencrifice\manafold-p16\build-p17-core\tests\test_creature_core.exe
```

Result: RC 0, final line `creature_core: all anchors green`.

An additional protected render/CRC was not run in the delegated implementation lane because the return was explicitly bounded to `test_creature_core`; the main lane performed that integration validation below.

## Main-lane review and integration verification

- Corrected the identity test arrays to value-initialize the whole `kMaxBones` buffer before a full-array `memcmp`; otherwise unused tail matrices could make the test pass or fail from uninitialized stack bytes.
- Rebuilt and reran `test_creature_core` after that correction: RC 0, `creature_core: all anchors green`.
- Clean direct-built every C++ object into `build-p17-scale`; no stale struct user remained.
- Rendered protected `manafold-hit` before/after: 141/141 files (140 RGB + receipt) byte-identical, sequence CRC `0x24122E5A`.
- Rendered protected `zixxtrixx-idle` before/after: 577/577 files byte-identical.
- Independent review found and closed two defects before commit: exact-length stale generated midpoint data is now discarded/regenerated at compile, and C2 phase seams now compare normalized local translation/uniform scale plus every deformation lane. New positive tests fire on stale-zero midpoint and scale/local/extra-lane seam faults.
- After review fixes: `test_creature_core` RC 0, clean all-object direct build RC 0, protected Manafold Hit still 141/141 byte-identical.

## Files changed by this wave

- `reference/include/zref/zref_creature.hpp`
- `reference/src/zcreature/creature_core.cpp`
- `spec/creature_rules.md`
- `tests/geometry/creature_core.cpp`
- this report

Other concurrent run evidence visible in `git status` was not created or modified by this wave.
