# P0 — Recover, reconcile, freeze the actual starting state

Delivered against §13.1 of `reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt`,
which says these are the FIRST ACTIONS, before writing another module. Its four
required outputs are the four sections below.

**The running fit was not killed and was not waited on**, per §13.1's explicit
instruction on both counts.

---

## 1. Frozen scope / owner map

**Branch** `claude/ceiling-architecture-20260912`
**HEAD** `d4f837d8` — *geometry: build GEOM.LIGHT as a shell AROUND the shared light engine*, 2026-09-18 19:32:23 +0200
**Worktrees**

```
C:/programmieren/zencrifice/zhaozhou-ceiling-lane-20260912  d4f837d8  [claude/ceiling-architecture-20260912]
C:/programmieren/zencrifice/zhaozhou-dsf01                  50783fb1  [dsf01/divider-fusion]
```

### The running measurement and its immutable snapshot

| | |
|---|---|
| job | `zhao_prod_top@whole-console-sizing`, `run_block_fit.ps1 -Device 5CEBA9F31C7` |
| process | `quartus_map` pid 16944, **123 min, 6,235 CPU s**, alive and burning a core |
| closure | 147 files from `design/fit_targets.yml` |
| **snapshot digest** | **`de21d0f5a7d1`** over 147 files → `zhao_prod_top@whole-console-sizing.sources.sha256` |
| isolation | *"snapshot: 147 source(s) copied into the workspace; the live tree cannot reach this fit"* |

This is why development continued during it, and the snapshot is the thing that
makes the result comparable later. **Preserve it rather than the live tree.**

### Ownership of work done today (all in this worktree, all committed)

| capability | files | owner |
|---|---|---|
| PART.STATE | `zhao_part_state.sv`, `part_state_directed.cpp`, `tests/mutants/zhao_part_state_child_order_mutant.sv` | integration (me) + worker |
| PART.UPDATE | `zhao_part_update.sv`, `part_update_directed.cpp` | worker |
| PART.COLLIDE | `zhao_part_collide.sv`, `part_collide_directed.cpp` | worker |
| PART.SPAWN | `zhao_part_spawn.sv`, `part_spawn_directed.cpp` | integration (me) |
| geometry group sequencer | `zhao_geom_group_seq.sv` + directed + mutant | worker |
| GEOM.LIGHT | `zhao_geom_light.sv` + directed | worker, in flight |
| POST.COMPOSITE | — | worker, in flight |

§13.11 assigns **one integration owner** for the top, source lists, manifests,
generated ABI, CTest registration and acceptance pins. **That is this session.**

---

## 2. Unresolved semantic conflicts, stated rather than reconciled

1. **`zhao_prod_top` is NOT the console, and must not be relabelled.** §1.3:
   *"Keep zhao_prod_top as a historical/component resource instrument, labelled
   `RESOURCE_CENSUS_DISCONNECTED`. Do not rename its status to console merely
   because more modules are listed."* The measurement now running is therefore a
   **resource census**, not a console fit, and calling it "the whole console fit"
   — as this session repeatedly did — was wrong. Two new tops are required:
   `zhao_console_core` and `zhao_console_board`.
2. **Today's five new blocks are adopted nowhere.** Zero references in
   `design/prod_manifest.yml`, `design/fit_targets.yml`,
   `fpga/rtl/prod/zhao_prod_top.sv`. `check_prod_manifest.py` returns RC=1 with
   all of them UNACCOUNTED. They contribute **0 ALM** to any number. Per §13.3,
   *"A resource-top selection change alone does not close P2"* — so the fix is
   `zhao_console_core`, not more rows in the census top.
3. **The plan corrects two classifications in
   `MISSING-ORGAN-REGISTER-20260918.md`**, and it is right on both:
   frozen FIELD v2 is **not** FIELD.SEQ.CORE completion, and deferred GEOM.WARP
   is **not** a prerequisite for feeding ordinary geometry into the shared
   projector. The second was independently reached here this afternoon by a
   worker that refused the assignment — two routes, same answer.
4. **Owner revoked all deferrals** (2026-09-18, this session). The plan's §1.1
   says *"Do not add deferred features back in merely because a contract filename
   exists"* and requires POST.ECHO to get an explicit current decision. These are
   not in conflict — the owner gave the explicit decision — but the revocation
   must be recorded as a ruling, not inferred from a filename. It is, in
   `MISSING-ORGAN-REGISTER-20260918.md`.
5. **Three contracts are blank.** GEOM.WARP, INPUT.SNAC, POST.ECHO read
   "Deliberately unwritten" in every section. Building them requires authoring
   the spec first. Parked pending this plan's own dispositions.
6. **PART.COLLIDE DSP estimate 13–14 against a 12 ceiling.** An estimate, never
   measured. Unresolved.

---

## 3. Current test status, with causes

| binary | result |
|---|---|
| `part_state_directed` | **78/78** |
| `part_state_child_order_control` (inverted-polarity mutant) | **5/5** |
| `part_update_directed` | **264/264** |
| `part_collide_directed` | **180/180** |
| `part_spawn_directed` | **22/22** |
| `geom_group_seq_directed` | **41/41** |
| `geom_light_directed` | **3473/3473** |
| `tools/budget/refmodel_liveness.py` | rc=0 (11 phantom reference models) |
| `tools/budget/uncashed_cheques.py` | rc=0 |

**No full `ctest` suite was run, deliberately.** Two workers are writing the tree
and a suite reads the LIVE tree, so its greens would be worth no more than its
reds (CLAUDE.md). Every result above is a binary run directly.

### The delivered package, verified rather than believed

| check | result |
|---|---|
| `DELIVERY_SHA256.json` | **14/14 files verified, 0 mismatched** |
| `python -m unittest discover -s tests` | **Ran 25 tests — OK** |

The delivery's own claim of 25 passing tests is confirmed by running them here,
not by reading its evidence file. Its two RTL candidates
(`zhao_cpl_ram_fifo.sv`, `zhao_cpl_upload_guard.sv`) are declared unverified by
the author and remain so: **no Verilator, no Quartus, no timing claim, no ALM
claim.**

---

## 4. Reproduction commands

```powershell
# the running measurement (DO NOT re-launch; it is pid 16944)
Get-Process quartus* | Select ProcessName,Id,CPU
Get-Content <task-output>\bweistto7.output -Tail 5

# any single bench (env MUST be sourced or verilator children block at 0 CPU)
. .\tools\env\zhao-env.ps1
cmake --build build --target test_part_spawn_directed
.\build\tests\test_part_spawn_directed.exe

# ledger vs oracle, and uncashed cheques
python tools\budget\refmodel_liveness.py
python tools\budget\uncashed_cheques.py

# the delivered package
cd reports\true-console-completion-package\zhaozhou_true_console
$env:PYTHONPATH = (Resolve-Path .).Path
python -m unittest discover -s tests -p "test_*.py"
```

---

## What P0 deliberately did NOT do

No speculative ALM saving was subtracted. No change was made to the selected
visible tier. No new multi-day audit was started — §13.1 asks for *"a small
source/evidence checkpoint"* and this is one.
