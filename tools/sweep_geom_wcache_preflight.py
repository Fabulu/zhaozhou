#!/usr/bin/env python3
"""Lint EVERY zhao_geom_wcache mutant before any of them is scored.

Same reason as `sweep_geom_lod_preflight.py`: a mutant that does not build is
DISCARDED by the sweep's guards, which is correct but late -- it turns a broken
mutation into a discard rather than into evidence, and a sweep that discards a
third of its mutants has not tested what it claims to. Worse, before guard 5
existed a non-compiling mutant was scored as CAUGHT, which is the most
flattering possible way to be wrong.

Quartus and Verilator disagree about what SystemVerilog is, and so do the
project's own -Wall settings. Known traps this catches: `W'sd0` where W is a
parameter is a syntax error; an always-false (or always-true) comparison fails
-Wall; a mutation that removes the last use of a signal fails -Wall UNUSED.

Reads the mutant table from `sweep_geom_wcache_mutants.py`, so the sweep and the
preflight cannot disagree about what a mutant is.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_geom_wcache_mutants as T  # noqa: E402

# THIS MODULE IS NOT SELF-CONTAINED, unlike the terrain blocks these sweeps
# were first written for. It reads zhao_pkg (framebuffer slot bases) and the
# generated ABI package, and it instantiates zhao_crc32c_fold. Linting the file
# ALONE fails on a package reference at line 182 -- and it fails identically
# for every mutant, which reads as "all 20 mutations are broken" when in fact
# none of them is. The list is the one tests/CMakeLists.txt uses for
# lint_zhao_geom_wcache, so the two cannot drift.
DEPS = []  # zhao_vertex_arena is self-contained

gold = T.read_rtl()
vr = os.environ["VERILATOR_ROOT"]
lint = os.path.join(vr, "bin", "verilator_bin.exe")

bad = []
try:
    for name, old, new in T.MUTANTS:
        try:
            text = T.mutate(gold, old, new)
        except ValueError as exc:
            bad.append((name, str(exc)))
            continue
        T.write_rtl(text)
        rc = subprocess.run(
            [lint, "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", "zhao_vertex_arena"] + DEPS + [T.RTL],
            capture_output=True, text=True)
        if rc.returncode != 0:
            lines = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            bad.append((name, lines[0][:100] if lines else "rc=%d" % rc.returncode))
finally:
    T.write_rtl(gold)

print("linted %d mutants, %d do not build" % (len(T.MUTANTS), len(bad)))
for n, why in bad:
    print("   %-52s %s" % (n, why))
sys.exit(1 if bad else 0)
