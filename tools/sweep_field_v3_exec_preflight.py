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
    "fpga/rtl/synth/zhao_probe_banked_rf.sv",
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
    # This block's mutants all live in one file, so the table is a 3-tuple
    # (name, old, new) rather than the multi-file 4-tuple form.
    for name, old, new in MUTANTS:
        try:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(mutate(gold, old, new))
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
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(gold)

    with open(path, "r", encoding="utf-8", newline="") as f:
        if f.read() != gold:
            print("PREFLIGHT BROKEN: RTL not restored")
            return 1

    if bad:
        print("PREFLIGHT: %d mutant(s) unusable" % bad)
        return 1
    print("PREFLIGHT: all %d mutants lint and restore" % len(MUTANTS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
