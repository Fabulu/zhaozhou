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
