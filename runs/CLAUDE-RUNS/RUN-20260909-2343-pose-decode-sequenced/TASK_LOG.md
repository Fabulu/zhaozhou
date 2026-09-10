# Task Log: RUN-20260909-2343 - [Describe objective here]

**Created:** 2026-09-09 23:43 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2343-pose-decode-sequenced/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 23:43 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-2343
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

## 2026-09-09 ~23:45 — lane opened (owner ruling R4: relax 1 bone/clock)

Scope: zhao_geom_quat2mat.sv, zhao_geom_mat3x4_mul.sv, zhao_geom_pose_decode.sv
and their tests. NO fit, NO commit. Quartus check: no quartus* process alive.

Findings against the brief, before writing a line of RTL:
1. The decode is NOT spatial today. mat3x4_mul is already sequenced (one
   element/cycle, 3 products) and SHARED across MUL1/MUL2 by pose_decode.
   The 18 DSP = quat2mat 9x(16x16 -> 1 DSP each) + mat3x4_mul 3x(32x32 -> 3
   DSP each), matching the calibration line exactly.
2. "Return 17" is unreachable at 32-bit operands: one 32x32 lane is 3 DSP
   (calibration cliff: 28..33 bits -> 3; only 18 pays). Floor on this route
   is 4 DSP (1 quat lane + 1 mat lane), return 14; 3 DSP (return 15) only by
   merging quat products into the 32x32 lane across the module boundary.
3. The contract demand figure "12 multiplies/bone" undercounts: real chain is
   9 (quat) + 36 (MUL1) + 36 (MUL2) = 81 products/bone (45 for bone 0).
4. Existing tests are NOT all latency-tolerant: quat2mat diff() assumes
   1-cycle latency; quat2mat section 8 checks m_valid one tick after accept;
   mat3x4 section 7 pins cycles == 12. "Tests pass unchanged" cannot hold
   with "latency may grow"; bit-exactness checks stay unchanged, timing pins
   move to the new declared walk.

Plan: MUL_LANES param on both submodules (default 1 = sequenced, legacy value
kept as generate arm), pose_decode passes params down, FSM untouched.
Mutant per new sequencer under tests/mutants/, inverted-polarity controls.
Fit gate (ONE, not run): fit_targets.yml zhao_geom_pose_decode leaf fit —
question: "4 DSP (1+3) at MUL_LANES defaults, Fmax holds with the operand-mux
cones?"

## 2026-09-10 ~00:5x — lane complete, evidence ledger

RTL: MUL_LANES on both submodules (default 1 = sequenced; legacy arms kept
verbatim under generate); pose_decode passes MUL_LANES_QUAT/MUL_LANES_MAT,
FSM/ports untouched. prod_top instantiates without overrides -> defaults
propagate, no regen needed.

All verification standalone in gitignored build-poselane/ (shared build/ was
contended by the island lane -- coordinator warning heeded; no ctest, no edits
to fit_targets.yml / prod_manifest.yml / blocks.yml).

- lint -Wall: clean, both parameterizations.
- check_quartus17_syntax.py: clean (220 files). check_prod_manifest.py: OK.
- quat2mat directed/random @1: 352 + 6,500 checks; @9: 352. Exact 10-cycle
  walk law holds; legacy walk 0.
- mat3x4 directed/random @1: 176 + 3,600; @3: 176. Exact 37-cycle walk law;
  legacy 12.
- pose_decode directed/random @1/1: 694 + 8,352; @9/3: 694. MEASURED: 3,694
  cycles / 32-bone palette (115.4/bone) at defaults; 1,799 (56.2/bone) legacy.
- Mutant controls (inverted polarity), both PASS = differential FIRES:
  schedule-swap 4/12 elements diverge (0/12 on identity); boundary-accumulator
  7/12 diverge, element 0 exact, products_done_o still 1.
- palettes_decoded_o now asserted (+1 per palette) in every fixture.

Corrections to the brief (report section A): pre-R4 was never 1 bone/clock
(56.2 measured); demand is 81 products/bone not 12 (worst frame 28.4% not
2.9%); return is 14 DSP (18->4) not 17 (calibration read directly:
s16->1, s32->3 dspBlocks); three directed-test timing pins had to move (bit-
exactness checks byte-identical, walks stay exact laws).

Deliverable: reports/POSE-DECODE-SEQUENCED-20260909.md. Fit gate (named, NOT
run): fit_targets.yml:1534 zhao_geom_pose_decode leaf -- "4 DSP at defaults,
M10K store intact, geometry_mantle Fmax with the 12:1 mux cone?"

NO COMMIT made (per brief). build-poselane/ deletable after review.
