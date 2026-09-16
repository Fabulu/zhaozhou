
## 15. Quartus 17.0.2 does not support `inside`, and Verilator does

Added 2026-09-05, after it cost a fit launch.

`zhao_texture_material_combine_v1` used SystemVerilog's set-membership operator:

```systemverilog
  if (f_recipe_i inside {R_DETAIL_LIGHT, R_DETAIL_MASK}) ...
```

Verilator lints it clean. Quartus 17.0.2 fails Analysis & Synthesis with

```
Error (10170): Verilog HDL syntax error ... near text: "inside";  expecting ")"
```

and then cascades four more errors from the confused parse, so the real cause is
the FIRST message and the rest are noise.

**The lesson is not "avoid `inside`".** It is that **a clean Verilator lint says
nothing about whether the synthesiser will accept the file.** The two tools
disagree about the language, Verilator is the more modern one, and the gap
always resolves in the direction of the lint being optimistic. Anything that
only synthesis can reject — `inside`, some `unique`/`priority` forms, unpacked
array ports, `automatic` in odd places — is not covered by the fast gate and is
discovered at fit time, which is 30 to 90 minutes later.

Write the case statement.

---

## 16. `$LASTEXITCODE` is EMPTY when PowerShell could not run the thing at all

Added 2026-09-05.

`verilator` in `oss-cad-suite/bin` is a Perl script with no extension. PowerShell
refuses to execute it in a pipeline — *"Es ist nicht möglich, in der Mitte einer
Pipeline ein Dokument auszuführen"* — and leaves `$LASTEXITCODE` **empty**, not
non-zero.

A lint invoked that way printed nothing and was read as a clean lint. It was a
NON-RUN. Empty output plus an empty exit code is the signature; a real clean
Verilator run still prints its verilation report, so **the absence of the report
is the tell**.

Call `verilator_bin.exe` directly, or run it from Git Bash. And when a checking
tool prints nothing at all, confirm it ran before recording that it passed --
this is the same class as a regex that matches nothing and reports "no problems
found".

---

## 17. `ctest` needs `zhao-env` sourced too, not just `cmake`

Added 2026-09-05.

CLAUDE.md's build note says to configure from PowerShell with
`tools/env/zhao-env.ps1` sourced, and explains what happens to `cmake` if you
do not. **The same applies to `ctest`, and the failure looks completely
different.**

Without the env, `ctest` resolves to a different binary and reports:

```
Test project /c/programmieren/zencrifice/zhaozhou/build
...
	 80 - material_combine_directed (BAD_COMMAND)
	 81 - texture_combine_diff      (BAD_COMMAND)
	 ... 500 more
```

`BAD_COMMAND` on *every* test, with a POSIX project path — while
`CTestTestfile.cmake` in that same directory plainly says
`Build directory: C:/programmieren/...`. So the build tree is fine and the
runner is wrong.

**This reads exactly like a broken build tree**, and it cost two rounds of
"nothing is built, reconfigure everything" before the tell was noticed: the
project path `ctest` prints is POSIX while the file it is reading is Windows. A
whole suite failing identically is a property of the runner, not of 500 tests.

With the env sourced, `ctest` resolves to
`C:\programmieren\dsstuff\mingw64\bin\ctest.exe`, prints a Windows project
path, and the same 16 tests pass in 192 s.

**The general rule:** when every member of a large set fails the same way,
suspect the thing they share before the things they do not.

---

## 18. MAP-CHECK BEFORE YOU FIT. It is 36 seconds against ten minutes.

Added 2026-09-16, after gotcha 15 happened again to a different form.

A G8B timing package added

```systemverilog
  for (genvar gsat = 0; gsat < 3; gsat = gsat + 1) begin : g_sat0
    assign dstep_sat[0][gsat] = ({14'b0, s3_dv[gsat][47:31]} >= s3_d);
  end
```

Verilator: **zero diagnostics**. `quartus_map`, 5.2 seconds in:

```
Error (10170): Verilog HDL syntax error ... near text: "for";
               expecting "endmodule"
```

Quartus 17.0 wants explicit `generate` / `endgenerate` and the genvar
declared outside the loop header. CLAUDE.md already records the implicit
generate as one of the two forms that prove a clean lint settles nothing; this
entry is not about that form.

