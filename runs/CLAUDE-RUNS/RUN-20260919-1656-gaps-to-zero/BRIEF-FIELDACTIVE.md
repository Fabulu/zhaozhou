# FIELDACTIVE — the composed console has NEVER RUN A FIELD. Make it run one.

**Branch `gz/fieldactive`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-I34CLOSE.md`
FIRST, in full.** It is the enumeration this packet acts on, and it found two
obligations six earlier packets never listed.

## THE ONE NUMBER THAT MATTERS

```
SMOKE: terrmat  field_composed=0
```

**The composed console has never once run a field.**

Entry `I34` is *about* the field-height lane. All four Earth channels — height,
velocity, material, nav — are **proven, in benches**: composepub 154/0, veljoin
19 rc0 on the repaired test, `part_terrain_tap` 1297/0, matjoin 50,
`clipfeed_mat` 41, `nav_service` 113/0, `wizards_nav` 77/0.

**Not one of those proves the path IN THE CONSOLE, because the console has never
exercised one.** A bench proving a chain and a console never running it is
exactly the shape this campaign keeps paying for: every component check passes
and the assembled thing has never done the job.

**Directive §13.7 commissions `-FieldActive`. It has SIX grep hits and EVERY ONE
IS PROSE.** Nothing in the tree implements it.

## YOUR JOB

**Make the composed console issue a real TerrainField and run it**, and prove the
four channels end to end **in the console**, not in a bench.

`zhao_cmd_exec`'s TerrainField 0x0200 arm exists and `zhao_terrain_fieldlist`
consumes it — FIELDARM composed that path. **What is missing is a console
stimulus that actually lowers one**, and the evidence that it reaches each
consumer.

**The shape is a console smoke control form**, as `-TerrainFlatLattice`,
`-Mutant` and `-BadVertex` are. Follow their pattern: a declared polarity, a
stated invariant, and **an assertion that can FAIL.**

## WHAT THIS PACKET IS **NOT**

**`TERRAIN.COMPOSED_MATERIAL` IS NOT YOURS.** I34CLOSE found it unbuilt, the
owner re-affirmed it live on 2026-09-26 **in the same breath as striking
`COMPOSED_NAV`**, and whether it is built or refused is **an owner decision that
is currently open.** Do not build it, do not refuse it, do not route around it.
**If your field run needs it, STOP and say so** — that is a finding, and it would
mean the two obligations are not separable after all.

**This packet is deliberately the half that the open decision does not gate.**
The console must run a field under **either** outcome.

## The fences

* **You own TERRAIN and FIELD and the console smoke's field stimulus.**
  `METASIDE` owns GEOM, the texture sidecar and `TriangleDescriptor` —
  **touch none of it.**
* **`tests/prod/tb_zhao_console_core_smoke.sv` and
  `fpga/rtl/prod/zhao_console_core.sv` are SHARED.** **Stage the HUNK, not the
  file** — `git add <file>` stages work you did not write, and `git checkout --`
  discards another packet's uncommitted work **with no reflog**.
* **DO NOT REGRESS THE BASELINE.** The plain smoke is `raster pixels=2816`,
  `frames_admitted=1`, `texture fragments=1216`, `tile[max/or]=[6 7]`. A new form
  must not move the plain form's numbers.
* **DO NOT REGRESS VELOCITY** — `terrain_veljoin_directed` 19/0 **on the repaired
  test**, which used to print a hardcoded `0 failures` and could not go red.
* **Do not raise `FAB_LANES`, `FAB_GROUP_PTS` or `FRONT_PTS`** — priced at
  **+11,979 ALUTs and +9 DSP** and refused. **If running a field is slow, that is
  a PERFORMANCE finding**, and owner authorization 4 says separate functional
  completion from performance qualification. **Report the clocks; do not buy
  them.**
* **Do not build a `zhao_terrain_patch_v2`.**
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010**. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.** Remedy: one flat array at a module's own scope (`zhao_dc_sdp_ram`).
* **No console or full-device fit.** Leaf `-MapOnly` only, device named.

## Evidence bar

* **`field_composed` NONZERO in the console smoke**, and the field's effect
  observed at **each consumer** — height into `u_terrain_patch`, velocity
  reaching `zhao_part_collide`, material at the mosaic, nav classified.
* **A value that could not have come from the authored lattice.** The whole point
  is that a *field* changed something. **If the run would produce identical
  output with no field issued, it proves nothing** — that is the anti-vacuity
  failure this console has already committed twice: an anti-vacuity check that
  passed on a **constant `0xFF`** because the smoke never authored the page, and
  `mat_cells` **counting cells rather than values**.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  In one week this campaign found a test printing a **hardcoded `0 failures`**, a
  positive control that **silently became a no-op** when arithmetic moved one
  file away, and an assertion whose own comment admitted **it could not fail**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE.** **It will almost certainly still read
  2** — `I34` also owes `COMPOSED_MATERIAL`, which is not yours. **Do not edit
  entry prose to move it.** The register classifies `I34` by regex-matching
  `BOUNDARY` in that entry, and that word is already known to be stale; **moving
  the number by rewording is the renamed gap the owner forbade.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **A control form builds its OWN model — `-SkipVerilate` is wrong for it**, and
  **a switch passed as a quoted string becomes POSITIONAL**; splat a hashtable.
  Both cost me a run today.
* **The console smoke verilates into a TEMP directory**, so a process filter on
  your worktree path will not find your own build. That mistake is mine, twice.
* **RUN GATE 31** (`check_console_closure_lint.py`) if you touch a port. **An
  unconnected output is `.port_o ()`, never an omission.**
* **A new core port costs FOUR things**: the port, **BOTH** `.*` wrapper mutants
  (R220 — nine ports went missing on the last merge), the bench's wire, and a
  **reader**.
* **`UNUSEDSIGNAL` is waived across whole directories** — count readers by hand.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **Other repositories' builds run on this machine** — classify by command line
  and parent PID, kill nothing you did not start, **read CPU as a RATE**.

## Deliverable

1. **`field_composed` nonzero in the composed console**, with the stimulus named.
2. **Each of the four channels observed in the console**, not in a bench.
3. **The anti-vacuity proof** — what would differ if no field were issued.
4. **The register, measured BARE.** State plainly that `I34` is still open on
   `COMPOSED_MATERIAL`, and **do not touch its prose.**
5. **The clocks a field costs**, reported not bought.
6. **Every claim in this brief or the findings you found FALSE.** The last four
   packets each found five in what they were handed; the briefs are mine.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **Whether `COMPOSED_MATERIAL` turned out to be inseparable from this work.**
9. Branch and commit hash. **Push `gz/fieldactive` only.** Never `--force`.
