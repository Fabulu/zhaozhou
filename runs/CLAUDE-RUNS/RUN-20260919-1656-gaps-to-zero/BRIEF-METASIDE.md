# METASIDE — I55's last 378 bits. The sidecar §4 authorises by name.

**Branch `gz/metaside`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is the last architecture item in the campaign**, and after eight packets
its open surface is **one number: 378 bits.** Read first, in full:

1. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-swapclose.md` —
   your immediate predecessor. **It built everything else and measured what is
   missing to a single line.**
2. **`reports/HANDOVER-20260919.md` §15.35** — why this gap was invisible for
   seven packets. **This is the most important thing you will read.**
3. `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md`,
   `DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md`, and entry `I55`.

## EVERYTHING ELSE IS BUILT. THIS IS THE LAST PIECE.

Landed and green before you start: **schema v2** carrying the colour at full
precision; the **fetch arm** returning every field bit-identical through real
SDRAM (550 checks); the **arena ids repaired** (`popped[k] == pushed[k]`);
the **raster door open** at **+983 ALUT / +0 DSP**; **TriangleDescriptor v2**
carrying `2A` and the scissored box with its non-circularity premise asserted
(620/0); **`zhao_geom_tilewalk`**, `t_first_o`/`t_last_o`, the **console-side
select**, and a **committed mutant firing at 3** (47/0).

**And the arrangement is PARKED at `GEOM_WALK_RASTER = 0`**, honestly and with
both states asserted — at 1 the sweep must run and complete, at 0 it must be a
**structural zero**. **Do not un-park it until it draws.**

## THE BLOCKER, MEASURED TO ONE LINE

```
frags[covered/blended] = [149 0]
```

**Fragments are produced and NONE blend, so no tile resolves** — and what they
are waiting on is **per-triangle material metadata that is not in the record and
that neither back end can compute.**

**The measurement that proves it is not a wiring problem:**

* **`zhao_geom_setup` contains ZERO references** to the flat request, the
  continuation tail or the fragment state.
* **`zhao_geom_attrpack`'s single reference is a COMMENT DISCLAIMING one of
  them.**
* They are **298 + 48 + 32 = 378 of `METAW`'s 1,877 bits**, and they arrive from
  **`zhao_material_window` on console wires**.

**So the "second setup and attrpack back end" the entry demanded for seven
packets would NOT have produced them.** The entry's own arithmetic said so —
*"437 fixed bits plus 1,440 of plane"* sits a paragraph above the sentence
attributing all 1,877 to two blocks. **Nobody subtracted.**

## YOUR JOB

**Carry those 378 bits with the triangle.** They are valid at **bin time**, when
`zhao_material_window`'s publication is live; the walk runs later, when it has
moved on. So they must be **captured per-triangle when they are valid and
replayed when the walk needs them.**

**§4 authorises the mechanism BY NAME** — *"a versioned extension or an immutable
sidecar keyed by the same identity."* **You do not need a ruling.** A sidecar
keyed by the triangle id is likely cleaner than widening TriangleDescriptor v2
again; **that choice is yours, and it is a BUILD, not a decision to escalate.**

**§4 forbids the shortcut in the same breath**, and it is the one that would pass
every gate:

> *"never silently overload a field, truncate a handle, or substitute a
> convenient zero"*

**"The material was the same for that span, so reuse the last publication" IS the
convenient zero in disguise** — true in this fixture, false in general, and
invisible to every test we have. **If you take any such assumption, it must be
enforced by a check that FAILS when it is violated**, not assumed by a comment.

## What §4 still binds you to

* **A parallel legacy on-chip path that still supplies the actual pixels is NOT
  closure.** The park at 0 is a permitted fallback under §7, **not** a close.
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.** **These 378 bits ARE
  that clause** — this is not scope you are inventing.
* **Identity is structural**; the sidecar's key must be the triangle's real
  identity, and **you must prove eviction or reuse cannot hand a triangle another
  triangle's metadata.** That failure mode has already happened once on this
  console: `u_geom_tidq` was one behind and mis-attributed **74 of 75**
  triangles with every range guard passing.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame — **not a dropped tail.**

## The fences

* **Do not un-park `GEOM_WALK_RASTER` until the console DRAWS at 1.** Parking is
  honest; a parked-but-claimed-closed entry is not.
* **Do not fake it.** `paramwalk` counters moving without pixels is what five
  packets refused.
* **Do not regress**: `geom_tidq_directed` 83 checks with its 21 base-RTL
  failures still failing; `geom_paramarena_directed` 550; TD v2's 620;
  `geom_bin_pipe_v2_door` 11,381 with its shut-door control at 4,084; the
  console smoke's `fetcharm` invariant `vread == 3 * tris`, **which is asserted
  and which fired at 45-against-42 during SWAPCLOSE's wedge** — it is a real
  instrument, treat it as one.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010**. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.** Remedy: one flat array at a module's own scope (`zhao_dc_sdp_ram`).
  **Rule 6 is not complete** — a live counterexample exists in the binner.
* **A sidecar is STATE. Put it in memory, not flip-flops** — the console needs
  **293,352 ALUTs against 227,120 present**, and memory is the slack.
* **Leaf `-MapOnly` only**, device named, `rtlCleanAtHead` read before quoting.
  **No console or full-device fit** — that one is mine and it comes after zero.

## Evidence bar

* **A PIXEL WHOSE BYTES WENT THROUGH SDRAM**, with `GEOM_WALK_RASTER = 1`. That
  is §4's own test and the only thing that closes the entry.
* **`frags[covered/blended]` with a NONZERO blended count**, and tiles resolving.
* **The sidecar's identity proven**, not argued: a test where triangle N's
  metadata cannot be substituted by triangle N−1's without failing. **Equality
  against the producer, per triangle** — the `tidq` lesson.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  This campaign has found, in one week: a positive control that **silently became
  a no-op** when arithmetic moved one file away; a test printing a **hardcoded
  `0 failures`**; an anti-vacuity check passing on a **constant `0xFF`**; a
  counter **counting cells, not values**; and an assertion whose own comment
  admitted **it could not fail**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted.
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **RUN GATE 31** (`check_console_closure_lint.py`). **An unconnected output is
  `.port_o ()`, never an omission** — that cost 31 findings on one merge.
* **A new core port costs FOUR things**: the port, **BOTH** `.*` wrapper mutants
  (R220), the smoke bench's wire, and a **reader**.
* **`UNUSEDSIGNAL` is waived across whole directories** — count readers by hand.
* **A struct-layout change means recompiling every `.cpp` that uses it.**
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **The console smoke verilates into a TEMP directory**, so a process filter on
  your worktree path will not find your own build. That mistake is mine, twice.
* **Other repositories' builds and suites run on this machine** — classify by
  command line and parent PID, kill nothing you did not start, **read CPU as a
  RATE.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The sidecar** — where it lives, how it is keyed, and why that key is safe.
3. **A pixel whose bytes went through SDRAM at `GEOM_WALK_RASTER = 1`**, with
   `frags[.../blended]` nonzero.
4. **The identity test** — triangle N's metadata not substitutable by N−1's.
5. **A leaf `-MapOnly` row** for what you added, device named.
6. **Every claim in this brief or the records you found FALSE.** Eight packets,
   and each of the last four found five in what it was handed. **Assume this one
   is wrong somewhere and find it.**
7. **What you refused, and anything you got wrong and caught yourself.**
8. **Whether the binner's drain became retirable** — and if you retire it, **BY
   REMOVAL**, never by tie-off.
9. Branch and commit hash. **Push `gz/metaside` only.** Never `--force`.
