# SPEC v1: Implement owner ruling R4 — sequence the pose decode onto shared multiplier lanes

**Run ID:** RUN-20260909-2343
**Created:** 2026-09-09 23:43 UTC+02:00
**Status:** Complete (working tree, awaiting review/commit)
**Previous Version:** N/A

---

## Objective

Relax GEOM.POSE's 1-bone/clock target per ruling R4: one operand-muxed
multiplier lane per engine (MUL_LANES knobs, defaults recommended), bit-exact
against zref::PoseBank's decode chain, latency declared honestly, demand and
DSP return re-derived rather than inherited, checker seen to fail on committed
mutants, one named fit gate, no fit run, no commit.

---

## Scope

**In Scope:**

- fpga/rtl/geometry/zhao_geom_pose_decode.sv, zhao_geom_quat2mat.sv,
  zhao_geom_mat3x4_mul.sv and their tests; GEOM.POSE.md; tests/CMakeLists.txt;
  tests/mutants/ additions; reports/POSE-DECODE-SEQUENCED-20260909.md.

**Out of Scope (other live lanes / forbidden):**

- reference/, design/blocks.yml, TERRAIN.SHADE.md, GEOM.LIGHT.md,
  fpga/rtl/terrain/, fpga/rtl/texture/, zhao_project_core.sv,
  fit_targets.yml, prod_manifest.yml, shared build/ tree, any Quartus fit,
  any commit.

---

## Constraints

- [Constraint 1]

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

---

## Open Questions

- [Question 1]
