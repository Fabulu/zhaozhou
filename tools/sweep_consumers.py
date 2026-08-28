#!/usr/bin/env python3
"""Which test targets ELABORATE a given RTL file.

WHY THIS REPLACES A `grep -B12 "TOP_MODULE <module>"` IN EVERY DRIVER
---------------------------------------------------------------------
Guard 7 exists because a mutated file elaborated by a target the sweep does
not run leaves a mutant-derived model sitting in the build tree, unscored.
Every driver implemented that guard by grepping for its block as a TOP_MODULE.

That rule was true only while every swept block was its own top. It stopped
being true the moment a block was composed into a bigger one: on 2026-08-28
`zhao_probe_v3_exec` became a submodule of `zhao_probe_v3_engine`, the grep
found nothing, and the sweep aborted with a roster error naming an empty set.
The guard was RIGHT to refuse -- it could no longer see what it was guarding.

The real rule is SOURCES, not TOP_MODULE: a file is elaborated by every
verilate() block that lists it, whether as the top or three levels down.

AND SOURCES IS OFTEN A VARIABLE. Most raster and terrain cones are written
`SOURCES ${ZHAO_RASTER_EARLYZ_SV}`, so a scanner reading only literals reports
"nothing elaborates this file" -- the same false all-clear this tool exists to
remove, wearing the other face. set() definitions are expanded first.

    python tools/sweep_consumers.py <rtl-path> [cmakelists]

Prints one target per line, sorted. Exit 1 if no target elaborates the file at
all, which is itself a finding: a swept block nothing builds is a block whose
sweep proves nothing.
"""

import io
import os
import re
import sys

# LF, NOT CRLF, AND THAT IS LOAD-BEARING.
#
# Python on Windows translates every '\n' on stdout into '\r\n'. The sweep
# drivers read this output into a shell variable and compare the names
# against their TARGETS list, so a stray carriage return makes 'foo' and
# 'foo' plus a carriage return different strings -- and the consumer-roster guard then reports a
# target it IS running as one it is not.
#
# That went unnoticed while every sweep had exactly ONE consumer, because
# the abort only fires on a name that fails to match and there was nothing
# else to compare. The first sweep with TWO consumers exposed it, which is
# an uncomfortable thing to learn about a guard: it had been comparing
# strings that could never be equal, and passing.
sys.stdout.reconfigure(newline="\n")

VERILATE = re.compile(r"^\s*verilate\((\w+)", re.M)
SETVAR = re.compile(r"^\s*set\((\w+)\b", re.M)
REF = re.compile(r"\$\{(\w+)\}")


def _balanced(text, open_at):
    """Index of the paren closing the one at open_at."""
    depth, j = 0, open_at
    while j < len(text):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                return j
        j += 1
    return len(text)


def expand_vars(text):
    values = {}
    for m in SETVAR.finditer(text):
        i = text.index("(", m.start())
        values[m.group(1)] = text[i + 1:_balanced(text, i)]

    # Two passes, because a source list may be built from another one.
    for _ in range(2):
        for name in list(values):
            values[name] = REF.sub(
                lambda k: values.get(k.group(1), k.group(0)), values[name])
    return REF.sub(lambda k: values.get(k.group(1), k.group(0)), text)


def consumers(rtl, cmakelists):
    want = os.path.basename(rtl)
    text = expand_vars(
        io.open(cmakelists, encoding="utf-8", errors="replace").read())

    found = set()
    for m in VERILATE.finditer(text):
        i = text.index("(", m.start())
        body = text[i:_balanced(text, i)]
        if want in body:
            found.add(m.group(1))
    return sorted(found)


def prefix_of(target, cmakelists):
    """The verilate() PREFIX for a target -- the model directory's name.

    THE MODEL DIRECTORY IS NAMED FOR THE TOP, NOT FOR THE MUTATED FILE. Every
    driver hardcoded `V<its own module>.dir` for the presence check and for the
    binary-hash discard check. That is the same TOP_MODULE assumption as the
    roster guard, and it broke the same way: once zhao_probe_v3_exec became a
    submodule its model directory stopped existing, the presence check failed,
    and the sweep aborted on a build that had in fact linked cleanly.

    Worse than the abort is what a coincidence would have done. The discard
    check hashes that directory to prove a mutant really re-elaborated; hashing
    a directory the mutation cannot reach would pass every mutant through as
    "changed" while scoring a model that never moved -- a sweep reporting full
    marks over nothing at all.
    """
    text = expand_vars(
        io.open(cmakelists, encoding="utf-8", errors="replace").read())
    for m in VERILATE.finditer(text):
        if m.group(1) != target:
            continue
        i = text.index("(", m.start())
        body = text[i:_balanced(text, i)]
        pm = re.search(r"PREFIX\s+(\w+)", body)
        if pm:
            return pm.group(1)
    return None


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: sweep_consumers.py <rtl-path> [cmakelists]\n")
        sys.stderr.write("       sweep_consumers.py --prefix <target> [cmakelists]\n")
        return 2
    if argv[1] == "--prefix":
        if len(argv) < 3:
            sys.stderr.write("usage: sweep_consumers.py --prefix <target>\n")
            return 2
        got = prefix_of(argv[2],
                        argv[3] if len(argv) > 3 else "tests/CMakeLists.txt")
        if not got:
            sys.stderr.write("no verilate() PREFIX for target %s\n" % argv[2])
            return 1
        print(got)
        return 0
    lists = argv[2] if len(argv) > 2 else "tests/CMakeLists.txt"
    got = consumers(argv[1], lists)
    if not got:
        sys.stderr.write("no verilate() target elaborates %s\n" % argv[1])
        return 1
    for t in got:
        print(t)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
