# Building and testing Zhaozhou on Windows

There is exactly one correct sequence, and PowerShell shell state does not
persist between separate tool invocations, so **all three lines must run in the
same shell**:

```powershell
. .\tools\env\zhao-env.ps1        # dot-sourced, not executed
cmake --preset windows-native
cmake --build build
ctest --test-dir build -L fast --output-on-failure
```

`tools/env/zhao-env.ps1` sets `VERILATOR_ROOT` and prepends the oss-cad-suite
bin/lib and the winlibs cmake/ninja/g++ to `PATH`. A `.bat` equivalent sits
beside it.

CI runs the same thing with `--preset windows-ci`, which differs only in
resolving `g++.exe` from `PATH` instead of pinning this machine's absolute
paths. **Keep the local lane matching CI.** A gate that silently skipped when a
tool was absent once hid weeks of drift, which is why the tool is pinned rather
than probed.

---

## The one failure that does not say what it means

```
CMake Error: Could not use disabled preset "windows-native"
```

**The preset is not disabled. The wrong `cmake` is running.**

`C:\devkitPro\msys2\usr\bin\cmake` sits early on the default `PATH` and reports
`${hostSystemName}` as `MSYS`, not `Windows`. `windows-base` is conditioned on
`Windows`, so it evaluates false and every preset inheriting it — which is all
of them — reports as disabled. Sourcing the env script puts the winlibs cmake
first and the error disappears.

The same shadowing has a second symptom, further downstream:

```
CMake Error: The current CMakeCache.txt directory C:/Programmieren/.../build
is different than the directory /c/programmieren/.../build where
CMakeCache.txt was created.
```

That is a cache the msys2 cmake wrote, recording MSYS-style paths, which the
Windows cmake then refuses to reconfigure. Delete `build/` and configure again
with the env sourced.

Check which one you have before diagnosing anything else:

```powershell
(Get-Command cmake).Source     # want C:\programmieren\dsstuff\mingw64\bin\cmake.exe
```

## A stale `build.ninja` can make correct CMakeLists look broken

Observed 2026-08-23. `tests/CMakeLists.txt` listed
`fpga/rtl/field/zhao_field_exec_shared.sv` correctly, and the build still failed
with:

```
%Error: ... Cannot find file containing module: zhao_field_exec_shared
```

Ninja regenerates `build.ninja` when `CMakeLists.txt` changes, but it must first
rebuild the Verilator dependency files that CMake includes — and it does that
using the **old** command lines, which predated the new source. The regeneration
aborts, so the new source list is never read, and the error names a file that is
sitting right there in the sources.

Symptom to recognise: the failure is reported under
`ninja: error: rebuilding 'build.ninja'`, not under a normal build step.

A clean reconfigure fixes it. Prefer that to hand-deleting subdirectories —
it also clears any mutant-derived sources a mutation sweep left behind in
consumers it never scored, which is a recurring source of tests that appear to
prove things about RTL they are not actually exercising.

## A stopped background task is not necessarily a stopped process

Observed 2026-08-23. A mutation sweep was stopped by the harness — and **kept
running**, rewriting `.sv` files under two live Quartus fits. The fits were
therefore reading RTL that changed underneath them, and their numbers described
neither the original nor the mutant.

This is the same family as everything else in this file: **build state that
looks like design behaviour.** A mutation sweep's whole job is to edit RTL in
place and put it back, so a sweep that outlives its stop signal is the most
destructive possible background process to leave running.

Before starting any fit or any long measurement:

```powershell
Get-Process | Where-Object { $_.Name -match 'quartus|verilator|python|btormc|yosys' }
```

and confirm nothing is holding the files you are about to measure. After
stopping a task, **verify the child processes actually died** rather than
trusting the stop — a stop signal reaches the shell, not always what it spawned.

The same check applies in the other direction: `git status` before a fit. A fit
started against a dirty tree records `rtlCleanAtHead: false`, which is the
report telling you the row may not describe any commit.

