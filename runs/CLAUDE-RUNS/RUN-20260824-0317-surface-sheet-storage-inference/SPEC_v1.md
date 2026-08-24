# SPEC v1: make `zhao_surface_sheet`'s texel store infer as block memory

**Run ID:** RUN-20260824-0317
**Created:** 2026-08-24 03:17 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`zhao_surface_sheet` is the largest resource item in the repository. Map-only,
against HEAD's exact RTL (`reports/synthesis/zhao_block_map.json`, sourceCommit
`991f13c3`):

| metric | HEAD |
| --- | ---: |
| `blockMemoryBits` | **0** (it asked for 131,072) |
| `inferredMemories` | `[]` |
| `inferredMemoryCount` | **0** |
| `registers` | **131,258** |
| `estimatedAlms` | **95,947** -- 229 % of the 41,910 the device has |
| `dspBlocks` | 0 |

Success is a map-only row for this module with `inferredMemoryCount > 0` and
`blockMemoryBits` equal to the declared storage (131,072 at `Slots = 2`), with
`estimatedAlms` falling by roughly the penalty factor, and **behaviour
bit-identical** to `zref::surface::SheetStore`.

A map reporting `blockMemoryBits = 0` after this change is a FAILED
implementation even if every test passes (`reports/QUARTUS_GOTCHAS.md` section 10).

---

## Diagnosis -- measured, not guessed

`python tools/budget/scan_rtl.py --modules zhao_surface_sheet zhao_forge_cliff`
(elaborated AST), read against `reports/QUARTUS_GOTCHAS.md` section 10's three
independent killers:

| array | line | bits | async read? | reset-touched? | byte enables? |
| --- | ---: | ---: | --- | --- | --- |
| `mem` | 170 | 131,072 | **no** (`syncReadSites: 1`, `asyncReadSites: 0`) | **no** | **YES** |
| `dir_handle` | 173 | 64 | yes (2 sites) | yes | n/a |

**Exactly one killer applies to `mem`, and it is the third one: byte enables.**
The write is

```systemverilog
if (mem_be[1]) mem[wr_addr][15:8] <= wr_word[15:8];
if (mem_be[0]) mem[wr_addr][ 7:0] <= wr_word[ 7:0];
```

which is character-for-character the template section 10's byte-enable row
measured at **0 memory bits / 45,134 ALM for 65,536 bits**. The read at `:285`
is already synchronous and the array is already, deliberately, not reset -- the
header comment at `:166` says so and it was right. The two killers everyone
looks for first are *already absent*; the one that is present is the one the doc
calls "the one most likely to be written by accident".

`dir_handle` is async-read AND reset-touched, but it is 64 bits, `scan_rtl`
classes it `expectedStorage: registers` / GREEN, and the associative lookup
(C1/C4) needs a combinational multi-way compare. **It stays in flops. It is not
the problem** -- 64 of 131,258 registers.

### A tool gap found on the way

`tools/budget/scan_rtl.py` has **no byte-enable detector at all**
(`grep -i byte tools/budget/scan_rtl.py` finds nothing). It correctly reports
`mem` as sync-read and not reset-touched, i.e. it reports the array as
*healthy*, and it emits `expectedStorage: RAM`. On this block the scanner cannot
see the killer that actually applies. Recorded as an open item.

---

## The change

Split the 16-bit word into the two byte planes the oracle already uses.
`zref::surface::Sheet` is literally

```cpp
struct Sheet { uint8_t tag[kSheetTexels]; uint8_t strength[kSheetTexels]; };
```

so the RTL stops being a byte-enabled 16-bit array and becomes two whole-word
8-bit arrays, which is the shape the reference has had all along:

```systemverilog
logic [7:0] mem_tag[Words];
logic [7:0] mem_str[Words];
```

* `wr_we_tag_i` gates a full-word write of `mem_tag`; `wr_we_strength_i` gates a
  full-word write of `mem_str`; the clear sweep writes both. **No part-selects,
  so no byte enables, so section 10's third killer is gone.**
