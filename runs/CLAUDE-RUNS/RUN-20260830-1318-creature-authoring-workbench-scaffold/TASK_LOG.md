# Task Log: RUN-20260830-1318 - Creature authoring workbench scaffold

**Created:** 2026-08-30 13:18 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260830-1318-creature-authoring-workbench-scaffold/

---

## Objective

Add a minimal executable creature workbench to the existing blueprint and a
compact current-state/history pack beside Zixxtrixx, using V14 as the proven
sample while preserving art, archives, noindex and explicit-only publication.

---

## Repository Contract

- Fresh Zhaozhou clone:
  `C:/programmieren/zencrifice/zixxtrixx-workbench-lane/zhaozhou`, branch
  `creature-workbench-scaffold`, base
  `910fd9636100cd15b73b6357ab756fdd0297ed55`.
- Fresh Upheaval clone:
  `C:/programmieren/zencrifice/zixxtrixx-workbench-lane/Upheaval`, branch
  `creature-workbench-scaffold`, base
  `04ab965b26e8d0c60272940d42da2d67e946f897`.
- Shared repositories remain untouched. No worktree, CMake build, Sacengine,
  `git add -A`, deployment, spring work, CRC repin, or unrelated hardware/test
  edits.

---

## Progress Timeline

### 2026-08-30 13:18 UTC+02:00 - Fresh lane and bounded run

- Verified both requested SHAs are the exact remote `main` heads, cloned both
  repositories into one fresh ordinary isolated lane and created matching
  feature branches.
- Initialized this run through the committed `init-run.ps1`.
- Read both repository instructions; the blueprint entry documents, all eight
  specifications, manifests and validation/scaffolding contracts; all fifteen
  then-existing Zixxtrixx `OWNER-DIRECTION-*` files; `ZixxtrixxReport.md` and
  `Headache.md`; and the committed V14 run, direct builder, selector, subject,
  encoder and site-manifest pattern.
- Confirmed no species-local `reports/` directory exists and V14 already recorded
  that the latest committed general report predates the completed v13 run.
- Set bounded validation budget `WORKBENCH-SCAFFOLD-1` before implementation.

### 2026-08-30 13:36 UTC+02:00 - Durable direct build seam

- Promoted V14's successful run-local direct compile roster into executable
  `tools/reel/build-direct.sh` with an explicit caller-owned output, clean mode,
  one/all target selection and no CMake/Ninja/Verilator path.
- The script waits all parallel compiler children on failure and terminates them
  on interruption rather than orphaning work.
- Passed shell syntax/help and a clean real `cel` direct build into
  `Upheaval/build/generated/creatures/zixxtrixx/scaffold-builder-check`.
- Committed/pushed zhaozhou milestones `922a82a77acf6267ff354d134a2da31c4deadc7b`
  and `bf54f8823d1bcf56b75e9268d885a5a590712f19`.

### 2026-08-30 13:52 UTC+02:00 - Blueprint-owned workbench

- Added `tools/workbench.py` inside the existing blueprint. The default is a
  read-only plan for one manifest default; `local`, `check-existing` and
  `publish` are explicit actions, `--all-presets` is deliberate, accepted media
  replacement requires `--replace-media`, and publication additionally requires
  a valid explicit branch.
- The workbench reuses only the canonical direct builder, fixed registered reel
  subject, existing encoder, `website/creatures.json`, assembler, exact noindex
  and guarded deploy script. It resolves Git Bash explicitly on Windows so the
  WSL app alias cannot intercept the direct build.
- Extended the existing README, quickstart, media/publication specification and
  validator rather than adding a competing pipeline. Existing validation now
  includes workbench syntax/help.
- Committed/pushed Upheaval milestone
  `b70263da5f2590a1f560880a1e374d562523d2c3`.

### 2026-08-30 14:03 UTC+02:00 - V14 sample and durable agent pack

- Added a ten-preset V14 manifest with Cool Cross as its safe default, exact
  fixed subject/camera/media/tool contracts and all accepted site mappings.
- Added compact `START-HERE.md` and `HISTORY-INDEX.md` with V14 invariants,
  editable lighting/model/animation controls, exact commands and paths, known
  CRC issue, every durable direction, relevant reports and all significant runs.
