# NOPROG — make the field EXECUTE, then build COMPOSED_MATERIAL

**Branch `gz/noprog`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**READ THESE TWO FIRST, IN FULL:**

1. **`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`** — the owner's
   ruling. **It is standing direction and it sets your acceptance bar.**
2. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-FIELDACTIVE.md` —
   your immediate predecessor. **It got the console to install a real field and
   then found why it does not run.**

## WHERE IT STOPS, MEASURED

The composed console **installs, seals and publishes a real Earth spell**
(`wave_pool`, a 1,472-byte ZFH2 capsule + 43 doorbell LOAD words):
`installs_ok=1`, `bad_crc=0`, `bad_meta=0`, `load_bytes=1536` over the real
bridge, `pub_ready=01`. `zhao_terrain_fieldlist` **resolves** it:
`unresolved=0`, `sealed=1`.

**`field_composed` is still 0, because the field does not EXECUTE.**

```
the field list says RESIDENT
zhao_field_earth_adapter says NOPROG -- exactly 1,089 times (33 x 33)
```

**One per lattice vertex**, so the patch's lane **did** fire per vertex, and
`add_fire_i` is the same handshake the patch takes.

**The adapter's own header documents this fingerprint VERBATIM** as the
intake-vs-replay race, records that the earlier repair left **"ONE NARROWED
RESIDUAL"**, and notes that **`lane_desync_o` structurally cannot see this
class.**

**Why no leaf bench ever caught it:** every leaf bench submits the record **and
then** the patch job. **This console has a job pending FIRST** — which is exactly
the composed-console ordering §13.7 exists to reach.

**FIELDACTIVE did not assert the mechanism, and neither should you until you
measure it.** Its named next diagnostic is **a replay counter on the adapter**.
Start there.

## YOUR JOB, IN ORDER

1. **Make the field execute.** `field_composed` nonzero in the composed console.
2. **Then build `TERRAIN.COMPOSED_MATERIAL`** — the owner declined the refusal
   and commissioned the build.
3. **Then meet the owner's six clauses**, which are a **checklist, not a
   summary**, each separately demonstrable:
   * a real production Field program **installed and executed**;
   * the field **covers the intended terrain**;
   * its material write produces a value that **cannot equal the authored
     baseline by accident**;
   * that composed material **reaches the intended production consumer**;
   * the **uncovered/control form restores the authored result**;
   * the **no-field forms remain unchanged**.

**Clauses 5 and 6 are half the evidence** — they are what prove the covered run
measured **the field** rather than the weather. `-FieldUncovered` already exists;
FIELDACTIVE built it alongside the positive form.

## SCOUTING FIELDACTIVE LEFT YOU — VERIFY, DO NOT INHERIT

* **The address is free**: `0x058B_0000` is reserved and `DEVSTORE` ends exactly
  there.
* **`DEVSTORE` is the enactment pattern.**
* **matjoin's composed write face is the tap point.**
* **`zhao_terrain_patch_acc` is BUILT AND COMPOSED NOWHERE** — check whether it
  is what you need before building anything new. *Before commissioning a block,
  grep the tree for the thing it replaces.*
* **The bench needs a new WRITABLE region.** FIELDACTIVE added HPS region 6 for
  the staging window `0x1000_0000` that `fld_ldr_stage_base_i` had advertised
  since the loader was composed **and nothing ever backed** — that is the
  pattern to copy.

## THE OWNER'S CONSTRAINTS, WHICH OVERRIDE CONVENIENCE

* **Do not silently cut field capacity, semantics, update behaviour, or the
  destination** to meet the bandwidth budget.
* **Measure the actual cost and report it either way.** Targeted local
  measurement is **authorised**; **another full-console fit is NOT.**
* **If the COMPLETED implementation proves the specified publication semantics
  cannot meet the frame/bandwidth contract, STOP with that measured conflict and
  escalate.** That would justify a new owner architecture decision. **The
  existing bandwidth estimate alone does not** — a refusal grounded in the old
  figure will be sent back; one grounded in your measurement of the built thing
  will be acted on.
* **`I34` must become TRUE IN THE ASSEMBLED CONSOLE, not close because the
  document was edited to match what happens to exist.**

## THE FORBIDDEN SHORTCUT, AND IT IS NEWLY TEMPTING

**Do NOT touch entry `I34`'s prose to move the register.** It classifies on a
regex for the word `BOUNDARY`, and **that word is already MEASURED STALE** — the
three ports it names occur in `zhao_console_core.sv` **five times, every one a
comment, zero as a port declaration.** That makes a prose edit the **easiest** way
to move the number and **the one thing the owner forbade.**

## The fences

* **`PHASEFIX` may still be live and owns the SHELL and the raster/phase path for
  I55.** You own **TERRAIN and FIELD**.
  `tests/prod/tb_zhao_console_core_smoke.sv`,
  `tests/prod/run_console_core_smoke.ps1` and
  `fpga/rtl/prod/zhao_console_core.sv` are **SHARED — stage the HUNK, not the
  file.** Those three have already produced one real merge conflict today.
* **Do not regress the plain smoke**: `raster pixels=2816`, `frames_admitted=1`,
  `texture fragments=1216`, `tile[max/or]=[6 7]`, `SMOKE: PASS`. **That is clause
  6** and FIELDACTIVE nearly broke it with an unconditional `PKT_MAX_C` change.
* **Do not regress velocity** — `terrain_veljoin_directed` 19/0 **on the repaired
  test**, `part_terrain_tap_directed` 1297/0, `composepub_acceptance` 154/0.
* **`REGS=32` needs NO widening.** FIELDACTIVE measured that `wave_pool` and
  `impact_wave` lower clean at 32 via `--scalar-base`. **There is no ALM purchase
  here** — the campaign carried that cost as inevitable and it was not.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010**. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.**

## Evidence bar

* **`field_composed` NONZERO**, with the mechanism **measured, not asserted.**
* **The six clauses, each answered separately**, in the owner's own terms.
* **Clause 3 is the one this console has failed twice**: an anti-vacuity check
  that passed on a **constant `0xFF`**, and `mat_cells` **counting cells rather
  than values**. Pick a value that **cannot** coincide with the authored
  baseline, and say why it cannot.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  `lane_desync_o` is a live example in this very block: it exists, it reads zero,
  and it **structurally cannot see** the class of fault actually present.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, reading **python's own exit code**,
  not a pipeline's.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **EVERY NEW SWITCH NEEDS A TAG** in `run_console_core_smoke.ps1`'s chain, or
  two forms silently share an object directory and one gate runs the other's
  binary. That file says so about itself, twice, because it has happened.
* **A control form builds its OWN model** — `-SkipVerilate` is wrong for it — and
  **a switch passed as a quoted string becomes POSITIONAL**; splat a hashtable.
* **The console smoke verilates into a TEMP directory**, so a process filter on
  your worktree path will not find your own build.
* **Diagnostics must print BEFORE the assertion they diagnose** — FIELDACTIVE
  caught that in its own code.
* **RUN GATE 31** if you touch a port; **an unconnected output is `.port_o ()`**,
  never an omission. A new core port costs **four** things: the port, **BOTH**
  wrapper mutants, the bench's wire, and a **reader**.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **Stop arming `until grep` poll loops.** Eleven waiting constructs were alive
  across two packets today. Use the task mechanism, or one watcher plus a direct
  process read, and **read CPU as a RATE** — age and CPU together.

## Deliverable

1. **`field_composed` nonzero**, and the noprog mechanism **measured**.
2. **`TERRAIN.COMPOSED_MATERIAL` built**, or the measured conflict that stops it.
3. **The six clauses, answered separately.**
4. **The cost, measured** — targeted local measurement only.
5. **The register, measured BARE.** **Do not touch `I34`'s prose.**
6. **Every claim in this brief or the findings you found FALSE.** FIELDACTIVE
   found **five in mine, four load-bearing.** Assume the same here and hunt them.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **If this is more than one packet, land what you have and DECLARE the rest.**
   Declared is fine; **trimmed quietly is not.**
9. Branch and commit hash. **Push `gz/noprog` only.** Never `--force`.
