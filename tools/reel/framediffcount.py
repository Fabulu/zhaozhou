#!/usr/bin/env python3
"""framediffcount.py -- how many frames differ between two rendered sequences?

    python tools/reel/framediffcount.py <dir-A> <dir-B>

Prints ONE integer: the number of same-index frames whose bytes differ. Nothing
else, so a shell can read it with `$(...)` and compare it numerically.

WHY IT IS A COMMITTED FILE AND NOT A HEREDOC IN THE GATE MATRIX. The pass-26
matrix needs this inside a bash function, and a python heredoc nested inside the
matrix's own heredoc is how a generated script acquires a quoting bug that only
shows up as a leg silently reporting an empty string -- which `[ "" -gt 0 ]`
then turns into a FAIL that looks like a dead control. A control that reports
dead because the harness miscounted is worse than no leg at all.

⚠ IT DELIBERATELY EXITS 0 WHETHER OR NOT ANYTHING DIFFERS. The caller decides
what the count means: a firing control wants it above zero, an identity leg
wants it at zero. Baking a verdict in here would make one of those two callers
read the exit code backwards.

Mismatched frame counts are a DIFFERENT fault from differing pixels, so they
exit 2 with a message on stderr rather than being folded into the count.
"""
import glob
import os
import sys


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: framediffcount.py <dir-A> <dir-B>\n")
        return 2
    a = sorted(glob.glob(os.path.join(argv[1], "*.rgb")))
    b = sorted(glob.glob(os.path.join(argv[2], "*.rgb")))
    if not a or not b:
        sys.stderr.write("framediffcount: no .rgb frames in %s\n"
                         % (argv[1] if not a else argv[2]))
        return 2
    if len(a) != len(b):
        sys.stderr.write("framediffcount: %d frames vs %d -- not comparable\n"
                         % (len(a), len(b)))
        return 2
    n = 0
    for x, y in zip(a, b):
        with open(x, "rb") as fx, open(y, "rb") as fy:
            if fx.read() != fy.read():
                n += 1
    sys.stdout.write("%d\n" % n)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
