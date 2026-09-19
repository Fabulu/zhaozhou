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
