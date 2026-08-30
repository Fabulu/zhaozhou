# Task Log: RUN-20260830-1049 - Correct Zixxtrixx light direction

**Created:** 2026-08-30 10:49 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260830-1049-zixxtrixx-v13-light-direction-fix/

---

## Objective

Establish the renderer's executable directional-light, normal and space conventions, repair the generic defect, and publish exactly one fixed-world `Corrected Toplight 1` held-pose orbit for owner review.

---

## Progress Timeline

### 2026-08-30 10:49 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260830-1049.
- Created the fresh ordinary Zhaozhou and Upheaval clones and pushed branch `zixxtrixx-v13-light-direction-fix` in each repository.
- Recorded base heads: Zhaozhou `c325f51c1488f2e3a3893b572961ffcf797a7e53`; Upheaval `8bfc1d4cee8d10df12ee485513808f0961dd88cc`.
- Read both repository instructions, every durable Zixxtrixx owner direction through #13, and the required reports.
- Added durable owner direction #14 in Upheaval and pushed it as `fc76bc8`.

### 2026-08-30 - Diagnosis Prepared

- Froze geometry, pose, animation, texture, pigment, cel thresholds and outline.
- Recorded the bounded six-question validation budget in `SPEC_v1.md` before tests.
- Located the smooth Lambert, normal generator, flat Lambert and creature composition implementations; no convention change or rig tuning has been made yet.

### 2026-08-30 - Root Cause Proven Before Fix

- Added `--light-sign` to the committed mesh probe and fresh-built it directly in the run-local tree.
- Synthetic identity fixture proved `skin_normal_lambert` and `shade_flat_tri_dir` both consume a surface-to-light direction: +Y top normals/winding returned 65536 under a +Y source and -Y undersides returned 0.
- Six actual idle-key-0 body samples derived their outward vector from complete posed 3D ring centres. Packed smooth normals returned `dot(normal,outward)` from -65374 to -65532: consistently inward.
- The compiled index winding returned outward face dots from +64711 to +64837. Reversing B/C, as the compositor currently does, returned the exact negative: the flat production lane is also inward.
- Root cause: the ring zipper was already outward-wound. Two historical assertions said it was inward and independently inverted it: `generate_smooth_normals` negated the correct cross, and `compose_creatures` reversed each triangle before flat Lambert. Smooth/flat agreement therefore meant both lanes agreed inward.
- Pre-fix executable evidence is `light-sign-before.txt`; expected exit was 1.

---

## Subagent Spawns

None. This run is intentionally single-agent.

---

## Files Created

- `SPEC_v1.md` — binding scope, constraints and bounded validation ledger.
- `TASK_LOG.md` — live run record.

---

## Decisions Made

- Treat v12's positive-Y constants and smooth/flat agreement as unproven, not as evidence of overhead lighting.
- Prove normal outwardness independently against known geometry and posed creature radial directions before changing light values.
- Define Lambert inputs explicitly as surface-to-light or incoming-ray direction at their API boundary; do not rely on comments.
- Stop validation after the six named bounded questions are answered.

---

## Next Steps

1. Trace flat shading, composition, world/view transform and cel/material response implementations.
2. Add and run the committed synthetic and actual-posed-normal fixtures.
3. Repair only the proven generic sign/space/orientation defect and direct-build dependent translation units.
4. Render, inspect, encode and publish exactly one `Corrected Toplight 1` orbit.
