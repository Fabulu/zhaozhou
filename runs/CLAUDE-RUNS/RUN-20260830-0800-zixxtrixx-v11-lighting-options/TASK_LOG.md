# Task Log: RUN-20260830-0800 - Zixxtrixx v11 lighting options

**Created:** 2026-08-30 08:00 UTC+02:00
**Status:** Complete
**Working Directory:** `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/`

---

## Objective

Diagnose residual lighting machinery versus artistic-rig insufficiency, author a small native-resolution baseline-plus-options choice set without changing approved creature art, publish it once to the noindex bestiary, and leave the queued larger gummy spring untouched until lighting selection.

---

## Repository Contract

- Zhaozhou isolated clone: `C:/programmieren/zencrifice/zixxtrixx-v11-lighting-lane/zhaozhou`, branch `zixxtrixx-v11-lighting-options`, base `94ae98178346e460eca74b8bfddd554903d60a01`.
- Upheaval isolated clone: `C:/programmieren/zencrifice/zixxtrixx-v11-lighting-lane/Upheaval`, branch `zixxtrixx-v11-lighting-options`, base `2ff2ffa12d4076d8147e6b54c474b06a4053ac3f`.
- Shared repositories and outputs are forbidden. No worktree, Sacengine, `cmake --build`, `git add -A`, catalogue regeneration or spring implementation.

---

## Progress Timeline

### 2026-08-30 08:00 UTC+02:00 - Run initialized and prerequisites read

- Created mandatory run `RUN-20260830-0800` in the isolated Zhaozhou clone.
- Read both repository `CLAUDE.md` files and every durable Zixxtrixx owner direction through #10.
- Read the only report newer than the v10 source milestone, `reports/FIELD_V3_QUAD_EXECUTOR.md`; it is unrelated concurrent hardware work and remains untouched.
- Recorded named bounded validation budget `V11-LIGHT-BUDGET-1` before any build, diagnostic or render.
- Wrote durable `OWNER-DIRECTION-11-2026-08-30.md`, including the queued larger whole-snake S / slower gummy bounce / pause / launch direction and explicit spring deferral.
- Committed and pushed owner direction #11 in Upheaval as `9ed62ae`.

### 2026-08-30 - Fresh direct current-main compositor diagnosis

- Audited v10's normal/lighting state flow: both bind normals are transformed by their pose matrices, the weighted direction is normalized once before Lambert, key and fill share world space, inward zipper winding is reversed only for outward face-light evaluation, and cel thresholds follow Gouraud interpolation.
- Fetched Zhaozhou `origin/main` at `0550405bbf659f26a9b8750fe38a36d5f92899fb`; no creature, reel, renderer or compositor-smoke source differs from the v11 base.
- Rebuilt all 29 reference translation units into run-local objects using `build-direct.sh`; never used CMake.
- Fresh `creature_core` is green. An instrumented clean binary reports clip W = 4 and 1,305/4,096 lit pixels, proving the relayed 4,096-pixel failure was stale pre-camera-fix executable state, not a source regression.
- Preserved the corrected camera, approved extent and existing gate unchanged.

### 2026-08-30 - Bounded structural verdict and lighting choice set

- Rendered exactly one matched native-resolution structural set: current cel-main baseline, unlit and normal visualisation.
- Unlit form is intact and bright; normal colours travel smoothly around the complete tube without an isolated flip or meshlet/bone seam. A minimal +/-Z axis probe showed coherent hemisphere response and located the baseline's key on the back/side-weighted hemisphere for the fixed comparison.
- Concluded no residual transform, normal-blend, winding, interpolation, state or light-space defect survives v10. The current artistic key/fill/ambient arrangement causes the shadow-clad read.
- Added generic named `CreatureLightRig` controls and a reversible `ZIXX_LIGHT` selector. Default baseline is byte-identical (`zixxtrixx-still` cel-main CRC32C remains `F0DA9F88`).
- Authored by eye three restrained and materially distinct alternatives: A Front Soft, B High Open and C Crossfill. Model, pigments, texture, crayon, smooth/face mix, cel thresholds, pose, animation and camera remain identical.
- Rendered one four-still native comparison and one 24-frame fixed-camera idle excerpt per rig, assembled as a single 2x2 motion comparison. Each comparison cell remains native 384x240; frame 0 matches its still byte-for-byte.
- Bounded review found coherent moving light and no state bleed. No catalogue, spring or broad regression campaign was run.

### 2026-08-30 - Noindex comparison authored and checked locally

