# Task Log: RUN-20260919-1656 - [Describe objective here]

**Created:** 2026-09-19 16:56 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-19 16:56 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260919-1656
- Created working directory
- Initial context: [brief description]

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

*Updated as progress is made*

## Coordinator log (appended)

- 16:56 Run initialised. Read HANDOVER-20260919. The uncommitted FORGE.SHADOW rewrite in zhao_console_core.sv was the OLDER, false text (it re-asserted LodState in meshfetch). Saved it as discarded-stale-forge-shadow-entry.patch and restored HEAD.
- Baseline at f98f5846 (isolated worktree): register 61 (33+25+3). inventory OK. **check_prod_manifest RED** (dsp2 lane / dual18_mul missing from the prod_top source list). **mutant_copy_drift RED** (material_resolve). q17 syntax RC0. prod_top and board fresh. Smoke PASS, 1536 px.
- Packets now work in PRIVATE WORKTREES and land via ff-push to origin (PACKET-PROTOCOL.md). This ends the shared-index hazards.
- Launched 3 packets: terrain, geom, texmat (general-purpose, background).
- Owner rulings R1-R9 in reports/OWNER-RULINGS-20260919-EVENING.md (b03b1d73), relayed to all three packets. Owner: "go with your recommended answers, don't stop to quiz me".
- NEXT when a slot frees: (a) particles/FIELD/surface: I1 I5 I7 I33 I42 I30 I32; (b) post/debug/cmd: I15-17 I45 I41 I20 + BUILD POST.ECHO; (c) BUILD INPUT.SNAC; (d) GEOM.WARP if geom hands it off.- geom landed dd716da2 (fixed a broken FRESH configure at HEAD: hdrread zhao_pkg, tb_zhao_shell trailing comma, G8B PINMISSING). GEOM.WARP split off: its contract is unwritten in every section, so it needs a spec first. Queued as the NEXT packet, ahead of the particles lane. A geom [IO.File] relative-path write hit the main checkout; the agent reverted it and I verified the checkout clean. Warning relayed to the other lanes.
- Refill queue: 1) GEOM.WARP spec+build, 2) particles/FIELD/surface, 3) post/debug/cmd + POST.ECHO, 4) INPUT.SNAC.
- Check-in: landed f8e0f906 (I21 analysis), c4449fb8 (GEOM.PROJECT -> shared client A, cited R3; 61->60; checked as legitimate sharing, not a removal), 3e4748a9 (material_resolve mutant refresh), b4336db3 (R4 N-client HPS arbiter). Phase-2 NOTE: prod_manifest still prices zhao_geom_project separately. The census must drop it before the fit, or the number double-counts.
- geom: 7228d596 (I37 CRC walker composed); register 59. Approved building zhao_geom_replay (I11/I12/I24/I38/I39/I13/DEPTHQUANT). Rulings R11 (M10K vertex-attribute store) and R12 (smoke pixel gate re-pinned from the reference, not the RTL; no trivial coverage) in e84eedc2. Relayed to the other lanes.
- Terrain pass 1 LANDED: register 57 (I6 closed, TERRAIN.PROJECT -> client B, PART.STATE tick-race bug fixed). FINDINGS transcribed; rulings R13-R16 in 2f7a9aa4. Slot refilled immediately: terrain2 (R14 writeback doorbell, R15 bake laws, I26, R13, R5, R8/I44, R6, R16, normalmap, velocity). Queue: GEOM.WARP next, then particles/FIELD/surface (I1 I5 I7 I33 I42 I30), post/debug/cmd (I15-17 I45 I41 I20) + POST.ECHO, INPUT.SNAC.
- Texmat pass 1 LANDED: register 56 (TEXTURE.TMU -> v3own under R9; N-client HPS arbiter; MEM.SHARE N=3). The baseline prod_manifest RED was an artefact of an incomplete env in my baseline worktree, not a tree defect. FINDINGS transcribed; rulings R17-R20 in 82fd601e. Slot refilled: cmdmem (R17 PublishResource + MEM.UPLOAD socket, R20 + material seam after replay, R18 tokens/governor, R19, I18/I19, I41, I45, I20). Terrain2 told to share the socket. Queue: GEOM.WARP, particles/FIELD/surface, post + POST.ECHO, INPUT.SNAC.
- Owner side request: cloned the MiSTer N64/Saturn/PSX cores to C:\temp\cores\{N64,Saturn,PS1} (shallow) and sampled them. Notes in C:\temp\cores\REFERENCE-NOTES.md: ROM+slope reciprocal (N64), UNR table+NR (PS1 GTE), one 9-client DDR3 arbiter, serial dividers, TMEM. GPL: technique only, no code.
- Check-in: fcfa8bbf (terrain2): R16 TERRAIN.VISIBLE superseded by T5's software walk (reference zref_sw_stream + directed test; there is NO HPS runtime yet. That caveat is recorded and belongs to the owner). The register's superseded_by excuse is guarded (cited ruling + existing files + built test; fired on 5 planted bad notes). Instrument defect: the gap lists were sliced to 20, hiding zhao_forge_cliff and zhao_post_gather. Now fixed. All three packets are progressing.
- OWNER: max TWO agents once the current three finish (token cost); do not interrupt current work. So the next slot that frees is NOT refilled; after that, refill to keep two running. Memory updated.
- OWNER: the coordinator does the merging; packets never rebase (one packet spent ~2h / 700k tokens re-gating after rebases). Protocol rewritten (5aaa3f53, merged 31332056) and all three running packets told to abort any rebase and push gz/<lane>. ce97827d (terrain2) landed I28 WRITEBACK behind the R14 doorbell.
- MERGES (coordinator): gz/geom 9bc3dd1e (GEOM.REPLAY; pixel gate 2560 reference-derived), gz/terrain2 786f52ba (loddev + LOD render), gz/cmdmem 312892ac (TERRAIN.BUILD socket + MEM.UPLOAD) as e4164f11. Duplicate new id I46 from two packets: cmdmem's renumbered to I47. Register instrument defect fixed: the closure used file stems, so zhao_hps_arbiter_n read NOT BUILT; now DECLARED modules, and the cross-check fired while only half was fixed. Register **51** (30+18+3), all gates green, smoke PASS.
- terrain2 FINISHED. Per the owner's max-2, NOT refilled. Running: geom, cmdmem. Findings transcribed; rulings R21-R24 (004aa435). OWNER to eyeball reports/terrain-lod-readings/lod_readings_contact.png (R22).
- TODO: add the -Mutant smoke to the gate list (terrain2 found a 129-port-stale mutant wrapper).
- geom FINISHED; gz/geom 5a1413d9 fast-forwarded and gated: register **50** (30+17+3), all gates green, smoke PASS. Findings transcribed; rulings R25-R31 (d18780cc). The protocol gate now reads 2560 px and adds the -Mutant smoke. Refilling ONE slot (max 2, cmdmem running): geom2 = R31 rate + VDECODE deadlock, I46 writer (+R21 terrain-normal client), R25 SetEnvironment/I48, R28 raster_state (I39/I24), R27 (I12).
- MERGE gz/cmdmem f4d08eca -> 8736ba33: R17 PublishResource end to end, I47 closed. Register **49** (29+17+3). The -Mutant smoke caught a stale wrapper port block (geom's I48 ports were missing from the combination); regenerated from production, and the mutant fires. R32: bounded write arm on RENDER.ASSET_POOL for MEM.UPLOAD, no-escape re-proved. Running: cmdmem (R20+R32 next), geom2.
- OWNER: back to THREE agents, plus a local Qwen REVIEWER (xhigh, 112k context, self-contained briefs <=30k tokens, calibrate by escalating). Third slot filled: POST lane (I15/I16/I17, post_gather, BUILD POST.ECHO). Qwen cal-1 (planted bug in desc_crc): found, 0 false positives. Qwen cal-2 (real HPS arbiter review) running.
- Qwen cal-2 (real arbiter): found a HIGH lost-pulse bug; I VERIFIED it against cmd_dma.sv:524 and assigned the fix to cmdmem. Merged gz/cmdmem 37bef328 (R20 + R32: material resolve end to end, guard write arm with a re-proved no-escape property and a failing mutant); register 49. R33: token budgets are counts, no invented capacity. OWNER pointed at the manafold-p16 Qwen relay; adopted as tools/qwen (511fd366). Q001 = review of R32's guard arm, running.
- Qwen Q001 (guard R32 arm): none found; verified by 3 spot checks. Q002 (geom2 deadlock fix, 24k-token prompt): partial. One real conditional hazard (a hole in StIdle is orphaned, so the job hangs unless the one-meshlet ordering holds structurally), forwarded to geom2; no false claims. The relay handles 24k prompts well, answering in about 4 min.
- POST lane FINISHED; merged 40ebd7d8 by hand. The guard kept both lanes' new arms, and the formal proof was re-run on the merged guard (PASS; the R32 mutant still fails it). The shell port policy was rebuilt as a union in shell port order; generated files were regenerated. Register **46** (28+16+2), all gates, smoke and mutant smoke green. Rulings R34-R38. Slot refilled: PFS lane (particles/FIELD/surface: I1 I5 I7 I30 I33 I34 I42, FIELD LANES saving).
- R39 (provisional, FLAGGED to the owner): R32 tie-offs changed the PROTECTED V1 shell's pinned hash, turning packet_c/e/g red. Re-pin every pin site together, with the TIE diff recorded beside each; cmdmem does it on top of 40ebd7d8. Qwen Q003 (POST.ECHO RTL, zref and contract, three-way) running.
- Qwen Q003 (three-way POST.ECHO, 16k-token prompt): 12 findings, 0 P1, all spot checks hold. The coverage holes are QUEUED for post pass 2: a span-overflow case, w==0/h==0, pass_start over a live pass, a pass after the fault latch. Qwen record: 5 jobs, 0 false claims so far.
- Qwen Q004 (post fbread + lease): F1 (view_o stale on the pass-start cycle, lease:204) confirmed, likely one-cycle; F2 (frame_admit aborts post without masking the reader/echo) confirmed in source; F3 (a phantom owed beat) and F4 (retire FIFO has no full guard) plausible. POST-2 QUEUE: these + Q003's coverage holes + R36 SetPost + R37 gather rule + R38 throughput + I17.
- PFS FINISHED: I1 closed (part_hps streamer, client 3 of the terrain HPS arbiter, N=4) and the FIELD distance service 8->2 roots (~2k ALM est.). The handover's 'forward LANES' advice was WRONG (it would drop points); fixed properly with GROUP_PTS. Merged 5999a676; register **45** on the merged tree, gates running. Rulings R40-R46. Slot refilled: POST2 (Qwen-found lease/fbread defects, echo coverage, R38 throughput, R36 SetPost, R37 gather). Qwen Q005 (vattr): a real timing-join hazard confirmed (uv read without a staged check), forwarded to geom2. Next queue: PFS2 (R40-R45: I5 I7 I33 I42 I34 I30), TERRAIN3 (BAKE R15, I26, R13, R21 normals, R23, R24, normalmap, velocity, I21, I27, I44), INPUT.SNAC, GEOM.WARP.
- GEOM2 FINISHED; merged 94add368, conflicts resolved (I46/I48 closed vs cmdmem's I49). Register **42** (24+16+2), all gates plus smokes green. R31: skin 41->10 clk/vtx, replay 56->7.5 clk/view-tri; the serial meshlet loop is ~119% -> R47 (two-bank ASSETFETCH overlap). Rulings R47-R50. Slot refilled: TERRAIN3.
- Qwen Q006 (recheck of the vattr fix): partial. F2/F3 are closed in the core composition (va_done gates the handle, va_poison ORs into the poison), which Qwen could not see and correctly left open. LOW residuals queued: lit on the batch_i clock, uv_waits undercount. Relay quirk: an empty 'root:' line swallows the next header line. Qwen tally: 7 jobs, 0 false claims.
- CMDMEM FINISHED; merged bea3e53e (CMD.EXEC union, six-record smoke packet, cmd_exec_directed rebuilt and PASS). Register **41** (24+15+2). Rulings R51-R54. Slot refilled: GEOM3 (R29 draw job -> I36/I41/I39/I24, R47 overlap, R26 LOD/GOVERNOR, R30). Qwen Q007 (arbiter-fix review) running. Queue: PFS2 (R40-R46, R54), HOST/DEBUG (R51/R52: I18 I19 I45 I20), INPUT.SNAC, GEOM.WARP.
