# FIELDMAJOR — I34's remaining half is a CLOCK COUNT, and the floor is 74% of the budget

**Branch `gz/fieldmajor`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `reports/DECISION-20260926-I34-PATCH-V2-CHANNELS.md` FIRST, in full.** It
is a decision record taken under the owner's delegation, it **retires the
four-channel prescription** this entry carried, and it scoped your packet so you
would not have to re-derive it. **Carry its comparison; do not rebuild it.**

## What was decided, and what is left

**`zhao_terrain_patch_v2` does not own four channels.** Measured, not inherited:

* **NAV is out by owner decision** — the CPU owns it, and the service ships.
* **VELOCITY is already owned, and NOT by any patch_v2** —
  `u_terrain_veljoin` → … → `zhao_part_collide`, composed by TERRVEL **three
  days after directive 13.2 was written**.
* **MATERIAL is the only channel with no owner**, and its blocker is an
  encoding, not a port. **I13CLOSE is on that seam — do not race it.**

**So what survives is §13.1 — the stream order — and it is a PERFORMANCE
commission.** That is your packet.

## THE MEASUREMENT THAT COMMISSIONS YOU

PATCHV2 measured the composed vertex-major Earth path through four production
blocks, as a **line, not a point**:

```
    clocks(L) = 4,431 + 1,089 * L        (slope asserted)
```

At the real engine price: **91,551 clocks.**

**And the allowance everyone quoted is retired.** Entry `I34`, two briefs and
the adapter's own header all say **10,416**; the owner's directive supersedes it
twice and `design/contracts/FIELD.SEQ.EARTH.md:167` is live at **≤ 6,000**.

**So the composed path is 15.3× its contract — and over it even with an engine
27× faster than real.** Neither allowance is reachable, so **the verdict does
not turn on which one you use.**

## WHY PIPELINING IS NOT THE ANSWER — arithmetic, carried so you can check it

Because the census is a **line**, it prices the alternative. Deepening
`zhao_field_host`'s front from one point in flight to `P` gives
`clocks(L,P) ≈ 4,431 + 1,089·L/P`.

* **The intercept is 4,431 clocks — 4.07 clocks per vertex** of work that is
  *not* engine latency: the per-vertex walk, the adapter's two clocks, the
  consumer's accept.
* **That floor alone is 74% of the 6,000-clock contract**, leaving 1,569 clocks
  for everything else.
* At the real `L ≈ 80`, pipelining alone needs **P ≥ ~56 points in flight** —
  and would still sit at 74% of budget **before the first field is evaluated**,
  with no margin for a second covering field.
* **The field-major walk is 273 INIT + 297 UPDATE + 273 DRAIN ≈ 843 clocks**
  (`FIELD.SEQ.EARTH.md:137-154` — and note it is **297** update groups, not 273)
  plus the executor's vector work. **It attacks the INTERCEPT, not just the
  slope**, because it stops paying a per-vertex round trip at all.

**PATCHV2 flagged everything after its two measured constants as arithmetic that
has NOT been built or benched. So BENCH IT.** If the field-major form does not
actually remove the floor, that is a finding and it is worth as much as a build.

## The blocks already exist

**`zhao_terrain_field_walk` and `zhao_terrain_patch_acc` are BUILT and both
`pending_compose`.** The entry's own words: those responsibilities *"are already
built and must be INSTANTIATED, not rewritten."* **Grep before you write
anything** — every packet this week that assumed absence found the thing
present, and two found it present under a different name.

## A COST NOTHING RECORDED UNTIL PATCHV2 FOUND IT

**`zhao_terrain_veljoin` rides the VERTEX-MAJOR lane stream and takes
`fld_covers_o`.** So composing a field-major v2 **also owes re-homing a chain
that is composed, tested and reaching a particle contact TODAY.**

**That is a working path you can break.** §13.1's *"do not regress a composed
consumer"* applies. Plan for it in the same change or declare it and stop.

## The fences

* **Do not regress velocity.** It reaches `zhao_part_collide` today, verified,
  over fabric with no SDRAM.
* **Do not build a frame-wide field-by-vertex matrix**, and **do not make
  another frame-sized flip-flop store.** Both are the directive's words — and
  ARENACOMPOSE measured today what the second one costs: **146,414 registers
  for one block, 87% of the shipping part's register budget**, because a
  145,152-bit array declared inside a generate went entirely to flip-flops.
  **That is the live example; do not repeat it.**
* **Preserve the numerical policy**: command order, saturating operations,
  material an **opaque full-width u32**, presence travelling with the result —
  **an absent optional output is NO WRITE, not a write of zero.**
* **Preserve the exact initial/final height and underside clamp rules**, the
  closed footprint test and dirty-border attribution. **Live Earth fields affect
  the TOP lattice; do not silently deform or recolour the underside.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting any row.**

## Evidence bar

* **The clock count re-measured on the composed path**, as a **line** — PATCHV2's
  single-point figure would have read **7,698** and sat *under* the retired
  allowance, which it caught itself nearly reporting as "the cost".
* **A real TerrainField issued.** With no field issued the list is empty,
  `fields_active_o` is 0 and the lane is never raised, so **the counter reads
  zero for a reason that has nothing to do with the cost.** That is the
  broken-instrument shape and the entry has fallen into it before.
* **Velocity still reaching its consumer**, demonstrated after the change.
* **Prove every counter you quote**, and **check any control you add CAN FAIL** —
  two packets today found their own controls vacuous, one because a guard was
  never true and one because a line number moved underneath it.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug** — I13CLOSE caught its own gate
  doing exactly that today, found by a control on the next run.
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's** — a packet read `tail`'s exit code for the register's today.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED** — a packet produced a row
  whose device was a file path. Check the row you just wrote.
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants** — one was missed today and `wrapper_port_parity` went red, a failure
  that reads like a broken core.
* **Regenerating a generated file is part of the change** — three have gone
  stale this week, one on a merge today.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_ram_inference.py` has a measured blind spot** — it did not see the
  array that failed to infer in `zhao_geom_arenabin`. Do not treat its silence
  as a verdict.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **The clock count on the composed field-major path**, as a line, against the
   **≤ 6,000** contract.
3. **Whether the intercept actually fell**, which is the claim the whole
   commission rests on.
4. **What happened to velocity's chain.**
5. **What you composed vs. what you found already built.**
6. **Every claim in this brief or the decision record you found FALSE.** Every
   packet this week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/fieldmajor` only.** Never `--force`.
