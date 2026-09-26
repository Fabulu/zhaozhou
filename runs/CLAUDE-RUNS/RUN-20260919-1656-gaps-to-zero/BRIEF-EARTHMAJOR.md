# EARTHMAJOR — I34's build. The decision is taken and the number says it pays.

**Branch `gz/earthmajor`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `reports/DECISION-20260926-I34-FIELDMAJOR-BENCHED.md` FIRST, in full.**
FIELDMAJOR benched the thing this entry has argued about for a week and refused
to build it **on sequence, not on doubt**: *"This is a BUILD, the decision is
taken, and the refusal is about sequence."* **You are the sequence.**

**`I34` is one of THREE remaining register entries.** `I13` closed today.

## What is MEASURED, so you build rather than re-derive

**The field-major form meets the contract**, co-elaborated for the first time
(`tb_terrain_fieldmajor.sv`, 248 checks, 0 failures):

```
  clocks(L) = 851 + (297/depth)*L     field-major
  clocks(L) = 4,431 + 1,089*L         vertex-major, replaced   -> 91,551 at L=80
```

**The slope is 297, not 1,089 — one round trip per GROUP OF FOUR, not per
vertex.** The deciding cell, measured rather than added up: **4,102 clocks =
0.68× the 6,000-clock contract**, at depth 32, conservative L=80, both ends at
today's rates. **It fits with 1.5× margin.**

## AND THE FINDING THAT SHAPES YOUR PACKET

**THE WALK ALONE DOES NOT CLOSE IT.** Two of the six depths are **real
configurations read out of the tree**, not chosen:

* **depth 2 = the console AS COMPOSED TODAY.** `u_field_host` gets
  `.FAB_LANES(1)` with `PROGS` at its default 8, so the executor is a **scalar**
  datapath and a four-point group occupies four contexts. **12,623 clocks at
  L=80 — still 2.10× the contract, reached AFTER the whole subsystem swap.**
* **depth 32 = the engine's own GATED configuration.** `lint_field_v3_engine_shipped`
  runs `-GCTX=32 -GLANES=4`, and `zhao_field_host.sv:115` names the same seven
  values with its own warning: *"a fit that does not override them is measuring
  the bench."* **1,372 clocks — 0.23× the contract, 4.4× margin.**

**So §13.1's stream order and the executor's WIDTH are ONE prerequisite, not
two.** The contract is met **from depth 4 upward**, and the crossing lies
between the console's present configuration and the engine's own. **Build the
walk and leave `FAB_LANES(1)` and you have done the work and missed the
contract.**

## The prerequisites FIELDMAJOR named — this is your work list

1. **A field-major Earth adapter does not exist.** It is exactly what §13.1
   commissions, and it is why the bench had to model the engine.
2. **`zhao_terrain_patch_acc` has NO BACKPRESSURE** — its own header says so in
   capitals: *"no ready/valid and no backpressure on any phase"*, with
   INIT/ACCUM/DRAIN exclusivity and two idle cycles as **caller obligations the
   RTL does not enforce.** §13.4 commissions the repair, and **it is a
   prerequisite because a real cache write port can stall.**
3. **A patch phase owner / lifecycle** — INIT, ACCUM, DRAIN, staging and drain.
4. **Re-homing `zhao_terrain_veljoin`**, which rides the **vertex-major** lane
   stream and takes `fld_covers_o`. **That chain is composed, tested and
   reaching a particle contact TODAY.**
5. **The executor's WIDTH** — see above; inseparable from (1).
6. **The compose cache's WRITE PORT.** `zhao_terrain_compcache_front.sv:414`
   accepts **one vertex per two clocks** while the accumulator's header assumes
   a one-group-per-clock sink. **Worth 1,911 clocks.**
7. **The authored-lattice SOURCE.** `zhao_terrain_pagestream`'s `S_EMIT` emits
   **one vertex per clock** while INIT wants four. **A further 819 clocks, and
   this one is STRUCTURAL:** vertex-major *overlaps* intake and write in one
   streaming pass, while the accumulator's phases are **exclusive**, so
   field-major **pays for the lattice twice.**

**(6) and (7) together are 2,730 clocks — more than three times what the whole
field-major walk costs.** Neither is a detail.

## YOU MAY LAND THIS INCREMENTALLY, AND SHOULD

**Seven prerequisites is more than one packet.** Take them in dependency order,
**commit each with its own evidence**, and stop at a declared, coherent state
rather than half-composing something. **A packet that lands the field-major
adapter and the executor width, measures the composed result, and declares the
rest open is a good packet** — that is the pair the measurement says is
decisive.

**What you may NOT do is compose a prefix.** If `zhao_terrain_patch_v2` goes in,
its consumer goes in with it. Six packets have refused a chain whose last link
does not exist.

## The fences

* **DO NOT REGRESS VELOCITY.** It reaches `zhao_part_collide` today, verified
  after FIELDMAJOR's work (`terrain_veljoin_directed` 19/0,
  `part_terrain_tap_directed` 1297/0). §13.1's *"do not regress a composed
  consumer"* is the rule.
* **Do not build a frame-wide field-by-vertex matrix**, and **do not make
  another frame-sized flip-flop store.**
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP.** Measured today with
  a nine-arm probe: a **fourth Quartus 17.0.2 inference killer**, which cost
  `zhao_geom_arenabin` **146,414 registers against 1,010** for the identical
  circuit. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.** `check_ram_inference.py` rule 6 catches it now — and note the
  checker was **100% false alarms and 100% miss** on that file before, so its
  silence is not a verdict.
* **Preserve the numerical policy**: command order, saturating operations,
  material an opaque full-width u32, presence travelling with the result — an
  absent optional output is **NO WRITE**, not a write of zero.
* **Preserve the exact initial/final height and underside clamp rules**, the
  closed footprint test and dirty-border attribution. **Live Earth fields affect
  the TOP lattice.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting any row.**

## Evidence bar

* **The composed clock count as a LINE**, against the **≤ 6,000** contract at
  the console's actual composed depth — not at the engine's gated one unless
  you also raise it, and if you raise it, **say what that cost.**
* **A real TerrainField issued.** With none issued the list is empty,
  `fields_active_o` is 0 and the counter reads zero **for a reason unrelated to
  the cost** — the broken-instrument shape this entry has fallen into before.
* **Velocity still reaching its consumer**, demonstrated after the change.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Five packets today found their own controls vacuous — a guard never true, a
  line number that moved, one that **asserted the bug**, a second guard that was
  **also** wrong, and **a watcher that matched the smoke's own banner and
  reported a finished run while `cc1plus` was still compiling.**
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **GATE 31 DEDUPLICATES ITS SOURCE LIST** while `run_console_board_lint.ps1`
  does not, so a duplicated line in `fit_targets.yml` makes it say **OK twice**.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants** — a missed one turned `wrapper_port_parity` red today, a failure
  that **reads like a broken core**.
* **Regenerating a generated file is part of the change** — four have gone stale
  this week, two on merges.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED.**
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **Which prerequisites you landed**, each with its evidence, and which remain.
3. **The composed clock count against the 6,000 contract, at the console's
   actual depth.**
4. **What happened to velocity's chain.**
5. **Every claim in this brief or the record you found FALSE.** Every packet
   this week found at least one; most were mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/earthmajor` only.** Never `--force`.
