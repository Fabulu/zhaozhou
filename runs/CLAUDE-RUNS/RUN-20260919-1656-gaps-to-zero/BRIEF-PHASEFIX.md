# PHASEFIX — I55 stops on ONE LINE, and it is a phase fault, not a datapath one

**Branch `gz/phasefix`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-metaside.md`
FIRST, in full.** It carried I55's last 378 bits **and then proved that the thing
everyone believed was blocking the frame is not.**

## THE LINE

```systemverilog
// fpga/rtl/common/zhao_shell_top_v2.sv:1653
assign rpx_ready = !post_phase_w && fbw_px_ready;
```

**Once the post phase opens, the framebuffer is muxed away from the raster.** The
walk runs *after* the frame protocol has treated geometry as finished — which is
by design, it is the drain window the whole multiplex depends on — and in that
window **the raster has nowhere to put a pixel.**

**That is a scheduling fault. The datapath is done.**

## WHAT IS ALREADY BUILT — DO NOT REBUILD ANY OF IT

* **The 378 bits are carried.** `TriangleDescriptor v3`, 48 bytes, third sixteen
  holding a **128-bit MATSTATE**. It is a **versioned extension, not a sidecar,
  and it has NO KEY** — the state is *in* the record, same offset, same burst —
  so §4's eviction-and-reuse proof is **structural, not argued**.
* **Both paths go through ONE layout.** The live path composes through the same
  unpack functions, so a field missing from the layout **cannot reach the binner
  on either path**.
* `geom_paramarena_directed` **647/0**, and its decisive check is an
  **inequality** — triangle N's state is not N−1's. Mutant
  `geom_paramwalk_holdstate_fires` **638/0 inverted**, with substitution observed
  **and `survived == 0`** proving the seam engaged.
* **The time multiplex ran in the console**: `GEOM.SETUP took 89 = 75 live + 14
  walk`, 14 being exactly `tris_emitted_o`.
* Leaf map at v3: **2,756 registers, 0 DSP, 0 block memory** — a **baseline**,
  the block had never been mapped.

## THE PREMISE THAT WAS FALSE, SO YOU DO NOT INHERIT IT

My previous brief said the frame stops because `frags[covered/blended] =
[149 0]` — *"fragments produced, none blend."* **It does not. A PASSING frame
reads `[1216 0]`.** **Zero blended is what SUCCESS looks like on this console**,
and my evidence bar demanded a number a correct console reads as zero.

**Entry `I55`'s account of where the frame stops was wrong in every clause.**
METASIDE rewrote it. **Read the rewritten entry, not your memory of it.**

## YOUR JOB

1. **Make the walk's pixels land.** Either the walk runs inside a window where
   the framebuffer still belongs to the raster, or the post phase opens later, or
   the raster's claim survives the phase change. **Which of those is right is
   yours to determine and it is a BUILD, not an escalation** — §4 and §7 both
   cover it.
2. **Then un-park.** `GEOM_WALK_RASTER` ships at **0**. **Do not set it to 1
   until the console DRAWS at 1** — and when it does, the entry closes on §4's
   own test: **a pixel whose bytes went through SDRAM.**
3. **Fire the three round-trip assertions.** METASIDE named this gap honestly
   rather than glossing it: they were **silent through 89 triangles and have NOT
   been seen to fire.** *A detector that has not been shown to fire has not been
   tested.* Fire them deliberately, or give them a committed mutant.

## The fences

* **Do NOT retire the binner's drain.** At `GEOM_WALK_RASTER = 0` **it is the
  thing drawing the picture**, and §7 requires it as the complete oracle.
* **Do not un-park to make a counter move.** A parked entry reported open is
  honest; a parked entry reported closed makes the register lie.
* **Do not regress the shipped console**: `raster pixels=2816`,
  `frames_admitted=1`, `texture fragments=1216`, `tile[max/or]=[6 7]`,
  `SMOKE: PASS`. **The parked state is ASSERTED** — at 0 it must be a structural
  zero, at 1 the sweep must run and complete. Keep both.
* **Do not regress**: `geom_paramarena_directed` 647/0; `geom_tidq_directed` 83
  with its **21 base-RTL failures still failing**; `geom_tilewalk_directed` 47/0;
  `geom_bin_pipe_v2_door` 11,381 with its shut-door control at 4,084.
* **`zhao_shell_top_v2.sv` is the SHELL.** A concurrent packet (`FIELDACTIVE`)
  owns **TERRAIN, FIELD and the console smoke's field stimulus**.
  `tests/prod/tb_zhao_console_core_smoke.sv` and
  `fpga/rtl/prod/zhao_console_core.sv` are **SHARED — stage the HUNK, not the
  file.** `git add <file>` stages work you did not write; `git checkout --`
  discards theirs **with no reflog**.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010**. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.**
* **No console or full-device fit.** Leaf `-MapOnly` only, device named,
  `rtlCleanAtHead` read before quoting.

## A GENERATOR BUG THAT WILL BITE THE FIT — REPORT, DO NOT FIX

METASIDE found that **`gen_prod_top.py` SILENTLY DROPS blocks whose port width
reaches a package constant — 89 instances became 86 — and its `--check` still
reports FRESH.** That is a generated file lying with a reassuring provenance line,
and it matters because the production top is what the console fit elaborates.

**I am taking that one myself.** If you see it interact with your change, say so;
do not fix it.

## Evidence bar

* **A PIXEL WHOSE BYTES WENT THROUGH SDRAM, at `GEOM_WALK_RASTER = 1`.** §4's own
  test and the only thing that closes this entry.
* **The shipped console unregressed at 0**, both asserted states intact.
* **The three round-trip assertions SEEN TO FIRE**, or a committed mutant that
  makes them evidence.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  In one week this campaign found: a positive control that **silently became a
  no-op** when arithmetic moved one file away; a test printing a **hardcoded
  `0 failures`**; an anti-vacuity check passing on a **constant `0xFF`**; a
  counter **counting cells, not values**; an assertion whose own comment admitted
  **it could not fail**; and — this packet's own lesson — **an evidence bar
  demanding a number that a correct console reads as zero.**
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE**, and
  `RC=$?` after a pipeline reports the pipeline's last stage.
* **The console smoke verilates into a TEMP directory** — a process filter on
  your worktree path will not find your own build.
* **A control form builds its OWN model**, so `-SkipVerilate` is wrong for it,
  and **a switch passed as a quoted string becomes POSITIONAL** — splat.
* **RUN GATE 31** (`check_console_closure_lint.py`) if you touch a port. **An
  unconnected output is `.port_o ()`, never an omission.**
* **A new core port costs FOUR things**: the port, **BOTH** `.*` wrapper mutants
  (R220 — nine went missing on one merge), the bench's wire, and a **reader**.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`, and
  beware heredoc traps; METASIDE hit four.
* **Other repositories' builds run on this machine** — classify by command line
  and parent PID, kill nothing you did not start, **read CPU as a RATE**.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The phase fix** — which of the three shapes you chose and why.
3. **A pixel whose bytes went through SDRAM at arrangement 1**, or the
   measurement that says why not.
4. **The three round-trip assertions fired.**
5. **The shipped console unregressed at 0**, both asserted states intact.
6. **A leaf `-MapOnly` row** for what you added, device named.
7. **Every claim in this brief or the records you found FALSE.** The last five
   packets each found several in what they were handed — **including this brief's
   predecessor, whose central premise was simply wrong.** Assume the same here.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/phasefix` only.** Never `--force`.
