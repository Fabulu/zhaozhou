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
