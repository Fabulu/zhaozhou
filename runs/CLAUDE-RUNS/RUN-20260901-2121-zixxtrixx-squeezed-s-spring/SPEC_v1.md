# SPEC v1: Zixxtrixx squeezed-S spring — Owner Direction 21

**Run ID:** RUN-20260901-2121
**Created:** 2026-09-01 21:21 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

The Direction 20 pass is rejected ("trash bin material"). Rebuild the
jump/somersault anticipation against Owner Direction 21's seven acceptance
points, judged by eye, in motion, at native resolution:

1. No part of the animal rotates through or past vertical during the spring.
2. The S is made by the WHOLE body, to the last tail segment.
3. A small aim toward the target, then a clear, unmistakable compression.
4. The compressed shape reads as the S squeezed from the top — shorter,
   broader, the same S.
5. Nothing interpenetrates the ground or itself; contact flattens and spreads
   sideways instead.
6. The launch reads as an explosion forward.
7. The salto is the previously accepted one (pre-Direction-20 wheel; the
   tapered-spiral/travelling-breathe wheel is rejected).

Direction 20's smoothness and slow-arming requirements still stand; its
lean-back coil wording is superseded.

---

## Scope

**In scope:** spring pose tables/route/timing in
`zhaozhou/tools/reel/zixxtrixx.h`; the airborne wheel restoration; contact
flatten/spread during the squeeze; `zixx_probe.cpp` band re-derivation;
`zixx_springpose.cpp` if it needs new poses; the 22-subject re-render, Archive
Generation Twelve, encode/assemble, merge to main, one publish.

**Out of scope:** neutral geometry, topology, head/neck, face wedge, pigments,
eyes/pupils/orange stripe, fins, normals, Cool Cross rig, the IDLE S (owner:
correct, keep exactly), reel CRC three-way discrepancy, creature-ownership
migration, FPGA/RTL, `sacengine`.

---

## Constraints

- Keep the previous pass's `spring_anchor_offset` metres/millimetres unit fix.
- Build only via `tools/reel/build-direct.sh`; never `cmake --build`.
- Render with explicit `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.
- Never `git add -A`; stage explicit paths. Commit and push as work happens.
- Author by eye; measurement on the comparison side only; every value a named
  editable constant.
- Judge from every-frame contact sheets and before/after pairs vs both the
  live (rejected) bank and Archive Generation Eleven.
- The golden monolithic attack and planned consumers share one schedule now
  (previous pass unified them) — verify the clip being judged is the one
  changed, on both paths.
- Never touch the shared checkouts (hardware lane).

---

## Don't Retry

- Rolling the front segments past vertical (rejected pass: segment 0 at ~165deg).
- Leaving rear segments as a straight rail (rejected: 2.7/-16.5/-38.5 deg).
- Reworking the airborne wheel (taper/breathe) — restore uniform pitch.
- Judging from evenly spaced stills.

---

## Open Questions

- Verify whether anything besides the pose tables (station weighting, deform
  metadata, profile mapping) stops the rear tail from curving — owner asked.
