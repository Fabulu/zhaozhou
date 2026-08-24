# Task Log: RUN-20260824-0317 - surface-sheet storage inference

**Created:** 2026-08-24 03:17 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260824-0317-surface-sheet-storage-inference/

---

## Objective

Make `zhao_surface_sheet`'s 131,072-bit texel store infer as block memory
instead of 131,258 flip-flops. It is the largest resource item in the
repository at an estimated **95,947 ALMs -- 229 % of the whole device** -- and
it has no multipliers, which is why two days of DSP work never looked at it.
Behaviour must stay bit-identical to `zref::surface::SheetStore`.

---

## Progress Timeline

### 2026-08-24 03:17 UTC+02:00 - Task Started

Run initialised with `runs/CLAUDE-RUNS/init-run.ps1 surface-sheet-storage-inference`.

### 03:20 - A trap found before any work: TWO git installations that disagree

`git status --short` returns **0 lines** in Git Bash and **291 modified files**
in PowerShell, on the same tree at the same moment.

| shell | git | `core.autocrlf` |
| --- | --- | --- |
| Bash tool (Git for Windows) | 2.45.2 | `true` (system gitconfig) |
| PowerShell tool | 2.49.0 | unset -> false |

Every diff is equal-insertions-to-deletions (`CMakeLists.txt | 76 +++---`,
38/38) -- line-ending phantoms, not content. `run_block_map.ps1`'s own header
already records this ("`-c core.autocrlf=true` on the git status, or every CRLF
file reads dirty"), and RUN-20260823-1736 disclosed `git add -A` staging 288 of
them. **Ruling for this run: all git goes through Git Bash.** Recorded in the
SPEC's Constraints and Don't Retry.

### 03:25 - Baselines taken BEFORE touching anything

* `npm run ledger:check` -> **already FAILS at HEAD**, one error:
  `V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal "tests/formal/field_seq_bound.sby"
  is recorded as "pending"`. Pre-existing, another lane's gate, out of scope.
  RUN-20260823-1736 recorded the identical single failure, so this is a stable
  baseline and not something this run introduced. **The bar here is: no NEW
  ledger error.**
* `cmake -S . -B build` fails with "no CXX compiler" unless
  `tools/env/zhao-env` is sourced. Sourced for every build in this run.

### 03:30 - DIAGNOSIS. Which of the three killers applies -- measured, not guessed

`python tools/budget/scan_rtl.py --modules zhao_surface_sheet zhao_forge_cliff`
(elaborated AST via Verilator), read against `reports/QUARTUS_GOTCHAS.md` §10:

| array | line | bits | async read | reset-touched | byte enables |
| --- | ---: | ---: | --- | --- | --- |
| `mem` | 170 | 131,072 | **NO** (`syncReadSites: 1`, `asyncReadSites: 0`) | **NO** | **YES** |
| `dir_handle` | 173 | 64 | yes (2 sites) | yes | n/a |

**Exactly ONE killer applies, and it is the third: byte enables.** The write at
`:287-288` is

```systemverilog
if (mem_be[1]) mem[wr_addr][15:8] <= wr_word[15:8];
if (mem_be[0]) mem[wr_addr][ 7:0] <= wr_word[ 7:0];
```

character-for-character §10's byte-enable template, measured there at **0 memory
bits / 45,134 ALM for 65,536 bits**.

The two killers everyone reaches for first are **already absent, deliberately**:
the read at `:285` is synchronous, and the header comment at `:166` says the
array is not reset *because* "a reset loop over the array is exactly what stops
M10K inference". Whoever wrote this block knew two thirds of the law. The third
is the one §10 calls "the one most likely to be written by accident".

`dir_handle` is async-read AND reset-touched -- both other killers -- and is
**correctly left alone**: 64 bits, `scan_rtl` classes it `expectedStorage:
registers`/GREEN, and C1/C4's associative lookup needs a combinational multi-way
compare. 64 registers of 131,258.

**No valid-bitmap is needed here.** The task offered that pattern for a reset
killer; reset is not the killer on this block, and the contract's C3 already
rejects a per-texel present bit at this scale on the record. Introducing one
would contradict a ratified rejected-alternative to fix a problem that is not
present.

### 03:32 - A TOOL GAP, found because the diagnosis had to be checked twice

`tools/budget/scan_rtl.py` has **no byte-enable detector at all** --
`grep -i byte tools/budget/scan_rtl.py` finds nothing in 1,577 lines. On this
block it reports `mem` as sync-read, not reset-touched, `expectedStorage: RAM`,
severity YELLOW: i.e. it reports the array as **healthy**. The scanner named in
§10 as the source of "expected bits" cannot see the killer that is actually
present in the repository's single worst block. Only the source read caught it.
Recorded; see Decisions for what was done about it.

### 03:40 - THE OPEN QUESTION, and why it was not answered from memory

The fix is to split the 16-bit word into the two byte planes the oracle already
uses (`zref::surface::Sheet` is literally `uint8_t tag[4096]; uint8_t
strength[4096];`), so each array is written whole-word and no byte enable
exists. But that leaves a real risk, and it is exactly the shape this
repository keeps getting hurt by -- **the calibration grid does not cover the
template the block actually uses.** Every `sync` point in `gen_calib.py`'s RAM
grid is

```systemverilog
always_ff @(posedge clk) if (we_i) mem[waddr_i] <= wdata_i;
always_ff @(posedge clk) rdata_o <= mem[raddr_i];     // SEPARATE process, NO read enable
```

whereas `zhao_surface_sheet` shares **one** process (that is what makes
read-during-write return the pre-write word -- contract C5 states the semantic
rather than leaving it to the synthesiser) and gates the read with an enable so
a stalled response does not lose its word. The repo's two working exemplars both
avoid the question: `zhao_dc_sdp_ram` uses two processes, and
`zhao_raster_tilestore` carries an **explicit same-address bypass**, i.e. it
deliberately does not rely on inferred read-during-write.

So "it will infer" was **not known**, and §10's whole subject is publishing what
the tool would do instead of measuring it. Extended the grid rather than guess:
new `ram_rdw` family in `tools/budget/gen_calib.py`, 2x2 over
{shared, split} x {read enable, none}, at **8192x8 -- one byte plane of
SURFACE.SHEET at `Slots = 2`**, i.e. the array this run proposes to build.

### 03:47 - MEASUREMENT: the shape is safe

`Get-Process | ? Name -match 'quartus|verilator|btormc|yosys'` -> nothing
running. Then `tools\quartus\run_calib.ps1 -Family ram_rdw`
(log: `calib_ram_rdw.log`):

| microbench | memory bits | est. ALM | seconds |
| --- | ---: | ---: | ---: |
| `calib_ram_8192x8_shared_re`   (the proposed shape) | **65,536** | **23** | 37.2 |
| `calib_ram_8192x8_shared_nore` | **65,536** | 22 | 16.7 |
| `calib_ram_8192x8_split_re`    | **65,536** | 23 | 18.3 |
| `calib_ram_8192x8_split_nore`  | **65,536** | 22 | 17.1 |

**All four infer, in full, at ~23 ALM.** Neither the shared read/write process
nor the read enable costs anything or blocks inference. The read-old semantic
that C5 makes load-bearing is therefore preserved **for free, with no bypass
network** -- nonblocking assignment in one process already gives it, exactly as
it did before. Two planes -> 131,072 bits at roughly 46 ALM of storage cost,
against 95,947 estimated ALMs today.

The open question is closed by measurement, and the four rows are now permanent
calibration data rather than a one-off probe.

### 04:05 - THE MEASUREMENT. It infers.

`tools/quartus/run_block_map.ps1 -Module zhao_surface_sheet -TimeoutSeconds 3600`,
map-only, `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2. Provenance checked before
reading anything: `sourceCommit 2eeef64` (this run's RTL commit),
`rtlCleanAtHead: true`, `sourceFileCount 94`.

| metric | HEAD (991f13c3) | after (2eeef64) | |
| --- | ---: | ---: | --- |
| `blockMemoryBits` | 0 | **131,072** | exactly the declared storage |
| `inferredMemoryCount` | 0 | **2** | one per plane |
| `registers` | 131,258 | **170** | -772x |
| `estimatedAlms` | 95,947 | **279** | **-344x** |
| `combAluts` | 60,354 | **187** | |
| `dspBlocks` | 0 | 0 | unchanged, as intended |
| map seconds | 1,095.8 | **32.5** | -34x |

```
altsyncram:mem_tag_rtl_0|...|ALTSYNCRAM  AUTO  Simple Dual Port  8192 x 8
altsyncram:mem_str_rtl_0|...|ALTSYNCRAM  AUTO  Simple Dual Port  8192 x 8
```

`ramConversionWarnings: 0`. **229 % of the device becomes 0.67 % of it.** The
block is no longer in the top eight resource items; it was number one by a
factor of nearly three. `zhao_forge_cliff` (33,109) is now the largest, which is
the stretch goal, and `zhao_field_seq` (7,958, still 0 bits inferred) the second.

The 344x is close to §10's own curve. The grid measured 502x at 32,768 bits and
806x at 36,864; this array is 131,072 bits, so the penalty was in the right
family and the recovery is what removing it predicts.

Note the map also got **34x faster**. 1,096 s of synthesis was Quartus building
131,072 flip-flops and their mux trees.

### 04:20 - The sweep, in a worktree

`git worktree add --detach /c/programmieren/zencrifice/zhaozhou-sweep-sheet 2eeef64`,
per the standing owner ruling. Guard 7 derived **six** consumers from
`tests/CMakeLists.txt` and they matched the declared set:

```
test_field_write_tag test_surface_sheet_directed test_surface_sheet_random
test_surface_sheet_store_diff test_surface_stamp_chain test_texture_aux_directed
```

Three of the six are not named for this block. A sweep that scored only the
`test_surface_sheet_*` lanes would have missed the case the block exists to
prevent: one patch's scars appearing under another patch's stamp.

Preflight: **`linted 18 mutants at Slots (2, 4), 0 do not build`** -- a non-zero
count, checked, because a fresh checkout once printed "linted 0 mutants" and
exited 0.

Two things the preflight caught before anything was scored:

* **S01 orphaned a signal.** Redirecting the tag write into `mem_str` leaves
  `mem_tag` written by nothing -> `-Wall` UNDRIVEN. Rewritten as a TRANSPOSE of
  the two planes, which keeps both driven and is a truer statement of "a write
  lands in the wrong array" anyway. **The mutation was fixed, not the guard.**
* **The block does not lint clean at `Slots = 1`** -- and neither does HEAD's
  pre-change version, so the split did not cause it:
  `Bit extraction of array[4095:0] requires 12 bit index, not 13 bits`.
  `SlotBits` is `(Slots <= 1) ? 1 : $clog2(Slots)`, so it is 1 even at one slot
  and `AddrBits = SlotBits + 12` is 13 against a 4,096-word array. The extra bit
  is always zero, so the block is functionally unharmed, but the parameter does
  not elaborate cleanly at its own lower bound. **Not fixed: changing an address
  width at an untested `Slots` is a behaviour change wearing a lint fix.**
  Docketed. The preflight lints at 2 and 4 and says in its own comment why not 1.

### 04:35 - THE STRETCH GOAL: zhao_forge_cliff, diagnosed before anything changed

The task said to read *why* two of its three tables infer and one does not,
before touching it. The answer is one line, and it is the same killer:

| table | shape | write sites | inferred? |
| --- | --- | --- | --- |
| `prio_mem_r` | 2048x32 | 1, **whole word** | **YES**, 65,536 bits |
| `run_mem_r` | 1024x17 | 2, **both whole word** | **YES**, 17,408 bits |
| `edge_mem_r` | 2048x18 | 2, **one of them PARTIAL** | **NO** |

```systemverilog
edge_mem_r[mhead_r][5:0] <= mtake_r;   // zhao_forge_cliff.sv:690, StMdead
```

All three are read `assign x = mem_r[idx]`, i.e. ASYNCHRONOUSLY, which §10 lists
as a killer in its own right -- and two of them converted anyway, because
Quartus sometimes rescues an async read by inserting a read-address register.
§10 says that rescue "cannot be planned against". **The rescue does not survive
a partial write.** That is the whole contrast, and it is the same defect that
cost SURFACE.SHEET 229 % of the device.

Split at the field boundary the table was already documented as having --
`{cj[4:0], ci[4:0], side[1:0], span[5:0]}` -- into `edge_key_r` (12 b) and
`edge_span_r` (6 b). `edge_rd_c` reassembles the same 18-bit word. Both writes
are now whole. Lint clean; `test_forge_cliff_directed` and
`test_forge_cliff_random` both green.

### 04:45 - TWO DEFECTS IN scan_rtl.py, the tool the task said to use

The task said "do not guess -- `tools/budget/scan_rtl.py` exists and reports
read style, reset-touching and byte enables per array; use it". It reports two
of those three, and one of the two it reports is wrong.

**Defect 1 -- there is no byte-enable detector at all.** `grep -i byte
tools/budget/scan_rtl.py` finds nothing in 1,577 lines. On the repository's
worst block it reported the array as sync-read, not reset-touched,
`expectedStorage: RAM`, severity YELLOW: **healthy**. The scanner §10 names as
the source of "expected bits" could not see the killer that was actually
present.

**Defect 2 -- `resetTouched` was true for arrays no reset touches.** The scan
walked the ENTIRE `IF` node, ELSE branch included, so anything written in the
operating logic of an `always_ff` with an async reset counted as reset-written.
All three `zhao_forge_cliff` tables reported `resetTouched: true` against a
reset branch that assigns thirty-one scalars and not one array element. This is
the field an agent checks §10's SECOND killer with, so the false positive sends
someone to fix a reset that is not there.

**The fix for defect 2 was wrong the first time, and the controls caught it.**
Taking `thensp` looked obviously right and is exactly as wrong as taking both.
Every block here writes `if (!rst_n) <reset> else <work>`, and **Verilator's
elaborated AST folds the `!` away by SWAPPING the arms**: the condition arrives
as a bare `VARREF rst_n`, with the WORK in `thensp` and the RESET in `elsesp`.
Confirmed by dumping the node -- `cond type: VARREF`, `thensp` first statement
at line 548, `elsesp` first statement at 506. Reading polarity off the NAME is
no better (`rst_n` reads active-low, `rst` active-high), and picking a detector
by name is a failure this repository has already disclosed.

The branch is now identified by what a reset branch IS: it drives things to
known values, so **every right-hand side in it is constant**, where the working
branch is full of VARREFs. Polarity-independent and frontend-independent. When
the test does not separate the two arms, BOTH are taken -- the old, over-broad
behaviour, which is the safe direction for a guard.

**Both detectors are given positive controls, in BOTH directions**, because
RUN-20260823-2226's fifth disclosed failure is a detector that returned zero
across 91 modules because it could never fire. `validate_scan_rtl_fixes.py`,
14 cases, **14/14**:

* partial writes must be **0** on the six arrays that are now whole-written, and
  **2** on the pre-change `mem` and **1** on the pre-change `edge_mem_r`, read
  out of git at `991f13c3`. A detector that says "no partial write" about
  everything passes a one-sided test just as well as a correct one.
* `resetTouched` must be **false** on the cliff tables and on `mem_tag`, and
  must still be **TRUE** on `zhao_surface_sheet.dir_handle` (which really is
  cleared in the reset branch) and on `zhao_field_seq.rf` (which §10 itself
  retro-explains as "written from a reset branch"). Before the second fix,
  `dir_handle` passed **by coincidence** -- it is also written in the operating
  logic, so the over-broad walk found it there.

### 05:00 - THE SWEEP FOUND A REAL HOLE: S05 SURVIVED

> `S05 read-during-write returns the POST-write word where C5 says pre-write  *** SURVIVED ***`

Six consumer lanes, 232 checks between them, and **not one of them can tell the
two behaviours apart.** The contract states the rule; no consumer generates it
(`SURFACE.STAMP`'s cursor marches forward and its write trails its read by two
texels); and the shipped differential cannot cover it **by construction**,
because `zref::surface::SheetStore` is a C++ model with no notion of a cycle.
A stated, unconsumed, untested semantic is precisely what a storage-shape change
moves without anything noticing.

**S05 is NOT an equivalent.** The two behaviours are distinguishable, and this
run already had the instrument that distinguishes them: the shape differential
catches the same defect as control C4 with **408 port-cycle mismatches**. So the
finding is a TEST GAP, and the rule is to kill it with a test rather than argue
it.

Added `test_read_during_write_returns_the_old_word` to
`tests/surface/surface_sheet_directed.cpp` -- 6 new checks, 58 -> 64:

* the whole-word collision returns the PRE-write word, and the raced write still
  lands;
* the **tag-only** collision returns the whole PRE-write word, and afterwards the
  tag has moved and the strength has not. That second half is the case the
  whole-word collision cannot see, and it is the one place the split into two
  planes could have gone wrong quietly.

`test_simultaneous_read_and_write` already existed and drives both ports in one
cycle at **different** addresses (500 and 501) -- the case SURFACE.STAMP actually
generates. The same-address case is the one nobody had written.

**This is the sweep earning its cost.** Every other mutant so far was caught;
the one that survived is the one the contract had argued in prose and nothing
had checked.

### 05:10 - S07 SURVIVED TOO, and the reason is worse than a missing test

> `S07 the read enable is dropped, so a stalled response loses its word  *** SURVIVED ***`

The contract promises exactly this and says which test checks it:

> "`pg_valid_o` holds its word until `pg_ready_i` -- the read-data register's
> enable is gated by the accept, so a stalled response does not lose the word.
> `surface_sheet_directed` holds `pg_ready_i` low for 20 cycles and checks the
> word is still there and still correct."

**That test exists and it is real, and it was passing for the wrong reason.**
Its hold loop is

```cpp
for (int c = 0; c < 20; ++c) { zhao::tick(dut); dut.eval(); }
```

which leaves `req_texel_i` parked on 9 for all twenty cycles. A block that
re-reads the array on EVERY cycle instead of only on an accepted request
therefore keeps fetching texel 9, and answers correctly by accident. **The check
was right; its stimulus was constant.** That is a subtler failure than a missing
test, and it is the one a reviewer reading the test list would never see.

Fixed by wiggling the address during the stall (`req_texel_i` alternates 7/11,
which hold 248 and 244 against texel 9's 246). Nothing is accepted during the
stall -- `req_ready_o` is low because the response register is occupied -- so it
must be invisible to a correct block, and it is: 64 checks still green.

### 05:35 - THE STRETCH GOAL LANDED, and it confirms the mechanism

`tools/quartus/run_block_map.ps1 -Module zhao_forge_cliff -RowLabel "@edge-split-wip"`.

| metric | before | after |
| --- | ---: | ---: |
| `blockMemoryBits` | 82,944 | **119,808** -- the FULL expected storage |
| `inferredMemoryCount` | 2 | **4** |
| `registers` | 40,655 | **3,875** |
| `estimatedAlms` | 33,109 | **7,664** -- 79 % of the device to 18 % |
| `dspBlocks` | 2 | 2, unchanged as intended |

```
altsyncram:edge_key_r_rtl_0   AUTO  Simple Dual Port  2048 x 12
altsyncram:edge_span_r_rtl_0  AUTO  Simple Dual Port  2048 x  6
```

**`edge_key_r` and `edge_span_r` inferred while STILL BEING READ
ASYNCHRONOUSLY.** Nothing about the read was touched. That is the confirmation
the diagnosis needed: §10's "rescue" -- Quartus inserting a read-address
register to save an async read -- is real, and it survives the async read and
does NOT survive a partial write. The two killers are independent but not equal
in strength, and that is now a rule with a mechanism behind it rather than a
shrug.


---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings |
|-----------|----------|---------|--------|----------|
| 03:28 | Explore | Extract the seven + five failure disclosures, Don't-Retry lists and exact commands from RUN-20260823-2226 and RUN-20260823-1736 | done | Summarised into Constraints/Don't Retry above and below |

Load-bearing items inherited from that report:

* `tools\quartus\run_block_map.ps1 -Module <m>` -- **one module per invocation**;
  the script writes `zhao_block_map.json` at the END of its module list, so a
  long invocation that dies writes nothing.
* `ctest --test-dir build -L fast --output-on-failure`, after
  `. .\tools\env\zhao-env.ps1`, all in one shell.
* `ledger_check` is itself a ctest test labelled `fast` -- so `ctest -L fast`
  is expected to show **exactly one** failure at baseline, and that one is the
  V16 Field error above.
* Sweeps run **detached, in a git worktree, at the shipping commit, with a
  verified non-zero mutant count**. `.gitattributes` pins `*.sh` and `tools/*.py`
  to LF; a preflight that lints nothing must FAIL, not pass.
* `git add -- <explicit paths>`, never `-A`; read `git diff --cached --stat`
  before every commit.
* A stopped background task is **not** a stopped process -- verify children died.

---

## Files Created

- `runs/CLAUDE-RUNS/RUN-20260824-0317-surface-sheet-storage-inference/SPEC_v1.md`
- `runs/CLAUDE-RUNS/RUN-20260824-0317-surface-sheet-storage-inference/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260824-0317-surface-sheet-storage-inference/calib_ram_rdw.log`

## Files Modified

- `tools/budget/gen_calib.py` -- new `ram_rdw_module()` + `ram_rdw` family (4 points)
- `tools/budget/calibration.json` -- 4 measured rows merged by `run_calib.ps1`

---

## Decisions Made

1. **Split the word into two byte planes rather than instantiate `altsyncram`.**
   §10's advice for a genuinely byte-enabled memory is to instantiate the
   megafunction. That is not needed here: the two halves are independently
   enabled but never partially written *within* a half, so two 8-bit arrays
   express the same behaviour with no vendor primitive, no simulation model
   divergence, and no loss of portability to the Steam/hardware lanes. It also
   makes the RTL structurally match the oracle, which stores exactly these two
   byte arrays.
2. **`dir_handle` stays in flip-flops.** Both other killers apply to it and both
   are correct there. 64 bits.
3. **No valid bitmap.** Reset is not the killer here; C3 rejected the bitmap on
   the record.
4. **Extend the calibration rather than probe once.** The 2x2 is committed as a
   generator family so the next agent inherits the answer.
5. **`scan_rtl.py`'s missing byte-enable detector: fixed in this run, with a
   positive control.** Budget-audit's own F5 is "believing a detector that
   reports zero" -- a new detector that fires on nothing would repeat it
   exactly, so the block's own pre-change source is the control.

---

## Next Steps

- RTL change; differential; sweep; map; ctest; ledger.
