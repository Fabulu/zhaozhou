# Task Log: RUN-20260912-1841 - Resume terrain projection pipeline composition

**Created:** 2026-09-12 18:41 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260912-1841-resume-terrain-projection-pipeline/

---

## Objective

Resume `RUN-20260910-1014-terrain-pipeline-composition` from its interrupted working tree; complete, independently verify, report, commit, and push the terrain projection pipeline packet without prematurely claiming production adoption or measured savings.

---

## Progress Timeline

### 2026-09-12 18:41 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260912-1841.
- Recovered the prior run `RUN-20260910-1014-terrain-pipeline-composition` and its uncommitted working tree.
- Confirmed no Quartus, Verilator, CMake, Ninja, or CTest process survived the machine/session transfer.
- Installed Python 3.12.10 because the new machine exposed only the Microsoft Store alias; repository hooks and budget/report tools require a real interpreter.
- Recovered the prior rate result: CLIP+SETUP sustains 1.0015 clocks/triangle; BINNER is 5.00 clocks/triangle and 128 triangles/frame; three projection replicas remain the authored shell decision.
- Current HEAD is `9b2b153d`; interrupted changes are limited to the projection subsystem, two new terrain pipeline modules, and three downstream-rate test files, plus run records.
- Located the full 280.3 MB prior transcript at `C:\Users\Fabs\.claude\projects\C--programmieren-zencrifice\09ceca27-1cda-4f55-9590-d94d18d26c96.jsonl`; it will be queried narrowly rather than loaded wholesale if source/run evidence leaves an ambiguity.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

1. Receive the terrain-pipeline implementation handoff, inspect every diff, and run independent gates.
2. Commit and push the coherent composition packet only after positive controls pass.
3. Decide whether the next measurement is a map-only syntax/RAM gate or a full subsystem-boundary fit; do not fit an incomplete closure.
4. Resume the owner-document disposition recon serially after the implementation agent is integrated.

## 19:00 — new-machine recovery and goal continuation

- Recovered the exact prior goal: finish all briefs and roadmaps, then architect optimizations until the required ceiling is cracked and comfortably below budget; use Fable architects for major rearchitectures; avoid Quartus fits unless genuinely needed.
- One Claude implementation agent owns the terrain-pipeline continuation. A second read-only recon agent was mistakenly started after reading only the memory index; reading the full `serial-agent-execution` memory showed the later, stricter rule (one agent at a time including recon), so it was stopped immediately before producing findings.
- A separate user-started Claude session was discovered and coordinated. It owns an isolated clone/branch and lane-local outputs, will avoid the main checkout and terrain files, and will not invoke Quartus until installation is complete.
- Installed Python 3.12.10, CMake 4.4.3 and Ninja 1.13.2 for the machine. Project builds remain bound to `tools/env/zhao-env.ps1`, verified to select the existing native Winlibs CMake 4.3.2, Ninja 1.13.2, g++ and repository Verilator 5.051. Python wrappers under `C:\Users\Fabs\bin` repair the current session's stale PATH and the failing hooks.
- Started the repository-carried Quartus Lite 17.0.0 + Cyclone V unattended installer. This is an installer, not a fit; the current stop hook matches `QuartusLiteSetup*` as though it were `quartus_fit`, a false-positive to repair after the active implementation packet.
- Verified formal tools: Yosys 0.68+64 and SBY 0.68.
- Verified GitHub authentication and that origin's `zixxtrixx-v8-closeout` remains exactly at local HEAD `9b2b153d`.
- Repository-transfer integrity check found two empty `.gitkeep` files illegally placed inside `.git/refs/tags` and `.git/objects/pack`. Inspected both, removed them, then `git fsck --connectivity-only --no-dangling` and `git count-objects -vH` passed with zero garbage.
- Dry-ran the committed render-intermediate purge: 56,254 stale files / 14.48 GB are reclaimable, but 1.2 TB remains free and no deletion was authorised or performed.
- Corrected the budget interpretation for the parallel architecture lane: the campaign's 99-DSP headline subtracts `ROWS_PER_PASS=1`, but that point takes 1,990,656 clocks for dense two-view terrain before geometry and is not shippable. The current legal two-view structural frontier is approximately 111 DSP (still conditional/unfitted), leaving roughly 23 to the roadmap allocation of 88—not 5.

## 2026-09-12 — terrain composition implementation handoff

### Delivered boundary

