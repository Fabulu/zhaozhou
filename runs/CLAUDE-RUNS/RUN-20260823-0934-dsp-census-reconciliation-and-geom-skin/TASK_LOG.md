# Task Log: RUN-20260823-0934 - Reconcile the DSP census with the merged RTL, then GEOM.SKIN

**Created:** 2026-08-23 09:34 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260823-0934-dsp-census-reconciliation-and-geom-skin/

---

## Objective

Get `main` and the machine-readable resource report to agree again, then continue
the DSP campaign with `zhao_geom_skin` (72 DSPs, the largest remaining block).

Two specific inconsistencies opened when the Field IR work landed:

1. `zhao_field_seq`'s row still read **79 DSPs / 10,623 ALMs** while the RTL in
   main was the **3-DSP** version. RTL and measurement disagreeing is worse than
   a stale measurement alone, because the V23 census reports a number nobody can
   act on.
2. `zhao_geom_lod`'s row read `status=failed:quartus_fit.exe` with no numbers.
   A re-fit I killed had overwritten a good row, so the measured 18 -> 6 DSP
   result was invisible in the report while remaining true in the RTL.

---

## Progress Timeline

### 2026-08-23 09:34 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260823-0934
- Entry state: `main` at `0561323` on origin, local `main` at `d7691db`
  carrying an unpushed merge of `wp/field-dsp`.

### 09:20 - Merged `wp/field-dsp` into `main`

Clean fast-forward to `d7691db`, **zero conflicts** — the Field agent had
already merged main into its branch twice and had dropped its own copy of
`run_block_fit.ps1` so main's newer version (with the `quartus_sta` stage and
the failed-fit protection) won cleanly. The only conflict it had to resolve was
`TASK_LOG.md`, where we had both appended; it kept both entries and corrected
the figure in mine from 79->4 to 79->3 in place, with a note, rather than
leaving two different numbers in one log.

Done on the owner's explicit instruction: *"we should get it into main soon
because main seems to be diverging, that's more dangerous than creating a red
pipe, which will help us bugfix anyhow."*

### 09:40 - Restored `zhao_geom_lod`'s lost row  (`7cd3d8c`)

**Not retyped.** Recovered the row the harness itself wrote at `09bbe05`, found
by walking the last 14 commits that touched the report:

| | value |
| --- | ---: |
| ALMs | 1,183 |
| DSPs | **6** / 112 |
| registers | 271 |

`git log 4385b1e..HEAD -- fpga/rtl/geometry/zhao_geom_lod.sv` is **empty**, so
the RTL has not been touched since that measurement and the row still describes
HEAD. The failed attempt was preserved in the `lastAttemptStatus` /
`lastAttemptCommit` / `lastAttemptSeconds` fields rather than discarded — which
is what `0561323` added those fields for.

### 09:45 - Pushed `main`  (`0561323..7cd3d8c`)

Divergence closed. Main now carries the 3-DSP Field engine.

### 09:50 - Re-fitting `zhao_field_seq` on HEAD rather than editing the report

The measured after-fit numbers exist (8,901 ALMs / 3 DSPs / 5,356 registers at
`62d7b0e`, constrained) but they live in the agent's findings, not in the
harness-written report, because the fits ran in throwaway worktrees whose
`reports/synthesis/zhao_block_fit.json` no longer exists.

**Deliberately did not hand-type them into the report.** Re-ran the real fit on
HEAD instead, which additionally routes through the new `quartus_sta` stage and
so gives the row a real `fmaxMhz` — the first genuine speed figure for this
block, since every one of the previous 47 rows was fitted with no timing
objective at all.

