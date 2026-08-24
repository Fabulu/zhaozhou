# Task Log: RUN-20260824-1039 — normalize dynamic shifts, and fit provenance

**Created:** 2026-08-24 10:39 UTC+02:00
**Status:** In Progress

---

## Objective

Two things, one of which I caused:

1. **Field wave 1** — remove the two dynamic barrel shifts from
   `zhao_field_normalize`'s mantissa path, which the Field ruling names as wave 1
   and which was still unimplemented after the leading-zero work.
2. **Fit provenance** — `run_block_fit.ps1` measures whatever is in the working
   tree at the moment Quartus reads it. I found this by breaking it.

---

## Progress Timeline

### 10:33 — I edited RTL while a fit was running

The rule I have enforced on every agent this week. The `zhao_field_seq` fit
launched before the break was still running when I changed
`zhao_field_normalize.sv`.

**And the harness does not copy sources.** `run_block_fit.ps1:325` emits
`set_global_assignment -name SYSTEMVERILOG_FILE <absolute path into the working
tree>`, so the fit elaborates the live file.

The timestamps decided it:

    edit        10:33:06
    map report  10:34:47     <- 101 seconds LATER

The edit sits inside the window where analysis was reading sources. **Which
version was elaborated cannot be established**, so the fit was killed. It may
have been a perfectly good measurement. It could not be *shown* to be, and an
unprovable number is worth nothing here — that is the whole argument this
project keeps re-learning.

### 10:40 — wave 1, and it is exact rather than approximate

The mantissa path was:

```systemverilog
rsh   = (d_exp > 0) ?  d_exp : 0;
lsh   = (d_exp < 0) ? -d_exp : 0;
m_val = 24'((h_rt >> rsh) << lsh);
```

Two 64-bit barrel shifts in series, built and muxed, to answer a question the
leading-zero search **has already answered**. `lz_t` is `h_rt << lz`, so its
top set bit is at bit 63 by construction and the normalised mantissa is a slice:

```systemverilog
assign m_val = (h_rt == 64'd0) ? 24'd0 : lz_t[63:40];
```

Equivalent in both directions — for `n >= 23` the old form shifts right by
`n-23` and drops the low bits, and `lz_t[63:40]` is those same bits; for
`n < 23` it shifts left by `23-n` and zero-fills, which is where the left shift
by `lz = 63-n` had already put them. Either way the MSB lands at bit 23.

This also retires the `lz_t_unused` hack that existed purely to keep `lz_t`
alive for `-Wall`. It is load-bearing now.

**Tests: 4/4**, including `field_normalize_random_nightly` at **50,000 random
cases**, plus lint clean — which is what confirms `rsh`, `lsh` and
`lz_t_unused` left nothing dangling.

### 10:45 — the guard, and three controls that tested nothing

The flow now hashes every source named in the QSF at QSF-write time, again the
moment `quartus_map` finishes, and once at the end. Any difference sets
`status = contaminated:source-changed-during-fit` **before the summary is
parsed**, so such a row cannot reach `ok` and cannot enter the census.

**Getting a control to fire took four attempts, and the first three failed
silently in three different ways:**

1. edited at a fixed `t=8s` — **before the QSF existed**, so the hash was taken
   of the already-edited file and nothing ever differed;
2. polled for the QSF and matched a **stale workspace**. There are **36 orphaned
   `zhao-block-fit-*` directories** in `%TEMP%`; it fired at `t=0.5s`, again
   before the real run had hashed anything;
3. baselined the workspaces correctly and then edited **terrain files, which are
   not in the 27-file shell cone**. The guard ignored them because it is
   supposed to.

`sourcesHashed: 27` on the row is what made (3) legible, and it matches
`grep -c SYSTEMVERILOG_FILE` on the shell QSF exactly.

> **A control that reports "not detected" is not evidence that the detector is
> broken.** Three times running, it was evidence that the control was. This is
> the same shape as the worktree checkout that silently did not take — and the
> counter-lesson to it, because there the tool was fine and the *observer* was
> wrong in the other direction.

