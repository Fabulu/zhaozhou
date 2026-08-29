#!/usr/bin/env python3
"""Preflight for tools/sweep_field_v3_svcpath.sh: every mutant must LINT.

A mutant that fails to elaborate would be scored CAUGHT by a compile failure,
which is the most flattering possible way to be wrong (house rule). Imports
the mutant module directly so the sweep and the preflight cannot disagree.

THE CONE IS SIX FILES HERE, NOT ONE, AND THAT MATTERS MORE THAN USUAL
---------------------------------------------------------------------
Every other block in this family is a leaf -- it reaches the multiplier bank
through ports, so its cone is the one file it lives in and a lint failure can
only be the mutant's fault.

This one INSTANTIATES the four blocks it composes, which is the whole reason
it exists. So the cone must be the same six files the verilate() target names,
in the same order, or the preflight lints a different design from the one the
sweep scores -- and the mutants in this table are almost all port maps, which
is exactly the class of edit that lints fine in isolation and fails only when
the module on the other end is real.

The list is duplicated from tests/CMakeLists.txt, which is a drift risk worth
naming: if the target gains a source and this does not, the preflight goes on
passing while linting a design missing a module. The consumer-roster guard in
the driver reads the real SOURCES, so the sweep itself cannot drift -- only
this early check can, and it fails LOUD (missing module) rather than quiet.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_field_v3_svcpath_mutants import MUTANTS, RTL, mutate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VROOT = os.environ.get(
    "VERILATOR_ROOT", r"C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator")
VBIN = os.path.normpath(os.path.join(VROOT, "..", "..", "bin", "verilator_bin.exe"))

CONE = [
    "fpga/rtl/field/zhao_field_ops_pkg.sv",
    RTL,
    "fpga/rtl/field/zhao_field_v3_dispatch.sv",
    "fpga/rtl/field/zhao_field_v3_noise.sv",
    # THE SECOND SERVICE, 2026-08-29. This cone and the SOURCES list in
    # tests/CMakeLists.txt are two places that must agree about the same
    # set of files -- the shape of seam defect this engine has produced
    # four times. They are not yet derived from one another; when this
    # list is next wrong, that is the fix.
    "fpga/rtl/synth/zhao_probe_curve_svc.sv",
    "fpga/rtl/field/zhao_field_v3_spline.sv",
    "fpga/rtl/field/zhao_field_v3_normalize.sv",
    "fpga/rtl/field/zhao_field_isqrt.sv",
    "fpga/rtl/field/zhao_field_rcp24_rom.sv",
    "fpga/rtl/field/zhao_field_v3_rot.sv",
    "fpga/rtl/field/zhao_field_sin.sv",
    "fpga/rtl/field/zhao_field_sin_rom.sv",
    "fpga/rtl/field/zhao_field_v3_mulbank.sv",
    "fpga/rtl/field/zhao_field_v3_wbarb.sv",
    "fpga/rtl/field/zhao_field_mul.sv",
]

CMD = [VBIN, "--lint-only", "-Wall", "-Wno-DECLFILENAME",
       "--top-module", "zhao_field_v3_svcpath"] + [os.path.join(ROOT, f) for f in CONE]


def main():
    path = os.path.join(ROOT, RTL)
    with open(path, "r", encoding="utf-8", newline="") as f:
        f.read()

    env = dict(os.environ, VERILATOR_ROOT=VROOT)
    r = subprocess.run(CMD, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        print("PREFLIGHT BROKEN: pristine RTL fails lint:")
        print(r.stderr[-2000:])
        return 1

    bad = 0
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
