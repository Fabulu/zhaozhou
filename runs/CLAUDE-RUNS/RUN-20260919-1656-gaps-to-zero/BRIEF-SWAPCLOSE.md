# SWAPCLOSE — I55's last three steps. Everything they depend on is built and priced.

**Branch `gz/swapclose`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES**, it has been refused **seven**
times, and **it is no longer blocked on anything unknown.** Read first, in full:

1. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-doorcost.md` — your
   immediate predecessor. **§5 names your three steps and §7.1 carries a live red.**
2. `reports/DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md` — step 1 is decided and
   costed. **Its deferral reason is now DISCHARGED.**
3. `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` — the multiplex, and the
   requirement that the select sit **upstream of the three-way fork**.
4. `runs/.../FINDINGS-muxbuild.md` and `FINDINGS-pvschema.md`.
5. Entry `I55` (`grep -n '^// I55\.'`, never a line number).

## WHAT IS ALREADY BUILT, PROVEN AND PRICED — DO NOT RE-DERIVE ANY OF IT

* **The record carries the colour.** Schema v2: `x s21 | y s21 | invw u24 |
  status u8 | u/w s32 | v/w s32 | r s32 | g s32 | b s32 | alpha s22` = 256 bits
  = 32 bytes, into stride slack already allocated. **No address space, no region
  change.**
* **The fetch arm works through real SDRAM.** `geom_paramarena_directed` **550
  checks, 0 failures**, every field of all three vertices bit-identical.
* **The ids are correct.** `u_geom_tidq` repaired; the arena agrees with the
  binner exactly. `geom_tidq_directed` is 83 checks with **21 failures against
  the base RTL**.
* **The door is OPEN and priced.** 28 bits, not 2,065 — **2,037 of those wires
  were already ports**. `src1walk` measures **+983 comb ALUT, +0 DSP** on the
  shipping part, `rtlCleanAtHead` true. **Below the smallest thing this campaign
  has ever refused, and zero on the axis that is 335% over.**
* **`JOB_SRC` selects at ELABORATION**, so the old arrangement survives as a
  complete buildable oracle (§7). **The run-time mux is refused and priced at
  2,050 comb ALUT**; both of MUXBUILD's routes needed it, so they were never
  alternatives on price.
* **The SDRAM slot question is closed both ways.** No new slot is needed (the arm
  rides `guard_req_o`, so a fourth request kind is a **state-machine** addition)
  and **none exists** — and **§7's reserved slot is structurally dead**:
  `zhao_vram_arbiter.sv:353` forces `port_grant[RESERVED_ID] = 1'b0`.
* **The three fetch-arm counters are exported and READ.** The console smoke
  asserts `vread == 3 * tris` and fatals on a refused record. **It reads
  `vread=0` today. That is your scoreboard.**

## YOUR THREE STEPS, IN DOORCOST'S OWN ORDER

1. **TriangleDescriptor v2.** Decided, costed (32 bytes, fits with 524,288
   spare), **and now unblocked** — its deferral's only stated ground was *"a
   consumer that cannot exist until the door opens"*, and the door is open.
   **It is NECESSARY, and not for size:** `zhao_geom_setup` consumes
   `tri_area2_i` and the four scissored box bounds, the 16-byte record carries
   **neither**, and **`zhao_geom_setup.sv:386` DEFINES `kc2` as `area2 - kc0 -
   kc1`** — so **the barycentric identity recovers nothing and a back end built
   on it would be "correct" for any garbage `area2`.** Do not take that escape;
   two packets have named it as circular.
2. **The console-side source select** feeding `u_geom_setup`/`u_geom_attrpack`
   from the walk during the drain window, **upstream of the three-way fork** as
   the architecture record requires. **Narrow** — corners, `area2` and the box,
   **not 2,065 bits** — and it lives in `zhao_console_core`.
3. **A sequencer on `walk_valid_i`** over the head table.

**Then, and only then, the binner's drain can be retired BY REMOVAL** (−606,592
memory bits, already measured as *available, not taken*). **That** is the
subsystem retirement — **about the binner, not the door.** If you get there, do
it by removal, never by tie-off.

## What §4 still binds you to

* **A parallel legacy on-chip frame arena that still supplies the actual pixels
  is NOT closure.** Today **no pixel comes from bytes that went through SDRAM**.