* The read stays synchronous and stays in the same `always_ff`, so
  **read-during-write still returns the pre-write word** (C5) with **no bypass
  network** -- nonblocking assignment gives it for free, exactly as before. The
  semantic is preserved, not re-derived, and it costs nothing.
* The array is still not reset. `dir_handle`/`dir_live` are untouched.

This is a change of storage SHAPE only. Every port, every status, every latency
and the counter are untouched.

**Nothing about killers 1 and 2 is being "fixed", because neither was present.**
In particular the valid-bitmap pattern the task offers for a reset killer is
**not needed here** and is not being introduced: C3's 4,096-cycle clear sweep
already makes the array's post-reset contents unobservable, and the contract
argues that trade explicitly (`design/contracts/SURFACE.SHEET.md` C3 rejects the
per-texel present bit at this scale, where `RASTER.TILESTORE.md` accepts it at
its own). Adding a bitmap here would contradict a ratified rejected-alternative.

---

## Scope

**In Scope:**

- `fpga/rtl/surface/zhao_surface_sheet.sv` -- storage shape only.
- `design/contracts/SURFACE.SHEET.md` -- the lines that describe the store as
  "one array ... byte enables on the two halves", which stop being true.
- A new `tools/sweep_surface_sheet.sh` + preflight (there is none today), with
  mutants aimed at the new shape.
- Stretch, only if the main fix lands: `zhao_forge_cliff`'s `edge_mem_r`.

**Out of Scope:**

- Any behaviour change. Any port change. `Slots` retuning.
- Particle-simulation, compositor and 2D block behaviour (owner-reserved --
  docket, do not invent).
- DSPs: this block has none and gets none.
- `dir_handle`, `dir_live`, the sweep FSM, the directory policy, the counter.
- The pre-existing ledger V16 failure (see Constraints).

---

## Constraints

- **One Quartus job at a time**, and **never edit RTL while one runs**.
- **Map first, fit second.** A map of this block takes ~1,096 s; a fit of it has
  timed out over an hour. Prove inference on the map.
- `git checkout <rev> -- <paths>` **STAGES**. Use `git restore --worktree`, and
  check `git status` after.
- Never hand-edit a measured number into a report; re-run the tool.
- Run the mutation sweep **in a git worktree** (owner ruling).
- **Two git installations are on this machine and they disagree.** Git Bash's
  2.45.2 has system `core.autocrlf=true`; the PowerShell 2.49.0 has no autocrlf
  and reports **291 files modified** on a clean tree. `run_block_map.ps1`'s own
  header records the same trap ("-c core.autocrlf=true on the git status").
  All git in this run goes through **Git Bash**.
- Build env: `tools/env/zhao-env.ps1` (or the bash equivalent) must be sourced --
  `cmake -S . -B build` fails with "no CXX compiler" without it.

### Baselines taken BEFORE any edit

- `npm run ledger:check` **already FAILS at HEAD**, with one error:
  `V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal "tests/formal/field_seq_bound.sby"
  is recorded as "pending"`. That is unrelated to this block and out of scope.
  The bar for this run is **no NEW ledger error**, and that one still being the
  only one at the end.

---

## Verification plan

1. **Oracle resolves first (rule V17).** Done before any RTL:
   `zref::surface::SheetStore` exists at
   `reference/include/zref/zref_surface.hpp:297`, and
   `tests/surface/surface_sheet_store_diff.cpp` already drives RTL and oracle
   over one operation stream and compares residency, occupancy and **every
   texel**. The differential is real, not an alias -- that file's own header
   records V17 refusing the directed suite as evidence.
2. RTL change, then the differential: `test_surface_sheet_store_diff`,
   `test_surface_sheet_directed`, `test_surface_sheet_random`, plus every other
   consumer of the file (SURFACE.STAMP chain, FIELD write-tag).
3. **Mutation sweep** in a worktree, with forced regeneration, model-directory
   hashing, exe-outside-target, pristine-first, and a preflight that lints every
   mutant and must report a **non-zero** count.
4. Map-only measurement, and it must show `inferredMemoryCount > 0`.
5. `ctest -L fast` green; ledger no worse than baseline.

