# MUXBUILD — I55's swap. The architecture is chosen and the blocker is GONE.

**Branch `gz/muxbuild`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES**, it has been refused **six**
times, and **every refusal's stated reason is now discharged.** Read, in full
and first:

1. `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` — SWAPBUILD chose the
   architecture: **a TIME MULTIPLEX, not a second instance.**
2. `reports/DECISION-20260927-PROJECTEDVERTEX-V2.md` — PVSCHEMA removed the
   blocker beneath it.
3. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-swapbuild.md` and
   `FINDINGS-pvschema.md`.
4. Entry `I55` (`grep -n '^// I55\.'`, never a line number).

**You are the build. Do not re-litigate either decision.**

## WHAT CHANGED UNDER YOU, AND IT IS EVERYTHING

**The premise five packets quoted was FALSE and is now TRUE.** The entry said
the ProjectedVertex *"carries what those need … so the planes are
RECOMPUTABLE"*. It did not — `zhao_geom_vertid.sv` stored colour through
`unit8_of_fx16`, `(v+128)>>8` clamped both ends, **so the Gouraud planes could
not be rebuilt by ANY back end.**

**PVSCHEMA fixed the record, not the wording.** Schema v2, declared **once** in
`zhao_pkg` as field offsets and widths (it had been **three hand-maintained
copies**):

```
x s21 | y s21 | invw u24 | status u8 | u/w s32 | v/w s32
      | r s32 | g s32 | b s32 | alpha s22        = 256 bits = 32 bytes
```

into stride slack **already allocated** — no address space, no region change.
**Declared cost: vertex writes go 3 → 4 beats, +33% SDRAM write traffic on that
arm.** Alpha is **s22 by measurement**: it is the only field with no plane
consumer (attrpack waives slot 6 by name), so all six plane inputs are stored at
full width **with no domain claim that could be wrong**.

**So the planes are genuinely recomputable now.** That is your foundation.

## THE ARCHITECTURE, DECIDED — DO NOT BUILD A SECOND INSTANCE

**Priced and refused:** `zhao_geom_attrpack` **had zero rows in either synthesis
database** — the block five packets demanded had **never been mapped**. With
`zhao_geom_setup`'s clean row a second instance is **+1,621 ALUT and +40 DSP:
35.7% of the device's entire multiplier budget**, and **4.4× the bill LANESCOST
refused for I34 this week.**

**Why the multiplex is available**, from the binner's own FSM rather than from a
schedule argument:

* `zhao_geom_binner_v2.sv:818` — `assign tri_ready_o = (state == S_IDLE) &&
  !drain_req_r;`
* `:931`, unconditional, outside the case — `if (frame_end_i) drain_req_r <= 1'b1;`
* `:941` — **`drain_req_r` is tested BEFORE `tri_valid_i`.**
* **Bin states are 0..5, drain states 6..11: disjoint, and ONE FSM owns both.**

**So `zhao_geom_setup` and `zhao_geom_attrpack` are provably idle for the whole
raster drain**, and neither can be corrupted by a second source because **neither
carries state across a triangle boundary** — measured: `grep -c frame
zhao_geom_setup.sv` → **0**; attrpack's three `frame` hits are **all comment
text**; its state is strictly per-triangle and returns to the lane-0 idle state
when a packet retires.

## THE CONSTRAINT THAT WILL BITE, AND IT IS NOT OPTIONAL

**The handover says one SDRAM share "carries two". IT CARRIES TEN, AND NEITHER
SHARE HAS A FREE SLOT.** SWAPBUILD found this and **refused to widen either**.

**You are feeding a back end from SDRAM. Confront this BEFORE you design the
feed.** If it needs a slot that does not exist, **that is a finding and a
refusal with a number** — a full deliverable — not something to route around.

**And `spec/memory_rules.md` contradicts itself about 22 MiB.** Do not resolve
it by picking the convenient half.

## What §4 binds you to

* **A parallel legacy on-chip frame arena that still supplies the actual pixels
  is NOT closure.** Every pixel still comes from the on-chip drain today.
* **Namespaces stay distinct** — pre-clip mesh IDs, transient replay slots,
  post-clip vertex IDs, triangle IDs. **Never pass one off as another.**
* **Identity is structural**; a shared vertex must not gain a new ID because a
  cache evicted it. **State the mapping lifetime and PROVE eviction cannot
  change a still-referenced identity.**
* **Carry clipping lineage explicitly**, collision-safe — *"neither a hash nor a
  CRC alone proves equality."*
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.**
* **R7's 65,536-vertex capacity is retained**, count ≥17 bits.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame.

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and **five** packets have refused it.
* **Do not fake `paramwalk`** — still `dirs=0 chunks=0 tris=0`, refused **five**
  times. A sequencer with every `t_*` dangling counts triangles and drops them.
