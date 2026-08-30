# Task Log: RUN-20260830-0329 - Creature authoring blueprint

**Created:** 2026-08-30 03:29 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260830-0329-creature-authoring-blueprint/

---

## Objective

Create and ship a durable, executable starting point for future Tribute Upheaval
creatures under `Upheaval/creature/CREATURE-AUTHORING-BLUEPRINT/`, grounded in
the repaired Zixxtrixx v10 implementation and generic zhaozhou APIs without
copying species-specific generated source.

---

## Progress Timeline

### 2026-08-30 03:29 UTC+02:00 - Task started

- Generated Run ID `RUN-20260830-0329`.
- Read root, zhaozhou, and Upheaval `CLAUDE.md` rules.
- Initially read directions 1-9; subsequently found and read durable Zixxtrixx
  `OWNER-DIRECTION-10-2026-08-29.md`. All ten directions govern this work.
- Confirmed `Upheaval/creature/Zixxtrixx/reports/` does not exist; followed the
  durable direction pointers to `zhaozhou/reports/ZixxtrixxReport.md` and
  `zhaozhou/reports/Headache.md` instead.
- Committed to clean clones beneath this run; no shared checkout reset, clean,
  staging, process stop, or production edit.

### Evidence and implementation review

- Created clean run-local clones/branches:
  - Upheaval `creature-authoring-blueprint`, base
    `9d70baa20a73bcc12356f7ffc26717c18c03b03c`;
  - zhaozhou `creature-authoring-blueprint-run`, base
    `94ae98178346e460eca74b8bfddd554903d60a01`.
- Enabled `core.longpaths` only in the isolated zhaozhou clone after Windows
  checkout initially failed on long historical paths, then restored that clone
  with `git reset --hard HEAD`; no shared checkout was touched.
- Read final v10 task/spec evidence, reusable/failure reports, ownership
  architecture, generic creature API/compiler/renderer/tests, final species
  source, texture recipe, mesh/contact/golden/planner/choreography probes, site
  generation manifest, WebM encoder, and noindex deployment guard.
- Confirmed migration-era production still lives under zhaozhou tools while the
  target dependency direction requires future Upheaval-owned species source and
  explicit Upheaval-local generated output.

### Blueprint authored

Created `Upheaval/creature/CREATURE-AUTHORING-BLUEPRINT/` with:

- navigable `README.md` and executable `QUICKSTART.md`;
- `template-manifest.json` and machine-checkable `references.json`;
- eight focused ownership/art/geometry/rendering/animation/contact/validation/
  media/failure/handoff specifications;
- 15 neutral species templates covering manifest/spec/direction, named art
  controls, generic provider composition, integer plans, explicit-output texture
  recipe, subjects/cameras, committed posed-vertex probe, media staging, bounded
  validation, and final handoff;
- deterministic `tools/new_creature.py` scaffolder with explicit Upheaval root,
  species ID/name and direct-child output, atomic absent/empty destination,
  stable inventory/digests, no clock/random/host path, no generated header, and
  immediate rendered-JSON parsing;
- bounded `tools/validate_blueprint.py` checking templates/JSON, local Markdown
  links, immutable commit:file references, Python syntax/help, independent and
  delete/rebuild scaffold/texture bytes, leading-whitespace PPM pixels, output
  containment, forbidden generated/build files, and unchanged repository state.

The texture PPM parser was corrected to trim only the exact excess header
separator bytes; it preserves a valid first pixel byte equal to whitespace.

### Bounded smoke evidence

`validate_blueprint.py` passed against the two isolated repositories:

- 15 templates;
- 22 local Markdown links;
- 23 pinned commit:file references;
- scaffold tree SHA-256
  `1387ea0528f2603b94bb1fbd7df418d952b916781bebe232322a788f45055528`;
- texture output tree SHA-256
  `a884385c199d81dc1c41f39856f4390825d5856682193a9f3b61a3ed7a4550a1`;
- independent outputs and complete delete/rebuild outputs byte-identical;
- escaping destination rejected;
- input repository HEAD/tracked state unchanged.

A direct, lane-local `g++` smoke (never `cmake --build`) syntax-checked all
rendered C++ skeletons including motion plans, directly linked the rendered
provider/probe with `reference/src/zcreature/creature_core.cpp`, and ran:

```text
POSED VERTEX PROBE: PASS — actual posed vertices, declared 3D policy
```

No Sacengine, renderer, encoder, browser, server, catalogue campaign, or deploy
was launched. This task is not a finished creature pass and must not publish.

---

## Subagent Spawns

None. This lane is being performed serially by the assigned implementation agent.

---

## Files Created

- `TASK_LOG.md`
- `SPEC_v1.md`
- `work/Upheaval-blueprint/creature/CREATURE-AUTHORING-BLUEPRINT/` (authored
  blueprint tree; commits recorded at close)

---

## Decisions Made

- Treat run initialization as the sole required shared-checkout filesystem action;
  authored production work and git operations happen in clean run-local clones.
- Keep Zixxtrixx as pinned evidence/example-only; new provenance starts empty and
  generic zhaozhou implementation stays linked rather than copied.
- Require all generation destinations explicitly beneath
  `Upheaval/build/generated/creatures/<id>/`; generated headers remain untracked.
- Keep current provider/reel/production paths visibly labelled migration-era
  rather than editing the independent migration lane.
- Restrict validation to the acceptance question: path/link/reference parsing,
  deterministic scaffold/recipe generation, direct C++ smoke, and repository
  cleanliness. No visual production or broad render/decode campaign.
- No deployment because a blueprint is infrastructure, not a finished creature
  pass.

---

## Remaining closeout

1. Stage exact blueprint paths, commit and push the feature branch.
2. Fetch current Upheaval main, verify nonoverlap, integrate without rewriting
   other-lane history, and push main.
3. Commit/push the dedicated run records on their isolated zhaozhou branch.
4. Record feature/main/run SHAs, final clean statuses, and targeted stopped-job
   verification.
