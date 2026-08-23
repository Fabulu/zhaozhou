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
