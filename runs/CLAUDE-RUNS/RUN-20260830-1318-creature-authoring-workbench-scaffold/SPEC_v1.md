# SPEC v1: Creature authoring workbench and Zixxtrixx agent pack

**Run ID:** RUN-20260830-1318
**Created:** 2026-08-30 13:18 UTC+02:00
**Status:** Active
**Previous Version:** V14 source `910fd9636100cd15b73b6357ab756fdd0297ed55`; Upheaval `04ab965b26e8d0c60272940d42da2d67e946f897`

---

## Objective

Turn the existing creature-authoring blueprint and the proven V14 one-binary,
many-preset lane into a compact durable workbench: edit named controls, run one
explicit command, inspect local output, and publish only through a separate
explicit action. Add a concise Zixxtrixx pack that carries current state,
frozen invariants, editable knobs, exact paths/commands, known issues, queued
spring direction and a curated link index, without replacing full history.

---

## Scope

**In Scope:**

- One generic direct reel build script derived from the committed V14 recipe,
  requiring an explicit output directory and never invoking CMake.
- One blueprint-owned executable workbench entry point accepting a small JSON
  manifest and named preset list.
- Default plan/dry-run plus explicit local render/VP9/site and explicit publish
  actions; publish must never occur by default.
- Existing fixed subject/camera, `ZIXX_EXP`/`ZIXX_LIGHT` selection, `tovideo.py`,
  `website/creatures.json`, assembler, noindex guard and deploy script.
- A V14 sample manifest and compact Zixxtrixx agent pack beside the creature.
- Bounded validation using the plan and at most one already-rendered V14 variant.
- Logical commits/pushes, current-main integration and stopped-job proof.

**Out of Scope:**

- Any renderer, subject, camera, animation, model, texture, lighting-table or
  website-visible content change.
- Rerendering or re-encoding all ten V14 variants; website deployment.
- Gummy-spring implementation or media.
- Investigating or repinning the three-way creature CRC disagreement.
- CMake builds, broad catalogue generation, Sacengine, FPGA/compiler/hardware
  paths, `tests/CMakeLists.txt`, `CLAUDE.md`, or a bespoke validation framework.

---

## Constraints

- Fresh ordinary clones in
  `C:/programmieren/zencrifice/zixxtrixx-workbench-lane`; no worktree and no
  shared checkout changes.
- Exact bases:
  - zhaozhou `910fd9636100cd15b73b6357ab756fdd0297ed55`
  - Upheaval `04ab965b26e8d0c60272940d42da2d67e946f897`
- Extend `Upheaval/creature/CREATURE-AUTHORING-BLUEPRINT`; do not create a
  competing creature pipeline or renderer.
- Workbench outputs remain explicit and lane-local beneath
  `Upheaval/build/generated/creatures/<id>/`; accepted website media remain
  untouched unless a future deliberate local action targets them.
- Every owner direction and relevant report is read before implementation.
- Stage exact paths only; never `git add -A`. Commit and push milestones as they
  land. Merge/push both mains only after fetching current origins.
- No deployment: this is invisible scaffolding, not a finished creature pass.

### Bounded validation budget — `WORKBENCH-SCAFFOLD-1`

| ID | Acceptance question | Bounded evidence |
|---|---|---|
| WB-PLAN | Does the default command print the exact build/render/encode/manifest/assemble plan for named V14 presets without writing or publishing? | Default plan plus selected-preset plan; before/after tracked status |
| WB-SAFETY | Can publication happen only through an explicit publish action with an explicit branch? | CLI/parser tests, plan output and source inspection; no deploy invocation |
| WB-SAMPLE | Does the scaffold understand one existing V14 variant and preserve manifest/media/noindex contracts? | One `diagonal-daylight` existing-media metadata check; no rerender/re-encode |
| WB-BLUEPRINT | Do the extended blueprint and tools remain internally sound? | Existing `validate_blueprint.py`, Python compile/help, shell syntax check |
| WB-PACK | Does the Zixxtrixx pack expose all required current/frozen/editable/history/issue information without deleting history? | File/link/manifest checks and direction inventory |

Stop when these five questions are answered. Do not turn scaffold validation into
an art, catalogue, renderer or publication pass.

---

## Don't Retry

- Do not duplicate the V14 subject or renderer; select the existing subject and
  named lights through the existing one-binary environment seam.
- Do not rerender ten videos to prove orchestration. Use plan output and one
  existing accepted asset check.
- Do not run `cmake --build`; the durable direct builder is the V14 recipe with
  explicit output ownership.
- Do not investigate the CRC mismatch. Record the three pairs and leave expected
  constants unchanged.
- Do not implement owner direction #13 spring work.
- Do not deploy an invisible scaffold.

---

## Open Questions

None. Reversible defaults are declared in the manifest and CLI; publishing stays
an explicit future decision.
