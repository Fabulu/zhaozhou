# ARENAINFER — 145,152 bits went to FLIP-FLOPS. Make them infer.

**Branch `gz/arenainfer`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is finishing gap work, not optimization.** ARENACOMPOSE composed
`zhao_geom_arenabin` for `I55` and the measurement it was authorised to take
found the block costs **146,414 registers — 87% of the shipping part's entire
167,640 register sites, for ONE block.**

**Composing did not create that cost. It revealed it.** BINARENA's *"177,984
bits on chip against the binner's 360,064"* reads as a saving and **measures as
the opposite**, and it survived unchallenged because that packet was forbidden a
fit. **The block is now composed and the number is real.**

## The measurement you are acting on

Three `-MapOnly` rows on the shipping part `5CSEBA6U23I7`, **all three
identical**: `zhao_geom_arenabin@arenacompose` (**`rtlCleanAtHead: true`**),
`@ramstyle` and `@ramstyle-literal` (both dirty — they are experiments and are
labelled as such).

**146,414 registers / 33,408 block memory bits.**

**Only the five module-scope directory arrays inferred.** The **145,152-bit
staging array, declared INSIDE A GENERATE, went entirely to flip-flops.**

**`(* ramstyle *)` was tried as a macro and as a literal. It changed nothing and
warned nothing, so ARENACOMPOSE REMOVED it rather than shipping a pragma that
does nothing** — which is the right call and is why those two rows exist.

**The limit is the INFERENCE, not the capacity: 28 M10K out of 553 if it can be
reached.** That is the whole prize.

## Why this is the highest-value thing in the tree

The console measurement (`reports/HANDOVER-20260919.md` §15.32, **and read its
CORRECTION**) puts memory at **62% used — the only axis with room** — while
logic is massively over. **Turning 145,152 register bits into ~28 M10K is the
"trade ALMs for M10K" lever at its purest**, and it is now measured-correct
rather than merely plausible.

## The job

1. **Make the staging array infer.** The block's own header claims *"generate
   infers where a 2-D array does not"* — **ARENACOMPOSE refuted that**, so the
   header is wrong and the shape inside the generate is the suspect.
   `tools/quartus/check_ram_inference.py` exists and its rules are measured, not
   guessed — **use it, and read rule 5 (part-select element write) and the
   deleted rule 2 before theorising.**
2. **Measure every attempt** with `-MapOnly` on the shipping part. Three
   identical rows already prove that *reasoning* about this block's inference is
   worthless; only rows count.
3. **If it cannot be made to infer, say so with the rows** and state what shape
   would. **A measured "this array cannot infer on Quartus 17.0.2 because X" is
   a full deliverable** — it converts an open question into a costed one.

## The fences

* **DO NOT un-compose the block to make the number go away.** ARENACOMPOSE
  refused that explicitly and was right: *"that would hide the number and
  restore the series §4 forbids."*
* **Do not ship a pragma that does nothing.** If `ramstyle` has no effect,
  removing it is correct — a pragma that neither works nor warns is a comment
  that looks like a control.
* **The arithmetic and the protocol may not change.** This is a storage-shape
  repair. If a repair changes what the block computes or when it handshakes,
  it is not this packet.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is
  yours; **every map must name its `-Device`**, rows on different devices must
  never be differenced, and **read `rtlCleanAtHead` before quoting any row.**
* **I13CLOSE is live in the tree** on the terrain/texture side. Stage your
  **hunk**, never `git add <file>`, never `git checkout --`.

## AND ONE DEFECT ARENACOMPOSE FOUND THAT NOBODY HAD PRINTED

**`u_geom_tidq` UNDERFLOWS EXACTLY ONCE PER FRAME, in all six smoke forms**, so
the external arena's lists are **four references short of the picture — 97
against 101.**

**Pre-existing since I54, and nothing printed it** — the smoke declares
`geom_tidq_underflow_o` at `:520` and only passes it to a formatter at `:6717`.
**That is the unread-counter shape this campaign keeps paying for**, on the very
queue whose dead clock I repaired this morning.

**It is yours if it is cheap; escalate to me if it is not.** Either way,
**assert on that counter** so it can never go unread again.

## Evidence bar

* **A `-MapOnly` row per attempt**, device named, `rtlCleanAtHead` true for
  anything you quote as a result.
* **Registers down AND memory bits up**, with the arithmetic unchanged —
  demonstrated, not asserted.
* **The block's directed tests still passing**: `geom_arenabin_directed` (275
  checks through the real guard/arbiter/controller/SDRAM path) and
  `geom_arenabin_price` (470).
* **Prove every counter you quote**, and **check any control you add CAN
  FAIL** — ARENACOMPOSE found **its own agreement check was vacuous** because
  its guard was never true, and caught it by *reading the output*, not by a
  gate.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's** — ARENACOMPOSE read `tail`'s exit code for the register's today.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED.** ARENACOMPOSE produced a
  row whose device was **a file path**. Check the row you just wrote.
* **Splitting `-ExtraSources` through `powershell -File` breaks it** — use a
  splat.
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — 291 sources; run
  after any port change.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants** — one was missed today and `wrapper_port_parity` went red.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **Do not edit a bench while a control is verilating it** — ARENACOMPOSE did
  and had to re-run them.
* **`reports/synthesis/zhao_block_fit.json` reserialises** — verify no row is
  lost, count before and after.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **Registers and memory bits before and after**, with a row per attempt.
2. **What shape made it infer** — or the measured reason it cannot.
3. **The arithmetic proven unchanged.**
4. **The `u_geom_tidq` underflow**: fixed, or costed and escalated — and
   **asserted either way.**
5. **Every claim in this brief you found FALSE.** Every packet this week found
   at least one; most were mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/arenainfer` only.** Never `--force`.
