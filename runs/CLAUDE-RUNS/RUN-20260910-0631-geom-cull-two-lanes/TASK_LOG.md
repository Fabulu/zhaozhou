# Task Log: RUN-20260910-0631 - zhao_geom_cull plane arithmetic onto MUL_LANES shared multipliers

**Created:** 2026-09-10 06:31 UTC+02:00
**Status:** Complete -- awaiting owner review (no commit)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0631-geom-cull-two-lanes/

---

## Objective

Sequence zhao_geom_cull's five multiplier sites (15 DSP) onto MUL_LANES shared 33x33 lanes (default 2 -> predicted 6 DSP), bit-exact against zref::cull, legacy spatial arm kept, no fit, no commit. Deliverable: reports/GEOM-CULL-TWO-LANES-20260910.md

---

## Progress Timeline

### 2026-09-10 06:31 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260910-0631
- Created working directory
- Initial context: roadmap lever "geom_cull -> 2 lanes, -9"; brief warns its own derivation may be wrong (it is: 40 products/eval, not 4)

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

## 2026-09-10 06:35 -- lane opened: zhao_geom_cull plane arithmetic onto shared lanes

Scope: fpga/rtl/geometry/zhao_geom_cull.sv + its tests. NO fit, NO commit
(per brief). Another agent owns zhao_project_core.sv + projection wrappers:
not touched. Shared build/ not touched; standalone verilator into
build-culllane/ (gitignored, build-*/).

Findings against the brief, BEFORE writing RTL (verified by reading):
1. "Four products per evaluation" (roadmap :109-120) is per PLANE-CYCLE, not
   per evaluation. S_EVAL runs 10 cycles (5 planes x 2 views), each with
   3 mul_pc + 1 mul_slack -> 40 products/evaluation. The roadmap's one-lane
   "exactly 100.0000%" is therefore 1000%; the coincidence it warns about
   does not exist because the arithmetic under it is 10x off.
2. Today's II is 11 clocks (1 accept + 10 S_EVAL), not 10; contract says 10.
3. workloads.yml zhao_geom_cull row (333,333/frame, requiredII 5, "ruled") is
   the LOD LADDER's rate (docket :2880 "THE LOD LADDER NOW TAKES FIVE
   CLOCKS") filed as cull demand. Even today's 4-multiplier block at II=11
   delivers 151,515/frame = 45% of it. measuredII: null is why nobody saw.
   Contract's own demand: ~6,100 meshlet decisions/frame (256 x 24).
4. GEOM.MESHFETCH.md: "latency may grow; initiation rate and exact arithmetic
   may not regress" sits next to "the obvious next candidate for the same
   lever" (cull). Internally inconsistent; reported, not silently resolved.
5. LEN_W header says sqrt(3*2^64) < 2^33.8; it is 2^32.79 < 2^33. Harmless
   (one spare bit), noted; LEN_W left at 34.
6. Consumer zhao_geom_meshfetch handshakes on cull_ready_i/cull_valid_i
   (S_CULL/S_WAIT), no cycle count assumed. prod_top u20_i: no parameter
   overrides, port list unchanged -> no gen_prod_top regeneration.
7. Existing directed test is NOT latency-pinned (dut_cull waits <= 64,
   wait_ready <= 4000): it passes unchanged. Exact walk laws ADDED.

Design decided: MUL_LANES in {1,2,4}, default 2. 4 = legacy spatial arm
(same circuit, II 11, 185/view). 1,2 = 33x33 signed lanes, product
registered (terrain_normals pattern), extraction squares on lane 0 (state
exclusivity makes it free), outside = sign(dot + slack) [exactly dot < -slack],
r*len split at bit 32 so no lane operand exceeds 33 bits (calibration has
s33 = 3 DSP measured; nothing between 33 and 40).

## 2026-09-10 06:50 -- RTL written, first evidence

- lint -Wall clean at MUL_LANES=2,1,4 (first pass).
- check_quartus17_syntax.py: clean, 220 files.
- check_prod_manifest.py: reports zhao_prod_top.sv STALE. NOT MINE: cull port
  list unchanged; the staleness is the other lane's zhao_project_core /
  project wrappers port changes (tree dirty in those files). NOTE: running
  `gen_prod_top.py --help` REGENERATED the file (no argparse); reverted to
  HEAD immediately with git checkout. Do not run it for --help.
