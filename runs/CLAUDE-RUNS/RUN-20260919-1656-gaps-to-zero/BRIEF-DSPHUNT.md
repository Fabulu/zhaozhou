# DSPHUNT — 375 against 112, and the campaign has moved it by SIX

**Branch `gz/dsphunt`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is OPTIMIZATION, and it is authorised.** The console was measured on
2026-09-26 and the result is in `reports/HANDOVER-20260919.md` §15.32. **Read
that section first.**

## THE MEASUREMENT YOU ARE ACTING ON

Row `zhao_console_core@console-snapshot-20260926`, 289 sources, digest
`a7c7a4593942`, clean tree. Analysis & Synthesis **succeeded**:

| | needed | shipping `5CSEBA6U23I7` | |
|---|---:|---:|---:|
| ALUTs | 293,352 | 83,820 | 350% |
| Registers | 271,464 | 167,640 | 162% |
| **DSP blocks** | **375** | **112** | **335%** |
| Block memory bits | 3,522,668 | 5,662,720 | 62% |

**DSP is a wall nobody has worked.** It is proportionally as bad as ALUTs, and
this entire campaign has moved it **by six**.

**The console's own mode census** (`tools/budget/dsp_mode_census.py --label
'@console-snapshot-20260926'`):

| mode | blocks | multiplies each |
|---|---:|---|
| Two Independent 18x18 | 142 | **2** |
| Sum of two 18x18 | 57 | 2 |
| Independent 18x18 plus 36 | 52 | — |
| **Independent 27x27** | **75** | **1** |

**The 27x27 column is the expensive one** — one multiply per block where an
18x18 block does two. **75 blocks is 67% of the entire shipping part's DSP
budget doing one multiply each.** That is where to look first.

## THE LEVER IS PROVEN, CHEAP, AND COUNTER-INTUITIVE

**Read `dsp_mode_census.py`'s header.** It used to say the 27x27 column
separates blocks that multiply wide VALUES from blocks that merely DECLARE wide
ones. **Measured on 2026-09-26, that is FALSE on Quartus 17.0.2.** Narrowing
`zhao_geom_attrsetup`'s declared widths one group at a time — 96x96 → 46x32,
46x46 → 22x21, 72x72 → 22x32 — **moved the row by ZERO blocks each time.**
Quartus already strips the plain `WIDE'(narrow) * WIDE'(narrow)` sign extension.

**What actually cost 9 of that block's 24 wide blocks was ONE OPERAND WRITTEN**

```systemverilog
(-(72'(cy_by))) * 72'(va_i)      // the negation taken INSIDE the cast
```

**Moving the minus sign OUTSIDE the multiply, at unchanged declared width,
recovered all nine.**

**So the pattern to hunt is AN ARITHMETIC OPERATION APPLIED TO A WIDENED VALUE
BEFORE THE MULTIPLY — not a wide literal.** Negation is the one proven; look
also for additions, subtractions, shifts and conditional selects taken inside a
cast that feeds a multiplier.

Evidence for all of it: `tests/probes/zhao_attrsetup_mul_probe.sv`, eight arms,
rows `zhao_attrsetup_mul_probe@probe-m0..m7`. **The probe is committed — use it
as the pattern for your own, do not invent a measurement method.**

## The job

1. **Find the 27x27 sites.** The console `.map.rpt` names every DSP by
   hierarchy path. Start there, not from a grep of the source.
2. **Grep those blocks for arithmetic inside a cast feeding a multiply.**
3. **Repair, measure, repeat** — `-MapOnly` per block on the **shipping
   device**, one change at a time, so every recovered block has a row behind it.
4. **Report the total recovered**, with a row per repair.

**A packet that recovers 20 DSP with four measured rows is worth more than one
that claims 80 from reading.** Measure each.

## THE ABSOLUTE FENCE: THE ARITHMETIC MAY NOT CHANGE

**Every repair must be BIT-EXACT.** Moving a negation outside a multiply is an
identity; many things that look similar are not. **Prove each repair against the
oracle or against an exhaustive sweep of the operand widths**, and say which.

**This is the one place this work can do real damage.** A DSP saving that
quietly changes a rounding or a sign is a wrong pixel shipped past every gate —
the exact failure this project's arithmetic laws exist to prevent. **If you
cannot prove a repair is an identity, do not ship it.**

## The other fences

* **`multstyle = "logic"` is NOT a free answer.** Converting a multiplier to
  logic moves the cost to ALUTs, and **ALUTs are at 350% while DSP is at 335%**
  — you would be paying from the more overdrawn account. If you propose it
  anywhere, **measure both columns and show the trade.**
* **Do not delete a multiply to save a block.** Reducing precision or dropping a
  term is a feature change, which the delegation does not cover.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours.
  **Every map you quote must name its `-Device`**, and **read `rtlCleanAtHead`
  before quoting ANY row** — a dirty row carried a live +33 M10K claim this week.
* **Rows on different devices must never be differenced.**

## Evidence bar

* **A `-MapOnly` row per repair**, device named, `rtlCleanAtHead` true, against
  the block's own before-row as the like-for-like.
* **A bit-exactness proof per repair** — oracle or exhaustive sweep, stated.
* **The console-level consequence NAMED, not assumed.** A leaf block's DSP fall
  is not automatically a console fall; say what you measured and what you infer.
* **Prove every counter you quote.**
* **Do not write a test that asserts the bug.**

## Traps

* **THE GATES DO NOT BUILD.** Run `cmake --preset windows-native` yourself from
  PowerShell with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit
  code, not a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across five directories** —
  count occurrences by hand when a claim turns on whether a value is consumed.
* **`git checkout -- <file>` discards uncommitted work with no reflog.**
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**
* **A `$fatal` guard registered as a build target returns RC=0** — it fires at
  run. Check any control you add can actually fail.

## Deliverable

1. **DSP before and after**, per block, each with a measured row.
2. **The bit-exactness proof for each repair.**
3. **The pattern census** — how many sites of the arithmetic-inside-a-cast shape
   exist, how many you repaired, and what the rest would need.
4. **Whether `multstyle` was considered anywhere, and the ALUT trade if so.**
5. **Every claim in this brief you found FALSE.** Every packet this week found
   at least one; most were mine.
6. **What you refused** — especially any repair you could not prove an identity.
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/dsphunt` only.** Never `--force`.
