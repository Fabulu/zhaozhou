# SPEC v1: Zixxtrixx, first creature — concept art to renderable model

**Run ID:** RUN-20260826-0617
**Created:** 2026-08-26 06:17 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Turn the two Zixxtrixx concept drawings (`Upheaval/creature/Zixxtrixx/Concept/{Front,Side}.png`,
S. Hofer) into a real creature that compiles through `zref::creature` and renders
in the reel at 384x240, then into a GIF on the Upheaval creature site.

Success = `zhao-reel.exe` emits a Zixxtrixx subject whose silhouette is
readable at 240p, and the creature's part/bone/clip decomposition is recorded
with the reasons, not just the numbers.

---

## Scope

**In Scope:**

- Ring-part decomposition and skeleton for a serpent (part count == bend smoothness)
- The owner's three deviations from the concept art, treated as requirements:
  bigger eyes, blockier three-prong tail, head more distinct and exaggerated
- A slither locomotion clip and a stance loop, authored on the integer path
- A reel subject + GIF for `Upheaval/website/public/renders/`
- Recording measurements taken from the donor, if any are taken

**Out of Scope:**

- CLUT8 texture pages. `SkinVertex` has no colour lane and `Meshlet` carries a
  flat r/g/b "CLUT8 page stand-in"; the page layout freezes with SW.TOOLS.ASSET
  at Phase 12 entry (`creature_rules.md` §5). Colour is per-part until then.
- Publishing the creature site (needs an explicit Fabian call; `deploy.ps1`
  refuses without `-Project`)
- The full 4-idle-flourish animation set (convention noted, not built this run)

---

## Constraints

- <=32 bones; **one part = one bone**, so a serpent's bend smoothness IS its part count
- <=2 bone influences per vertex (compile gate warns >1%, rejects >3% of bound radius)
- Meshlet: <=64 verts, <=96..126 tris. `side = (rings-1)*2*segments`, `caps = segments`
- Ring segments must be 3..32 (`compile_creature` rejects outside)
- Parts orient by QUARTER TURNS only (`pitch_q`/`yaw_q`). Any other angle belongs
  in the bone bind pose
- U texcoord is not authorable: U = the ring angle's high byte, offset by `align`
- Animation 30 Hz, baked samples, no interpolation, no blending, no IK
- 384x240. Silhouette legibility beats correctness (the star-boil/pulsar lesson)
- Nothing is copied from Sacrifice. Measurements and derived laws only.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- Do not model the concept's longitudinal stripes (pink dorsal / green flank) as
  texture. There is no texture path today. They are geometry or they are nothing.

---

## Open Questions

- Where does Zixxtrixx sit on the donor height ladder (posed peasant 1.70 units,
  mid-tier 1.6-3.5)? A snake is the awkward case: length, not height, is its
  defining dimension.
- How many body waves make one slither cycle? The donor law is "one complete gait
  cycle, exactly two footfalls, 0.50-0.55 s per footfall" and a snake has no feet.
- Is a D toolchain worth installing (dub/ldc absent) to run sacengine's
  `saxs2obj` against the Steam install, or do the existing measured notes suffice?