- Refactored `zhao_proj_subsystem` to service + three-copy terrain arena shell with generic reference inputs and an explicit landed-fill arena output. The old topology walker is no longer embedded.
- Completed `zhao_terrain_group_seq`: per-view allocation, ModeVtx selected-view fill fan-out, accepted/landed accounting, safe seal, ModeRef selected-view replay, and release only after all reference handshakes complete.
- Completed `zhao_terrain_pipe`: real TESS ModeVtx/ModeRef -> sequencer -> shared projection subsystem, with client A geometry pass-through and lattice/cell-state ports kept upward.
- Added `release_unsafe_o` and a renamed committed mutant that releases on reference presentation. Production reads zero under legal traffic; the inverted-polarity mutant test observes a positive count.
- Added a both-view differential against retained `zhao_terrain_project`, with exact `w` checked against `project_vertex`, plus dense/bitmap sparse fill, LOD 0..3, morph, stitch, void, underside, order/rider/material, contention, geometry client A and output-stall coverage.

### Bugs found by the new evidence

- Full-width arena encodings were being truncated during landed-fill accounting, so invalid high encodings could alias legal arenas. Fixed with complete `ARENA_W` comparisons.
- Successful groups were released at ModeVtx completion and again after ModeRef. The first dense run caught it exactly: expected 12 releases, got 21. Fill-stage release is now only for rejected/empty groups; successful arenas stay held through replay.
- The original release detector could not see the successful-fill early-release class. It now watches both unreplayed successful fills and references whose selected-view handshakes are incomplete.
- The recovered binner source lacked `ZHAO_GEOM_DEV_BINNER`, compiling all binner harness types out. Fixed before the probe was accepted.

### Explicit receipts produced in this working tree

- Fresh configure: `CONFIGURE_RC=0` (`build-terrain-pipe-20260912`).
- Full composition lint `LINT_RC=0`; mutant lint `MUTANT_LINT_RC=0`.
- Intentional failing receipt before lifecycle repair: `DENSE_BUILD_RC=0`, `DENSE_TEST_RC=1`, release count expected 12 / observed 21.
- Corrected dense: `DENSE_REBUILD_RC=0`, `DENSE_RETEST_RC=0`; 478 terrain packets, 200 geometry vertices, 2,343 cycles, 36 checks.
- Bitmap: `ALT_BUILD_RC=0`, `BITMAP_TEST_RC=0`; 478 packets, 200 geometry vertices, 2,228 cycles, 32 checks.
- Release mutant: `MUTANT_TEST_RC=0`, 3 checks; positive detector count required to pass.
- Retained shell: dense `WCACHE_DENSE_RC=0` / 18,131 checks; RPP1 `WCACHE_RPP1_RC=0` / 9,871; bitmap `WCACHE_BITMAP_RC=0` / 18,134; 8,192 compared triangles each.
- Related rebuild `RELATED_REBUILD_RC=0`.
- CLIP->SETUP `DOWNSTREAM_RATE_RC=0`, 17 checks: 4,096 triangles / 4,102 clocks; 1,536 rejects do not change offered duration; 30% stalls preserve 4,096 outputs in order over 5,860 clocks.
- BINNER `DOWNSTREAM_BINNER_RC=0`, 133 checks: 128 one-tile triangles / 640 bin clocks = 5.00 clocks/triangle; drain 1,666; `TRI_CAP=128`.
- Selected lints only, no broad CTest: `RELEVANT_LINT_CTEST_RC=0`, 4/4.
- Static scanner only, no Quartus process: `QUARTUS17_STATIC_SCAN_RC=0`; 228 RTL files plus 3-fire/6-no-fire scanner self-test.
- The first manifest/top invocation ran outside the repo and failed before reading files (`FileNotFoundError`, both RC=1). The corrected repo-root invocation passed: `CHECK_PROD_MANIFEST_RC=0` (223 = 66 top + 78 inside + 79 excluded), `GEN_PROD_TOP_CHECK_RC=0` (fresh, 66 instances). No top regeneration was necessary.

### Ledger state and corrected claims

- `tests/CMakeLists.txt` contains the composed tests/lints and downstream probes.
- `design/fit_targets.yml` removes the walker from the subsystem closure and registers `zhao_terrain_pipe` as the actual composed fit boundary. Its named future questions are one core, three RAM-inferred replicas, total ALM/DSP/M10K, and 100-MHz arena-read/projector timing.
- `design/prod_manifest.yml` keeps subsystem, sequencer, pipe and arena shell explicitly not-yet-adopted; `zhao_terrain_topo` is retained superseded legacy/test support.
- The inherited “arena reference counter exists” claim is false; there is no such counter. Independent release instrumentation plus its committed mutant replaces that argument.
- `ARENAS=4` is scheduling/lifetime capacity, not replica count; three physical arena copies provide simultaneous corner reads.
- RPP1 is correctness evidence only and is illegal as a dense two-view throughput/resource headline. Do not repeat the campaign's conditional 99-DSP/gap-5 claim.
- No resource saving, budget ceiling, RAM inference or Fmax result is claimed. No Quartus map or fit ran.
- CLIP and SETUP accept one triangle per clock; no current block beyond SETUP does. The measured BINNER consumes 5 clocks per one-tile terrain triangle and has `TRI_CAP=128`.

