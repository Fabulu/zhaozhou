#!/usr/bin/env python3
"""Is a swept RTL file free of every mutant its table can apply?

WHY THIS EXISTS
---------------
2026-08-28. A mutation sweep was killed mid-run three times in one session.
Twice it stranded a mutant in the RTL, and once that mutant was COMMITTED AND
PUSHED -- `wb_reg_o = s4_dst_r + RW'(1)` rode into a commit whose message said
the block was fixed.

It survived every check that was actually run because:

  * it changes only an OBSERVATION port, so the block still computed correctly
    and the differential still passed;
  * the eyeball diff before committing was filtered through `head -8` and a
    comment filter, and the line did not make the window;
  * and the killed task left ORPHANED bash processes that kept mutating the
    file after the harness reported the task dead -- so a tree verified clean
    at one moment was dirty a minute later.

A sweep restores by copying a `gold` snapshot back. If the snapshot is taken
when the file is ALREADY mutated, "restore" faithfully restores the mutant.
Nothing in the driver can notice that. This checks from the outside.

USAGE
  python tools/sweep_check_clean.py tools/sweep_field_v3_exec_mutants.py

Exit 0 = clean. Exit 1 = a mutant's text is present. Exit 2 = usage/other.
"""

import importlib.util
import io
import os
import sys


def load_table(path):
    spec = importlib.util.spec_from_file_location("mut", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: sweep_check_clean.py <mutants.py> [more...]\n")
        return 2

    bad = 0
    for table_path in argv[1:]:
        mod = load_table(table_path)
        rtl = mod.RTL
        if not os.path.exists(rtl):
            sys.stderr.write("MISSING: %s\n" % rtl)
            bad += 1
            continue
        text = io.open(rtl, encoding="utf-8", newline="").read()
        nl = "\r\n" if "\r\n" in text else "\n"

        found = []
        for entry in mod.MUTANTS:
            name, old, new = entry[0], entry[-2], entry[-1]
            o = old.replace("\n", nl)
            n = new.replace("\n", nl)
            # A mutant is PRESENT when its replacement text is in the file and
            # the text it replaced is not. Checking both ways matters: some
            # mutations only add a term, so the mutated text can legitimately
            # contain the original as a substring.
            if n in text and o not in text:
                found.append(name)

        if found:
            print("MUTANT TEXT PRESENT in %s (from %s):" % (rtl, os.path.basename(table_path)))
            for f in found:
                print("  %s" % f)
            bad += 1
        else:
            print("clean: %s (%d mutants checked)" % (rtl, len(mod.MUTANTS)))

    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
