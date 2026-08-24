"""sweep_apply_mutant.py -- apply one anchored mutation to an RTL file.

Every sweep in this tree so far inlines this as a `python - <<'PY'` heredoc.
RUN-20260824-0317's eighth disclosed failure was two such heredocs failing to
parse with "unexpected EOF" on syntactically fine content, costing two retries;
the workaround -- write the script to a file first -- is the thing worth
knowing, so it is written down here instead of rediscovered.

Reads OLD, NEW and F from the environment. Exits nonzero unless the anchor
occurs EXACTLY once and the file actually changes. A mutation that silently
applied nowhere would leave the pristine source in place, the lanes would pass,
and the sweep would report a SURVIVOR that was never tried -- which is the same
failure class as scoring a build error as a caught mutant, in the other
direction.

`os.utime(p, None)` stamps NOW, never the future: a mutant stamped forward
outranks the pristine source restored after it, and the build system then keeps
using the mutant.
"""

import io
import os
import sys

path = os.environ["F"]
raw = io.open(path, encoding="utf-8", newline="").read()

CR, LF = chr(13), chr(10)
NL = CR + LF if CR + LF in raw else LF

old = os.environ["OLD"].replace(LF, NL)
new = os.environ["NEW"].replace(LF, NL)

n = raw.count(old)
if n != 1:
    sys.stderr.write("ANCHOR NOT UNIQUE in %s (%d occurrences)\n" % (path, n))
    sys.exit(9)
if old == new:
    sys.stderr.write("MUTANT IDENTICAL TO BASE in %s\n" % path)
    sys.exit(9)

io.open(path, "w", encoding="utf-8", newline="").write(raw.replace(old, new, 1))
os.utime(path, None)