- Added one experimental owner-choice collection after the selected v10 production clips: four native 384x240 stills and one short 768x520 VP9 four-up comparison. The production Idle remains initially selected.
- Regenerated the site from `website/creatures.json`; exact `<meta name="robots" content="noindex, nofollow">` remains present once and all protected 2026-08-27/28 archives remain untouched.
- Bounded Edge checks passed at 1440x1000 and 390x844: all five choice items load, stills report 384x240, video reports 768x520, and neither layout has horizontal overflow.
- Stopped the local HTTP server and browser children after evidence capture.

### 2026-08-30 - Safe integration, one deployment and production closeout

- Fetched both remotes before integration. Zhaozhou current main advanced only in concurrent field-v3 Earth-fit work; it was merged into the feature without overlap. Upheaval main remained at the feature base.
- Pushed Zhaozhou feature `7531ff33b6704b0221f6ca3c767a665b5a2165c0`, then integrated it into main at `059dc736f636da9e700c05e33f966be08557355a`. Pushed Upheaval feature `245b836f82e02212baa11a881f49b7fa0e707d69`, then integrated it into main at `a9411ce4b84646f10d7c22a86a7f9562fb1f2bcc`. No force push or shared checkout was used.
- Invoked `website/deploy.ps1 -Project upheaval -Branch main` exactly once. Wrangler published deployment `https://4b269d53.upheaval.pages.dev`; the production alias is `https://upheaval.pages.dev`.
- Cache-bypassed exact-byte checks passed on both URLs for the generated index, all four choice stills and the comparison WebM. Production contains exact noindex once, exposes the choice section, and still starts on the v10 Idle.
- Production Edge checks passed at 1440x1000 and 390x844 with all media dimensions and no horizontal overflow intact.
- Restored the deploy-time generated timestamp change, removed untracked render output, and found no lane-owned renderer, ffmpeg, HTTP server, Playwright Edge, Wrangler or build child remaining.
- Protected 2026-08-27/28 archives remain untouched. The animation catalogue was not regenerated, the spring was not changed, and migration work was not released.

---

## Validation Ledger — `V11-LIGHT-BUDGET-1`

| ID | Acceptance question | Bounded input | Status / decision |
|---|---|---|---|
| L11-S | Does any normal/transform/winding/light-space/state defect survive v10? | One source/state audit; one identical-pose unlit/normals/baseline set; smallest existing normal/temporal probe | **PASS** — machinery coherent; baseline rig is back/side-weighted for the fixed native shot |
| L11-A | Do named alternatives differ only in lighting and remain stable/readable at native resolution? | One identical 384×240 baseline-plus-options set; one short deforming sample only if needed | **PASS** — baseline + three named rigs, one 24-frame 2x2 comparison, no state bleed |
| L11-W | Does comparison media load with exact noindex on local and production site? | Media existence/decode; desktop+narrow section check; bounded cache-bypassed production bytes | **PASS** — exact noindex, selected v10 Idle, five choice items, responsive layouts and exact production media bytes verified |

No catalogue rerender/redecode or spring checks unless a concrete regression signal triggers escalation.

---

## Subagent Spawns

None. This remains the sole implementation/modelling lane.

---

## Files Created

- `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-11-2026-08-30.md`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/SPEC_v1.md`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/build-direct.sh`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/current-main-fresh-direct-build.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/current-main-camera-diagnostic.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/current-main-camera-conclusion.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/structural-{render,verdict}.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/structural-{baseline,unlit,normviz}.png`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/lighting-options-values.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/lighting-options-{contact,motion-check}.png`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/option-{render-final,motion-render}.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/website-local-{browser.txt,desktop.png,narrow.png}`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/deployment.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/production-verification.txt`
- `runs/CLAUDE-RUNS/RUN-20260830-0800-zixxtrixx-v11-lighting-options/evidence/website-production-{browser.txt,desktop.png,narrow.png}`

---

## Decisions Made

- V11 is an owner-choice lighting comparison, not a production-standard replacement.
- Approved model, proportions, geometry, eyes, textures, crayon treatment, cel thresholds, animation and camera are frozen.
- Machinery diagnosis precedes rig authorship; any generic defect is committed separately from artistic options.
- Spring implementation waits for owner lighting selection.

---

## Closeout

Run complete. V10 remains the selected production presentation; the next implementation pass begins only after the owner chooses a lighting direction, at which point the durable queued gummy-spring direction can be implemented.
