# SPEC v1: Reusable Tribute Upheaval creature-authoring blueprint

**Run ID:** RUN-20260830-0329
**Created:** 2026-08-30 03:29 UTC+02:00
**Status:** Complete
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
- Generic `CreatureSourceProvider`, extracted `zreel`, and Upheaval-local direct
  build scripts are target contracts, not all landed APIs at the tested mains.
- Current production Zixxtrixx source/recipe/subjects/probes remain under
  `zhaozhou/tools/`; active links must move after migration while their pinned
  historical commit:path evidence remains.
- `mkcreaturepage.py` and `zhao_reel.cpp` retain legacy source-location/default
  output or registration assumptions that new species must not copy.
- Main integration is permitted only if latest remote changes are cleanly nonoverlapping with the new blueprint folder.

---

## Acceptance Result

**PASS.** The committed quickstart created the neutral package, all rendered JSON
parsed, 23 repository commit:file pointers and 22 local Markdown links resolved,
and independent LF/CRLF plus delete/rebuild generations were byte-identical.
Generated texture output was likewise identical and preserved a leading
whitespace-valued pixel. Both repository snapshots remained unchanged. All C++
skeletons syntax-checked through the pinned generic include, and the directly
linked neutral posed-vertex probe passed.

The question is answered within budget. No visual production, catalogue render,
Sacengine, `cmake --build`, encoder, browser/server, deployment, or broad decode
was needed or run.
