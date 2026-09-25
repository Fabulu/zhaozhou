# ATTRSETUP — 45 multipliers in 164 lines, and the widths are declared, not needed

**Branch `gz/attrsetup`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is the first DSP packet of phase 3.** PALRAM is the ALM lever; this is the
other ceiling, and **the M10K trade does nothing for it.**

## The measurement, and it is a fresh one

`reports/HANDOVER-20260919.md` §15.22 has the whole-console picture. The short
version: the failed console fit measured **375 DSP against the shipping part's
112 — 335%.** Logic is ~3.5×, registers 2.4×, DSP 3.3×, and memory 0.53×. **Three
of the four ceilings are breached and only one is bought back by the M10K trade.**

I then mapped your block standalone, on 2026-09-26, and it took **14.3 seconds**:

```
zhao_geom_attrsetup@dsp-census-20260926   map_only   14.3s
  Total DSP Blocks       45
  Total registers       225
  Block memory bits       0
  digest bd84da4c2517, rtlCleanAtHead true
```

**45 DSP — 40% of the entire shipping device — from 164 lines and 225
registers.** It is the densest DSP block in the console outside the per-pixel
tile pipe, and the standalone row means nothing about the composition is
inflating it.

The breakdown, from the map report's DSP Block Usage Summary:

| | used |
|---|---:|
| Independent 27×27 | **24** |
| Two Independent 18×18 | 15 |
| Sum of two 18×18 | 6 |
| Fixed Point Dedicated Pre-Adder | 6 |
| Fixed Point Dedicated Output Adder Chain | 6 |

## The hypothesis — and it is a hypothesis, not a finding

**Every multiply in the block is written at a declared width far above what its
operands can hold**, and DSP inference follows declared width.

```systemverilog
// operands: w is 46 bits, va is 32.  Declared 96 x 96.
n0_c  = 96'(w0_0) * 96'(va_i) + 96'(w1_0) * 96'(vb_i) + 96'(w2_0) * 96'(vc_i);

// operands: a coordinate difference is 22 bits, va is 32.  Declared 72 x 72.
dndx_c = ((-(72'(cy_by))) * 72'(va_i) + ...) <<< PIXEL_SHIFT;

// operands: 22 bits and 21 bits.  Declared 46 x 46.
w0_0 = -(46'(cx_bx) * 46'(by_i)) + (46'(cy_by) * 46'(bx_i));
```

**The block says so itself, and the reasoning is sound about the wrong
resource.** Its own comment:

> *"96 is carried so the widths are obviously sufficient rather than exactly
> sufficient — this block is once per triangle, not per pixel."*

That is a correct trade against **ALUTs and latency**, and it is exactly why the
block is only 938 ALUTs. **The cost landed in DSPs instead**, on the ceiling that
is 335% breached.

**Do not take this as established.** Quartus does strip redundant sign extension
sometimes, the 24 × 27×27 may not decompose the way the widths suggest, and
"I can see a wide literal" is not a measurement. **Your first act is the
experiment**, and it costs 14 seconds a run:

```
tools/quartus/run_block_fit.ps1 -Module zhao_geom_attrsetup -MapOnly -RowLabel '@<your-label>'
```

(`-RowLabel` must start with `@` or `-`; the row name is `$mod$RowLabel` glued.)

**Change one width at a time and map after each.** A causal answer — *"the 24
27×27s are the three 96-bit products and narrowing them to 46×32 costs N"* — is
worth far more than a smaller number nobody can explain.

## The census puts your block alone in the expensive mode

I mapped five more DSP consumers standalone after yours —
`reports/synthesis/dsp_census.md`, built by `tools/budget/dsp_census.py`. Six
leaf blocks are **95 DSP, 85% of the whole device**, and the modes separate them
cleanly:

| block | DSP | 18x18 pairs | 18x18+36 | **27x27** |
|---|---:|---:|---:|---:|
| `zhao_geom_attrsetup` | 45 | 15 + 6 | | **24** |
| `zhao_geom_skin_norm` | 21 | 6 | 7 | 8 |
| `zhao_geom_skin` | 9 | 6 + 3 | | |
| `zhao_twod_plane` | 8 | 4 | 4 | |
| `zhao_geom_cull` | 6 | 4 + 2 | | |
| `zhao_geom_meshfetch` | 6 | 4 + 2 | | |

**A `Two Independent 18x18` block does two multiplies; an `Independent 27x27`
block does one.** Four of the six blocks use no 27x27 at all — their multipliers
are narrow and busy, and there is nothing to reclaim. **Yours holds 24 of the
census's 32 wide blocks.** That does not prove the declarations are the cause,
but it is the shape the hypothesis predicts, and it is why you are first.

**And the standalone counts match the composed entity table EXACTLY** — 45, 21,
9, 8, 6, 6 in both. The composition neither shares nor inflates DSPs, so
**whatever you save here is saved in the console, one for one.**

