#!/usr/bin/env python3
"""Preflight for tools/sweep_field_plan.sh: every mutant must COMPILE.

Rationale inherited from sweep_geom_wcache_preflight.py: before the discard
guards existed, a non-compiling mutant was scored as CAUGHT, which is the
most flattering possible way to be wrong. This preflight applies each mutant
to the pristine tree, syntax-checks the one translation unit that includes
both mutated files (zfield_plan.cpp includes zfield_steps.hpp), and restores
the gold in a finally.

Imports the mutant module directly so the sweep and the preflight cannot
disagree about what a mutant is.
"""

import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_field_plan_mutants import MUTANTS, PLAN, mutate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GXX = os.environ.get("ZHAO_GXX", r"C:\programmieren\dsstuff\mingw64\bin\g++.exe")

CMD = [
    GXX, "-fsyntax-only", "-std=c++17", "-Wall",
    "-I", os.path.join(ROOT, "reference", "include"),
    "-I", os.path.join(ROOT, "runtime", "include"),
    os.path.join(ROOT, PLAN),
]


def main():
    files = sorted({m[1] for m in MUTANTS})
    golds = {}
    for rel in files:
        path = os.path.join(ROOT, rel)
        with open(path, "r", encoding="utf-8", newline="") as f:
            golds[rel] = f.read()

    # the pristine tree itself must pass the same check, or the tool is broken
    r = subprocess.run(CMD, capture_output=True, text=True)
    if r.returncode != 0:
        print("PREFLIGHT BROKEN: pristine tree fails the syntax check:")
        print(r.stderr[-2000:])
        return 1

    bad = 0
    for i, (name, rel, old, new) in enumerate(MUTANTS):
        path = os.path.join(ROOT, rel)
        try:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(mutate(golds[rel], old, new))
            r = subprocess.run(CMD, capture_output=True, text=True)
            if r.returncode != 0:
                print("PREFLIGHT FAIL: %s does not compile:" % name)
                print(r.stderr[-1500:])
                bad += 1
            else:
                print("preflight ok: %s" % name)
        except ValueError as e:
            print("PREFLIGHT FAIL: %s anchor: %s" % (name, e))
            bad += 1
        finally:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(golds[rel])

    # prove the restore
    for rel in files:
        path = os.path.join(ROOT, rel)
        with open(path, "r", encoding="utf-8", newline="") as f:
            if f.read() != golds[rel]:
                print("PREFLIGHT BROKEN: %s not restored" % rel)
                return 1

    if bad:
        print("PREFLIGHT: %d mutant(s) unusable" % bad)
        return 1
    print("PREFLIGHT: all %d mutants compile and restore" % len(MUTANTS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