- Durable Owner Direction 16 arrived during scaffolding and was recorded beside
  Zixxtrixx as CURRENT Task #28 direction only. It queues small dangerous
  head/neck-first adjustments, full-head dorsal pink, Cool Cross, the existing
  whole-body gummy spring, normal-animation rerenders and a spatially responsive
  visible-local-light animation. No art, spring, rendering or publication was
  implemented here.
- Committed/pushed Upheaval milestone
  `cc2cef04dfb54ebea6836174c3b39d169b71f2cf`.

### 2026-08-30 14:08 UTC+02:00 - Bounded validation complete

- Default Cool Cross plan and selected two-preset plan printed exact commands;
  tracked/untracked repository-status snapshots were byte-identical before and
  after the default plan.
- Missing publish branch and branch-on-plan both returned 2 without deployment;
  local use of an accepted name without `--replace-media` returned 2 before
  build/render and left repository status unchanged.
- One representative existing preset only, `diagonal-daylight`, passed:
  VP9/yuv444p, 384x240, 600 frames, 60 fps, 10.000000 s, 1152x720 poster,
  manifest reference and exact noindex.
- Full existing blueprint validation passed: 15 templates, 25 internal links,
  23 pinned commit:file references, workbench syntax/help, identical independent
  LF/CRLF/delete-rebuild scaffold bytes and unchanged input repository state.
- Agent pack inventory passed with 16/16 owner directions, ten unique presets,
  Cool Cross default and 51 valid local/cross-repository links.
- No render, encode, site assembly, browser, server or deploy process was
  launched. No V14 media or expected CRC constant changed.

---

## Validation Ledger — `WORKBENCH-SCAFFOLD-1`

| ID | Acceptance question | Status |
|---|---|---|
| WB-PLAN | Default exact plan without writes/publish? | PASS — Cool Cross default and selected two-preset plans; status snapshots unchanged |
| WB-SAFETY | Publish explicit action plus explicit branch only? | PASS — parser/action gates and accepted-media guard returned 2 before side effects |
| WB-SAMPLE | One existing V14 variant understood and intact? | PASS — diagonal-daylight manifest/media/poster/noindex only |
| WB-BLUEPRINT | Extended existing blueprint internally sound? | PASS — full validator plus shell/Python/help/direct-build checks |
| WB-PACK | Required compact Zixxtrixx state/history exposed? | PASS — 16 directions, ten presets, 51 links and required state/issues/queue |

---

## Subagent Spawns

None. This is the sole implementation lane.

---

## Files Created

- `runs/CLAUDE-RUNS/RUN-20260830-1318-creature-authoring-workbench-scaffold/SPEC_v1.md`
- `runs/CLAUDE-RUNS/RUN-20260830-1318-creature-authoring-workbench-scaffold/TASK_LOG.md`
- `tools/reel/build-direct.sh`
- Upheaval `creature/CREATURE-AUTHORING-BLUEPRINT/tools/workbench.py`
- Upheaval `creature/Zixxtrixx/OWNER-DIRECTION-16-2026-08-30.md`
- Upheaval `creature/Zixxtrixx/AGENT-PACK/START-HERE.md`
- Upheaval `creature/Zixxtrixx/AGENT-PACK/HISTORY-INDEX.md`
- Upheaval `creature/Zixxtrixx/AGENT-PACK/V14-WORKBENCH.json`

---

## Decisions Made

- The general direct compiler remains zhaozhou-owned; the manifest-driven
  authoring orchestration remains blueprint/Upheaval-owned.
- The V14 sample points at the existing corrected subject and named rig selector;
  it does not duplicate renderer, subject, camera or media architecture.
- Default action is a read-only plan. Local execution and publication are
  separate explicit actions; publication additionally requires an explicit
  branch.
- The one bounded representative check consumes existing accepted V14 media; it
  does not render, encode, assemble or deploy.
- Cool Cross is the V14 manifest default per current Owner Direction 16; all ten
  presets remain addressable, but require named selection or `--all-presets`.
- Tool paths that can execute or publish are canonical, not manifest-extensible:
  the species manifest selects data/presets while the workbench reuses the one
  existing builder/encoder/assembler/deployer.
- Owner Direction 16 is recorded and linked as queued Task #28 work; implementing
  any of it would violate this scaffold run's scope.

---

## Next Steps

- Fetch both origins and verify each remote main still equals the requested base.
- Fast-forward both feature branches to main without force, push and record exact
  main SHAs.
- Remove the one lane-local builder-check output, verify no owned child survives
  and prove both repository trees clean.