### 10:55 — BOTH ARMS PROVEN

| arm | control | result |
| --- | --- | --- |
| start-vs-end | `zhao_debug_counters.sv`, left edited | **CONTAMINATED**, ALM suppressed |
| during-map | `zhao_input_rumble.sv`, **reverted after the map** so the tree ends byte-identical | **CONTAMINATED** |
| negative | three clean runs | `ok`, `sourcesHashed: 27` |

The during-map arm is the one worth having: at the end of that run the file was
byte-identical to its committed form, so start-vs-end alone would have called it
clean.

**The harness behaved better than I designed.** On contamination it prints
`KEEPING the previous measurement (222 ALM / 0 DSP) rather than erasing it` —
so a poisoned attempt cannot destroy a good row either.

**One self-inflicted casualty:** the first control restored its file with
`Set-Content -NoNewline`, which rewrote all 1,452 lines with different line
endings. Restored with `git checkout`; later controls use
`[IO.File]::WriteAllBytes` for a byte-exact revert.


### 11:20 — the gate found two failures and NEITHER was noise

`ctest -L fast`, 630 s:

* **`format_check` was ALREADY RED at `b3a16a0`.** The violation is in
  `tests/terrain/terrain_normals_directed.cpp` — the input-poisoning lines from
  this morning's `TERRAIN.NORMALS` run — and `git status` shows the file
  unmodified, so it was **committed in violation**. That run reported its gates
  green. Formatted with the pinned `node_modules/clang-format` (the version pin
  is the point: a system clang-format from another LLVM reformats half the tree).

  `git diff --stat` claimed 262 insertions/deletions on a file I had not
  touched; `git status --porcelain` showed it clean. That is the `core.autocrlf`
  artefact this repo already documents — **the diff was not evidence of a
  change.**

* **`ledger_check` V20 fired on MY OWN comment.** `zhao_field_normalize.sv:288`
  states "by construction" and "exactly equivalent, in both directions" with no
  `ENFORCED-BY:` within ten lines. The rule is right and I broke it: an
  equivalence argument written in a comment is exactly the kind of claim that
  rots silently. Now names
  `tests/differential/field_normalize_directed.cpp:main`, matching the
  convention already used at line 255 of the same file.

Errors 2 → 1. The remainder is the recorded V16 `FIELD.SEQ.CORE` baseline.

### 11:35 — committed the harness work, `92e279d`

Separately from the RTL, because it stands on its own and the RTL still owed a
sweep.

### 11:40 — the sweep, and a mutant whose TARGET no longer exists

Removing `rsh`/`lsh` broke two mutants:

* **M36** ("the zero-length guard is dropped") merely moved — re-anchored on the
  slice form. Its equivalence proof survives the move because it is semantic:
  with `h_rt == 0` the leading-zero search leaves `lz_t` zero, so `lz_t[63:40]`
  is zero anyway, exactly as `(0 >> 0) << 0` was.
* **M35** ("the normalise shifts go the wrong way") **cannot be expressed at
  all** now. It swapped two signals that no longer exist.

**Deleting M35 would have shrunk the sweep silently**, which is the failure this
project has already recorded in the other direction (a mutant surviving because
it no longer does anything). It was given a **successor testing the same
property against the structure that exists**: the mantissa is taken from the
right bits, `lz_t[63:40]` → `lz_t[62:39]`. That one should be CAUGHT, unlike
M36.

Preflight: **38 mutants across 11 files, 0 do not build** — non-zero count
parsed, both re-anchored mutants confirmed buildable.

### 11:50 — SWEEP 38/38, and the successor mutant is CAUGHT

    attempted=38  expected=38  accounted=38  caught=33
    SURVIVOR: M01, M07, M20, M36, M38

**No mutant was discarded by the re-elaboration hash guard** (`attempted` equals
`accounted`), so all 38 genuinely rebuilt. The five survivors are the same five
as before and all are documented proven equivalents — **no coverage hole**.

