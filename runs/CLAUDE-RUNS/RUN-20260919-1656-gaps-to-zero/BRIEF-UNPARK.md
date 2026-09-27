# UNPARK — I55 DRAWS. Two measurements stand between that and closure.

**Branch `gz/unpark`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**READ FIRST, IN FULL:**
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-phasefix.md`, then
`FINDINGS-metaside.md`, then entry `I55` **as currently written** — its account of
where the frame stopped has been rewritten twice and both earlier versions were
wrong.

## THE CONSOLE ALREADY DRAWS FROM SDRAM

At **`GEOM_WALK_RASTER = 1`**: **2,816 pixels** — the shipped arrangement's own
**reference-derived** count — **11 tiles resolved, EVERY BYTE THROUGH SDRAM.**
That is directive §4's own test, met.

**The evidence is six modules and six register enables, not one counter:**

```
arenabin refs=101 == binrefs 101 == tilewalk jobs=101 == door=101
         == rasterdiag started=101 == paramwalk tris=101
vread=303 == 3 x 101        setup_submitted=176 == 75 live + 101 walk
```

**And the fix is not where two briefs said it was.** The symptom was
`zhao_shell_top_v2.sv:1653`; the decision is two modules up —
`raster_done_c = raster_quiet_i && !raster_px_i && fbw_drained_i`, where
`raster_quiet_i` is the **BINNER's** quiet line. At `JOB_SRC = 1` the binner
issues **no job at all**, so it is **honestly TRUE** at `frame_end` while the
machine about to draw the frame has not started. **A correct signal asking the
wrong question.** The repair adds a term on `zhao_post_lease` enabled by a
**different producer**, so the two sides are **not in lockstep**.

## WHY IT IS STILL PARKED, AND IT IS NOT A DECISION

PHASEFIX declined to un-park for a reason that is **two measurements**:

1. **Five committed control forms assert pixel counts taken against the BINNER's
   DRAIN, and have NEVER RUN in arrangement 1.** Their numbers may be right, may
   be wrong, and nobody knows which — **and a form that passes for the wrong
   reason is worse than one that fails.**
2. **Arrangement 1 has never been fitted.**

**The un-park itself is ONE LINE with its evidence attached.**

## YOUR JOB

1. **Run all five control forms in arrangement 1.** Establish, per form, whether
   its asserted count is **correct for that arrangement**, **stale**, or
   **arrangement-dependent and needing a second expected value.** Fix what is
   stale; **do not weaken an assertion to make it pass.**
2. **Then un-park, or state precisely what still blocks it.** If the forms come
   good, `GEOM_WALK_RASTER = 1` becomes the shipped arrangement and **`I55`
   closes on §4's test, which is already met.**
3. **Do NOT fit arrangement 1.** The area question belongs to the **full console
   fit** that follows closure, and the owner has said not to spend a full fit on
   closing an entry. **Leaf `-MapOnly` is available if a specific question needs
   it; say what the question was.**

**If un-parking is right, say so and do it. If it is not, the blocker must be a
MEASUREMENT you took, not an inherited caution** — this entry has been refused
eight times and three of those refusals rested on claims that turned out false.

## The fences

* **DO NOT RETIRE THE BINNER'S DRAIN.** At arrangement 0 it is **the thing
  drawing the picture**, and **§7 requires it as the complete oracle.** Even if
  you un-park, the oracle stays.
* **Both arrangements must stay ASSERTED.** At 0 the sweep is a **structural
  zero**; at 1 it must **run and complete**. PHASEFIX proved its new gate
  **absent** rather than assumed absent — `phasehold=0 phasesweeps=0`. **Keep
  that property.**
* **Do not regress**: `geom_paramarena_directed` 647/0; `geom_tidq_directed` 83
  with its **21 base-RTL failures still failing**; `geom_tilewalk_directed` 47/0;
  `geom_bin_pipe_v2_door` 11,381 with its shut-door control at 4,084; all three
  round-trip assertions still **firing**.
* **A concurrent packet (`MATFIELD`) owns TERRAIN, FIELD and the field
  stimulus.** You own the **SHELL**, `zhao_post_lease`, the raster/phase path and
  the **walk-arrangement control forms**.
  `tests/prod/tb_zhao_console_core_smoke.sv`,
  `tests/prod/run_console_core_smoke.ps1` and
  `fpga/rtl/prod/zhao_console_core.sv` are **SHARED — stage the HUNK, not the
  file.** Those three produced **two real merge conflicts today**, both in the
  switch-tag chain.
* **EVERY NEW SWITCH NEEDS A TAG** in that chain, or two forms share an object
  directory and one gate runs the other's binary.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — 146,414 registers
  against 1,010. **The LOOP is the killer.**

## Evidence bar

* **Five forms, five verdicts**, each with its expected value **justified for
  arrangement 1** rather than carried over.
* **If you un-park: the shipped console still PASSES**, and `I55`'s closure rests
  on **a pixel whose bytes went through SDRAM** — already demonstrated, so say
  where.
* **If you do not: the measurement that stopped you.**
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Cautionary examples from this entry alone: a positive control that **silently
  became a no-op** when arithmetic moved one file away; three assertions **silent
  through 89 triangles** and never seen to fire until PHASEFIX fired them; an
  evidence bar (mine) **demanding a number a correct console reads as zero**; and
  `fld_earth_noprog_o`, which **ORs two causes** and so could never identify the
  fault it was cited for.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, reading python's own exit code. **Do
  NOT touch `I55`'s head line** — `NOT a tie-off` there is the one string that
  removes an entry from the count, and using it without the machine behind it is
  the renamed gap the owner forbade.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **A control form builds its OWN model** — `-SkipVerilate` is wrong for it — and
  **a switch passed as a quoted string becomes POSITIONAL**; splat a hashtable,
  **not through `powershell -File`.**
* **The console smoke verilates into a TEMP directory**, so a process filter on
  your worktree path will not find your own build.
* **The production console only started passing `quartus_map` TODAY** — two
  `Error (10170)` on a second header `import`, present at base and invisible to
  `check_quartus17_syntax.py`. **Do not reintroduce a second import.**
* **RUN GATE 31** if you touch a port. An unconnected output is `.port_o ()`.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **Stop arming `until grep` poll loops** — eleven accumulated today. Use the task
  mechanism, and **read CPU as a RATE.**

## Deliverable

1. **The five control forms, run in arrangement 1, with a verdict each.**
2. **Un-parked, or the measurement that stopped you.**
3. **The register before and after, measured BARE**, and whether `I55` closed.
4. **Both arrangements still asserted**, and the new gate still proven absent at 0.
5. **The shipped console unregressed** at whatever arrangement ships.
6. **Every claim in this brief or the entry you found FALSE.** **Seven
   consecutive packets found their brief wrong in a load-bearing place.** Two
   earlier briefs on THIS entry named the wrong line and the wrong blocker.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **What the full console fit will need to know about arrangement 1** — you are
   the last packet to see it closely before that fit runs.
9. Branch and commit hash. **Push `gz/unpark` only.** Never `--force`.