Field RTL is not in the shell source cone, so all 17 `fpga/rtl/field/*.sv` files
were passed as `-ExtraSources`.

    tools/quartus/run_block_fit.ps1 -Module zhao_field_seq `
        -ExtraSources <17 files> -TimeoutSeconds 3000

Result: [pending]

### 10:00 - Killed my own `zhao_field_seq` re-fit; it was a duplicate and a race

The Field agent was **already** running `quartus_sta` on the after side for the
same block. Two flows merging rows into one
`reports/synthesis/zhao_block_fit.json` is precisely the mechanism that lost
`zhao_geom_project`'s 5,806 ALM row permanently.

Stopped the job, confirmed only the agent's `quartus_sta.exe` remained, and
confirmed `git status` showed the report **unmodified** -- no row was written,
so nothing was lost. The agent owns that measurement.

### 10:05 - The ledger is RED on main, for one honest reason

    V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal "tests/formal/field_seq_bound.sby"
         is recorded as "pending", not green

Exactly one error against 92 blocks / 40 ops. This is the gate working. The
Field agent deliberately committed the formal status as `pending` rather than
banking a pass it had not obtained, and is running the depth-172 solo proof now.
**Not fixing this by demoting and re-promoting the maturity claim** -- that is
churn, and the proof is in flight.

Depth is derived, not chosen: `FORMAL_PROLOGUE=2`,
`MAX_RUN_CYCLES = 2*MAX_OP_CYCLES + 8 = 168`, `PROVEN_DEPTH = 172`. The binding
assertion is `a_progress` at 171, not `a_op_bounded` at 163 -- a machine could
retire every instruction inside 80 clocks and still never leave `busy`, and only
the counter would catch it.

### 10:10 - `ctest -L fast` could not run at all: the build was broken

Not a test failure -- `BAD_COMMAND` on ~30 tests, meaning the executables did
not exist. Three causes, none of them design defects, all of them build state:

1. A stale `build.ninja` re-ran an old Verilator command line that predated
   `zhao_field_exec_shared.sv`, so a **correct** `tests/CMakeLists.txt` (the
   file is listed at line 1148) produced
   `Cannot find file containing module: zhao_field_exec_shared`. The give-away
   is that the failure appears under `ninja: error: rebuilding 'build.ninja'`.
2. The cache had been written by the devkitPro msys2 cmake, which records
   MSYS-style paths; the Windows cmake refuses to reconfigure it.
3. `cmake --preset windows-native` then failed with **"Could not use disabled
   preset"** -- which does not mean what it says. The msys2 cmake reports
   `${hostSystemName}` as `MSYS`, so `windows-base`'s `equals Windows`
   condition is false and every preset inheriting it reports as disabled.

The fix was in `CMakePresets.json`'s own `ZHAO_NOTE` field the whole time:
dot-source `tools/env/zhao-env.ps1` first. Written up in **`docs/BUILD.md`**
(`a2eba8b`). Deleted `build/` and reconfigured clean rather than patching around
it, which also clears mutant-derived sources a sweep may have left in consumers
it never scored.

### 10:30 - THE FINDING OF THE DAY: the Field engine is 12x too slow, and so was the old one

| | Fmax | WNS setup | TNS setup |
| --- | ---: | ---: | ---: |
| before (79 DSP) | **7.72 MHz** | -119.5 ns | -2,389,303 ns |
| after (3 DSP) | **8.59 MHz** | -106.4 ns | -2,122,226 ns |

Against a 10 ns target. **The DSP work bought 11% of the speed and was never
going to buy more.** Multipliers were not the critical path and never were.

TimeQuest named the wire rather than leaving it to be guessed. All three worst
paths are the same one, at **78 logic levels in a single cycle**:

    from  zhao_field_normalize|h_rt[41]    the integer root's held answer
    to    zhao_field_normalize|o1_o[1]     a normalised output lane

It is the exponent extraction, written as two **64-iteration combinational
loops** that unroll into 128 sequential compare-and-shift stages on a 64-bit
value, feeding both the seed ROM index and the per-lane rescale shift in the
same cycle. The loops are in the pre-change design too, which is why both sides
sit at 8 MHz -- **and why this was invisible until the SDC defect was fixed.**

Deliberately NOT fixed in this pass. `zhao_field_rcp` already does it correctly
a few lines away, with a leading-zero count at log depth. Replacing the loops,
or simply registering `(m, e)` into a state of their own, costs **one clock on
an operation that has thirteen clocks of margin.** But it is a different change
with its own verification cost -- it would invalidate the mutation sweep and
both fits -- so it goes on the docket with the measurement attached, to be taken
with the next Field touch rather than as a special trip.

**The implication is bigger than this block.** Every one of the 47 rows was
fitted with no timing objective. Area was the only thing anyone was reading, and
area was the thing least affected. There is now good reason to expect other
blocks to be similarly far from closing, and no evidence either way until they
are re-fitted with the corrected SDC. That is a new axis of work, not a
continuation of the DSP campaign.

Sustained rate on the new engine, since the ruling requires allocation be
justified by frame demand: **277,778 simple instructions or 24,876 NORMALIZE3
per 60 Hz frame at 100 MHz, on three DSP blocks.**

### 10:35 - Census as it stands

| | DSPs |
| --- | ---: |
| measured total, 41 of 47 rows | **327** / 112 |
| with the Field row corrected to its measured 3 | **251** |
| with GEOM.SKIN at its 12-18 target | **~194** |

Largest remaining: `zhao_geom_skin` 72, `zhao_terrain_project` 33,
`zhao_surface_stamp` 28, `zhao_texture_tmu` 28, `zhao_terrain_normals` 18,
`zhao_geom_cull` 15, `zhao_geom_binner` 12.

Restoring `zhao_geom_lod` moved the total **up**, 321 -> 327. That is correct
and worth stating plainly: the block had been counted as zero because its row
carried no numbers, so the census was understating the problem.

### 10:45 - Clean build green; STATUS corrected on three claims

`cmake --preset windows-native` + `cmake --build build`: **1,570 targets, exit
0**, after dot-sourcing `tools/env/zhao-env.ps1`. `ctest -L fast` launched
against it.

The Field agent checked the five claims I had published in `STATUS.md` on its
behalf. All five stand; three needed refinement, and **one changes the decision
a reader would make**:

* **"the fix costs one clock" understated it — the good fix is free.**
  Registering `(m_val, e_val)` into their own state costs a clock; a
  **leading-zero count costs zero** and is log-depth instead of linear.
  Phrasing it as a clock invites "is it worth a clock?", and the good answer is
  not. Corrected in `787ccd2`.
* **The 78 levels are DOMINATED by the loops, not identical to them.** The
  attribution is by construction, not enumeration: `o1_o` is
  `resc_s(product, shift_amt)`, `shift_amt` is `31 + e_val`, and `e_val` comes
  only from the exponent extraction, so no other route exists from `h_rt` to
  `o1_o`. The rescale is perhaps 8-10 of the 78. Nobody walked the cells.
* **The unconstrained-fit problem is worse than a missing column.** With no
  timing to meet, the fitter optimises for **area** instead. The 47 ALM figures
  are therefore the *optimistic* end for designs later asked to close timing —
  measured against the wrong objective, not merely missing one.

Verified with no change needed: the 7.72 -> 8.59 / 12x / 11% arithmetic;
"present in both designs" (the loops are byte-identical, preserved verbatim
through the DSP rewrite); the deferral rationale; and the shell claim —
`zhao_shell_top` really does declare `gpu_clk`/`vid_clk`/`audio_clk` at lines
104-106, which is exactly why its SDC binds and the leaf blocks' did not.

### 11:00 - Promoted the normalize fix over the DSP queue

Fabian's outside reviewer independently identified the same two loops and made
the argument that decides sequencing:

> Killing the worst path reveals the second-worst. Fixing normalization could
> take us from 8.6 MHz to 28 MHz, not 100. Or everything else is at 8-9 ns and
> we're basically done. We don't know until the next STA.

So **the deliverable is the Fmax after the fix, not the fix.** That number
decides between one embarrassing bug and a timing-rearchitecture campaign across
the design, and it is the highest-information measurement available anywhere in
the project. It therefore outranks continuing the DSP campaign, and was promoted
rather than left to wait for an unrelated Field visit.

The reviewer's reassuring observation is worth keeping: had sharing the
arithmetic created a mux/interconnect bottleneck, the after side would be
*worse*. It is 11% better, with the pre-existing critical path untouched. **The
79 -> 3 is not implicated.**

### 11:20 - The replacement is written, and it is bit-identical

Oracle checked first (V17): `zfield::interpret`'s `normalize2` and
`zref::normalize3_approx` (qformats 7.4) carry the identical two-loop shape and
both guard `n2 == 0` *before* it, so `len >= 1` and has an MSB — which is what
makes a closed form possible at all.

The equivalence is argued, not asserted. With `n` = index of `len`'s MSB: the
first loop runs only when `n < 23` and stops after exactly `23 - n` doublings;
the second only when `n >= 24`, after exactly `n - 23` halvings; at `n == 23`
neither runs. In every case the MSB lands on bit 23 and `e = n - 23`. Repeated
**truncating** halvings compose exactly — `floor(floor(x/2)/2) == floor(x/4)` —
so the second loop is one truncating right shift rather than an approximation of
one. With `lz` the 64-bit leading-zero count, `n = 63 - lz`, `e = 40 - lz`, and
the mantissa is a single barrel shift. **Bit-identical**; there is no rounding
in this function to get wrong.

Five mutants added (M34-M38, sweep now 38), aimed at the corners an equivalence
argument gets wrong: exponent offset off by one, shift direction inverted, zero
guard dropped, the search's last stage neutered, and a stage testing the wrong
half.

**The sharpest detail: the comment sitting on those loops already said "the
loops become a leading-zero count either way."** The diagnosis was written down
and never acted on, and the correct shape was already in the same file forty
lines away in `zhao_field_rcp`. This was a defect, not a trade-off.

### 11:25 - Formal: a negative result worth more than a pass

Unabstracted corroboration, no `cutpoint`, depth 100: **bmc PASS (5,473 s),
cover PASS (5,499 s)**. Depth 100 fully exercises `a_op_bounded` on the first
instruction (needs 83) and `a_pc_bounded` throughout; it does not reach
`a_progress`'s 171, so it corroborates the abstraction over its window rather
than replacing it. Had the free product admitted a case the real multiplier
forbids, 100 steps were available to disagree, and it did not. The **cover** task
passing unabstracted is the non-vacuity evidence on the real design, which is
why covers were left unabstracted.

It also priced the abstraction and settles the timeout question in advance:
**5,473 s at depth 100 against an 1,800 s lane budget**, and the unabstracted run
at the full derived depth of 172 was abandoned at k=114 after 2h20. **There is no
version of this proof that runs unabstracted inside its own budget.** Recorded in
`formal_runs.yml` as a measurement, not a claim.

### 11:40 - `ctest -L fast` on merged main: 259 of 262

Total 1,643 s against the clean rebuild. **No RTL failures anywhere** — every
differential and every Verilator lint passes, including the whole Field engine
after 79 -> 3 DSPs and the sequenced terrain and creature LOD ladders.

Three failures:

| test | verdict |
| --- | --- |
| `ledger_check` | **expected and correct** — the single V16 error, formal `pending` |
| `cppcheck_check` | **a real test defect**, see below |
| `format_check` | clang-format drift, mechanical |

Both lint failures are in `tests/differential/field_seq_directed.cpp`, which my
merge put on main before the Field agent had run the lane. That is my error, not
the agent's.

**The cppcheck finding is worth more than the lane it came from.**
`zfield::Table` declares `uint8_t kind;  // 0 curve, 1 spline` (zfield.hpp:120).
Line 1522 declares `zfield::Table tbl;`, fills `x`, `y` and `dy`, and **never
sets `kind`** — then pushes it into `p.tables` at three sites (1535, 1642, 1836).