* **Do not build any part standalone.** A module nothing instantiates needs a
  `pending_compose` disposition, converting a clean **`BUILT BUT NOT CONNECTED:
  0`** into a declared deferral.
* **Do not regress the id repair.** `geom_tidq_directed` is 83 checks; it must
  still pass, and its **21 base-RTL failures must still fail**.
* **Do not touch `zhao_geom_setup`'s `tri_src_id` width.** A separate decision
  (`reports/DECISION-20260927-I34-MATERIAL-CARRIER.md`) widens that rider for
  `I34`, and **that packet runs after you.** If your multiplex makes the
  widening harder, **say so in your findings** — do not pre-empt it.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — it cost
  `zhao_geom_arenabin` **146,414 registers against 1,010**. Generate-**IF**
  infers; module scope infers; **the LOOP is the killer.** The correct remedy is
  one flat array **at a module's own scope**, its own module instantiated inside
  the loop — `zhao_dc_sdp_ram` is that module. **Rule 6 is NOT complete**:
  SWAPBUILD found a live counterexample in the binner and the cause is not
  established.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## TWO WARNINGS ABOUT INSTRUMENTS, BOTH EARNED THIS WEEK

* **A positive control can silently become a no-op when arithmetic moves one
  file away.** PVSCHEMA found the align mutant had exactly that: its mutation
  `PV_SLOT_B = PV_B` created misalignment only because `PV_B` was 24; at v2's 32
  it installs a **perfectly aligned layout** and `burst_unaligned_o` **cannot
  move**. Provenance perfect, drift green **and correct to be**, mutation
  intact. **If you change a width, re-fire every control that depends on it.**
* **A test that cannot fail reads exactly like a passing test.**
  `terrain_veljoin_directed` printed a hardcoded `0 failures` and `exit_hard(0)`
  — and its "19/0" was quoted as a fence in three briefs, mine. **Before you
  cite a number, confirm the thing producing it can go red.**

## Evidence bar

* **A pixel that depends on bytes that went through SDRAM** — §4's own test, and
  the one that closes the entry. Today `paramwalk dirs=0 chunks=0 tris=0` sits
  beside `raster pixels=2816`.
* **The colour arriving through the arena at full precision** — v2's whole
  point. A value **not** recoverable under the old 8-bit store.
* **The multiplex proven not to collide**: the drain-window idleness exercised,
  not argued, including the boundary where a triangle arrives as `frame_end_i`
  pulses.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Eleven packets this week found their own controls vacuous.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES. **The paramarena mutants are regenerated by a committed
  COMMAND** — `tools/rtl/gen_paramarena_mutants.py` — not by a three-way merge.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **`UNUSEDSIGNAL` is waived across whole directories** by
  `tests/shell/v3_closure_inherited.vlt` — **a dead wire you create raises
  nothing.** Count readers by hand.
* **An intentionally unconnected output must be an EXPLICIT EMPTY CONNECTION**,
  `.port_o ()`, never omitted — an omission is a `PINMISSING` and gate 31
  refuses it. This turned gate 31 red on the PVSCHEMA merge and I repaired it.
* **GATE 31: `check_console_closure_lint.py`** — run after any port change. **It
  deduplicates its source list** while `run_console_board_lint.ps1` does not.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper mutants.**
* **A struct-layout change means recompiling every `.cpp` that uses it.**
* **Regenerating a generated file is part of the change** — seven went stale
  this week, and one was a report handing out a known-fatal remedy.
* **`git show` hands back INDEX content at LF while working copies vary** —
  **`zhao_console_core.sv` is LF**; normalise before believing merge conflicts.
* **Do not edit RTL while a build or a suite is reading it.**
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree.**

## LAND IT INCREMENTALLY AND DECLARE WHERE YOU STOPPED

**You are the only packet running.** No other lane is in GEOM. That is
deliberate — the next packet needs `zhao_geom_setup` too, and two packets in one
file is the hazard this campaign has already paid for.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The multiplex**: how the second source is admitted, and the proof that the
   drain window is genuinely exclusive at its boundary.
3. **The SDRAM slot question, answered** — a slot, or a refusal with a number.
4. **Whether `paramwalk` moved off zero, and whether that was REAL.**
5. **A pixel whose bytes went through SDRAM**, or the measurement that says why
   not.
6. **That the id repair and schema v2 both still hold**, with their controls
   re-fired.
7. **Every claim in this brief or the records you found FALSE.** Every packet
   this week found at least one; most were mine.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/muxbuild` only.** Never `--force`.
