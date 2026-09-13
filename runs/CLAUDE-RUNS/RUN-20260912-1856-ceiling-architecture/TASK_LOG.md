# Task Log: RUN-20260912-1856 - Ceiling architecture continuation

**Created:** 2026-09-12 18:56 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260912-1856-ceiling-architecture/

---

## Objective

Finish unresolved rescue-roadmap architecture and continue non-terrain production optimization until clean composed evidence is comfortably below the owner's tightened **30,000 ALM / 85 DSP** targets, without duplicating the terrain-pipeline composition already owned by another session.

---

## Progress Timeline

### 2026-09-12 18:56 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260912-1856
- Created working directory
- Initial context: resumed after the committed projection subsystem and terrain TESS modes; the shared main checkout is dirty with terrain-pipeline composition owned by another session.
- Recovered HEAD `9b2b153d` on `zixxtrixx-v8-closeout`; origin matches.
- Created isolated clone `C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912`, branch `claude/ceiling-architecture-20260912`, with lane-local build/output ownership.
- Coordinated with `zencrifice-ac`: this lane will not touch the main checkout's terrain composition and will not launch Quartus while 17.0.0/17.0.2 installation is in progress.
- Ran committed `dsp_census.py`: still reports partial mixed evidence at 192 DSP / 58,359 fitted ALM / 147 M10K, with 34 DSP-unpriced and 45 ALM-unpriced production roots. This is a stale evidence baseline, not a current production estimate.
- Ran `uncashed_cheques.py`: self-test 4 fire / 4 no-fire passed; eight open rootless deferrals remain. Terrain/projection rows are owned by the other session; independent candidates include `zhao_field_progdir`, `zhao_forge_cliff_ram`, and `zhao_forge_prim_eval`.
- Found `domain_scoreboard.py` crashed after printing its allocation table because Python 3 cannot order the `None` software-exclusion bucket against string domain names. Fixed the display sort with an explicit key. Rerun RC=0 and reconciled exactly against the census: DSP 192, ALM 58,359, M10K 147.
- Reconciled the two-view DSP frontier against the committed dense-fill workload. `ROWS_PER_PASS=1` requires 663,552 fills x II 3 = 1,990,656 clocks, 119.4% of the raw frame before geometry, so the historical 99-DSP path is struck as non-shipping. The legal conditional path keeps RPP=3 and reaches 111 DSP: 140 corrected baseline -9 MATW=18 -9 cull -11 conservative bake. Gap remains 23 against the 88-DSP allocation.
- Wrote `reports/CEILING-FRONTIER-RECONCILIATION-20260912.md`, explicitly separating partial census evidence, implemented-but-unfitted structural deltas, legal workload points, and clean composed receipts.
- `zencrifice-ac` confirmed Quartus Lite 17.0.2 Build 602 installed at `C:\intelFPGA_lite\17.0`, version-checked, with no Quartus process active. The toolchain is available; this lane still will not fit before D3 names and passes its simulation/preflight boundary.

### 2026-09-12 20:09 UTC+02:00 - Packet A active; terrain review landed; DSP packing lead recovered

- The serial D3 Packet A implementer is active in this isolated lane. Its current untracked output is confined to the explicit shell policy/parser/generator/elaboration files; it has not committed, pushed, run Quartus, or touched this task log. Coordinator review and tests wait for its handoff rather than duplicating work.
- Fetched and read independently reviewed terrain composition commit `1f9a4f95` from `origin/zixxtrixx-v8-closeout`. It functionally composes `zhao_terrain_pipe` and registers the correct future subsystem fit boundary, but deliberately leaves production adoption, NORMALS, DEPTHQUANT, and every physical resource/timing claim open. It therefore validates the composition dependency behind the conditional 111-DSP frontier without converting any structural delta into a measurement or installed saving.
- Deferred cherry-picking `1f9a4f95` until Packet A completes because both packets edit `tests/CMakeLists.txt`; the terrain lane is holding its checkout and Quartus idle. This avoids a live-agent collision.
- Generated `field-rtl-scan.json` from a clean committed AST pass over all 12 FIELD roots. It found no scanner failure and confirmed that `zhao_field_v3_mulbank` already centralizes four `zhao_field_mul` lanes (the structural 12-DSP roadmap shape); this is research evidence, not a price or fit receipt.
- Recovered the stronger remaining-DSP lead hidden behind the MATW18 report: Quartus 17.0.2 maps each narrow independent multiply into its own physical variable-precision block even though Cyclone V supports two independent 18x18 products per block. Existing fitted hierarchy proves the waste (`zhao_geom_quat2mat`: nine 16x16 products -> nine DSP; projector: 22 blocks in `Two Independent 18x18` mode plus 11 sum blocks). An explicit dual-lane primitive/megafunction packing architecture could therefore be larger than the earlier hand-split estimate. No saving is claimed; the exact primitive semantics, signed recombination, simulation fallback, ALM/timing cost, and one named MapOnly discriminator still need architecture and proof.

### 2026-09-12 20:21 UTC+02:00 - Packet B preflight prepared without touching the active packet

- Re-read the approved D3 Packet B boundary against the current QSF, SDC, TimeQuest script, fit runner, fitter hierarchy format, and DSP census loader. The current QSF still selects `zhao_shell_top` and contains explicit virtual-pin assignments; the runner still has the obsolete PowerShell shell-port parser, schema-v1 receipt, and `sourceConeParity` provenance alias. No Packet B file was edited while Packet A is active.
- Confirmed the existing fitter report exposes fractional `ALMs needed` separately from final-placement, dense-packing, unavailable-ALM, register, RAM, DSP, pin, and virtual-pin fields. Packet B must preserve the fitter's own `zhao_shell_top:u_shell` row and a named unaccounted remainder rather than manufacturing wrapper cost by subtraction.
- Confirmed `tools/budget/dsp_census.py` still loads shell cleanliness from `sourceConeParity`; Packet B must switch it to actual schema-v2 `rtlCleanAtHead` only.
- Restored upstream tracking for `claude/ceiling-architecture-20260912`; origin already contained the committed architecture packets. No new commit was pushed.

### 2026-09-12 20:54 UTC+02:00 - Packet A handoff verified; independent review active

