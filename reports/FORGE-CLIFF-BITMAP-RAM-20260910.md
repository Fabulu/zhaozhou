# FORGE.CLIFF bitmap-RAM candidate — Roadmap Commit6, storage only

Date: 2026-09-10. Branch `zixxtrixx-v8-closeout`, working tree, NOT committed.
Author: implementation subagent (lane `fpga/rtl/forge/` + its tests).
Scope: ALM-Liberation Roadmap §6 / §14 Commit6, architecture report
`reports/FORGE-CLIFF-REARCH-ARCHITECTURE-20260909.md` step D1.
Working directory with logs and scripts:
`subagents/20260910-0707-forge-cliff-bitmap-ram/`.

**No Quartus was run. No fit, no map.** Every ALM/ALUT/register number about
the candidate below is either arithmetic from the source or explicitly
UNKNOWN. The one gate is named in §6.

---

## 0. What was built, in one paragraph

`fpga/rtl/forge/zhao_forge_cliff_ram.sv` is a second module BESIDE the golden
`zhao_forge_cliff.sv` (untouched — that is the legacy arm), same law, same
algorithm, same ports plus one 8-bit diagnostic counter `walk_fault_o`. The
1,156-flop SOLID window became a 34 x 34 RAM behind three 34-bit row
registers and the RAM's own 34-bit output register (the prefetch row); the
2,048-flop alive bitmap was DELETED — after the merge phase the span field is
the liveness information, so StKeep/StEmit walk the table by span and a
compaction pass runs only on the way into the 33 threshold passes, which need
a dense table. The 32-pass threshold selector is byte-for-byte the golden's.
The golden's own directed and random suites pass UNMODIFIED against the
candidate (verilated under the golden's prefix); a new three-way differential
(oracle / golden / candidate in one process) is bit-identical on 752 pages
with every coverage counter nonzero; three committed mutants show the checker
and the counter failing/firing on deliberate breaks.

---

## 1. Verified vs inherited — what I checked myself

| claim from the brief / architect | my check | result |
|---|---|---|
| map row 7,664 ALM / 8,149 ALUT / 3,875 reg / 2 DSP / 119,808 bits, map-only, `rtlCleanAtHead` | read `reports/synthesis/zhao_block_map.json` directly | **verified** (row `zhao_forge_cliff`, `stage: map-only`, `sourceCommit 0d25ed0`) |
| the estimate describes the CURRENT source | `git rev-parse 0d25ed0:fpga/rtl/forge/zhao_forge_cliff.sv` == `git hash-object` of the working file | **verified**, both `e220bd6d…` |
| all four payload tables already infer as RAM, bits sum to 119,808 | `inferredMemories`: 4 x Simple Dual Port, 2048x12 + 2048x6 + 2048x32 + 1024x17 = 24,576 + 12,288 + 65,536 + 17,408 | **verified** = 119,808 exactly; `ramConversionWarnings: 0`. **Not a saving, not claimed as one.** |
| the two bitmaps are 3,204 of 3,875 registers (83%) | summed every declared register width in the golden by script (`logs/../register_arithmetic.txt`): 41 registers, **3,646 bits**; `solid_r` 1,156 + `alive_r` 2,048 = 3,204; everything else 442 | **arithmetically consistent, NOT measured.** The map row has no per-register breakdown; 3,204/3,875 = 82.7% assumes Quartus kept every declared bit. Two `solid_r` bits (indices 0 and 1155) are never read (`win_base_c` ± offsets span 1..1154) and would be swept, so the netlist figure is at most 3,202. The 3,875 − 3,646 = **229** the architect attributes to "RAM rescue registers and duplication" is a plausible diagnosis, not a measurement: 4 read-address registers (43 bits) + 4 output registers (67 bits) explain 110 of it; **119 bits are unexplained**. |
| `models/cliff_radix.hpp`, `tests/test_cliff_cpp.cpp` do not exist | `find` over the tree excluding build dirs | **verified**, absent |
| `tests/forge/cliff_rearch_model.py` is committed | `git log` shows it in `eb06f882` | **verified** |
| compaction removes the alive RAM: 16 M10K not 17 for the full plan | M10K arithmetic in §3 | **consistent**; for THIS commit (no histogram yet) the candidate sheet is **15** |
| roadmap §6.2 sheet: "solid window 1156 x1 — 1 M10K" | the predicate consults five bits per cell at one side per clock; a 1156 x 1 memory has one read port and cannot serve five bits in four clocks | **the geometry as written is wrong; the block count is right.** A row-word layout (34 words x 34 bits) is REQUIRED, and that is what the roadmap's own §6.1 row buffers imply. Recorded as the geometry the sheet should carry. |
| `design/contracts/FORGE.CLIFF.md`: "Roughly 13 M10K plus ~3.2 k flops" | 2048x18 (4 M10K) + 2048x32 (7) + 1024x17 (2) = 13 for the PRE-split layout; after the 2026-09-0x key/span split it is 3 + 2 + 7 + 2 = **14** | the contract's number is **stale by one M10K** since the split. Not this lane's file; noted for the owner. |
| "storage only, algorithm untouched" | the threshold search (StBsCount/StBsStep/StGtCount/StKeep decision) and the merge law are copied verbatim; only the iteration domain (dense/sparse) changed | **held** — see §2 for the one place I departed from the architect's plan and why |

**Where the brief was wrong or loose, plainly:**

1. Commit6's phrase "phase-owned alive storage" describes the roadmap's §6.2
   alive RAM. The brief itself then says compaction deletes alive state
   entirely, and that is what was built: there is NO alive storage in any
   form. The count/phase authorisation the roadmap asks for is carried by
   `cnt_r` and `sparse_r`.
2. "Same ports" is not literally true: the candidate has one extra output,
   `walk_fault_o[7:0]`. It is the instrument the brief requires ("every
   counter seen to fire"), and the golden's port list has nowhere to put it.
   Adopting the candidate into `zhao_prod_top` will need `gen_prod_top.py`
   re-run; not done here (other lane's file, and adoption is gated).
3. The architect's §3 plan compacts after every merge phase ("only when a
   merge happened or selection is needed"). Built literally, the
   differential's cycle table showed **+512 clocks on every merge-only page**
   (the page with the 20+13 prefix merge went from 49,757 to 50,244) and
   **+1,069 on a merged page with vdist and no priority degrade** (the
   priority schedule was being paid for a pass nobody read). The golden
   skips a dead entry in one cycle; a span walk skips it in zero. So the
   shipped shape compacts ONLY on the way into the threshold passes. The
   architect's §4 cycle table was schedule-derived, not measured, and this
   is exactly the kind of thing it could not see.

---

## 2. The design, and its departures from the plan

Storage sheet (in the module header too, roadmap §3.1 form):

| memory | geometry | W (phase) | R (phase) | init / authorisation |
|---|---|---|---|---|
| `win_mem` | 34 x 34 | StLoad, whole word, one per 34 bits | StPrime (rows 0,1,2), StEnum (row cj+3, once per cell row) | none; read only after all 34 rows of THIS page were written |
| `edge_key_r` | 2048 x 12 | one site: StEnum(idx) / StCompact(wr) / StKeep(wr) | every phase, idx < cnt | none; idx < cnt_r always |
| `edge_span_r` | 2048 x 6 | one site: StEnum / StMwrite(head) / StCompact / StKeep | every phase | same |
| `prio_mem_r` | 2048 x 32 | one site: StCompact(wr) | StBsCount/StGtCount/StKeep, idx < cnt' | written for every dense entry before any read |
| `run_mem_r` | 1024 x 17 | one site: StRuns | StMsel, ridx < runs | none |

Rules honoured: no reset touches any array (all five live in clock-only
processes without reset); no partial writes; StCompact/StKeep write `wr`
while reading `rd` and the write is suppressed while `wr == rd`, so no
same-address read/write (a simulation assertion `a_compact_no_same_addr`
watches this and is live under `--assert`); no validity tag; no epoch needed.

**Departure A — table reads stay asynchronous (`assign x = mem[idx]`).**
The rule says "no asynchronous indexed wide read hidden behind a reassuring
ramstyle attribute". There is no ramstyle attribute anywhere. The golden's
reads have this exact shape and Quartus rescued them (measured: 4 inferred
SDPs). Converting every consumer phase to a one-cycle-latency read is a
third change axis (StRuns, StMsel, the walk's `rd += span[rd]` dependency)
and is not what Commit6 names. It is the named follow-up. **Risk:** the
rescue "is not a guarantee and cannot be planned against" (QUARTUS_GOTCHAS
§10) — the gate checks it.

**Departure B — one write site per table (S3 in the header).** The golden
writes its tables from several `case` arms; the candidate decodes
`*_we_c/_wa_c/_wd_c` in one `always_comb` and has exactly one write
statement per table. This is a shape change on tables the brief said not to
touch. Reason: the compaction adds a second and third write SOURCE to
`edge_key_r`/`edge_span_r`, and the calibration grid's inferring template is
one whole-word write under one enable; I would rather present that shape than
four case-arm writes. The gate checks whether it helped or hurt.

**Departure C — compaction only on the way into the threshold passes**
(§1 item 3). `sparse_r` marks "a merge happened"; StKeep and StEmit step by
`span` while sparse and by one when dense; StCompact runs only when
`over_budget && vdist_en`, fused with the priority build at the golden's
3-clock schedule. Correctness is identical (the same entries in the same
order); only which state skips the dead ones changed.

Everything else — prime sequence, row rotate, the five 34:1 selects, the
33-bit assembly shifter — is as the architect's §2 describes, with one
difference recorded in the header: the "shift the rows one bit per cell"
option is NOT implemented and NOT offered as a knob (a knob that does nothing
is an uncashed cheque). All shapes are named localparams: `WinDim`, `RowW`,
`WinAW`, `PrimeLen`, `WalkFW`, plus the law's `Budget`, `MaxEdges`, `EIW`,
`MaxRuns`, `RIW`.

---

## 3. M10K arithmetic — shown, not asserted

Cyclone V M10K port shapes: 256x40, 512x20, 1024x10, 2048x5, 4096x2, 8192x1
(depth x width, one port). Blocks per table = ceil(width / width-at-depth):

| memory | geometry | shape used | blocks |
|---|---|---|---|
| `win_mem` | 34 x 34 | 256x40 (34 rows of 34 bits fit one block; Quartus may choose MLAB — 34 rows exceeds an MLAB's 32, so two MLABs or one M10K) | **1** |
| `edge_key_r` | 2048 x 12 | 2048x5 → ceil(12/5) | **3** |
| `edge_span_r` | 2048 x 6 | 2048x5 → ceil(6/5) | **2** |
| `prio_mem_r` | 2048 x 32 | 2048x5 → ceil(32/5) | **7** |
| `run_mem_r` | 1024 x 17 | 1024x10 → ceil(17/10) | **2** |
| **total, this commit** | | | **15** |

The golden's four tables are the same 14; the candidate adds 1 for the
window and deletes nothing in RAM (the deletions are flip-flops). The
roadmap's §6.2 sheet said 17 (with a 2048x1 alive RAM and a 256x12
histogram); the architect's said 16 (no alive); this commit has no histogram
yet, so 15. **These are layout candidates from port-shape arithmetic. The
map row reports memory bits, not blocks; no block count has ever been
measured for this module.**

---

## 4. The register saving — split as instructed

| quantity | value | status |
|---|---|---|
| golden declared register bits | 3,646 (41 registers) | **MEASURED from the source** (script in the working dir) |
| candidate declared register bits | 630 (48 registers) | **MEASURED from the source** |
| declared delta | **−3,016** | arithmetic |
| of which bitmaps removed | −1,156 (`solid_r`) −2,048 (`alive_r`) | arithmetic |
| of which added | +136 rows (`row_n/c/s_r`, `win_q_r`), +33 `ld_asm_r`, +12 `ld_wi/wj_r`, +12 `wr_r`, +3 `prime_k_r`, +1 `sparse_r`, +8 `walk_fault_o`; −11 `ld_cnt_r`, −6 `mstep_r` | arithmetic |
| golden Quartus register count | 3,875 | measured (map-only row) |
| Quartus count minus declared | 229 | measured minus arithmetic — **unexplained beyond ~110** (§1) |
| candidate Quartus register count | ~630 + (rescue registers for 5 memories, order 100–250) ≈ **750–900** | **STRUCTURAL PREDICTION** |
| golden estimated ALM | 7,664 | an ESTIMATE (map-only), never fitted |
| candidate ALM | — | **UNKNOWN.** No map exists. Even after the gate it will be an estimate-to-estimate delta. The structures that left are the ones QUARTUS_GOTCHAS §10 says cost ALMs (a 1156-way decoder, five ~1156:1 selects, three 2048-way decoders, four 2048:1 selects, a 2,048-bit clear), which is a reason to expect a large drop and not a number. |
| DSP | 2 (the two 16x16 index products, unchanged) | structural — same expressions, same operands |

---

## 5. What the differential measured (Verilator, `--assert` on)

Executables in `build-forgelane/` (gitignored), logs under
`subagents/20260910-0707-forge-cliff-bitmap-ram/logs/`, build recipe
`build-forgelane.ps1` there (absolute includes, `-std=gnu++17`, no spaces).

**Unmodified golden suites against the candidate** (candidate verilated with
`--prefix Vzhao_forge_cliff --top-module zhao_forge_cliff_ram`; the .cpp files
are byte-identical to HEAD; the shared header `forge_cliff_dev.hpp` gained a
DUT template parameter and an optional load-stall mask, default behaviour
unchanged):

* `forge_cliff_directed`: all green; worst page 10,414 clocks (golden binary
  in `build/`: 11,946 — re-run today, `logs/golden_directed_baseline.log`).
* `forge_cliff_random`: all green; lane G 150 trials (87 undegraded, 62 both
  degrades), lane L 60 trials (16 merge-only, 7 drop-only, 37 both).

**Three-way differential** `tests/forge/forge_cliff_ram_differential.cpp`
(oracle `zref::forge::rim_plan`, golden, candidate in one process; every
lattice compared oracle↔golden, oracle↔candidate, golden↔candidate, plus
`emitted_bodies + dropped == enumerated` on both RTLs):

* 246 lattices / **752 pages**, 0 mismatches. Coverage (from the ORACLE's
  plan so a broken RTL cannot inflate it): merged pages 128, dropped 122,
  both 103, merge-only 25, **a merged span dropped 92** (dropped bodies >
  dropped entries), merged span emitted 123, the 20/20-need-31 **13-span 3**,
  partial pages 182, cw = 1 pages 2, ch = 1 pages 2, load-stalled 37,
  output-stalled 70, vdist on 66. Every coverage assertion is a check that
  fails at zero.
* New directed cases the golden never had: a one-cell-wide page (four-clock
  cell rows — the tightest prefetch window), one-cell-tall, two- and
  three-row pages (the prime rows ARE the page), a PAUSED load stream in
  three patterns, load and output stalls together on the merge fixture, ten
  full-width bites (236 bodies merged in one page, `wr` trailing `rd` by
  hundreds), a page whose bites are the farthest vertices so merged spans
  are dropped by priority (29 merged, 1,258 dropped).
* `walk_fault_o == 0` throughout; `triangles_submitted` equal between the
  two RTLs.
* **Cycle counts, measured:** total 5,540,759 (golden) vs 5,383,571
  (candidate), **−2.84%**. Worst page 85,740 → 84,208. Checkerboard with
  null vdist 11,946 → 10,414 (the 1,536 dead-entry emit iterations are
  gone). The candidate is slower on 99 of 246 lattices — by exactly **+4
  clocks per page** (the prime: three RAM reads landing a cycle later), the
  only regression. A merged page is never slower after Departure C.
* **Uninitialised RAM contents:** the pair rebuilt with `--x-initial unique`
  and run with `+verilator+rand+reset+2`, so every never-reset array
  (`win_mem`, the four tables) starts as noise — the generated constructor
  shows `VL_SCOPED_RAND_RESET_Q(34, …)` for `win_mem` and
  `VL_SCOPED_RAND_RESET_I(12, …)` for `edge_key_r`. **All green**, same 752
  pages (`logs/pair_differential_xinit_randreset.log`). That is the
  "reset authorisation state, not payload" rule demonstrated rather than
  argued. (A first grep for `VL_RAND_RESET` in the generated code returned 0
  and nearly got reported as "the flag did nothing"; the macro is
  `VL_SCOPED_RAND_RESET_*`. A zero from a wrong pattern, again.)

**The checker seen to fail, and the counter seen to fire** — three committed
mutants under `tests/mutants/`, each a renamed copy of the candidate with ONE
substantive line changed, regenerated from the current source by
`regen-mutants.sh` in the working dir (a mutant of an old version is a
control for a block that no longer exists):

| mutant | the line | control | result |
|---|---|---|---|
| `zhao_forge_cliff_ram_rowoff_mutant.sv` | prefetch row `cj+2` instead of `cj+3` | `forge_cliff_ram_rowoff_control.cpp` — passes iff a one-row page still MATCHES (the weak fixture the break passes), an 8-row page DIFFERS, and `walk_fault_o` stays 0 | **PASS**: 8x1 matches, 8x8 gives 120 edges vs 128, counter 0 |
| `zhao_forge_cliff_ram_mutant.sv` | merge writes span 0 | `forge_cliff_ram_mutant_control.cpp` — passes iff the counter FIRES, the machine does not hang, the plan differs | **PASS**: 2 faults on the 96x96 fixture, 1 on the end-of-table fixture |
| `zhao_forge_cliff_ram_over_mutant.sv` | merge writes take+1 | same control | **PASS**: 0 faults mid-table (see below), 1 on the end-of-table fixture |

Two lessons the mutants taught, both recorded in their headers:

* The first zero-span mutant broke the ENUMERATION span (1 → 0) and the
  counter read zero — with every span 0 no run forms, nothing merges, and no
  walk is entered. A positive control that never reaches the detector is not
  a control.
* A span one too long does NOT overshoot when unit entries follow the run: the
  walk skips one live entry and lands back on the table grid, ending exactly
  at `cnt`. The overshoot trigger needs the inflated span to straddle the
  table's end, so the control builds an interior page (solid halos) whose only
  run is a page-edge vertical bite with nothing after it (537 edges pre-merge,
  the 24-run merged whole, oracle-checked before use).

And one the differential taught before any mutant: the first cut rotated the
prime rows on cycles 2..4 and every page-row-0 north edge was missing — the
RAM's data lands the cycle AFTER the address. The checker fired on its first
run.

---

## 6. The one gate — F-CLIFF1, named in `design/fit_targets.yml`

One `quartus_map` of `zhao_forge_cliff_ram`, batched with the next
forge-subsystem map (fit discipline, 2026-09-08). Its question:

> With the 34x34 window as a 34-word RAM behind three row registers and the
> alive bitmap deleted: (a) does `inferredMemories` list FIVE memories —
> `win_mem` (M10K or MLAB; flip-flops is the failure) plus the four payload
> tables still SDP at 2048x12 / 2048x6 / 2048x32 / 1024x17 — with
> `ramConversionWarnings: 0`; and (b) what are estimated ALM, comb ALUT and
> registers against the golden's map-only 7,664 / 8,149 / 3,875?

**What it settles:** structure. Whether the 3,204 bitmap flops and their cones
left; whether Departure B's single-site writes kept (or broke) the tables'
inference; whether the async-read rescue survived the extra write sources;
an estimate-to-estimate ALM delta (same tool, stage, device) that is the
first number anyone may quote.

**What it cannot settle:** a fitted ALM or Fmax for either module. The
golden itself has never completed a fit with the split tables (its pre-split
fit timed out at 5,000+ s, GOTCHAS §10). Both rows will be estimates, and
the delta between two estimates is a third estimate.

**What needs no Quartus:** correctness, order, accounting, handshakes, the
compaction's same-address suppression, cycle counts — all answered above.

---

## 7. Not verified — the instrument per item

| item | why not | instrument that would |
|---|---|---|
| `win_mem` infers as memory | no map run | F-CLIFF1 `inferredMemories` |
| the four tables still infer with one-site explicit writes and 3–4 write sources | no map run | F-CLIFF1 |
| any ALM / ALUT / register / Fmax number for the candidate | no map run; the baseline is itself an estimate | F-CLIFF1 (estimate); a full fit for Fmax |
| M10K block count 15 | port-shape arithmetic; map rows report bits, not blocks | a fit's resource report |
| Quartus 17.0 synthesizability | `check_quartus17_syntax.py` passes and lint is `-Wall` clean, but "a block that has never been through `quartus_map` has not been shown to be synthesizable" (CLAUDE.md) | F-CLIFF1's first 40 s |
| behaviour with uninitialised RAM contents | **verified** in simulation (`--x-initial unique` + `+verilator+rand+reset+2`, all green, §5) — but only against Verilator's randomisation, not against silicon power-up | the F-CLIFF1 map cannot settle this either; only hardware or a formal authorisation argument can, and the argument is in the module header's memory sheet |
| the cmake registration builds in the SHARED tree | validated in a lane-local tree only (`build-forgelane/cmake`: configure 432 s OK, the seven targets build, `ctest -R forge_cliff_ram` **8/8 passed** incl. the `--nightly` differential); `build/` was left alone because other lanes are using it | `cmake --preset windows-native` + `ctest -R forge_cliff_ram` by the owner |
| the whole-lattice random suites at `--nightly` depth (600/240 trials) | ran the fast depth (150/60) | `test_forge_cliff_ram_random --nightly`, `test_forge_cliff_ram_differential --nightly` |
| adoption into `zhao_prod_top` / `gen_prod_top.py` regeneration | not this commit; the candidate is declared `not-yet-adopted` in the manifest | after F-CLIFF1 |
| the wall-vertex EMITTER | unpriced, unbuilt, not in the 7,664 and not in any number here (architecture §6) | separately scheduled D3 |

---

## 8. Files (working tree, uncommitted)

New, this lane:
* `fpga/rtl/forge/zhao_forge_cliff_ram.sv` — the candidate (lint `-Wall` clean; `check_quartus17_syntax.py` clean)
* `tests/forge/forge_cliff_ram_differential.cpp` — the three-way differential
* `tests/forge/forge_cliff_ram_mutant_control.cpp` — inverted-polarity control for both walk-fault mutants
* `tests/forge/forge_cliff_ram_rowoff_control.cpp` — the differential seen to fail on the row break
* `tests/mutants/zhao_forge_cliff_ram_mutant.sv`, `…_over_mutant.sv`, `…_rowoff_mutant.sv`
* `subagents/20260910-0707-forge-cliff-bitmap-ram/` — build script, mutant regeneration script, the two patch scripts, register arithmetic, logs, FINDINGS.md

Modified, this lane:
* `tests/forge/forge_cliff_dev.hpp` — driver templated on the DUT class; optional `ld_stall_mask` (default 0 = old behaviour). The two suite `.cpp` files are untouched.

**WARNING — three of my hunks are ALREADY IN HEAD, committed by another
lane.** Commit `c37f97b4` ("FIELD program directory: 16 parallel-searched rows
become one memory", 2026-09-10 07:44) staged `design/prod_manifest.yml`,
`design/fit_targets.yml` and `tests/CMakeLists.txt` whole, and my anchored
blocks in them (written minutes earlier) went with it. `git grep
zhao_forge_cliff_ram HEAD -- <those three>` returns 1 / 2 / 11 hits and the
working tree shows no diff against HEAD for them. Consequence: **HEAD's
`tests/CMakeLists.txt` registers seven targets whose sources
(`zhao_forge_cliff_ram.sv`, the three test .cpp files, the three mutants)
exist only in this working tree**, so a clean checkout of HEAD will fail at
`cmake` configure until the rest of this lane is committed. I did not commit
anything; the fix is to commit the files listed under "New" above promptly
(or revert those three hunks if the candidate is refused).

Modified, minimal and localised (files other lanes also have dirty — my hunks
are self-contained and anchored after the golden's entries; see the WARNING):
* `design/prod_manifest.yml` — ONE line: `zhao_forge_cliff_ram: not-yet-adopted …` in `excluded:` (the manifest check refused the module as UNACCOUNTED without it; `check_prod_manifest.py` is OK again). The wording is the deferral form `uncashed_cheques.py` watches for, deliberately.
* `design/fit_targets.yml` — ONE target `zhao_forge_cliff_ram` after `zhao_forge_cliff`, with gate F-CLIFF1's question in the comment.
* `tests/CMakeLists.txt` — ONE block after the golden's cliff tests: `lint_forge_cliff_ram`, `forge_cliff_ram_directed`, `forge_cliff_ram_random`, `forge_cliff_ram_differential` (+`_nightly`; two `verilate()` calls on one target), `forge_cliff_ram_mutant_control`, `forge_cliff_ram_over_mutant_control`, `forge_cliff_ram_rowoff_control`.

Not touched: `zhao_forge_cliff.sv`, `design/blocks.yml`,
`design/contracts/FORGE.CLIFF.md` (carries the stale "13 M10K / 3.2 k flops"
description of the golden — an owner's edit when the candidate is adopted),
`fpga/rtl/prod/zhao_prod_top.sv`, everything in the other three lanes.