**M35's successor is caught**, which is the result that matters: the slice form
is genuinely covered, not merely un-mutated. Had I deleted M35 when its target
vanished, the score would still have read 33/37 and looked identical.

Committed `440a1a1`, pushed. Harness work committed separately at `92e279d`.

### 11:56 — THE FORMAL LANE HAS BEEN DEAD SINCE THE 23rd, BEHIND A `pending` LABEL

The last ledger error is V16: `FIELD.SEQ.CORE` is `RTL_VERIFIED` while
`tests/formal/field_seq_bound.sby` is recorded `pending`. The entry says the
status flips "only by a run that passes inside the lane's own budget", so I ran
it.

**It did not elaborate at all. It failed in 0.92 seconds.**

    zhao_field_seq.sv:328: error: identifier 'exec_writes' used before its declaration
    ... 7 errors, then 4 more at :368 for rd_a_q/rd_b_q/rd_c_q/rd_h_q

**The register-file rewrite did this.** It put a combinational write decode and
a clocked memory process *above* the declarations they reference. Verilator
accepts a forward reference; **`read_slang`, the formal frontend, does not.**
Eleven errors, two ordering classes, no signal changed meaning.

> **A red already labelled *expected* stops being looked at.** V16 exists so
> banked evidence cannot back a maturity claim, and it was doing that job
> honestly — but the same label removed the only pressure that would have
> revealed the lane could not even start. The entry was truthful and it still
> hid a completely broken lane.

This is the sibling of the `SKIP`-if-absent gate that hid weeks of drift, and of
the map-only census that looked healthy because the scanner walked the wrong
branch. **The failure is never the label; it is that a label ends the looking.**

### 12:25 — elaborating, and the wall time is finally being measured

Depth **129 of 172**, every property UNSATISFIABLE so far, `cover` queued behind
`bmc` on the single solver. The derivation did **not** need re-deriving: it keys
off the package constant `MAX_OP_CYCLES = 80`, not measured latency, and
NORMALIZE3's worst case at 67 + 1 for the new state is still far under it.

Deliberately running **no builds or tests concurrently** — the number this owes
the ledger is the run's *solo* wall time, and contending for CPU would inflate
the very measurement being taken.

Noted in passing: yosys warns `no driver for u_norm.r24_next[31:24]`. Benign and
pre-existing — `r24_next` is clamped to `0x00FF_FFFF` and only `[23:0]` is ever
consumed, so the top bits are optimised away. Not from the mantissa change.

### 12:26 — the proof does not FAIL, it does not FIT

    Total Test time (real) = 1800.20 sec   <- exactly the budget

`ZHAO_FORMAL_WRAPPER_TIMEOUT` is **1800**, and `tests/CMakeLists.txt:65` says
the wrapper budget is the one that BINDS — a lane's CTest `TIMEOUT` is only a
backstop. So this is a **timeout at k~130 of 172**, not a violated property.
Every bound reached was UNSATISFIABLE.

The policy for this is already written and sanctioned: *"a lane that needs more
sets it before its `configure_file` and puts it back afterwards"*, with
`RUN_SERIAL TRUE` because **"wall time must approximate solo time"**. That is
also the justification for not co-scheduling anything while measuring.

`mem_vram_arbiter_liveness` went through exactly this in August and its comment
carries the warning worth repeating: a proof lane that times out gets read as a
failure, **or gets "fixed" by deleting tasks.**

Measuring the solo number rather than picking a round one. At 13:25 it is at
**k=140 after 57 minutes**, and the tail is steepening — k=130 took ~29 min and
the next ten bounds took another 28. This is a multi-hour lane, which is itself
the finding.

### 13:30 — I MISREAD A SUPERSEDED SECTION AS A LIVE BLOCKER

Chasing FRAMEBLIT (priority a), `REMAINING_BLOCKERS.md` appeared to say CMD.DMA
still had two defects — a one-cycle payload CRC re-walk and a `slot_buf` that
could not infer as RAM — described as *"NOT yet done, and it is a real
redesign"*, gating the composed fit and FRAMEBLIT step 8.