- Standalone build recipe: verilator_bin --build; exes must be RUN with
  winlibs bin ahead of oss-cad-suite bin (else 0xC0000139
  ENTRYPOINT_NOT_FOUND from the suite's older libstdc++-6.dll).
- BASELINE (pristine RTL from HEAD, UNCHANGED test): 17,212 checks passed.
- NEW RTL MUL_LANES=2, UNCHANGED test (objects predate my test edit):
  17,212 checks passed, output byte-identical to baseline.

## 2026-09-10 07:05 -- walk laws, all arms, mutant

Test additions (ADDITIVE; the 17,212 original checks untouched): exact walk
law per instance (ZHAO_CULL_WALK) + ready-with-valid (II = walk+1) in
dut_cull; section 12 pins one-write extraction (ZHAO_CULL_EXTRACT), both
views. Laws DERIVED first (21/191, 41/191, 10/186), then measured:
- MUL_LANES=2 (default): 39,402 checks passed; walk 21 (II 22), extract 191.
- MUL_LANES=1:           39,402 passed; walk 41 (II 42), extract 191.
- MUL_LANES=4 (legacy):  39,402 passed; walk 10 (II 11), extract 186.
Mutant tests/mutants/zhao_geom_cull_mutant.sv (2 lines differ from source:
rename + `fin = first_q ? (acc + kterm_q) : acc`):
- UNCHANGED directed test vs mutant: 7,621 / 39,402 FAILED -- checker SEEN
  TO FAIL -- while walk 21 / extract 191 still hold on the mutant (the pin
  is blind to it).
- control (inverted polarity): 1,208 checks passed; 268/400 diverge;
  first-plane-decided verdict still right (boundary-fault signature).
scan_rtl.py on the new default: 2 nonconstant 33x33 multiplies, 0 constant.
It flagged my hi_term loop as a variable shift + a serial loop -> unrolled by
hand (2-bit hi_mul) and fin rewritten as a generate prefix chain; LEN_W==34
elaboration guard added beside MUL_LANES's. Re-lint + rebuild all five next;
mutant regenerated (it is a copy).
Contract GEOM.MESHFETCH.md: latency row, "PORTS" bullet, "next candidate"
paragraph, resource row updated to the shipped default; dated note added
naming the initiation-rate conflict and the workloads.yml misfiling.

## 2026-09-10 07:25 -- lane complete, evidence ledger

Refined RTL (hi_mul unrolled, psum prefix chain, LEN_W guard): lint -Wall
clean x3; scan_rtl: 2 nonconstant 33x33, 0 variable shifts, 0 comb loops;
check_quartus17_syntax clean (220 files).
Section 13 (negative radius, rails camera, INT32_MIN) added; all arms:
- MUL_LANES=2: 40,988 checks / 11,487 instances passed; walk 21, extract 191
- MUL_LANES=1: 40,988 passed; walk 41 / 191
- MUL_LANES=4: 40,988 passed; walk 10 / 186
(count predicted 40,988 before the run; matched.)
Regenerated mutant (2 lines differ): unchanged directed test FAILS
7,621/39,402 with walk law intact; control 1,208 passed, 268/400 diverge.
Random 4000 at default: 55,411 passed.
Contract GEOM.MESHFETCH.md updated (latency row, PORTS bullet, next-candidate
paragraph, resource row, dated initiation-rate note).
prod_top: re-confirmed the M is the other lane (u29/u57 only, 0 u20 lines).
Report: reports/GEOM-CULL-TWO-LANES-20260910.md.
Fit gate (named, NOT run): run_block_fit.ps1 -Module zhao_geom_cull --
"6 DSP at MUL_LANES=2 (two 3-DSP 33x33 lanes, shift-add in ALMs, p_q in DSP
output regs), ALMs vs 1,102, first-ever Fmax". NO COMMIT made.
build-culllane/ deletable after review.
