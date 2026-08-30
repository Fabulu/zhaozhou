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

### 2026-08-30 - Generic Orientation Defect Repaired

- Removed the erroneous cross-product negation in `generate_smooth_normals`.
- Removed the erroneous B/C reversal at both flat directional-light calls in `compose_creatures`.
- Made both APIs' direction contract explicit: a unit vector from the surface toward the source, never incoming ray travel.
- Fresh direct rebuild recompiled both dependent creature translation units.
- The identical bounded probe now returns +65374..+65532 for actual smooth `dot(normal,outward)` and +64711..+64837 for index-order flat faces; the obsolete reverse order is negative. Synthetic top/underside remains 65536/0 in both lanes.
- Post-fix executable evidence is `light-sign-after.txt`; exit 0.

### 2026-08-30 - One Corrected Candidate Authored and Reviewed

- Added exactly one named rig, `Corrected Toplight 1`: key `(-18000,59000,-22000)`, fill `(26000,56000,22000)`, ambient `(24904,26214,28180)`, key gain `45875`, fill RGB `(6554,7864,11141)`.
- Added exactly one new subject, `zixxtrixx-corrected-toplight-1`, inheriting the frozen held signature-S, framing, 600-frame duration and continuous view-only orbit.
- Direct-built the cel-main reel and rendered this one animation once at native 384x240. Result: 600 frames, 60 Hz target, sequence CRC32C `0xCB8C0E8A`.
- Inspected the four quarter turns and an every-frame sheet. The pink/cyan dorsal crown is plainly brightest, sides retain blue/green modelling, and underside remains subordinate but readable. The orbit is continuous and no pose/art change occurred.
- Traced a +Y dorsal sample through production arithmetic: key/fill Lambert `59000/56000`, pre-toon gain mean `65536`, frozen bright toon level `82000` (1.2512). Native frame-0 dorsal pixel `(240,96)` is RGB `(255,105,206)`, proving the direct term survives to a visibly bright top pixel.
- Source and executable transform trace confirms `worldm = world * pose` feeds both vertices and normals, while the rig vectors bypass `vp`; the orbit mutates only `view_projection`.

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
