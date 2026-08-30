# SPEC v1: Reusable Tribute Upheaval creature-authoring blueprint

**Run ID:** RUN-20260830-0329
**Created:** 2026-08-30 03:29 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

A future agent can execute a neutral quickstart, author a second creature by eye, generate only into an explicit Upheaval-local output, validate determinism and ownership, finish motion/probes/media, and hand off safely without copying Zixxtrixx-specific generated code or moving generic machinery into Upheaval.

---

## Scope

**In Scope:**

- A navigable blueprint index plus focused specifications, checklists, neutral templates, manifest/spec examples, and small scaffold/validation machinery.
- Verified links to current generic zhaozhou APIs/tools and immutable/final Zixxtrixx examples.
- Full source-to-cartridge/golden/site-media lifecycle, art authorship, geometry/LOD/texture/rendering/rigging/animation/contact/attacks/determinism/diagnostics/media/validation/integration coverage.
- Deterministic quickstart generation with explicit output location and deletion/rebuild cleanliness checks.

**Out of Scope:**

- A new finished creature, model, render, catalogue campaign, production ownership migration, generated headers, website deployment, or changes outside the blueprint folder and run records.
- Edits to `CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md`, migration production paths, zhaozhou generic machinery, or existing Zixxtrixx assets.

---

## Constraints

- Work only in clean run-local clones on `creature-authoring-blueprint`; never mutate shared checkouts.
- Author by eye; diagnostics compare and guard but never derive artistic values from 2D projections.
- Generated headers are untracked and generation requires an explicit Upheaval-local output path.
- Same inputs generate byte-identical outputs; deleting outputs and rebuilding leaves both repositories clean.
- No sacengine, `cmake --build`, catalogue render/redecode campaign, site deployment, `git add -A`, or unrelated cleanup.
- Commit and push logical milestones, then integrate latest Upheaval main without rewriting other-lane history.

---

## Validation Budget and Acceptance Question

**Question:** Can a future agent start a neutral creature package, understand every ownership and authoring gate, and produce deterministic clean scaffold outputs using paths that exist today?

**Budget:** one scaffold generation, one same-input comparison, one delete/rebuild comparison, JSON/manifest parsing, internal-link and repository-path checks, executable syntax/help checks, and clean-repository checks. No visual production or broad render campaign.

Stop when this question is answered and all checks pass; do not expand validation merely because more tests are available.

---

## Don't Retry

- Do not infer body radii, pose, or pigment choices mechanically from concept-sheet pixels.
- Do not use image pixels to infer ground penetration.
- Do not use `cmake --build`; stale Ninja/Verilator regeneration can run old binaries.
- Do not stage broadly or touch the migration lane.
- Do not deploy: this is infrastructure, not a finished creature pass.

---

## Open Questions / Migration Assumptions

- Current generic API paths may move when the independent production-ownership migration lands. Record every verified path with a commit and a stable role so later path updates are localized.
- Main integration is permitted only if latest remote changes are cleanly nonoverlapping with the new blueprint folder.
