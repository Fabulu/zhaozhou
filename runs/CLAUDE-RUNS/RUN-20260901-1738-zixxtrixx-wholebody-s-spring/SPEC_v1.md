# SPEC v1: Zixxtrixx whole-body S spring — Owner Direction 20

**Run ID:** RUN-20260901-1738
**Created:** 2026-09-01 17:38 UTC+02:00
**Status:** Active

---

## Objective

The published jump/somersault anticipation is rejected on six counts
(`Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-20-2026-09-01.md`). Rebuild it so
that, judged by eye in motion at native resolution against the tail-balance clip
played beside it:

1. The arming leans **back**, and the S is visibly **larger** at the loaded pose
   than at rest.
2. Nothing reads rigid — not the arming, not the hold, not the airborne
   somersault.
3. No visible jerk at idle-into-compression or at landing-back-into-idle.
4. The arming is slow and steady; the release is not.
5. Motion quality matches the tail-balance clip's wobbly, organic settle, and
   the creature genuinely rises onto its tail.
6. The S runs to the last tail station, on jumps and somersaults alike.

---

## Scope

**In scope:** `zhaozhou/tools/reel/zixxtrixx.h` spring pose tables, route,
timing, life layer, airborne wheel, landing; `zixx_probe.cpp` bands; a committed
`zixx_springpose.cpp` pose probe; the 22-subject re-render, Archive Generation
Eleven, encode/assemble, and one publish.

**Out of scope:** neutral geometry, topology, head/neck, face wedge, pigments,
eyes/pupils/orange stripe, fins, normals, the Cool Cross rig, the reel CRC
three-way discrepancy, the creature-ownership migration, FPGA/RTL.

---

## Acceptance

- Every-frame contact sheets of `zixxtrixx-spring-side` show a continuously
  changing, backward-leaning, growing S with no straight rail and no dead hold.
- The spring's arming plays as the same family of motion as the balance rise.
- Frame-to-frame motion energy shows no spike outside the two events that should
  be impulsive (the spring firing, the landing impact).
- `zixx-probe` PASSES with bands re-derived from the accepted art, and every
  ground contact is authored and declared.
- The bit-exact release and pre-lift parity gates pass across all consumers.

## Method

Author by eye. Render. Look. Compare. Adjust. Measurement only on the
comparison side, after a look has chosen the value. Every shape, timing and
lean value stays a named editable constant.
