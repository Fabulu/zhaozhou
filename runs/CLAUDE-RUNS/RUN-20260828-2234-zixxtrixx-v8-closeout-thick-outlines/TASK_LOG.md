# Task Log: RUN-20260828-2234 - Zixxtrixx v8 closeout and thick outlines

**Created:** 2026-08-28 22:34 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-2234-zixxtrixx-v8-closeout-thick-outlines/

---

## Objective

Continue the interrupted v7 creature pass with a strong dedicated modelling agent and finish every remaining Zixxtrixx job across form, animation, design, texturing, experiments, presentation, gates, site assembly, publication, evidence, and both-repo delivery. Re-audit the claimed 15 completed queue items visually rather than treating their gates as likeness evidence.

---

## Progress Timeline

### 2026-08-28 22:34 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-2234.
- Read `Upheaval/creature/00-START-HERE.md`, all six creature law/spec guides, all three durable owner-direction files, the two reports they require, and both v7 handoffs in full.
- Checked `zhaozhou/reports/` for files modified after the v7 run began; none exist.
- Owner clarified the goal: finish every remaining Zixxtrixx job across modelling, animation, design, texturing, experiments, presentation, gates, site assembly, and publication — not merely mechanical closeout.
- Owner clarified the experiment scope: BOTH normal shading + thick outline and cel shading + thick outline must be separate delivered variants, and this should be a broad creative variant menu rather than a minimal two-file closeout.
- Standing user override was updated at 22:36: use a strong modelling agent for all modelling/implementation work, commit and push routinely, and publish `upheaval.pages.dev` when the finished job is ready. This supersedes the earlier no-deploy instruction.

### 2026-08-28 23:11 UTC+02:00 - Mandatory reading, inherited-state audit, and first thick renders

- Completed the required creature map/spec/owner-direction/report/handoff reading. `docs/coding_agents/SUBAGENT_GUIDE.md`, referenced by the map, is absent from the repository; the stale link was verified by a repository-wide search rather than silently skipped.
- Verified no inherited Zixxtrixx renderer or `zhao-reel.exe` process remained. A separate FPGA lane is actively running `tools/sweep_field_v3_exec.sh`; it mutates `fpga/rtl/synth/zhao_probe_v3_exec.sv`. Per the repository's sweep law, no checkout, branch switch, staging, or commit will touch the zhaozhou tree until that sweep finishes and restores its file. Reel-only implementation and direct compilation do not touch the sweep's RTL or build tree.
- Inspected both repositories. Upheaval's inherited v7 site/renders remain uncommitted and were carried onto dedicated branch `zixxtrixx-v8-closeout`. Zhaozhou is still on `main` pending the live sweep; the dedicated branch will be created before this run's first zhaozhou commit.
- Confirmed the v7 experiment factory reached `EXPERIMENTS DONE`, but never reached its promised `FALL REENCODED`. The current public fall poster is the stale cropped render; the final `cam_k=290000` reel binary is newer and fall must be re-rendered and re-encoded.
- Visually audited the final Side.png comparison, head close-ups, walk junction, tail-balance, taunt/head-wobble, hit-fold peaks, six-salto wheel, flying target, death2 eye modes, and inherited experiment posters. The front-S/head block, junction, taunt looseness and head roll, hit deformation, held six-salto curl, and flying-target wings read complete. The stale fall presentation is not complete. Several subtle texture axes need a better comparison presentation because their idle poster alone is visually near-identical.
- Implemented named render-side outline radius control plus separate `ZIXX_EXP=thick` and `ZIXX_EXP=celthick` modes in `tools/reel/zhao_reel.cpp`; default remains off. Direct-built the reel and contour-page experiment executable without `cmake --build`.
- Rendered and encoded the required normal+thick and cel3+thick idle variants at 384x240. Radius 5 reads as intentionally heavy concept-sheet ink at the site camera, including the thin blades, without altering the shipping page.
- New owner direction arrived at 23:08: after the fully verified main v8 pass is committed, pushed and published, make a SECOND delivery containing cel3+thick versions of every canonical clip. Expose the complete alternate set behind one additional experimental site tab, preserve useful gameplay-distance comparisons, commit/push it, and redeploy production. This is task #8 and is deliberately blocked by task #5/main publication.

### 2026-08-28 23:58 UTC+02:00 - Every-frame audit and explicit shared style architecture