The `kCases` list at 1505-1519 contains **CURVE, DCURVE and SPLINE sharing that
one table.** So the per-opcode latency figures that block prints may be
attributed to opcodes run against the wrong kind of table. It presumably passes
because oracle and DUT read the same indeterminate byte and agree — which is
precisely the shape of a test that looks green while proving less than it
claims. Handed to the Field agent, which owns the file and has it open;
editing it from here would only have caused a conflict.

### 11:45 - Run tooling moved INTO the repo  (`2286e82`)

Found while trying to archive this run: I had initialised it into the **wrong
archive.** `init-run.ps1` and the templates lived at the workspace root, which
is not a git repository, while the tracked archive is `zhaozhou/runs/CLAUDE-RUNS`.

The README already named this as the reason runs were forgotten for four whole
days, and prescribed copying each finished run into the repo — **and that copy
step was itself getting forgotten.** The root copy was already missing
`RUN-20260818-0341` and `RUN-20260821-1200`.

So the tooling moved rather than the runs. `runs\CLAUDE-RUNS\init-run.ps1 <slug>`
now creates the run directly in the tracked directory; the script resolves
templates relative to its own parent, so `docs/coding_agents/claude_run_templates`
was copied in to make that work. Verified end to end by generating a run and
deleting it. `ARCHIVE.md` is tracked now too — the index of the archive had been
the one file the archive could not protect.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 09:30 | (prior) `abb3ab5` | Field IR shared arithmetic engine, 79 -> 3 DSPs | Completed, merged at `d7691db` | `reports/FIELD_IR_ENGINE.md` |
| 09:55 | `a7a69ac` | GEOM.SKIN 72 -> 12-18 DSPs | Running | own run dir |

