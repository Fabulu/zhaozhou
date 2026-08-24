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

Run initialised with `runs\CLAUDE-RUNS\init-run.ps1 surface-sheet-storage-inference`.

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