- Generated and looked through every frame of all 17 canonical presentation subjects at 384x240 (96x60 tiles, 16 per row). No stray geometry, accidental camera clipping, seam reopening, pose collapse or unauthorised ground fault was found. The attack's five-second rigid spear and each death's terminal stillness are declared choreography, not systemic rigidity; all living/transition clips retain slow shape change. The corrected fall now keeps the entire animal in frame through every long vertical pose.
- Preserved the two selectable style directions in ONE renderer. Normal and cel3+thick share geometry compilation, animation, skinning, visibility, projection, camera and texture sampling; `ZIXX_EXP` selects only the contour-page build plus narrow lighting/post-presentation state. There is no duplicated creature renderer, no second animation asset, and no experimental overwrite of the shipping page.
- The first cel pass quantised Gouraud vertices and then interpolated them, silently rebuilding a gradient at 240p. Revised the experimental branch to select one of three named light levels from the triangle face light and hold it across the triangle; normal never enters this branch. The cel3 and cel3+thick outputs now read as distinct flat-band treatments at gameplay distance.
- Rendered the pencil-underdrawing experiment. Its inherited two-texel, 16% graphite was genuinely invisible at 384x240, so named line width, alpha, cadence and graphite controls were authored by eye until the long searches and broken cross-contours read clearly without changing creature pigments.
- Hardware mapping was independently audited at 00:23. The current split is the intended silicon-shaped one: cel3 is a face-light comparator plus authored RGB-band table mode in the shared shading path; interior ink remains ordinary RGB565 texture/mips; the five-pixel inward silhouette models a future visible-creature effect tag carried through resolve into the already-chartered `POST.GATHER` / `POST.COMPOSITE` one-bit mask edge pass. No cel accelerator or second renderer is implied.
- Re-generated the shipping page twice byte-identically and rendered the default 96-frame run from the current binary: all 96 RGB dumps are byte-identical to the pre-experiment canonical frames.

### 2026-08-29 01:01 UTC+02:00 - Faceted topology audit and second honest toon read

- Inspected the face-flat cel output specifically as a topology diagnostic. The continuous tube has no zipper/meshlet-band seam, accidental silhouette crack, or stray long sliver; the visible crown, cheek, throat and ring planes form coherent authored patches. The faceted style does expose those planes strongly, as intended, without requiring a duplicate head or outline shell. Fins remain ordinary weighted geometry under the post-mask outline.
- Kept the existing coherent local/shared vertex normals and unified continuous surface intact. The model was NOT remodeled around the faceted look, so a smooth-surface style can still interpolate one light scalar over those normals.
- Added that second style as `smoothcelthick`, separately named and separately rendered rather than replacing `celthick`. It uses the shared Gouraud light lanes, thresholds their mean per fragment through the same named three-level table, then applies the existing interior RGB565 ink and five-pixel inward mask silhouette. At 384x240 it reads distinctly: faceted cel exposes bright triangle planes; smooth toon produces broad surface-following bands. Matched evidence is `evidence/cel-read-comparison-frame000.png`.

### 2026-08-29 01:34 UTC+02:00 - First-stage harvest, deterministic assembly, and clean gates

- Replaced the inherited harvest's one-Python-byte-at-a-time CRC32C hot path with the installed incremental hardware-backed `crc32c` module while retaining the exact portable table fallback. Regenerated all 17 sequence CRCs and the idle/walk/attack/fall every-frame golden sheets from the final canonical frame dumps.
- Reassembled the 58-render first-stage site twice; SHA-256 outputs are identical. Verified `noindex,nofollow`, all four newly required selectable treatments, 35 live tabs, one archive tab, and deliberate absence of the post-publication all-clips cel collection.
- Validated every manifest source: all 56 WebMs decode at native 384x240, all 56 posters are the intended 3x nearest-neighbour 1152x720 presentation assets, and both retained archive GIFs are 384x240.
- Made a run-local direct-build script, deleted every run-local reference object, recompiled the full reference set after the `TriMode` layout change, then relinked reel, probe, golden, choreo, planner, headaim, sideprofile, meshcheck and striketip without `cmake --build`.
- Fresh gates pass: probe, choreo, planner, striketip and meshcheck all exit zero; two independent golden dumps are byte-identical; all 41 generated golden files compare byte-clean against `Upheaval/creature/Zixxtrixx/golden`; `zhao-reel --check` ends `all sequence CRCs match` with stdout redirected to `evidence/final-check.txt`.

---

## Subagent Spawns

No subagents. Implementation remains serial as required.

---

## Files Created

- `evidence/` (for final experiment and gate evidence)

---

## Decisions Made

- Preserve all v7 model, rig, animation, texture, and golden work unless a closeout defect forces a correction.
- Implement outline thickness in the reel post-process, not baked into the texture: it must remain legible at site-camera distance instead of shrinking and disappearing through mips.
- Keep effects isolated: thick normal uses contour page + thick silhouette under normal shading; thick cel uses the same page + thick silhouette + three-tone cel shading.
- Never stage unrelated FPGA lane changes identified by the v7 handoff.

---

## Next Steps

1. Create the dedicated zhaozhou branch now that the live FPGA mutation sweep has restored its temporary file; stage only explicit Zixxtrixx/run paths and preserve every unrelated RTL, synthesis, capture and render directory.
2. Make logical commits in both repositories, push both dedicated branches, then deliver this first-stage state to `main` without folding in the all-clips cel collection.
3. **FIRST PUBLICATION:** run `deploy.ps1 -Project upheaval -Branch main`, verify production and log the URL/output.
4. **TASK #8, ONLY AFTER STEP 3:** generate cel3+thick versions of every canonical animation, expose all clips behind one extra Zixxtrixx experimental collection tab, inspect at gameplay/site distance, make a second commit/push, and make a second production publication.
