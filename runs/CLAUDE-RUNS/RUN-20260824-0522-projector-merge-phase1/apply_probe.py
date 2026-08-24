"""apply_probe.py -- apply one anchored text substitution to an RTL file.

Written to a FILE rather than piped as a `python - <<'PY'` heredoc: two such
heredocs failed to parse with "unexpected EOF" on syntactically fine content in
RUN-20260824-0317 (failure 8), costing two retries. The workaround is the thing
worth knowing.

Reads OLD, NEW and F from the environment. Exits nonzero unless the anchor
occurs EXACTLY once and actually changes the file -- a probe that silently
applied nowhere would report "not caught" and be indistinguishable from a
differential that is blind.
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
    sys.stderr.write("PROBE IDENTICAL TO BASE in %s\n" % path)
    sys.exit(9)

io.open(path, "w", encoding="utf-8", newline="").write(raw.replace(old, new, 1))