**All of it is already done, and the document says so.** The text I read sits
inside a block headed *"(SUPERSEDED, kept for the reasoning)"*, and one
paragraph above it the live text reads **"This unblocks the composed fit and
FRAMEBLIT step 8."**

The evidence agrees, which is why I caught it:

| check | result |
| --- | --- |
| `slot_ram` inference | **32,768 memory bits** on the map — exactly 4,096 bytes |
| the fit that "never succeeded" | `status ok`, **3,607 ALMs**, 1,293 s |
| the commit | `f5e067e`, *"CMD.DMA FITS: 83,977 ALMs -> 3,607, and the staging buffer is real memory"*, **2026-08-22** |
| the CRC re-walk | gone at `fd262de`; the RTL says *"crc_final() is gone"* and folds per beat |

> **Reading a long report is a lane with its own failure mode.** A superseded
> section is not stale data to be corrected — it is correct data about the past,
> and treating it as current would have had me "fix" something already fixed and
> then report a blocker cleared that was never blocking. The label that saved
> this was the document's own, written by whoever chose to keep the reasoning.

So **FRAMEBLIT step 8 is unblocked, and it is "rerun the composed Quartus
synthesis"** — a machine task, queued behind the formal measurement, since two
heavy Quartus/solver jobs must not co-schedule.

### 13:45 — I CORRUPTED THE MEASUREMENT I HAD SPENT AN HOUR PROTECTING

`ctest -R "field_seq"` also matches **`formal_field_seq_bound`**. So the run I
started to verify the declaration reorder launched a **second copy of the same
two-hour proof**, which then competed with the solo measurement for about thirty
minutes before timing out at 1800 s.

An hour of refusing to co-schedule anything, undone by an unanchored regex.

The four lanes I actually wanted all passed — `field_seq_directed`,
`field_seq_random`, `field_seq_random_nightly`, `lint_zhao_field_seq` — so the
reorder is behaviour-neutral, and the sweep preflight still reports **38 mutants,
0 do not build**, meaning no anchor moved. Committed `0d80f64`, pushed.

**The wall-time figure is therefore an upper bound, and is recorded as one.**
For sizing a timeout that errs in the safe direction — the lane's own warning is
about lanes that pass solo and fail contended, i.e. under-sizing — but it is not
the solo number the policy asks for and must not be written down as though it
were.

### 14:30 — THE PROOF PASSES

    engine_0 (btor btormc) returned pass
    engine_0 did not produce any traces
    DONE (PASS, rc=0)
    Elapsed clock time: 2:02:17 (7337 s)

**`bmc` passes at depth 172.** 19 inputs, 152 states, 4 bad-state properties, 6
constraints; every bound UNSATISFIABLE and no trace produced.

**7,337 s is 4.1x the 1,800 s wrapper budget** — with ~30 minutes of that
contended by my duplicate. The lane was never failing. It was never *finishing*.

That also settles the depth question the RF change raised: the derivation keys
off the package constant `MAX_OP_CYCLES = 80`, not measured latency, so
NORMALIZE3's worst case at 67 + 1 for the new state stays far under it and **172
did not need re-deriving.**

---

## Decisions Made

**Hashing rather than copying.** Copying sources into the workspace would make
provenance immutable by construction and is the better fix. The tree has one
`` `include `` (`sdram_params.svh`), so a copy must preserve directory
structure — real surgery, and not something to land untested while a long fit is
queued. The limitation is **written into the gotcha rather than left implied**:
an edit made and undone entirely inside the elaboration window is still
invisible.

---

## Next Steps

- [ ] `ctest -L fast` + ledger, then commit
- [ ] **relaunch the `zhao_field_seq` fit at the committed commit** — still the
      first I/O-constrained Fmax for the Field engine; the 33.86 MHz predates the
      SDC I/O fix and may itself be optimistic
- [ ] consider clearing the 36 orphaned `%TEMP%` workspaces