- Packet A agent completed without Quartus, protected-path edits, terrain edits, task-log edits, commit, or push. It delivered the exact policy/parser/generator, independent Verilator-AST census, generated wrapper/manifest, QSF and receipt fixture parsers, manifest-driven smoke monitor, generated smoke bench, C++ driver, and CMake/CTest registration.
- Agent evidence: 18/18 parser/generator/AST tests, 17/17 preflight/report tests, full source-pool `-Wall` lint, 154 shell ports / 3,386 bits, six wrapper HDL ports / ten physical bits, baseline traffic at the truthful 500,000-half-step bound, and independent GPU/video/audio freeze controls.
- Coordinator reran the complete focused CTest group serially from the lane-local configured build: 10/10 passed in 9.35 seconds. Protected-path diff is empty; `zhao_shell_top.sv` remains SHA-256 `00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783`.
- Started one read-only independent review of every Packet A file against the approved architecture. No integration or commit will occur until its findings are resolved.

### 2026-09-12 21:19 UTC+02:00 - Owner tightened closure targets

- Owner replaced the prior roadmap closure levels with **30,000 ALM / 85 DSP**, both requiring comfortable margin. The legal conditional 111-DSP frontier is therefore now 26 DSP above the target rather than 23; it remains structural and must not be banked as a composed receipt.
- Wrote `reports/RESOURCE-CLOSURE-TARGET-20260912.md` so the tightened global rule does not get lost behind historical 36,000 / 88 tables. It explicitly leaves per-domain redistribution unratified rather than inventing proportional cuts.
- Owner reiterated using agents where appropriate and asked that difficult rearchitectures try Astra when available. The active serial review was resumed after session interruption; no Astra endpoint appears in the currently available agent/model list, so the instruction is recorded for the next hard architecture commission rather than emulated with an invented model.
- Verified zero Quartus processes before resuming. No fit has started.

### 2026-09-12 21:22 UTC+02:00 - Packet A review rejected initial handoff

- The independent static review completed after session-resume and found twelve concrete defects: four high, six medium, two low. High findings were premature render completion, overlapping geometry requests that can corrupt return framing, whole-domain activity passing when only one bit toggles, and descendant-unsafe `u_shell` hierarchy matching.
- Medium findings covered warning evidence from the wrong report, substring-only clock proof, freeze controls that only stop the clock, presence-only receipt validation, silently ignored QSF Tcl, and FRAME_RING DONE overwritten by FREE in the same cycle. Low findings covered incomplete counter-window coverage and missing directed pad/HPS-progress cases.
- Rejected the green 10/10 initial smoke/preflight run as insufficient evidence rather than committing it. Resumed the same serial implementer with every finding, exact anchors, required positive controls, and instructions to prove any refutation. Packet A remains uncommitted and Packet B/Fable/terrain integration remain held.

### 2026-09-12 21:36 UTC+02:00 - Accidental review fan-out stopped and drained

- The resumed implementer invoked the generic `code-review --fix` skill despite the serial-agent constraint. It spawned a reviewer plus multiple read-only audit children before the coordinator intercepted it. This was a process violation; none was authorized as a parallel lane.
- Stopped the implementer, supervisor, and every live child, then repeatedly checked the agent roster until no child remained running. The accidental reviewers did not edit, build, run Quartus, commit, or push; their corroborating observations were treated only as review input.
- Resumed the original implementer alone with an explicit prohibition on Skill/Agent/workflow/reviewer delegation. Exactly one agent is now running. Additional surfaced checks include configurable packet provenance, policy-to-driver-handler set equality, cleanliness-first receipt validation, and per-detector-arm fire controls.
- Inspected the accidental skill-created `RUN-20260912-2124-review-ceiling-lane` (only an in-progress boilerplate spec/log describing the forbidden fan-out), then removed that untracked directory after transferring the incident and useful findings here. No unique implementation evidence was discarded.

### 2026-09-12 23:24 UTC+02:00 - Corrected Packet A handoff rejected by adversarial review

- The direct repair handoff completed with 41/41 focused CTests, 54 Python tests, 34 individual smoke/control runs, fresh generated artifacts, exact 154-port / 3,386-bit shell and six-port / ten-bit wrapper censuses, and the protected shell hash unchanged. No Quartus, commit, push, terrain, production QSF/SDC/runner, shell, or task-log action occurred in the agent.
- A new serial read-only adversarial review reran those gates and then used five targeted probes. It found nine remaining defects, so the green handoff is rejected again rather than committed.
- High findings: the entropy-width render lane is unreachable after the first drain; geometry accepts early `last` and can wait forever for a verdict; receipt cleanliness/source hashes still compare co-moving metadata rather than raw bytes/constraints; QSF parsing accepts trailing Tcl and ignores alternate source mechanisms such as QIP; and the fitter-summary parser rejects real Quartus 17.0.2 field names.
- Medium findings: virtual-clock validation accepts empty/unbound prose; hierarchy receipts drop the fitter's self value and required supporting columns/map evidence; elaboration comparison ignores packed range direction and signedness; and HPS first-beat latency is one cycle later than the established shell harness.
- Resuming only the original implementer for a bounded second repair. Packet A, terrain integration, Packet B, packing architecture, and Quartus remain held until the corrected diff survives another adversarial pass.

### 2026-09-13 00:55 UTC+02:00 - Second corrected Packet A handoff rejected

- The second repair reached 46/46 focused CTests and the coordinator independently reproduced 46/46, all protected paths clean, shell SHA-256 unchanged, wrapper SHA-256 `4d3e72b34128c1582d71cb80355a0dfee1b0edd2ed0c401a472ed2fc48d897d8`, and manifest SHA-256 `dbbf16cc3e215c9d7bf08e65feb4c7e803ea7499e16208fc174d35cd070edb45`.
- A second serial adversarial pass nevertheless found seven concrete defects, so Packet A remains rejected and uncommitted. High: the entropy render values are overwritten before `tri_valid` acceptance while a co-moving mode bit falsely credits coverage; Git cleanliness/source-pool parity can still be co-mutated; map parsing uses a synthetic schema and lacks per-entity attribution. Medium: delayed extra geometry beats escape the one-cycle detector; TimeQuest parsing rejects genuine `report_clocks`/STA formats; warning scanning accepts synthetic or line-wrapped/unrelated evidence; QSF constraint-file membership is collected but never validated.
- Resuming only the original implementer for a third bounded direct repair with the genuine archived Quartus artifacts as fixtures. No Quartus, terrain integration, Packet B, packing commission, commit, or push until this pass is independently reviewed.

