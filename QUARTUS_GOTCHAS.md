
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
