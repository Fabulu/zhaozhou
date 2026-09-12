# SPEC v1: Resume terrain projection pipeline composition

**Run ID:** RUN-20260912-1841
**Created:** 2026-09-12 18:41 UTC+02:00
**Status:** Complete
**Previous Run:** `RUN-20260910-1014-terrain-pipeline-composition`

---

## Objective

Recover and complete the interrupted terrain pipeline composition packet from the prior run without overstating adoption: finish and verify the generic projection subsystem refactor, group/view/arena sequencer, composed terrain pipe, differential/positive controls, bookkeeping, and `reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md`; then commit and push the verified logical packet.

---

## Recovered Evidence

The prior run measured the downstream boundary before authoring the shell:

- `GEOM.CLIP -> GEOM.SETUP`: 4,096 accepted triangles in 4,102 clocks, including 30% downstream stalls; effectively one triangle/clock.
- `GEOM.BINNER`: 5.00 clocks/triangle and 128 triangles/frame; this is the present downstream wall, not CLIP/SETUP.
- Three projection arena replicas remain justified by the upstream frame demand and CLIP/SETUP rate, while the report must state that no current block beyond SETUP consumes that rate.
- Tess vertex mode is view-independent: tessellate/fill once, then project for each view.
- The inherited claim that the current arena already has a reference counter is false; release safety must be implemented and positively controlled.

---

## Scope

**In Scope:**

- Inspect and preserve the interrupted working tree from `RUN-20260910-1014`.
- Complete `zhao_proj_subsystem` walker-out/generic-ref-port refactor.
- Complete `zhao_terrain_group_seq` and `zhao_terrain_pipe`.
- Complete the composed differential against the retained projection oracle/path for both views.
- Verify group release cannot reopen storage under a live reader; every counter must be seen to fire or have a committed mutant control.
- Keep fit-target and production-manifest bookkeeping explicitly `not-yet-adopted`.
- Regenerate/check `zhao_prod_top.sv` if ports or manifest closure require it.
- Write the composition report, update run records, commit, and push.

**Out of Scope for this packet:**

- Declaring production adoption before the composed subsystem is measured.
- Deleting `zhao_terrain_topo`.
- Silent stitch/morph/void/underside topology changes.
- Claiming ALM/DSP/M10K savings without a clean subsystem fit.
- The unresolved normals leg and DEPTHQUANT integration, except to name and price the remaining work honestly.

---

## Constraints

- Use one repository-writing implementation lane at a time. The original no-Qwen constraint was superseded on 2026-09-12 by the owner's calibration instruction: at most two local Qwen jobs, initially short, read-only, and independently graded.
- Do not overwrite the interrupted files before inspecting them.
- No Quartus fit until correctness and composition gates pass; the later fit must ask the subsystem-boundary area/Fmax/RAM/DSP question.
- Direct Verilator builds must use the pinned repository toolchain and explicit return codes; rerun an executable directly if the known exit-deadlock yields RC=1 with no output.
- `gen_prod_top.py --check` only until a port/manifest change specifically requires regeneration.
- Stage explicit paths, then inspect both staged and unstaged status before committing.

---

## Don't Retry

- Do not collapse the arena to one replica merely because `GEOM.BINNER` is slow; measured CLIP/SETUP accepts one triangle/clock and the current binner is not a composed terrain consumer.
- Do not cite the old arena reference counter: it was a design-study feature and is absent from the current shell.
- Do not re-tessellate per view; mode 1 output is view-independent.
- Do not run the entire 280+ MB transcript through context; search it narrowly only if run records, reports, source, and git history leave an ambiguity.

---

## Open Questions

- Exact normals ownership in the arena architecture: mode-0 pass, world-space arena, or the ratified normal-map path.
- Whether the completed subsystem packet is ready for the next clean subsystem-boundary fit, or exposes another structural prerequisite first.