---

## Files Created

- this run directory
- `reports/synthesis/zhao_block_fit.json` — modified, not created

---

## Decisions Made

**A measured report is never hand-edited.** The Field row is stale and wrong in
main right now, and the correct repair is a re-fit, not a retype. Typing a true
number into a measurement file destroys the one property that makes the file
worth having: that every row was produced by the tool.

**Restoring a row from git is not the same thing.** `7cd3d8c` recovers bytes the
harness wrote, at a commit whose RTL is provably unchanged, and keeps the failed
attempt visible. That is recovery of a measurement, not authorship of one.

**One Quartus fit at a time.** Three concurrent constrained fits exhausted this
24 GB machine and two were killed, which is how `zhao_geom_project`'s 5,806 ALM
row was lost permanently. The GEOM.SKIN agent was told explicitly to do all
design, RTL, differential and mutation work before touching the fitter, and to
confirm the machine is free first.

---

## Next Steps

- [ ] `zhao_field_seq` re-fit completes -> harness writes the row -> census drops
      from 321 toward ~245
- [ ] `ctest -L fast` and the ledger check on merged main, reported honestly
      including red
- [ ] re-measure `zhao_geom_project`, `zhao_terrain_bake`, `zhao_terrain_velocity`
      (rows lost or never taken)
- [ ] GEOM.SKIN agent reports back
- [ ] then serially: GEOM.CULL 15->4-6, SURFACE.STAMP 28->4-6, TMU 28->8-12,
      NORMALS 18->~6, TERRAIN.PROJECT cache-then-sequence
- [ ] update `STATUS.md` so Fabian can read progress from the repo
- [ ] archive this run in `ARCHIVE.md`