### 2026-09-13 01:36 UTC+02:00 - Parallel non-overlapping work authorized

- Owner explicitly authorized using more subagents concurrently and asked that independent work proceed while implementation runs, superseding the earlier serial-only scheduling constraint for disjoint work.
- Packet A remains exclusively owned by its active implementer. In parallel, launched one architecture-only agent owning only `reports/DSP-DUAL18-PACKING-ARCHITECTURE-20260912.md`, and one read-only reconnaissance agent for the terrain cherry-pick conflict recipe plus evidence-qualified non-terrain ALM candidates.
- Neither parallel lane may edit Packet A, run Quartus/builds, integrate terrain, commit, or push. The architecture agent uses Opus because no genuine Astra endpoint is available; no substitute endpoint was invented.

### 2026-09-13 01:55 UTC+02:00 - Third Packet A repair handed off and independently rerun

- The third direct repair handed off with 47/47 focused CTests, 30/30 generator/parser Python tests, 51/51 preflight/report Python tests, genuine Quartus 17.0.2 fixtures, exact sourceCommit/CMake/QSF/SDC binding, delayed post-last and accepted-entropy fire controls, and all protected paths untouched.
- Coordinator independently reconfigured the lane-local Windows build, rebuilt `test_shell_fit_smoke`, and reran the focused suite serially: 47/47 passed in 29.90 seconds. No CTest debris preceded the run.
- `zhao_shell_top.sv` remains SHA-256 `00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783`; wrapper `68c0ead43603344681ede357dae19f3ffe288311dce5b3743253436f0118bd66`; manifest `89ef5075cdd849d5135e9587fa753ebee361f57cf8e1e7eac6687967566f8294`; receipt fixture `4bcfea4d80cb2ea6b91b794a146078e98dcbad682a321ffa2054f190089df11f`.
- Started a third read-only adversarial verification pass against the complete uncommitted Packet A diff. No commit/integration is allowed until that review returns clean.
- Read-only terrain integration reconnaissance found no committed-tree overlap with `1f9a4f95`; the only current blocker is the dirty `tests/CMakeLists.txt`, whose Packet A and terrain blocks are separated. Packet A must be committed first, then terrain cherry-picked with both registration sets retained.

### 2026-09-13 02:15 UTC+02:00 - Third Packet A handoff rejected after fresh adversarial probes

- The third adversarial pass reproduced 51/51 preflight tests and 30/30 tool tests, checked generated freshness, ran fresh temporary baseline/fault controls, and found three surviving defects. Packet A remains rejected despite coordinator and implementer green gates.
- High: receipt validation binds only nine special inputs plus wrapper/manifest, not every member of the 55-file compile source pool; a dirty `zhao_abi_pkg.sv` passed when paired with co-mutable empty archived Git status evidence. The live comparison helper was not called.
- High: directed and alleged entropy-width render jobs use identical triangle coefficients, vertices, bounds, top-left, and source ID. Existing masks largely measure reset-to-fixed transitions; mode/fill/clear differ, but the valid triangle payload is not width-covered.
- Medium: a stray geometry beat after the next request handshake but before its verdict is ignored because request acceptance clears old-frame ownership and the response-pending branch does not inspect beat-valid.
- Resumed only the original Packet A implementer for a fourth bounded direct repair with exact end-to-end positive controls for a dirty non-special source-pool member, accepted distinct entropy triangle payload, and response-pending delayed extra beat. Other agents remain confined to disjoint architecture/read-only work.

### 2026-09-13 02:18 UTC+02:00 - Dual-18 packing architecture committed and pushed

- Reviewed the architecture-only report. It establishes direct `cyclonev_mac` `m18x18_full` as a credible but unproved vendor boundary, derives exact signed/mixed-width decompositions, preserves the legal RPP=3 workload, and refuses to bank any saving before the named MapOnly discriminator plus vendor-model equivalence.
- The conditional projector target is 24 -> 11 DSP, but remains structural only. The report's honest priced-scope lattice can fall below 85 only after multiple MapOnly/subsystem-fit/adoption promotions and still excludes unpriced roots; it does not claim closure.
- Committed the report and its bounded commission as `e6f254c5` (`docs(fpga): architect dual-18 DSP packing`) and pushed the branch. No Quartus command or production rewrite occurred.
- Launched a disjoint calibration-only implementer for the wrapper, behavioral/oracle tests, renamed mutants, `gen_calib.py` revisions, and genuine-format map parser. It cannot touch Packet A, `tests/CMakeLists.txt`, production paths, or Quartus; the physical gate remains held until functional evidence and vendor-model availability are reviewed.

### 2026-09-13 02:20 UTC+02:00 - Tightened closure evidence committed and pushed

- Committed the 30,000-ALM / 85-DSP closure addendum, current live run history, and clean FIELD structural scan as `e3764572` (`docs(fpga): tighten resource closure target`) and pushed the branch.
- The FIELD scan remains structural research, not a resource receipt; the target addendum does not invent per-domain allocations or bank any of the 26-DSP gap.

### 2026-09-13 02:35 UTC+02:00 - TEXJOIN ownership supersedes an immediate RAM rewrite

- Independent current-evidence ALM reconnaissance found the clean 3,824-ALM `zhao_raster_texjoin_v2` row remains technically current, but likely charges an accounting-only root: its only RTL instantiation is the generated unconnected resource hierarchy, while the selected V3 texture island already owns and emits ordered textured fragments.
- Wrote `reports/TEXJOIN-OWNERSHIP-ALM-RECON-20260913.md`. The decision is ownership-first: prove exactly one connected RASTER-to-TEXTURE owner and retire any duplicate manifest root before considering an M10K rewrite. Any 3,824-ALM census change would initially be an accounting correction, not a physical composed saving.
- Pre-registered `g8a_raster_texture_single_owner_characterization` as the eventual composed subsystem fit question; source/elaboration/simulation must settle ownership before that fit is spent.
- The ALM scout briefly failed on a WebSocket reset, was resumed once, and completed read-only without file/build/Quartus actions.
- Committed and pushed the reconciliation report with this run-log snapshot as `7f886f90` (`docs(fpga): reconcile TEXJOIN ownership cost`).

### 2026-09-13 02:52 UTC+02:00 - Fourth Packet A repair handed off and independently rerun

