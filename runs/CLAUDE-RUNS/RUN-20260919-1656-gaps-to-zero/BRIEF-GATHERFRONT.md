# GATHERFRONT — the gate for I34, and there is a cheap win before the new block

**Branch `gz/gatherfront`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `reports/DECISION-20260926-I34-EARTHMAJOR-FRONT-FIRST.md` FIRST, in
full.** It reordered this entry's prerequisites on a measurement and named your
packet. **`I34` is one of THREE remaining register entries.**

## What EARTHMAJOR measured, so you do not re-derive it

**The field-major transpose ALONE IS A WASH.** At the same measured L,
vertex-major is **71,949** and field-major **68,369–74,507** — 5% better to 4%
worse. **The 297-vs-1,089 slope is a property of a GROUP-WIDE FRONT, not of the
stream order**, because with a front that answers one point per run **both forms
pay one round trip per covered vertex.**

**The cell that decides it, measured through the real front:**

| | clocks/group | association | vs 6,000 |
|---|---:|---:|---:|
| **console as composed today** | **248** | **74,507** | **12.42×** |
| per-point clear skipped | 120 | 36,491 | 6.08× |
| target | **≤ 17.3** | ≤ 6,000 | 1.00× |

**A group must cost ≤ 17.3 clocks. It costs 248.**

**Engine overlap is 1.00**, measured from a 62.00-clock run latency against a
62.00-clock accept-to-accept interval, **by different events**, with `StOk` and
`out[0] == 42` proving real work rather than a stall counter reading zero.

**And the per-point decomposition nobody had:**
**`E_ZERO` = 32 clocks — 52% of a run.** **`E_WRITE` = `IN_LANES` = 15**, half of
what remains, on a `fab_pre_data` port **already `FAB_LANES*32` wide.**

## STEP 1 IS CHEAP AND NEEDS NO NEW BLOCK

**`E_ZERO`'s 32 clocks are skippable TODAY under FH08's `INIT_PROOF`.** That is
**52% of every run**, and EARTHMAJOR measured it: **248 → 120 clocks/group,
12.42× → 6.08×.** No new block, no subsystem swap.

**Take it first, measure it, commit it.** It is the cheapest clock in this
entry's history and it has been sitting there.

### THE TRAP IN IT, which EARTHMAJOR hit and caught

**Its first attempt measured REFUSALS and called it a 62× speed-up.** It loaded
`INIT_PROOF` **after** the header, and `LdInitProof` sets
`hdr_loaded[slot] <= 1'b0` (`zhao_field_host.sv:1472`), so **every run answered
`StNoProgram` in one clock.**

**The run-count check PASSED. Only the status check caught it.** So: **assert
`StOk` and assert the output VALUE**, not merely that runs completed. A fast
number from a machine that refused everything is the flattering direction.

## STEP 2 IS THE GATE, AND IT IS A NEW BLOCK

**Prerequisite (5) is NOT a parameter.** My earlier brief and the previous
decision record both called the executor's width a `FAB_LANES` change.
**EARTHMAJOR measured that `FAB_LANES` REPLICATES and adds no evaluation
through this front**, and `zhao_field_host.sv`'s own header calls the needed
thing **"not built"**.

**The gathering front is worth ÷4** — it is what turns one round trip per
covered *vertex* into one per *group of four*, which is the entire 297-vs-1,089
difference.

**The route to the contract, declared by EARTHMAJOR as arithmetic on
measurements and NOT benched:** front (÷4) + `INIT_PROOF` (−32/pt) + two runs
outstanding (÷2) ≈ **15 clk/group ≈ 5,300 per association.** **The first
arrangement any measurement here has put under 6,000.** **Bench it; do not
inherit it.**

## The fences

* **DO NOT REGRESS VELOCITY.** It reaches `zhao_part_collide` today
  (`terrain_veljoin_directed` 19/0, `part_terrain_tap_directed` 1297/0).
* **Do not compose a prefix.** If a block goes in, its consumer goes in with it.
* **Do not modify `zhao_field_host_v2`'s four composed clients casually** —
  EARTHMAJOR refused that as a subsystem swap and it was right.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP.** A **fourth Quartus
  17.0.2 inference killer**, measured this week: it cost `zhao_geom_arenabin`
  **146,414 registers against 1,010** for the identical circuit. Generate-**IF**
  infers; module scope infers; **the LOOP is the killer.**
  `check_ram_inference.py` rule 6 catches it now — and note the checker was
  **100% false alarms and 100% miss** on that file before, so **its silence is
  not a verdict.**
* **Preserve the numerical policy** and the exact height/underside clamp rules.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, and **read `rtlCleanAtHead` first.**

## Evidence bar

* **Clocks per group, measured, at the console's actual configuration** — and
  the association figure against **≤ 6,000**. Not at the engine's gated depth
  unless you raise it, and **if you do, say what that cost.**
* **`StOk` and the output VALUE asserted**, not run counts. See the trap above.
* **Velocity still reaching its consumer**, demonstrated.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Six packets this week found their own controls vacuous — a guard never true, a
  line number that moved, one that **asserted the bug**, a second guard that was
  **also** wrong, **a watcher that matched the smoke's own banner and reported a
  finished run while `cc1plus` was still compiling**, and **a comparison taken
  at two different engine prices.**
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — and note
  **`| tail` reports TAIL's exit code**, which EARTHMAJOR hit on its first
  baseline. That trap applies to **every gate**, not just builds.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **Gate 31 deduplicates its source list** while `run_console_board_lint.ps1`
  does not, so a duplicated line in `fit_targets.yml` makes it say **OK twice**.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants** — a missed one turned `wrapper_port_parity` red this week, a failure
  that **reads like a broken core**.
* **Regenerating a generated file is part of the change** — four went stale this
  week, two on merges.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED.**
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree**, and other lanes' build trees are
  live on this box — classify by command line and parent PID, touch none.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **`INIT_PROOF` landed and measured**, with `StOk` and the value asserted.
3. **The gathering front**: built, or costed with what it needs.
4. **Clocks per group and per association at the console's actual
   configuration**, against ≤ 6,000.
5. **What happened to velocity's chain.**
6. **Every claim in this brief or the record you found FALSE.** Every packet
   this week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/gatherfront` only.** Never `--force`.
