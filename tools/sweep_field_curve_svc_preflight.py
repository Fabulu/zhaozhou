#!/usr/bin/env python3
"""Preflight for tools/sweep_field_curve_svc.sh: every mutant must LINT.

A mutant that fails to elaborate would be scored CAUGHT by a compile failure,
which is the most flattering possible way to be wrong (house rule). Imports
the mutant module directly so the sweep and the preflight cannot disagree.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_field_curve_svc_mutants import MUTANTS, RTL, mutate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VROOT = os.environ.get(
    "VERILATOR_ROOT", r"C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator")
VBIN = os.path.normpath(os.path.join(VROOT, "..", "..", "bin", "verilator_bin.exe"))

CMD = [VBIN, "--lint-only", "-Wall", "-Wno-DECLFILENAME",
       "--top-module", "zhao_probe_curve_svc", os.path.join(ROOT, RTL)]


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
    for name, rel, old, new in MUTANTS:
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