- Fourth direct repair now binds every recomputed compile-pool member to its `sourceCommit` blob and live worktree bytes, drives and accepts two genuinely distinct full triangle payloads, and retains geometry ownership through the next request and pending verdict.
- New positive controls dirty a non-special compile source (`zhao_abi_pkg.sv`) in an end-to-end temporary Git repository and inject a stray beat during response-pending ownership. The new `guard-verdict-extra` control fires arm `0x200`; prior delayed-extra and render-stability arms remain registered.
- Coordinator independently reconfigured and rebuilt the lane-local target, then reproduced the complete focused suite serially: **48/48 passed in 32.62 seconds**. Protected shell hash remains `00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783`; wrapper `eee445bc7ece3d0981cd5e02297188fe4b79f938988b72608319d77d6d69efdf`; manifest `f7f9a31a40ef14a7ce5ad93730fafa5be9decf58d4f7b60a39c15bbdbf9e700c`; receipt `f6f804607b8da228f33803b8e5573766acf034c1ce6bd8a63e6f8002a75a8a1d`.
- Started a fourth read-only adversarial pass. Packet A remains uncommitted until the fresh probes return clean.
- The disjoint dual-18 calibration implementation also handed off functionally green, but remains unreviewed and its physical gate is HOLD: no Quartus MapOnly and no encrypted vendor-model run occurred.

### 2026-09-13 03:08 UTC+02:00 - Fourth Packet A handoff rejected; dual-18 calibration review rejected

- Packet A's fourth adversarial pass found two surviving defects. High: `git cat-file --filters` applies live mutable filters, so an uncommitted path-specific `.gitattributes` plus smudge command replaced raw `sourceCommit` bytes with the dirty live source and made a full 56-entry closure pass. Medium: an unsolicited geometry beat one cycle after a denied `violation` verdict remains unobserved.
- Resumed the original Packet A implementer for a fifth direct repair: raw object reads only, end-to-end filter-attack rejection, and a distinct post-denial beat control. Packet A remains rejected and uncommitted.
- Independent review of the dual-18 calibration packet also rejected its green behavioral handoff. High: the map checker accepted zero or one fixed-point multiplier despite claiming two live lanes; and stale same-revision map reports were not content-bound to the regenerated source/QSF. Medium/low controls missed full lane-B 8-bit pair coverage, asymmetric A/B signedness configurations, enabled reset priority in both wide shells, and hosts without installed Quartus metadata.
- Resumed the disjoint dual-18 implementer to repair all six findings. `dual18_physical_pack_discriminator` remains HOLD; no DSP saving is banked and no Quartus run is authorized yet.

### 2026-09-13 03:33 UTC+02:00 - Fifth Packet A rerun green; second dual-18 review rejected

- The fifth Packet A repair handed off with raw `ls-tree`/`cat-file blob` provenance, replacement refs disabled, no filtered Git reads, post-denial geometry quarantine, and two new protocol controls. Coordinator independently reran the exact focused group serially: **49/49 passed in 35.37 seconds**.
- Recomputed all named Packet A hashes; they match the handoff. Protected `zhao_shell_top.sv` remains byte-for-byte unchanged at SHA-256 `00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783`; `git diff --check` passes. A fifth read-only adversarial review remains active, so Packet A is still uncommitted.
- The corrected dual-18 handoff independently reproduced 1/1 generation test, 11/11 map-parser tests, and the full direct Verilator suite. All arithmetic/sign/reset/stall tests and backend/CE/lane-swap controls fired as expected; generated Quartus output directories remain empty.
- The second dual-18 adversarial review confirmed the six original repairs but found four remaining evidence defects: distinct mapped routes were source/synthetic rather than mapped evidence; a restored old run-preparation plus co-mutated config could bypass freshness because the parser ignored the top manifest; vendor metadata evidence could be forged outside the content witness; and extra Verilog/VHDL/QIP compile mechanisms were not rejected.
- Rejected the green dual-18 handoff and resumed its disjoint implementer for a bounded third repair. Physical MapOnly and encrypted vendor-model gates remain **HOLD**; no production multiplier migration or DSP saving is permitted.

### 2026-09-13 03:37 UTC+02:00 - Fifth Packet A handoff rejected

- The fifth adversarial review confirmed raw-object lookup, line-ending rejection, mutable-smudge-filter rejection, disabled replacement refs, exact `ls-tree`, and raw `cat-file blob`, but found two surviving high-severity false-pass paths.
- Receipt cleanliness still trusts captured empty status/diff files because `verify_captured_git_evidence()` is defined but never invoked. A fresh temporary repository with three modified tracked files still returned RC=0 and `receipt=raw-bound`.
- Geometry beats before the first accepted legal verdict remain invisible during initial idle, first request-wait, and first response-pending because neither frame ownership nor denial quarantine is active. Existing fault 13 proves only post-frame quarantine.
- Rejected Packet A despite the coordinator's 49/49 rerun and resumed the same implementer for a sixth bounded repair with direct dirty-tree reconciliation and a distinct pre-first-verdict beat control. Packet A remains uncommitted; terrain integration, Packet B, and the named fit remain held.
- Started a disjoint read-only TEXJOIN ownership proof while both repairs run. It may trace the live V3 raster-to-texture path and prepare a no-Quartus manifest-retirement packet, but may not edit, build, or claim a physical ALM saving.

### 2026-09-13 03:53 UTC+02:00 - TEXJOIN ownership settled; latest dual-18 repair rerun

- The read-only ownership audit proved `zhao_raster_texjoin_v2` has no functional RTL consumer: its only instance is private unconnected census scaffolding. Inside the selected V3 island, `zhao_texture_v3own:u_own` is the sole lifecycle allocator, accepted-issue recorder, return validator, ordered retire selector, and releaser.
- The audit also found the more important composition gap: neither TEXJOIN nor V3 is instantiated by the current shell. The shell's raster tile pipe consumes externally supplied flat texels and explicitly contains no sampler. V3 is therefore one-owner internally but is not yet live console hardware.
- Updated `reports/TEXJOIN-OWNERSHIP-ALM-RECON-20260913.md` with the proven ownership chain and corrected decision. TEXJOIN may move from selected `top` to `excluded: superseded`, retaining RTL/oracles/leaf evidence, but this is only an accounting correction and cannot be claimed as a physical 3,824-ALM saving. Evidence-complete retirement still needs role-aware duplication, full-identity stall, and uninterrupted-handshake controls after Packet A releases `tests/CMakeLists.txt`.
- The latest dual-18 repair removed the synthetic mapped-route PASS, so absent per-result mapped evidence now produces explicit HOLD. It also added manifest-bound rollback protection, canonical recomputed vendor metadata in the content witness, and exact-QSF rejection of every extra compile mechanism.
- Coordinator independently reproduced **1/1 generation**, **15/15 map-evidence**, and the full direct Verilator suite; all positive controls fired, manifest SHA-256 is `6514be4493c93f0c120b484088ba0ff79d3db01377f0782c75a2536107f843a6`, and all four Quartus output directories remain empty. A third adversarial review is active; physical route and encrypted vendor-model gates remain HOLD.
- Committed and pushed the corrected TEXJOIN ownership report plus run-log snapshot as `fd2e2dd9` (`docs(fpga): settle TEXJOIN ownership boundary`).