## What must NOT change, and it is the whole point of the block

**THE BITS OF THE RESULT.** This block's entire justification is that it emits
*exactly* the oracle's numerator:

> *the edge functions step by CONSTANTS … so the NUMERATOR is itself an exact
> integer plane … Proved over 32,805 pixel-attributes and five triangle shapes
> in `tests/proofs/attribute_plane_equivalence.cpp`.*

So:

* **Narrowing a MULTIPLY is legal; narrowing a RESULT is not.** Multiply at the
  operands' true widths and sign-extend the product. `n0_o` stays 96 bits,
  `dndx_o`/`dndy_o` stay 72, and the emitted value is bit-identical.
* **The bounds argument in the header is the specification of those output
  widths** — *"a coordinate difference is at most 2^22 and an attribute 2^31, so
  a w is under 2^45 and a term under 2^76; three of them need 78 bits"*. If you
  believe a bound there is wrong, that is a finding to write up, not a licence to
  shrink a port.
* **`PIXEL_SHIFT = 8` is not a knob.** The header records that getting it wrong
  *"makes every gradient 256× too small and every triangle flat, which is the
  same units mistake this project has already made once in a test."*
* **The asymmetry in the partials is orient's, not a transcription slip** — the
  header says so explicitly. Do not "fix" it.
* **One attribute per request stays one attribute per request.** The block is
  small *because* the attribute count is a scheduling decision upstream, and that
  is what lets early-Z pay for only `invw24`. Batching attributes here to share
  multipliers would move a scheduling law into an arithmetic block.

**And the owner's directive fence applies as it did to PALRAM:** *"NOT authority
to delete a feature … waive a correctness failure, or call reduced work
equivalent merely to reach zero or fit a device."* Reducing precision to save a
DSP is that move. If the only way to hit the ceiling is to lose bits, **stop and
write it up** — it is the owner's call.

## If the widths are not the answer

Then the block genuinely needs 45 multipliers and the question becomes the one
`reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md` already names and records
as **unbuilt**:

* **R1** — a reusable quarter-square primitive, a 26+6 full-width hybrid, a
  coefficient-table primitive. *"Do not exist."*
* **R4** — the quarter-square / coefficient-memory replacements. *"Absent."*

A quarter-square multiply trades a multiplier for two table lookups and an
adder, and **memory is the resource we have** — 53% used, roughly 259 M10K free.
That is the same lever as PALRAM's pointed at a different ceiling. **Do not start
building an R1 primitive without saying first, in numbers, what it costs and what
it buys**; a general primitive written speculatively is how this campaign
acquired blocks nothing consumes.

**Time-multiplexing is also on the table and is cheaper to reason about.** The
block is `v_ready_o = !r_valid_o` — one triangle in flight, one attribute per
request. Its sibling `zhao_geom_attrpack`'s header already argues the
multiplexing case for six lanes and says the decision *"is in the OPERAND MUX,
not in the arithmetic"*. Read it before inventing a scheme; the reasoning may
already be written down.

## Evidence bar

* **A before/after `quartus_map` row for `zhao_geom_attrsetup`**, both recorded
  in `reports/synthesis/zhao_block_fit.json`, with the DSP Block Usage Summary
  for each. Baseline is `@dsp-census-20260926`: **45 DSP, 225 registers**.
* **Bit-identical output**, proved and not asserted.
  `tests/geometry/geom_attrsetup_directed.cpp` is the block's own ENFORCED-BY
  bench, and `tests/proofs/attribute_plane_equivalence.cpp` is the 32,805-case
  proof. Both must pass unchanged, and you should add a differential that runs
  old and new arithmetic on the same stimulus if you change the expressions.
* **Do NOT start a console fit or a full-device fit.** Those are 01:54:49 and the
  coordinator schedules them. `-MapOnly` on this block is 14 seconds and is
  yours.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's**.
* **Verilator lint-clean is not Quartus-synthesizable**, and this block is
  arithmetic-heavy where that bites. Map everything you write.
* **`mutant_copy_drift` keys on COMMIT TIME** — check how many committed mutant
  copies any file has before your first edit; editing even a comment stales them.
* **`-RowLabel` must start with `@` or `-`**, or the harness refuses it — it
  would otherwise glue a name into the report that nobody recognises.
* **One `ctest` at a time per build tree**; after killing one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*` before restarting.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **Whether the declared widths are the cause**, as a causal answer from
   one-change-at-a-time maps, with the row and the DSP summary for each step.
2. **Before/after DSP, registers and ALUTs.** Measured.
3. **The proof the output is bit-identical**, naming the benches and their counts.
4. **What you refused**, especially anything that would have cost precision or
   moved a scheduling law into this block.
5. **If the widths were NOT the cause**, say so plainly and price the R1/R4
   route in numbers rather than starting it.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/attrsetup` only.** Never rebase or push
   the shared branch, and never `--force`.
