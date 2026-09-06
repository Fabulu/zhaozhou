# Task Log: RUN-20260906-2032 - [Describe objective here]

**Created:** 2026-09-06 20:32 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-2032-terrain-seq/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 20:32 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-2032
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

## TERRAIN.SEQ — step 5 of the world-layer build sequence

Delivered contract-first. Four artefacts plus a ledger entry reported, not written.

| artefact | state |
|---|---|
| `design/contracts/TERRAIN.SEQ.md` | written |
| `reference/include/zref/zref_terrain_seq.hpp` | written — `zref::terrain::seq::Sequencer` |
| `fpga/rtl/terrain/zhao_terrain_seq.sv` | written, lint clean `-Wall` |
| `tests/terrain/seq_rtl_directed.cpp` + `tb_terrain_seq.sv` | written, 74 checks green |
| `tests/CMakeLists.txt` | `seq_rtl_directed` + `lint_terrain_seq` registered |

### Composed vs written
- COMPOSED: `zref::swstream::PatchRecord`, `PatchFlags`, `kPageBudgetPerFrame`,
  `kPriority*` — T5's record used verbatim as the input type. Nothing about the
  32-byte record is redeclared.
- WRITTEN: only the sequencing law (`Sequencer::step`), which is a pure function
  of (record, directory answer, frame state). The directory's answer is an
  ARGUMENT, not modelled — same boundary TERRAIN.COMPCACHE draws.

### Defect found by the differential on its first run
The compose-slot index was held under a low `is_cslot_valid_o`. Caught as
`A2 allocator`, 16 divergences, `cs=0/1` vs oracle `cs=0/0`. Fixed by clearing
`cs_slot_q` with its valid bit. It is now mutation M14 and produces 19 failures.

### Two harness defects, both reading LOW
1. The fire harness trusted `BUILD_RC == 0`; four mutants reported their
   predecessor's behaviour, which looks exactly like "the check did not fire".
2. Deleting the exe to force a relink was not enough — ninja relinked the same
   objects. It now demands re-verilation and fingerprints each failure set.
3. Deleting the whole `Vtb_terrain_seq.dir` DID break the build the way
   CLAUDE.md documents: `copy_if_different` of `Vtb_terrain_seq.cmake` failed
   and ninja could not regenerate its own graph. Repaired with
   `cmake --preset windows-native`.

### Fire evidence (CORRECTED run -- the first two runs were stale-binary)
14 mutations, each applied / built / run / reverted. All 14 fire. Failure counts:
M1 20, M2 24, M3 17, M4 2, M5 19, M6 17, M7 8, M8 1, M9 2, M10 17, M11 17,
M12 15, M13 19, M14 15.
Four of these differ from the stale run (M3 24->17, M5 2->19, M12/M14), which is
the evidence that fixing the harness mattered. The harness now spares the
.cmake files, forces a re-verilate, and fingerprints each failure set so an
identical consecutive pair cannot pass unnoticed.

check_ingress_capture.py was also fire-tested on this file: a live
`rec_flags_i[FLAG_REQUIRED]` read produces
"LATE INGRESS READ ... zhao_terrain_seq.sv:301 rec_flags_i".

### Final state
75 checks, 0 failures. ctest: seq_rtl_directed PASS, lint_terrain_seq PASS.
Measured cost: 5.38 clocks per resident static record (43 clocks / 8 records).
tools/rtl/check_ingress_capture.py gained an entry for this block; whole-tree
run clean.

### Randomised distribution (measured, then floors set at ~2/3)
320 frames, 10,487 records, 24,718 actions.
issued 4,820 / prefetch-resident 1,639 / skipped 3,236 / faulted 71 / drained 721.
refused 527, same 314, dirty-writeback 661, budget-deferred 647, cslot 3,193,
static-issue 1,627. 64 of 64 distinct record shapes.