### What remains exactly

1. NORMALS: choose and verify a face/world-normal seam for the ModeVtx/ModeRef architecture; the current pipe does not produce the legacy ModeTri stream consumed by `zhao_terrain_normals`.
2. DEPTHQUANT: carry each projected corner's `w` through downstream records/caches to `zhao_geom_depthquant`; outputs exist but are not connected to that consumer.
3. FIT: one clean `zhao_terrain_pipe` subsystem fit at legal RPP3, inspecting entity count, all three arena RAMs, total ALM/DSP/M10K, and 100-MHz timing. MATW18's structural 24-DSP one-core point is not a fit receipt; RPP1 must not be used as the headline.
4. ADOPTION: only after 1-3, atomically replace the duplicated production projection accounting, regenerate the production top, and rerun manifest/census checks. Nothing in this packet flips adoption.

No agent was spawned by the implementation lane, no Quartus executable was run, no existing file was deleted, and no commit or push was made. The working tree is ready for the reserved independent inspection/commit session.

## 2026-09-12 — independent review and closeout

- Inspected the subsystem refactor, sequencer, composed pipe, retained-shell adaptation, differential, committed mutant, downstream probes, CMake registrations, fit target, and production-manifest disposition.
- Verified the normalized mutant differs from production in one substantive line only: `StRef && t_done_c` becomes `StRef && t_ref_valid_i`.
- Found a second detector blind spot: the handed-off `release_unsafe_o` watched only an incomplete current-triple fan-out. An erroneous release after the last selected-view handshake of a non-final triple would have compared false. Repaired the independent obligation to remain live for the complete ModeRef job until `t_done_c`.
- Strengthened the positive control to use one view. Its first presented triple can complete its entire fan-out in the release cycle, so the mutant passes only if the detector still sees later ModeRef work.
- Strengthened the release/reopen witness to compare every output field on every stalled cycle and to require an arena open while the older copied output remains stalled.
- Final direct receipts after repair: dense 478 terrain packets + 200 geometry vertices / 2,343 clocks / 37 checks; bitmap+sparse 478 + 200 / 2,228 clocks / 33 checks; mutant 3 checks.
- Registered terrain composition and lint selection: 6/6 passed. Retained shell and downstream selection: 5/5 passed.
- Quartus-17 static scanner passed on 228 RTL files (self-test 3 fire / 6 no-fire). Production manifest passed at 223 modules (66 top + 78 inside + 79 excluded). Generated top is fresh at 66 instances. `git diff --check` passed.
- Wrote `reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md`. The report keeps the pipeline not adopted, makes no resource/Fmax/RAM claim, rejects the illegal 99-DSP headline, and names NORMALS, DEPTHQUANT, the legal RPP3 subsystem fit, and atomic production adoption as remaining work.
- No Quartus executable ran.

### Qwen calibration and workspace repair

- First local Qwen arithmetic test was useful on Euclidean traces and core arithmetic risks, but added speculative acceptance rows and contradicted itself on `q >= 0` versus `N >= 0`; it is not yet trusted for acceptance design.
- First ledger test failed the central classification: it called the active 36,000/88/464 programme target a measured fact and mixed the separate 440 finishing limit into that ledger. Qwen is not trusted for roadmap reconciliation.
- Started one bounded read-only terrain-witness review as the next calibration class; never more than one Qwen job ran concurrently in this review lane.
- Added workspace `.mcp.json` pointing at HomeAI so future sessions launched from `C:\programmieren\zencrifice` expose the worker MCP instead of rediscovering the WSL CLI path. This session cannot hot-load it.
- Confirmed no Windows `qwen` or `homeai-pi` command exists on PATH. The direct fallback remains `MSYS_NO_PATHCONV=1 wsl.exe -d HomeAI -- /usr/local/bin/homeai-pi ...`.
- Corrected an initial mistaken inference that the harness token budget was the model context. The authoritative launcher now declares the actual 272K `gpt-5.6-sol` context with both `CLAUDE_CODE_MAX_CONTEXT_TOKENS` and `CLAUDE_CODE_AUTO_COMPACT_WINDOW`; no 14M claim remains.
