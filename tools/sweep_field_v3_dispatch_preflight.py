#!/usr/bin/env python3
"""Preflight for tools/sweep_field_v3_dispatch.sh: every mutant must LINT.

A mutant that fails to elaborate would be scored CAUGHT by a compile failure,
which is the most flattering possible way to be wrong (house rule). Imports
the mutant module directly so the sweep and the preflight cannot disagree.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_field_v3_dispatch_mutants import MUTANTS, RTL, mutate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VROOT = os.environ.get(
    "VERILATOR_ROOT", r"C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator")
VBIN = os.path.normpath(os.path.join(VROOT, "..", "..", "bin", "verilator_bin.exe"))

# THIS BLOCK INSTANTIATES NOTHING. It is a leaf: the multiplier bank is the
# engine's and reaches it through ports, which is the whole point of the shape.
# So the cone is one file, and a lint failure here is the mutant's fault rather
# than a missing module.
# THE PACKAGE COMES FIRST. This block stopped carrying its own opcode table
# on 2026-08-28 and derives it from zhao_field_ops_pkg instead, because the
# private copy is how it and the executor came to disagree about SPLINE and
# RING -- a disagreement that parked a context forever.
CONE = ["fpga/rtl/field/zhao_field_ops_pkg.sv", RTL]

CMD = [VBIN, "--lint-only", "-Wall", "-Wno-DECLFILENAME",
       "--top-module", "zhao_field_v3_dispatch"] + [os.path.join(ROOT, f) for f in CONE]


def main():
    path = os.path.join(ROOT, RTL)
    with open(path, "r", encoding="utf-8", newline="") as f:
        gold = f.read()

    env = dict(os.environ, VERILATOR_ROOT=VROOT)
    r = subprocess.run(CMD, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        print("PREFLIGHT BROKEN: pristine RTL fails lint:")
        print(r.stderr[-2000:])
        return 1

    bad = 0
    # Entries here are all (name, old, new) against RTL, because the cone is
    # one file. The per-file snapshot machinery is kept anyway so this preflight
    # stays interchangeable with the others rather than quietly diverging.
    golds = {}
    for entry in MUTANTS:
        f_rel = entry[1] if len(entry) == 4 else RTL
        if f_rel not in golds:
            with open(os.path.join(ROOT, f_rel), encoding="utf-8", newline="") as f:
                golds[f_rel] = f.read()

    for entry in MUTANTS:
        name = entry[0]
        f_rel = entry[1] if len(entry) == 4 else RTL
        old, new = entry[-2], entry[-1]
        f_abs = os.path.join(ROOT, f_rel)
        try:
            with open(f_abs, "w", encoding="utf-8", newline="") as f:
                f.write(mutate(golds[f_rel], old, new))
            r = subprocess.run(CMD, capture_output=True, text=True, env=env)
            if r.returncode != 0:
                print("PREFLIGHT FAIL: %s does not lint:" % name)
                print(r.stderr[-1200:])
                bad += 1
            else:
                print("preflight ok: %s" % name)
        except ValueError as e:
            print("PREFLIGHT FAIL: %s anchor: %s" % (name, e))
            bad += 1
        finally:
            with open(f_abs, "w", encoding="utf-8", newline="") as f:
                f.write(golds[f_rel])

    for f_rel, g in golds.items():
        with open(os.path.join(ROOT, f_rel), encoding="utf-8", newline="") as f:
            if f.read() != g:
                print("PREFLIGHT BROKEN: %s not restored" % f_rel)
                return 1

    if bad:
        print("PREFLIGHT: %d mutant(s) unusable" % bad)
        return 1
    print("PREFLIGHT: all %d mutants lint and restore" % len(MUTANTS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