### Mutants aimed at the new shape specifically

| id | what it breaks |
| --- | --- |
| S01 | a write lands in the **wrong array** (tag data into the strength plane) |
| S02 | the tag enable also writes strength -- the per-plane enable law lost |
| S03 | the strength enable also writes tag |
| S04 | the clear sweep clears only one plane (stale data for an invalidated address) |
| S05 | read-during-write returns the **post-write** value where the law says pre-write |
| S06 | the address pipeline off by one cycle |
| S07 | the read enable dropped, so a stalled response loses its word |

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- Do **not** run git through PowerShell in this repo -- see Constraints.
- Do **not** `cmake -S . -B build` without sourcing `tools/env/zhao-env`.
- Do **not** reach for the valid-bitmap / reset fix here: reset is not the
  killer on this block, and C3 already rejected the per-texel bitmap on the
  record.

---

## Open Questions -- ANSWERED

- **Does Quartus 17.0.2 Lite infer an M10K when the synchronous read and the
  write share one `always_ff` (the read-old template)?** **YES, and the read
  enable is free too.** New `ram_rdw` calibration family, 2x2 at 8192x8: all
  four variants infer 65,536 bits at 22-23 ALM and ZERO registers, as
  `ALTSYNCRAM [AUTO Simple Dual Port]`. **No bypass network is needed**, so C5's
  read-during-write semantic is preserved for free. The fallback plan
  (separate read process plus an explicit read-old bypass) was never required.
- **`scan_rtl.py` cannot see byte enables. Fix here, or docket?** **Fixed here**,
  and a SECOND defect was found while doing it: `resetTouched` walked the whole
  `IF` including the ELSE, so every array written in the operating logic of any
  `always_ff` with an async reset read as reset-touched. Both carry positive
  controls in both directions, `validate_scan_rtl_fixes.py`, 14/14.
- **Why do `zhao_forge_cliff`'s `prio_mem_r` and `run_mem_r` infer when
  `edge_mem_r` does not, given all three are async-read?** **Because
  `edge_mem_r[mhead_r][5:0] <= mtake_r` is a partial write and the other two are
  not.** Quartus's rescue of an async read survives the async read and does not
  survive a byte enable. Split at that field boundary, `edge_key_r` and
  `edge_span_r` infer **while still being read asynchronously** -- which
  confirms the mechanism rather than merely removing the symptom.
  33,109 -> 7,664 estimated ALMs, 82,944 -> 119,808 memory bits.

## New questions this run raises

- **`zhao_field_seq` is now the largest uninferred storage in the tree** (0 bits,
  7,958 ALMs). Its `rf` has two REAL killers -- async read and a genuine reset
  loop -- so it is the one block where the valid-bitmap pattern this task
  offered is actually the right answer. Not touched here.
- **`zhao_surface_sheet` does not lint at `Slots = 1`** (`AddrBits` is 13 against
  a 4,096-word array). Harmless, pre-existing, and a behaviour change to fix.
- **No fit was run.** Both blocks now have M10Ks on paths that previously held
  flops; no Fmax or slack has been measured for either, and none should be
  quoted.

## Original Open Questions (kept for the record)

- Does Quartus 17.0.2 Lite infer an M10K when the synchronous read and the write
  share one `always_ff` (the read-old template)? The repo's two exemplars split
  them (`zhao_dc_sdp_ram` uses two processes; `zhao_raster_tilestore` reads in
  one process and carries an explicit same-address bypass, i.e. it does **not**
  rely on inferred read-during-write). **To be answered by measurement, not
  recall** -- a map row is the answer, and if the shared process blocks
  inference, the fallback is a separate read process plus an explicit read-old
  bypass, whose cost must then be measured and reported rather than waved at.
- `scan_rtl.py` cannot see byte enables. Fix here, or docket?
- `zhao_forge_cliff`: all three arrays are async-read AND reset-touched, yet
  `prio_mem_r` and `run_mem_r` inferred and `edge_mem_r` did not. Read *why*
  before changing anything (stretch goal).