### 2026-09-13 04:32 UTC+02:00 - Sixth Packet A rerun green; dual-18 replay root still open

- The sixth Packet A repair now invokes direct repository reconciliation before resource interpretation and adds fault 15 for unsolicited beats during initial idle, request-wait, and first response-pending. The control independently observes all three pre-ownership states while preserving a same-cycle legal first beat.
- Coordinator independently reran the complete focused group serially: **50/50 passed in 39.33 seconds**. All named hashes match the handoff; protected shell, production QSF/SDC/runner, and architecture remain unchanged; `git diff --check` passes. A sixth adversarial review is active, so Packet A remains uncommitted.
- Third dual-18 review confirmed mapped-route HOLD, canonical metadata binding, and exact-QSF rejection, but found one remaining co-moving replay: restoring the full old same-content preparation/report/summary and recomputing config plus manifest hashes could still certify `oneBlockResourceGate=pass` because the manifest was not an independent root of trust.
- Rejected the latest dual-18 evidence handoff and resumed its implementer to add a caller-supplied, fresh, external invocation anchor. Physical route evidence and vendor differential simulation remain HOLD; no DSP saving is banked.

### 2026-09-13 04:34 UTC+02:00 - Sixth Packet A handoff rejected on Git index concealment

- The sixth adversarial review confirmed the direct dirty-tree reconciliation and all geometry repairs, including fault 15's exact `0x200` arm, three-state witness, and legal same-cycle verdict/first-beat behavior.
- One high provenance escape remains: both `git status` and `git diff` honor `assume-unchanged` and `skip-worktree`. Fresh temporary repositories hid a dirty unrelated tracked file with each flag; the full parser incorrectly returned RC=0 and `receipt=raw-bound`.
- Rejected Packet A again and resumed the same implementer for a seventh bounded repair that rejects tracked-index concealment flags and positively tests both attacks. Packet A remains uncommitted; terrain integration, Packet B, and the named fit remain held.

### 2026-09-13 04:44 UTC+02:00 - Seventh Packet A repair independently rerun

