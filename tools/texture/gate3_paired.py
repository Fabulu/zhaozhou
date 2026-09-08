"""P0-C Stage C, GATE 3: the two island tops, one stimulus, one retired stream.

The architecture's third gate is the paired run: the ORACLE
(`zhao_texture_island_top`, unmodified) and the V3 composition
(`zhao_texture_island_v3_top`) driven with identical stimulus, their retired
streams compared byte for byte on rgb/a/tag/refused AND ON ORDER.

WHY THE COMPARISON IS A FILE DIFF AND NOT A THIRD HARNESS
---------------------------------------------------------
The obvious shape is a test that instantiates both tops and drives them
together. That is a SECOND DRIVER, and a second driver is a copy of the first.
`island_composed_directed.cpp` already argues this about itself and refuses to
be copied for exactly this reason: the moment a paired harness and the
119-check harness disagree about stimulus, the comparison is between two
different experiments while still producing two numbers that look comparable.

So both tops run the SAME source with `--dump`, and this compares the files.
A byte compare cannot drift from anything.

WHY ORDER IS PART OF THE RECORD
-------------------------------
v3own's 5.4 emits in strict owner order. A composition that retired every
correct colour in the wrong sequence would satisfy a set comparison and be
broken. The records are compared positionally.
"""

import io
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ORACLE = os.path.join(ROOT, "build", "tests", "test_island_composed_directed.exe")
V3 = os.path.join(ROOT, "build", "tests", "test_island_v3_composed_directed.exe")


def read_stream(path):
    return [l.rstrip("\r\n") for l in io.open(path, encoding="utf-8")
            if l.strip()]


def compare(a, b):
    """(n_compared, [descriptions of the first differences])."""
    diffs = []
    if len(a) != len(b):
        diffs.append(
            "STREAM LENGTH: oracle retired %d records, v3 retired %d" %
            (len(a), len(b)))
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            diffs.append("record %d: oracle [%s] != v3 [%s]" % (i, a[i], b[i]))
            if len(diffs) >= 8:
                diffs.append("... further differences not listed")
                break
    return min(len(a), len(b)), diffs


# A KNOWN-BAD PAIR, checked on every run. A comparator that has never been shown
# to catch a difference has not been tested, and this one's whole job is to
# report "no difference".
_A = ["0 1 AABBCC 255 0", "0 2 112233 128 0", "1 3 445566 64 1"]
_B_SAME = list(_A)
_B_COLOUR = ["0 1 AABBCC 255 0", "0 2 112234 128 0", "1 3 445566 64 1"]
_B_ORDER = ["0 2 112233 128 0", "0 1 AABBCC 255 0", "1 3 445566 64 1"]
_B_SHORT = _A[:2]


def self_fire_test():
    if compare(_A, _B_SAME)[1]:
        return False                      # must report identical as identical
    if not compare(_A, _B_COLOUR)[1]:
        return False                      # one wrong channel
    if not compare(_A, _B_ORDER)[1]:      # SAME SET, wrong order
        return False
    if not compare(_A, _B_SHORT)[1]:
        return False                      # truncated stream
    return True


def run(exe, out):
    if not os.path.exists(exe):
        print("GATE 3: missing %s -- build it first" % exe)
        return None
    if os.path.exists(out):
        os.unlink(out)
    r = subprocess.run([exe, "--dump", out], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = r.stdout.decode("utf-8", "replace")
    if r.returncode != 0:
        print("GATE 3: %s exited %d -- its own checks must pass before the "
              "paired comparison means anything" % (os.path.basename(exe),
                                                    r.returncode))
        print(text[-2000:])
        return None
    # THE DUMP MUST EXIST AND BE NEWER THAN THE RUN.
    #
    # A binary built before `--dump` existed IGNORES the flag, exits 0, and
    # leaves whatever file was there last time. The comparator then diffs two
    # stale files and reports a clean pass -- a broken instrument lying in the
    # flattering direction, which is exactly what happened on this tool's
    # first run. Unlinking first turns that into a loud failure.
    if not os.path.exists(out):
        print("GATE 3: %s ran but wrote no stream to %s -- the binary" % 
              (os.path.basename(exe), out))
        print("        predates --dump. Rebuild it; do NOT compare stale files.")
        return None
    return out


def main(argv):
    if not self_fire_test():
        print("GATE 3 COMPARATOR BROKEN: it no longer distinguishes a changed "
              "colour, a reordered stream or a truncated one. Refusing to "
              "report a pass.")
        return 2

    tmp = os.path.join(ROOT, "build", "tests")
    a = run(ORACLE, os.path.join(tmp, "gate3_oracle.txt"))
    if a is None:
        return 1
    b = run(V3, os.path.join(tmp, "gate3_v3.txt"))
    if b is None:
        return 1

    sa, sb = read_stream(a), read_stream(b)

    # A comparison of two empty streams passes trivially and means nothing.
    if len(sa) < 100:
        print("GATE 3 VACUOUS: the oracle retired only %d records. A byte "
              "compare of near-empty streams is not evidence." % len(sa))
        return 2

    n, diffs = compare(sa, sb)
    print("GATE 3: compared %d retired records (oracle %d, v3 %d)"
          % (n, len(sa), len(sb)))
    if diffs:
        print("GATE 3 FAILED -- the two tops do not retire the same stream:")
        for d in diffs:
            print("  - " + d)
        return 1
    print("GATE 3 PASSES: the V3 composition retires byte-identical rgb, alpha, "
          "tag and refused, in the same ORDER, as the unmodified oracle.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