* **Namespaces stay distinct**; identity is **structural**; clipping lineage is
  explicit and collision-safe — *"neither a hash nor a CRC alone proves
  equality."*
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.** Schema amendments go by
  **versioned extension or immutable sidecar**, and **never** by silently
  overloading a field, truncating a handle or substituting a convenient zero.
* **R7's 65,536-vertex capacity retained**, count ≥17 bits.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame.

## The fences

* **Do NOT half-do it.** ORing the walk into the live stream is forbidden and
  **seven** packets have refused it; the bundle split now makes it awkward as
  well as forbidden.
* **Do not fake `paramwalk`.** A sequencer is step 3 and it must drive a **real**
  consumer; a counter moved without one is the thing five packets refused.
* **Do not regress**: `geom_tidq_directed` 83 checks with its 21 base-RTL
  failures still failing; `geom_paramarena_directected` 550;
  `geom_bin_pipe_v2_door` 11,381 with its shut-door control at 4,084;
  `terrain_veljoin_directed` 19/0 **on the repaired test**; composepub 154/0.
* **Do not touch the run-time mux probe** `fpga/rtl/synth/zhao_probe_doorcost_jobmux.sv`
  except to cite it. It is evidence, not a design.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010** for the identical circuit. Generate-**IF** infers; module scope
  infers; **the LOOP is the killer.** Remedy: one flat array **at a module's own
  scope**, its own module instantiated inside the loop (`zhao_dc_sdp_ram`).
  **Rule 6 is NOT complete** — a live counterexample exists in the binner.
* **Leaf `-MapOnly` only**, device named, `rtlCleanAtHead` read before quoting.
  **No console or full-device fit** — that one is mine and it comes after zero.

## Evidence bar

* **A PIXEL WHOSE BYTES WENT THROUGH SDRAM.** §4's own test and the thing that
  closes the entry. The smoke's `fetcharm vread=` line must move off zero **and
  its invariant `vread == 3 * tris` must hold**, because it is asserted.
* **`paramwalk dirs/chunks/tris` off 0/0/0, and REAL.**
* **The `area2` and box values proven not circular** — a test that would pass on
  garbage `area2` is not a test.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Fourteen packets this week found their own controls vacuous. In the last two
  merges alone: a positive control that had **silently become a no-op** because
  arithmetic moved one file away; a test printing a **hardcoded `0 failures`**;
  a mosaic anti-vacuity check that passed on a **constant `0xFF` and never on the
  page**; and a console assertion that **said in its own comment it could not
  fail**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE** — I hit
  that twice today.
* **RUN GATE 31** (`check_console_closure_lint.py`). The last three merges landed
  1, then 31, then 0 PINMISSING findings; the first two were packets whose gate
  list omitted it. **An intentionally unconnected output is `.port_o ()`, never
  an omission.**
* **A new core port costs FOUR things**: the port, **BOTH** `.*` wrapper mutants
  (R220 — fix the wrapper, never the module), the smoke bench's wire, and a
  **reader**.
* **`-SkipVerilate` is wrong for a control form that builds its own model** — it
  will reuse or fail to find another form's build. And **passing a switch as a
  quoted string makes it POSITIONAL**; splat a hashtable.
* **A struct-layout change means recompiling every `.cpp` that uses it.** **TD v2
  is one.**
* **`UNUSEDSIGNAL` is waived across whole directories** — a dead wire raises
  nothing; count readers by hand.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** and silently eat your
  words — use `git commit -F <file>`. I did this today and lost a sentence's
  subject.
* **`zhao_console_core.sv` is LF**; `git show` hands back INDEX content at LF.
* **Other repositories' builds and suites run on this machine.** Classify by
  command line **and parent PID**, and **kill nothing you did not start**. Read
  CPU as a **RATE** — age and CPU together — or you will diagnose a wedge that
  is a process which just spawned.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **TD v2**, with the `area2`/box values carried and proven non-circular.
3. **The console-side select**, upstream of the fork, and the sequencer.
4. **A pixel whose bytes went through SDRAM**, with `fetcharm vread` off zero and
   its invariant holding.
5. **Whether the binner's drain became retirable, and if you retired it, BY
   REMOVAL.**
6. **A leaf `-MapOnly` row** for what you added, device named.
7. **Every claim in this brief or the records you found FALSE.** Every packet
   this week found at least one, and the last three each found **five in the
   brief I handed them.** Hunt them deliberately; the briefs are the weak link.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/swapclose` only.** Never `--force`.