**IT IS ABOUT THE CHEAP GATE NOBODY REACHES FOR.**
`tools/quartus/run_block_fit.ps1 -MapOnly` runs Analysis & Synthesis and
stops. On this block that is **35.7 seconds**. A full fit is ten minutes here
and hours on the island. Every syntax and elaboration rejection Quartus has --
the whole class gotcha 15 is about -- is caught by the short one, and the long
one has nothing extra to say about it.

So the sequence for any RTL change that will be fitted is:

1. `verilator --lint-only -Wall` — one tool's opinion, seconds.
2. the directed tests — behaviour, seconds to minutes.
3. **`run_block_fit -MapOnly`** — *does the synthesiser accept it*, under a
   minute, **and it also reports registers and memory bits**, so a change that
   was supposed to delete flip-flops can be checked here rather than at the end.
4. the fit — area, Fmax, DSP, RAM inference. The expensive question, asked once.

Step 3 was skipped, step 4 failed in five seconds, and the only cost was the
embarrassment plus one wasted launch. It is a habit rather than a rule because
a launch is recoverable — but it is free, it answers a different question from
both its neighbours, and a MapOnly row is retained evidence in its own right.

**A failed MapOnly is not a failed budget.** The row lands as
`incomplete:failed:quartus_map.exe`, which is a failed MEASUREMENT --
distinct from `failed:structure`, which is a fit that COMPLETED and whose
budget rules refused the numbers. Keep both; deleting the first removes the
only record that a form does not synthesise.

---

## 19. Do not hand-optimise ARITHMETIC for this synthesiser. Optimise STRUCTURE.

Added 2026-09-17, from the G8B timing campaign, which produced five packages
that worked and two that did not, and the two that did not are the same shape.

| package | what it changed | measured |
|---|---|---:|
| T3a | registered the window mask (WHERE it is computed) | **+11.7 MHz** |
| T5 + T6 | registered the lattice base; split the output cone | **+9.7 MHz** |
| T8 | **deleted** a register whose value was already implied | **+8.6 MHz** |
| T7 | moved a constant add into the register ahead of it | **−7.9 MHz** |
| T9 | strength-reduced a constant add | **0.0** |

**T9.** `rescale16` computed `(x + 2^15) >>> 16`. Writing x = H·2^16 + L, that
equals `(x >>> 16) + x[15]` -- a 36-bit increment by a single bit instead of a
52-bit add. The identity is exact; it was verified over the full domain before
a line was written, with a negative control that fired 70,000 times.

The fit came back **byte-identical in every field** -- same ALM, registers,
RAM, DSP, Fmax, slack and TNS -- from a different digest and a different
commit. Quartus had already done it. Adding a constant whose only set bit is
at position 15 and then discarding bits 15 downward is a pattern the
synthesiser recognises without help.

**T7** is the same lesson with a price attached. It moved that rounding add
one stage earlier, into `ln2_prod_q <= m_prod`. That register was **the DSP's
own output register**, so an adder in front of it evicts the product from the
DSP into fabric: multiply-to-DSP-register became
multiply-out-through-an-adder, and it cost 7.9 MHz to save 0.245 ns elsewhere.

**The rule.** Quartus's local arithmetic optimiser is better than hand
rewriting, and it knows about hard-block boundaries that the RTL does not
mention. What it CANNOT do is decide where your pipeline registers go or
whether one of them is redundant -- that is whatever the RTL says. So:

* **Worth doing:** register a value one cycle earlier; split a cone; delete a
  register whose value is implied by its neighbours; move a computation out of
  a consumed path into a next-state cone.
* **Not worth doing:** re-associating adds, strength-reducing constant
  arithmetic, folding rounding terms. At best zero; at worst it breaks a hard
  block's packing and costs more than the path was worth.

**And neither of these is visible in simulation.** Verilator has no DSP and no
carry chains; T7 and T9 were both bit-exact, both had live equivalence
assertions that stayed silent, and both were confirmed useless or harmful only
by a fit. That is what gotcha 18's MapOnly step cannot cover either -- it
answers "does this synthesise", not "is this faster".
