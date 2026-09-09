# Task Log: RUN-20260909-2120 - [Describe objective here]

**Created:** 2026-09-09 21:20 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2120-vertex-arena-dense-seal/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 21:20 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-2120
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

## 2026-09-09 21:20 — arena consolidation begins
Task: fold zhao_proj_arena3's dense-fill valid mechanism into zhao_vertex_arena
as VALID_MODE (0=bitmap default, 1=dense-seal), extend the formal proof, widen
the wcache payload 75->106 (w field, per WCACHE-DROPS-W), regen prod_top, tests
+ committed mutant, report. NO commit, NO fit.
Found while reading (before any edit):
- No quartus process alive; wcache fit-target closure free to edit.
- REAL LATENT DEFECT confirmed: arena addresses fill/look by {arena, index[IW-1:0]}
  concat but open-clears valid_q[arena*DEPTH+i] LINEAR, and mem is sized
  ARENAS*DEPTH. At DEPTH=1089 arena 1 writes OOB from index 130. Never seen:
  directed test uses DEPTH=16, formal DEPTH=4, both pow2. proj_arena3's header
  flagged this; the brief did not mention it. Fix: linear addressing (matches
  the zref oracle exactly).
- "58 formal assertions" (brief, manifest, report) is not the real count:
  8 labelled asserts + 6 covers + 6 assumes.
- gen_prod_top.py run against clean tree reproduces committed prod_top byte-identical.

## 21:26-21:55 — build + evidence
- VALID_MODE param landed in zhao_vertex_arena (generate g_bitmap/g_dense; four
  mode wires; linear addressing repair; arena_seal_short_o sticky output).
- prove_dense FIRST RUN FAILED, step 7: same-cycle open+seal left an arena
  SEALED with cnt=0 (seal-wins is bitmap's historical race resolution and is
  unsound for dense). Fix: dense seal loses the race, refused+sticky. The
  PROOF found this before any directed test existed.
- All 8 sby tasks PASS: bmc(251s) cover(252s) prove(261s) [mode 0, unchanged
  config = bit-identity evidence], prove_np2/cover_np2 [DEPTH=6 non-pow2,
  addressing repair], bmc_dense(299s d14) cover_dense(14s, 7/7 covers reached
  with traces) prove_dense(10s inductive). Whole sweep ~5.5 min wall.
- Oracle grew dense mode; vertex_arena_dense_directed (4x81, non-pow2): 394
  checks PASS; both sticky bits SEEN TO FIRE on stimulus.
- Mutant zhao_vertex_arena_dense_mutant.sv (seal == -> <=): directed suite vs
  mutant FAILS 149/394 (checker seen to fail); inverted-polarity control
  PASSES with arena_misses_o == 1 (unreachable counter demonstrated alive).
- Existing suites on new RTL: geom_wcache_directed 73 PASS, random 7 PASS.
- wcache PAYLOAD_W 75->106 (w[104:74]); prod_top regenerated (2 lines);
  manifest check OK; quartus17 gate clean; no_control_bytes clean; VALID_MODE=2
  elaboration $fatal SEEN TO FIRE (--binary run, named message, rc!=0).
- Traps re-met: pipeline RC (tail's 0 masked exe's 127); mingw64-vs-winlibs
  libstdc++ (0xC0000139); basic_string(&&) without -std=gnu++17.

## 21:55 — FINDINGS (final)
1. DENSE_SEAL is now VALID_MODE=1 of zhao_vertex_arena; default (0) unchanged.
   Proof extended to 8 tasks, all PASS; prove_dense is inductive.
2. THE PROOF FOUND A DESIGN BUG FIRST: same-cycle open+seal must lose in dense
   mode (sealed-but-empty otherwise). Ten seconds of pdr beat 394 directed
   checks to it.
3. REAL LATENT DEFECT repaired in the sanctioned primitive: mixed
   concat/linear addressing was OOB for arena>=1 at non-pow2 DEPTH, incl. the
   shipped 2x1089 shape (arena 1 lost 88% of its rows). All prior instruments
   ran pow2 shapes. prove_np2/cover_np2 + the 4x81 directed suite hold it now.
4. wcache PAYLOAD_W 75->106 (w carried); "parameter moves and nothing else
   does" verified true of hand-maintained artefacts, false only of the
   generated prod_top (regenerated, 2 lines).
5. Savings are REGISTER counts, no ALM (no fit rows exist): terrain 4x81
   dense = 29 vs 324 bitmap regs; wcache keeps bitmap (sparse fill).
6. zhao_project_service adoption: primitive-level blocker CLEARED; remaining =
   terrain shell + replay wiring + THE ONE FIT (projection subsystem; question
   pre-stated in reports/VERTEX-ARENA-DENSE-SEAL-20260909.md par.8).
7. Brief errata: "58 formal assertions" is a keyword-grep artifact (real:
   8 asserts + 6 covers pre-change); arena3 made TWO findings, not one.
Deliverable report: reports/VERTEX-ARENA-DENSE-SEAL-20260909.md. Not committed.
