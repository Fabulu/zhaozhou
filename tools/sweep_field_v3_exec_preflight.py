#!/usr/bin/env python3
"""Preflight for tools/sweep_field_v3_exec.sh: every mutant must LINT.

A mutant that fails to elaborate would be scored CAUGHT by a compile failure,
which is the most flattering possible way to be wrong (house rule). Imports
the mutant module directly so the sweep and the preflight cannot disagree.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_field_v3_exec_mutants import MUTANTS, RTL, mutate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VROOT = os.environ.get(
    "VERILATOR_ROOT", r"C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator")
VBIN = os.path.normpath(os.path.join(VROOT, "..", "..", "bin", "verilator_bin.exe"))

# This block INSTANTIATES three others, so the lint must see the whole cone.
# Linting the top file alone would fail on missing modules for every mutant
# equally -- which reads as "no mutant builds" rather than as a broken command.
CONE = [
    RTL,
    "fpga/rtl/field/zhao_field_v3_rf.sv",
    "fpga/rtl/field/zhao_field_alu.sv",
    "fpga/rtl/field/zhao_field_mul.sv",
]

CMD = [VBIN, "--lint-only", "-Wall", "-Wno-DECLFILENAME",
       "--top-module", "zhao_probe_v3_exec"] + [os.path.join(ROOT, f) for f in CONE]


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
    # Entries are (name, old, new) against RTL, or (name, path, old, new) when
    # the mutation lands in another file of the cone -- the register file is a
    # separate module and is swept from this table. Each file gets its own gold
    # snapshot, and each is restored after every mutant.
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