- Packet A now directly inspects NUL-delimited cached index records and rejects both assume-unchanged and skip-worktree before resource interpretation. New end-to-end temporary-repository controls cover clean ordinary records, visible unrelated dirt, and both hidden-dirt attacks; `gitIndexFlags` is included in bound evidence.
- Coordinator independently reran the complete focused group serially: **50/50 passed in 43.30 seconds**. Report binder SHA-256 is `f350fb963cb2d7f08dd6beaba2245540485f265c4ece9e392693487a0d3a0016`; receipt `c7d88da0de06cc3306eb9526712407b490aa14a7831d3b52d9f6a57af4513cde`; empty index-flags evidence `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. All other generated hashes and protected paths remain exact; `git diff --check` passes.
- Started a seventh read-only adversarial review focused on index-record parsing, stale/co-mutated captured flag evidence, and a broad final false-pass sweep. Packet A remains uncommitted pending that verdict.

### 2026-09-13 04:49 UTC+02:00 - Seventh Packet A handoff rejected on malformed index records

- The seventh adversarial review confirmed assume-unchanged/skip-worktree attacks now reject for unrelated and compile-pool files, leading-space and Unicode paths parse, captured evidence cannot co-move into acceptance, and all prior raw-object/filter/line-ending/direct-dirt and geometry controls remain clean.
- One medium fail-open parser case remains: unknown `git ls-files -v -z` tags and a non-NUL-terminated final record are filtered into empty clean evidence. No malformed/unknown-tag control existed.
- Rejected Packet A and resumed the same implementer for an eighth bounded repair requiring terminal-NUL framing and explicit clean-tag allowlisting with malformed/unknown records rejected. Packet A, terrain integration, Packet B, and the named fit remain held.

### 2026-09-13 04:55 UTC+02:00 - Eighth Packet A and anchored dual-18 reruns green

- Packet A's index parser now requires terminal NUL, exact one-byte tag/space/nonempty-path records, and accepts only uppercase `H` as ordinary clean cached state. Lowercase, skip-worktree, dirty, unmerged, removed, killed, empty, unknown, and malformed records reject; leading-space and UTF-8 paths remain legal.
- Coordinator independently reran the focused group serially: **50/50 passed in 52.70 seconds**. Preflight now reports 66 direct tests. Binder SHA-256 is `ca62e736b033557520acb67c000b96ee8d0b4b29d11a01b147e858d9fca83436`; all other named hashes and protected-path checks remain exact. A final bounded parser-only review is active.
- Dual-18 now emits a fresh 256-bit invocation nonce and final manifest hash to a caller-supplied anchor outside the replaceable evidence tree. The checker requires the canonical external anchor path, anchor hash, nonce, and manifest hash; missing, forged, old, wrong-location, and mismatched anchors reject. Full-chain rollback with co-mutated preparation/config/manifest/report/summary rejects against the retained current anchor.
- Coordinator independently reproduced **1/1 generation**, **18/18 map-evidence**, and the full direct Verilator suite. All controls fired; anchor SHA-256 `61c0203bc914dc38bb291d4be33da0f27e568e7aec09d492ade74a8a399c0aa1`, anchored manifest `ed4929c21f693c5f604baafd871ee1c61b2c66a755e1d85c43fc62eecbfd7ad5`; all four Quartus output directories remain empty. Final focused anchor review is active; physical route and vendor-model gates remain HOLD.
- The bounded eighth Packet A review returned **clean** after 11/11 focused hostile controls. Terminal framing, tag grammar, clean-tag allowlist, concealment and dirty-tag rejection, opaque path handling, CLI fail-closed propagation, and both real Git index attacks were all demonstrated. Packet A is accepted for logical commit; this is simulation/evidence infrastructure, not a fit or resource saving.
- Staging exposed trailing spaces in generated monitor condition continuations that untracked-file checks had not seen. Corrected the monitor generator to emit each comparison on one line, regenerated, and reran **50/50** in 52.36 seconds. Final monitor-generator SHA-256 is `8f52ee5494ec48bd118de7eb9b5768ce85c42fce82138a42364c4359c89382bb`; generated monitor `fa9af881b8022f329979b82f6bd39b082262200aeba5ff16283243d73dc67c1b`; staged `git diff --check` is clean.

### 2026-09-13 05:02 UTC+02:00 - Packet A committed; terrain composition integrated

- Committed and pushed accepted Packet A as `cc5c219f` (`feat(fpga): add truthful shell fit wrapper`). This lands characterization infrastructure only; no Quartus result or resource saving exists yet.
- Cherry-picked independently reviewed terrain composition `1f9a4f95`; Git merged the separated CMake registration blocks without conflict and created local integration commit `1c64c296`. Packet A's 50 registrations and all eight terrain registrations are retained.
- Regenerated the Windows build with `zhao-env.ps1` and the `windows-native` preset, built the five terrain executables, then ran Packet A plus terrain serially: **58/58 passed in 58.00 seconds**, including two terrain mutant/lint controls. Terrain remains functional/unfitted and not production-adopted; NORMALS, DEPTHQUANT, physical fit, and atomic adoption remain open.
- Committed and pushed the terrain-integration run-log evidence as `d84a2f34`; the cherry-picked terrain commit `1c64c296` was pushed with it.

### 2026-09-13 05:03 UTC+02:00 - Packet B implementation and final dual-18 path repair active

- The focused dual-18 anchor review confirmed full-chain rollback and all forged/stale/hash/nonce/timestamp controls, but found one low trust-boundary defect: resolving paths before comparison allowed symlink indirection to place the canonical anchor and manifest directory inside the replaceable evidence tree. Resumed the dual-18 implementer to reject symlink, junction, and Windows reparse-point components at the lexical canonical paths.
- Attempted to resume the completed Packet B preflight agent for implementation; its read-only agent definition correctly refused edits/tests. No files changed. Launched a fresh write-enabled implementation agent with the same disjoint Packet B ownership and no-Quartus constraint instead.

### 2026-09-13 05:18 UTC+02:00 - Final dual-18 path guards independently rerun

- Dual-18 now compares lexical canonical paths before resolution and rejects symlink, junction, and Windows reparse-point indirection on every relevant component. Anchor/manifest files must be regular non-reparse files; configs, QPF/QSF, preparations, sources, reports, and summaries are checked without following indirection. Windows controls cover file symlinks, intermediate directory symlinks, junctions, and `..` aliases with no skips.
- Coordinator independently reproduced **1/1 generation**, **21/21 map-evidence**, and the full direct Verilator suite; all positive controls fired. Checker SHA-256 is `bcd5986dd464d0cab5de3a93fff29060724b2b0a77510ca001a0dc1fe234c2b6`; anchor/manifest roots are unchanged; all four MapOnly output directories remain empty. A final bounded indirection-only review is active; mapped-result routing and vendor differential simulation remain HOLD.

### 2026-09-13 06:02 UTC+02:00 - Dual-18 evidence accepted; Packet B and texture architecture handed off

- Final bounded dual-18 review found no surviving trust-boundary false-pass. It independently confirmed rejection of `..` aliases; file, intermediate-directory, and revision/output-tree symlinks; Windows junction/reparse indirection; non-regular anchors; and indirection across the anchor, manifest, config, QPF/QSF, preparation, source, report, and summary chain.
- Accepted the calibration-only behavioral/evidence packet for commit after the coordinator's **1/1 generation**, **21/21 map-evidence**, and full direct Verilator rerun. This does **not** accept `dual18_physical_pack_discriminator`: genuine mapped `resulta`/`resultb` routing and encrypted vendor-model differential simulation remain **HOLD**; no production migration and no DSP saving are banked.
- Packet B implementation handed off without Quartus or shared CTest execution. It reports an exact 56-source generated-wrapper QSF, no virtual or physical pin assignments, archive/private-index execution, raw-Git reconciliation, preserved raw map/fit/STA/custom outputs, schema-3 emit-then-bind receipts, exact shell hierarchy ingestion, explicit remainder, and `rtlCleanAtHead`-only census use. Started a disjoint read-only hostile review before any named fit.
- Opus completed `reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md` only. It concludes the live shell cannot remain byte-identical if V3 becomes the real raster texture owner and proposes staged single-owner seams, controls, and composed fit gates. Started a disjoint read-only architecture review; no production RTL change or resource claim exists yet.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 2026-09-12 19:11 UTC+02:00 | Agent tool attempt | Fable architecture of D3 truthful shell fit-top split | Failed before work: backend expanded `fable` to unsupported `claude-fable-5-1` | none |
| 2026-09-12 19:14 UTC+02:00 | local Claude CLI, `claude-fable-5` | Same D3 architecture brief, restricted to Read/Grep/Glob/Write/Edit | Complete, exit 0; coordinator reviewed | `reports/SHELL-FIT-TOP-SPLIT-ARCHITECTURE-20260912.md` |
| completed 2026-09-12 20:52 UTC+02:00 | Agent `ab717719b6d124a1a` | Implement D3 Packet A exactly from the approved architecture; no Quartus/terrain/log/commit/push | Initial implementation complete; coordinator test rerun clean | pending corrected handoff |
| completed 2026-09-12 21:22 UTC+02:00 | Agent `a6edff802916ab3eb` | Read-only independent review of every Packet A file | Found 4 high, 6 medium, 2 low defects; no files changed | findings relayed to implementer |
| 2026-09-12 21:22-21:36 UTC+02:00 | Agent `ab717719b6d124a1a` resumed | Verify/repair all Packet A review findings | Stopped after it violated serial execution by invoking a fan-out review skill | none |
| 2026-09-12 21:25-21:36 UTC+02:00 | Accidental `code-review` supervisor and audit children | Unrequested fan-out from implementer | All stopped/drained; read-only findings only, no repository action | none |
| completed 2026-09-12 23:00 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Verify/repair first review findings without delegation | Corrected handoff green on 41 focused gates; rejected by adversarial review | nine findings relayed for second repair |
| completed 2026-09-12 23:24 UTC+02:00 | Agent `a8d3c005cf2f4c5c1` | Read-only adversarial review of corrected Packet A | Found 5 high and 4 medium remaining defects; no repository edits | findings relayed to implementer |
| completed 2026-09-13 00:32 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Repair nine adversarial findings without delegation | Second corrected handoff green on 46 focused gates; rejected by second adversarial pass | seven findings relayed for third repair |
| completed 2026-09-13 00:55 UTC+02:00 | Agent `a8d3c005cf2f4c5c1` resumed | Read-only adversarial verification of nine repairs | Found 3 high and 4 medium defects; no repository edits | findings relayed to implementer |
| completed 2026-09-13 01:48 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Repair seven second-pass findings without delegation | Third corrected handoff green on 47 focused gates; rejected by third adversarial pass | three findings relayed for fourth repair |
| completed 2026-09-13 02:18 UTC+02:00 | Agent `aa983a62b177c46e7` | Author the disjoint dual-18x18 packing architecture report only | Complete; coordinator reviewed, committed, and pushed as `e6f254c5` | `reports/DSP-DUAL18-PACKING-ARCHITECTURE-20260912.md` |
| completed 2026-09-13 | Agent `ab9ea5469aca02fcb` | Read-only terrain cherry-pick conflict map and current-evidence non-terrain ALM ranking | Complete; one dirty-file blocker, no expected committed merge conflict; evidence-qualified ALM ranking delivered | findings recorded in timeline |
| completed 2026-09-13 | Agent `a44feecd4e349cefa` | Read-only Packet B integration preflight against stable QSF/runner/census inputs | Complete; exact one-fit integration checklist delivered | pending Packet B implementation |
| completed 2026-09-13 02:14 UTC+02:00 | Agent `a8d3c005cf2f4c5c1` resumed | Third read-only adversarial verification of Packet A | Found two high and one medium defect with fresh probes; no repository edits | findings relayed for fourth repair |
| completed 2026-09-13 02:35 UTC+02:00 | Agent `a52a3426075a8e97a` | Read-only architecture scout for clean fitted 3,824-ALM `zhao_raster_texjoin_v2` | Complete after one transport reset; ownership-first recommendation, no file/build action | `reports/TEXJOIN-OWNERSHIP-ALM-RECON-20260913.md` |
| completed 2026-09-13 02:48 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Repair three third-pass findings without delegation | Fourth corrected handoff green on 48 focused gates; rejected by fourth adversarial pass | two findings relayed for fifth repair |
| completed 2026-09-13 | Agent `a8e917989c634bd4d` | Implement calibration-only dual-18 wrapper/tests/mutants/map parser without Quartus | Functional handoff green; rejected by independent review | six findings relayed for repair |
| completed 2026-09-13 03:08 UTC+02:00 | Agent `a8d3c005cf2f4c5c1` resumed | Fourth read-only adversarial verification of Packet A | Found one high and one medium defect with fresh probes; no repository edits | findings relayed for fifth repair |
| completed 2026-09-13 | Agent `abb36757fbbc0d4f0` | Read-only adversarial review of dual-18 calibration packet | Found two high, two medium, and two low gaps; no repository edits | findings relayed for repair |
| completed 2026-09-13 | Agent `ab717719b6d124a1a` resumed direct | Repair raw-Git-filter and post-denial geometry gaps | Fifth corrected handoff green on 49 focused gates; rejected by fifth adversarial pass | two findings relayed for sixth repair |
| completed 2026-09-13 | Agent `a8e917989c634bd4d` resumed direct | Repair six dual-18 calibration review findings | Corrected behavioral handoff green; rejected by second adversarial review | four findings relayed for third repair |
| completed 2026-09-13 03:37 UTC+02:00 | Agent `a81d75fdc237af727` | Fifth read-only adversarial verification of Packet A | Found two high false-pass paths; no shared build/CTest or repository edits | findings relayed for sixth repair |
| completed 2026-09-13 03:33 UTC+02:00 | Agent `abb36757fbbc0d4f0` resumed | Second read-only adversarial review of corrected dual-18 packet | Original six findings corrected; found four remaining evidence gaps | findings relayed for third repair |
| completed 2026-09-13 03:51 UTC+02:00 | Agent `a8e917989c634bd4d` resumed direct | Repair mapped-route, manifest freshness, vendor metadata, and complete QSF-source gates | Corrected handoff; coordinator reproduced 1/1, 15/15, and full Verilator; rejected on co-moving replay | one finding relayed for invocation-anchor repair |
| completed 2026-09-13 04:31 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Repair direct dirty-tree reconciliation and pre-first-verdict geometry detection | Sixth handoff; coordinator reproduced 50/50; rejected on index concealment | one finding relayed for seventh repair |
| completed 2026-09-13 03:49 UTC+02:00 | Read-only Explore agent | Prove TEXJOIN/V3 ownership and connected-shell path | TEXJOIN accounting-only; V3 has sole internal owner but no shell seam | report amended; implementation held on Packet A CMake ownership |
| completed 2026-09-13 04:30 UTC+02:00 | Agent `abb36757fbbc0d4f0` resumed | Third adversarial review of dual-18 evidence repairs | Prior four repairs held; found one co-moving full-chain replay | finding relayed for invocation-anchor repair |
| completed 2026-09-13 04:34 UTC+02:00 | Agent `a81d75fdc237af727` resumed | Sixth adversarial verification of Packet A | Geometry/direct-dirt repairs held; found assume-unchanged and skip-worktree escape | finding relayed for seventh repair |
| completed 2026-09-13 04:54 UTC+02:00 | Agent `a8e917989c634bd4d` resumed direct | Add independent dual-18 invocation anchor and replay controls | Handoff; coordinator reproduced 1/1, 18/18, and full Verilator | focused anchor review active |
| completed 2026-09-13 04:43 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Reject assume-unchanged and skip-worktree provenance concealment | Seventh handoff; coordinator reproduced 50/50; rejected on malformed records | one finding relayed for eighth repair |
| completed 2026-09-13 04:49 UTC+02:00 | Agent `a81d75fdc237af727` resumed | Seventh adversarial verification of Packet A | Prior provenance/geometry repairs held; found fail-open unknown/truncated index records | finding relayed for eighth repair |
| completed 2026-09-13 04:53 UTC+02:00 | Agent `ab717719b6d124a1a` resumed direct | Fail closed on malformed and unknown Git index records | Eighth handoff; coordinator reproduced 50/50 | accepted after bounded review |
| completed 2026-09-13 04:55 UTC+02:00 | Agent `a81d75fdc237af727` resumed | Bounded verification of eighth Packet A parser repair | Clean; 11/11 hostile controls passed | Packet A committed as `cc5c219f` |
| completed 2026-09-13 05:02 UTC+02:00 | Agent `abb36757fbbc0d4f0` resumed | Focused verification of dual-18 invocation anchor | All replay/forgery controls held; found symlink/reparse boundary bypass | finding relayed for final repair |
| completed 2026-09-13 05:17 UTC+02:00 | Agent `a8e917989c634bd4d` resumed direct | Reject dual-18 anchor/manifest path indirection | Handoff; coordinator reproduced 1/1, 21/21, and full Verilator | accepted after bounded review; physical/vendor gates HOLD |
| completed 2026-09-13 05:03 UTC+02:00 | Read-only Packet B preflight agent resumed | Implement Packet B | Correctly refused because its agent definition is read-only; no action | reassigned to write-enabled agent |
| completed 2026-09-13 06:00 UTC+02:00 | Write-enabled Packet B agent | Implement clean-archive fit flow, schema-3 receipt, hierarchy and census ingestion | Handoff complete; no Quartus/shared CTest, commit, or push | hostile review active |
| completed 2026-09-13 06:00 UTC+02:00 | Agent `abb36757fbbc0d4f0` resumed | Bounded verification of dual-18 path-indirection repair | Clean; no trust-boundary false-pass survived | calibration evidence accepted; route/vendor HOLD |
| completed 2026-09-13 05:57 UTC+02:00 | Opus architecture agent | Design the missing shell-to-V3 production composition seam | Report complete; no RTL/build/Quartus/commit/push | `reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md`; review active |
| active from 2026-09-13 06:02 UTC+02:00 | Read-only review agent | Hostile review of Packet B implementation | In progress; no shared test/build ownership | pending verdict |
| active from 2026-09-13 06:02 UTC+02:00 | Read-only review agent | Verify shell-to-V3 composition architecture against current RTL | In progress; report-only review | pending verdict |

---

## Files Created

- `domain-scoreboard.txt` — executable per-domain census snapshot; reconciles exactly to the selected bill.
- `dsp-census.json` — machine-readable selected census snapshot for this run.
- `fable-d3-prompt.md` — restricted architecture commission for the truthful shell fit top.
- `reports/CEILING-FRONTIER-RECONCILIATION-20260912.md` — correction striking the illegal 99-DSP two-view point and establishing 111 as conditional/structural.
- `reports/SHELL-FIT-TOP-SPLIT-ARCHITECTURE-20260912.md` — Fable D3 architecture, reviewed: wrapper-only split, generated exact port accounting, three domain-local sequential signatures, and one shell-boundary fit gate.
- `reports/RESOURCE-CLOSURE-TARGET-20260912.md` — owner-direction addendum replacing the closure totals with 30,000 ALM / 85 DSP while preserving evidence classes and historical receipts.
- `field-rtl-scan.json` — clean-HEAD elaborated AST research over the 12 FIELD roots; confirms the centralized four-lane multiplier-bank structure but is not resource evidence.
- `fable-dsp-packing-prompt.md` — restricted architecture commission for explicit Cyclone V dual-18x18 packing; completed as report commit `e6f254c5`.

---

## Decisions Made

- Keep this lane isolated from the main checkout's terrain composition and from its build tree.
- Treat 58,359 ALM / 192 DSP / 147 M10K only as a reconciled partial mixed-evidence census, never as a current production floor or ceiling.
- Strike 99 DSP as a legal two-view shipping point; it relies on an RPP=1 configuration that overruns the raw frame on terrain fill alone.
- Carry 111 DSP only as a conditional structural frontier. It now misses the owner's tightened 85-DSP target by 26 and still depends on composed projector adoption plus unfitted cull/bake deltas.
- Treat 30,000 ALM / 85 DSP as the closure targets and require comfortable margin under both on clean committed composed evidence; the earlier 36,000 / 88 roadmap levels are superseded for closure.
- Use D3 to repair shell ALM attribution, not to claim a resource saving.
- Accepted Fable's minimal D3 architecture after coordinator review: keep `zhao_shell_top` byte-for-byte as `u_shell`; do not create `zhao_shell_core`, move `tb_zhao_shell`, invent a board top, or repeat the obsolete four-fit proposal. Add a generated ten-bit fit wrapper with explicit policy, registered protocol-aware stimulus, native-domain capture/MISR/serialization, and fitted hierarchy attribution.
- The 3,214 virtual-pin / 1,608 ALM-containing-virtual-pins figures are genuine historical artifacts at commit `f8c2b32`, but are not current and are not linearly subtractable. Current source audit is 154 ports / 3,386 bits and must be reproduced independently in implementation.
- D3's one named fit gate is `shell_fit_top_clean_characterization`; all parser, generator, activity, mutation, source-parity, and report-fixture checks are pre-fit gates. The first post-D3 ALM candidate is the 16-column `zhao_raster_edgewalk.g_col` structure only if current hierarchy attribution confirms its rank.
- Quartus is now available, but continue holding it until D3 Packet A simulation/accounting gates are clean and the independently reviewed terrain packet is committed.

---

## Next Steps

- Receive and independently verify the fourth Packet A repair, then run another focused adversarial pass; commit/push Packet A only if no finding survives.
- Cherry-pick terrain composition commit `1f9a4f95`, preserve both separated CMake registration blocks, and rerun Packet A plus terrain gates before pushing integration.
- Review the calibration-only dual-18 implementation handoff; keep the physical MapOnly gate held until behavioral, positive-control, parser, and vendor-model prerequisites are truthful.
- Implement Packet B from committed clean Packet A: generated fit top in QSF, zero virtual pins, clean-cone provenance, schema-v2 receipt/hierarchy attribution, post-map witnesses, and truthful DSP-census ingestion.
- Then run only `shell_fit_top_clean_characterization`; use fresh fitted hierarchy to choose the first non-terrain ALM rewrite, considering the clean 3,824-ALM texjoin evidence alongside the conditional edgewalk candidate.