## The wrong `ctest` on PATH fails all 278 tests at once, and not for any reason in the test

Symptom: `ctest --test-dir build -L fast` reports **BAD_COMMAND** for every
single test, in 0.00 seconds each, while the test executables exist and run
correctly when launched by hand.

Cause: three `ctest.exe` are installed on this machine and PATH order decides
which one answers.

```
C:\devkitPro\msys2\usr\bin\ctest.exe        <- wins on PATH
C:\Program Files\CMake\bin\ctest.exe
C:\programmieren\dsstuff\mingw64\bin\ctest.exe   <- the one that configured build/
```

The build records the ctest it belongs to:

```
$ grep CMAKE_CTEST_COMMAND build/CMakeCache.txt
CMAKE_CTEST_COMMAND:INTERNAL=C:/Programmieren/dsstuff/mingw64/bin/ctest.exe
```

The msys2 build is POSIX-path-flavoured. Handed a `CTestTestfile.cmake` written
by a native Windows CMake, it does not recognise `C:/...` as absolute and glues
it onto the test's working directory, producing commands like:

```
Command: "/c/programmieren/zencrifice/zhaozhou/build/tests/C:/Programmieren/zencrifice/.tools/oss-cad-suite/bin/verilator_bin.exe"
```

which of course cannot be launched. The log in
`build/Testing/Temporary/LastTest.log` prints that concatenated path in full,
and it is the fastest way to recognise this: **look at the `Command:` line, not
at the test.**

Fix: invoke the ctest the cache names, explicitly.

```powershell
& 'C:\Programmieren\dsstuff\mingw64\bin\ctest.exe' --test-dir build -L fast -j 4
```

Two related notes:

* Building has the same hazard in a different shape -- `cmake --build` through
  the Bash tool fails in `ccache` with *"The USERPROFILE environment variable
  must be set"*, because that environment does not carry it and exporting it
  from inside bash does not reach the child. Build from PowerShell.
* An all-tests-fail result is not evidence about the tree. Before believing a
  red gate, check that ONE test fails for a reason printed inside the test.

## Reconfiguring the build must happen in Git Bash, with `VERILATOR_ROOT` set

`build/` was configured from Git Bash: its cache records the source directory as
`/c/programmieren/...`. Running `cmake -S . -B build` from PowerShell therefore
fails twice over —

```
CMake Error: The current CMakeCache.txt directory C:/Programmieren/.../build
is different than the directory /c/programmieren/.../build where it was created
%Error: Cannot find verilated_std.sv ... '/yosyshq/share/verilator\include/...'
```

— the second because `verilator_bin.exe` falls back to a baked-in
`/yosyshq/...` path when `VERILATOR_ROOT` is absent from the environment, and
PowerShell does not carry it unless `tools/env/zhao-env.ps1` has been sourced.

What works:

```bash
export VERILATOR_ROOT="C:/programmieren/zencrifice/.tools/oss-cad-suite/share/verilator"
export PATH="/c/programmieren/dsstuff/mingw64/bin:$PATH"
cmake -S . -B build
```

Then **build** from PowerShell (ccache needs `USERPROFILE`, see above) and run
tests with the ctest the cache names. Three shells, one for each job, which is
ugly but is what this toolchain actually wants.

**This matters more than it looks.** `verilate()` elaborates at CONFIGURE time,
so adding a `.sv` file to a target's `SOURCES` does nothing until the configure
succeeds — and a failed configure leaves the old `build.ninja` in place, so the
build carries on with the OLD source list and reports `MODMISSING` for a module
whose file you just added and listed correctly. The error names the instantiating
file, not the configure failure that is actually responsible.

Also note `cmake` itself is shadowed on PATH exactly like `ctest`: a bare
`cmake -S . -B build` picked up a different CMake and reported
`The CMAKE_CXX_COMPILER: C is not a full path`, which is a corrupted-looking
error with a PATH cause.
