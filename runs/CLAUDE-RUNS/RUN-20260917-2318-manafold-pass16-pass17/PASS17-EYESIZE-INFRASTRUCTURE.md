# Manafold Pass 17 eye-size infrastructure

## Implemented

- `EyeForm` routes lens construction through named long/wide/deep controls while preserving the exact 270/84/40 legacy/current default.
- Same-binary diagnostic form overrides are parsed before Manafold type construction; invalid/nonpositive/out-of-range values return 2 rather than clamp or silently mean “unset.”
- `Rig` resets all bone scales to Q1.15 identity, lazily writes the optional scale sidecar only for explicitly enabled clips, and converts named per-eye per-mille values deterministically.
- Independent L/R/both diagnostic mutes restore only the selected eye to identity.
- `manafold-eyesize` / `meyesize` is registered in direct and CMake builds. It proves current clips allocate no scale track, legacy/current form equality, exact sidecar shape, independent L/R values, no leakage to other bones, Eye→Pupil basis propagation, visible lens/star ownership and invalid-value rejection.

## Review corrections before commit

The first delegated gate failed for two gate defects, not production defects:

1. `RingSpec::deform_strength_ex` is a C array; comparing it with `==` compared addresses, so two byte-identical forms were reported different. The gate now compares every lane and also includes the previously omitted `cz` field.
2. Pupil bind pivots intentionally sit at the lens centre (`kEyeShiftPivotMm=0`), so a parent eye scale must **not move the pupil centre**. The first gate incorrectly demanded centre travel. It now compares the complete Eye/Pupil 3×3 bases: correct parent scaling keeps both registered; the wrong-bone control breaks only the selected pair.

A renderer review also caught that negative form overrides were treated as the `-1` sentinel and silently ignored. Each present override now validates its own safe range before assignment.

## Verification

- clean direct `meyesize` build: RC 0;
- CMake `manafold-eyesize` target: RC 0;
- normal gate: RC 0, L/R Q1.15 `49152/24576`, both Eye/Pupil registrations green;
- `--fail-mute L`: RC 0, left alone restored to identity;
- `--fail-mute R`: RC 0, right alone restored to identity;
- `--fail-wrong-bone`: RC 0, left Eye/Pupil registration breaks while right remains green;
- invalid form and invalid mute renderer inputs: RC 2;
- default protected Manafold Hit compared under identical Pass-16 shell settings: 141/141 files byte-identical.

The first protected comparison reported 141/141 differences because it compared the new Direction-15 shell against Pass-16 shell bytes. Re-rendering with explicit 180/800/750 shell control isolated the eye infrastructure and restored exact equality. That rejected comparison is another instance of measuring across mismatched configurations.

No public clip allocates or consumes the scale sidecar yet. Base-form and expression values remain an authored by-eye step.
