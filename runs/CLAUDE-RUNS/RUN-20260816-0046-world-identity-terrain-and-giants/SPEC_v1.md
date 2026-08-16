# SPEC v1: World identity — terrain, creatures, giants (specification wave)

**Run ID:** RUN-20260816-0046
**Created:** 2026-08-16 00:46 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Turn recons S1/S2/S3 into repo-resident SPECIFICATION and CONTRACTS with the
island rim-topology decision as the centerpiece. No RTL; nothing promoted
above SPECIFIED.

---

## Scope

**In Scope:**
- spec/terrain_rules.md (Island Patch v1: dual heightfield + void + breach law)
- spec/creature_rules.md (creature/anim/pose/giant law)
- spec/cartridge.md additive page kinds 6/7/8/9
- Contract fills: TERRAIN.{PATCH,TESS,BAKE}, FORGE.CLIFF, SW.CPUCOLL,
  TEXTURE.MOSAIC, GEOM.LOOM; new GEOM.POSE contract
- blocks.yml: +GEOM.POSE (SPECIFIED), edge symmetry, regen diagrams
- ADDENDUM in this run dir; intermittent pushes to origin/main

**Out of Scope:**
- RTL of any kind; maturity promotions; edits to reference/src/zsky/**,
  spec/sky_and_beams.md, Noctis star/flare addendum (owned by next agent)
- commands.zidl edits (ReparentTransform proposed only — abi-gen owner lands it)
- qformats.md edits (quat decode numerics proposed as a future amendment)

---

## Constraints

- Charter §21 (contract+reference+tests before RTL), §26 refusals, §29-6/7.
- Every number derived or marked "not costed".
- Winlibs ctest only (devkitPro ctest mangles paths — reconfirmed this run).
- Verify fast suite green before each push; report pass/skip/fail separately.

---

## Don't Retry

- Running bare `ctest` (resolves to devkitPro) — always invoke
  C:/programmieren/dsstuff/mingw64/bin/ctest.exe explicitly.

---

## Open Questions

- Quantized-quat lane format (proposed S 1.0.14) — needs qformats amendment.
- Giant content-tier fragment reservations — Phase 11.
- Diagonal rim smoothing — paired sim+tess amendment, or never.
