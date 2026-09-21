r"""A wrapper mutant's PORT LIST against the module it wraps, both directions.

`mutant_copy_drift.py` answers one question: *is this copy older than what it
copies?* It is deliberately blind to a second one, and owner ruling R162 says so
in as many words:

    a wrapper can't drift in its body but its PORT LIST can, and
    mutant_copy_drift is blind to that half by design.

This is that half. A wrapper instantiates the real module with `.*`, so every
port must exist by name in the wrapper's own header. Two lanes landing in the
same window break it in opposite directions at once, which is exactly what
happened on 2026-09-21 (owner ruling R220):

  * packet UNTEX added a wrapper mutant;
  * packet POSTGATHER, branched BEFORE it existed, ADDED seventeen `gather_*`
    ports to the core and REMOVED fourteen `post_gd_*` / `post_gg_*` ports that
    had been entry I17's boundary tie-off, composing them internal;
  * POSTGATHER updated the one wrapper it could see. It could not see the other.

The result elaborated nowhere and **every static gate stayed green**:
`mutant_copy_drift` returned RC 0, because commit order was fine. The control
was discovered by RUNNING it, which is the expensive way.

WHY BOTH DIRECTIONS MATTER, and why a one-sided check would have missed half of
it: a port the wrapper LACKS breaks `.*` immediately, and a port the wrapper
still declares after the core dropped it breaks `.*` just as hard. The first
reads as "the mutant is out of date"; the second reads as "the core is broken",
which is the more expensive misdiagnosis -- a lane meeting it goes looking in
production RTL. Both are one `git diff` away from being obvious and neither is
visible to any other gate here.

NOT a substitute for elaborating the mutant. This compares two port lists as
TEXT; it cannot see a width that changed, a direction that flipped, or a body
that stopped meaning what it meant. It exists because those failures are loud
and this one is silent.
"""
from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# Each wrapper and the module it wraps. A wrapper that stops appearing here is
# a wrapper nobody checks, so the list is explicit rather than globbed.
PAIRS = [
    ("tests/mutants/zhao_console_core_untex_decl_mutant.sv",
     "fpga/rtl/prod/zhao_console_core.sv"),
    ("tests/mutants/zhao_console_core_slot_overflow_mutant.sv",
     "fpga/rtl/prod/zhao_console_core.sv"),
]

# A port declaration line. The trailing `_i` / `_o` is this tree's own
# convention and is what keeps parameters and locals out of the set.
_PORT = re.compile(
    r"^\s*(?:input|output)\s+.*?\b(\w+_[io])\s*,?\s*(?://.*)?$", re.M)


def ports(path):
    """The port names declared in a module header, as a set."""
    with io.open(os.path.join(REPO, path), encoding="utf-8",
                 errors="replace") as fh:
        text = fh.read()
    # The header ends at the first `\n);` -- everything after is the body, and
    # a body may legitimately mention a name that is not a port.
    end = text.find("\n);")
    return set(_PORT.findall(text if end < 0 else text[:end]))


def main():
    rc = 0
    for wrap, real in PAIRS:
        for p in (wrap, real):
            if not os.path.exists(os.path.join(REPO, p)):
                print("wrapper port parity: %s -- MISSING" % p)
                return 1
        wp, rp = ports(wrap), ports(real)
        missing, stale = sorted(rp - wp), sorted(wp - rp)

        name = os.path.basename(wrap)
        print("%-46s wrapper=%-5d real=%-5d missing=%-3d stale=%d"
              % (name, len(wp), len(rp), len(missing), len(stale)))

        if missing:
            print("  PORTS THE REAL MODULE HAS AND THE WRAPPER DOES NOT --")
            print("  `.*` cannot bind these; the control will not elaborate:")
            for p in missing:
                print("      %s" % p)
        if stale:
            print("  PORTS THE WRAPPER STILL DECLARES AND THE REAL MODULE "
                  "DROPPED --")
            print("  `.*` cannot bind these either, and the failure READS LIKE "
                  "A BROKEN CORE:")
            for p in stale:
                print("      %s" % p)
        if missing or stale:
            print("  Fix the WRAPPER, never the module. See owner ruling R220.")
            rc = 1
    if rc == 0:
        print("wrapper port parity: every wrapper matches its module exactly.")
    return rc


# SELF-CHECK. A parser that matched nothing would report perfect parity for
# every pair forever -- this repository's most repeated failure, and the one
# this very tool exists because of. Prove the pattern still bites, in both
# the comma and the final-port forms, and that it ignores what it must.
assert _PORT.findall("  input  var logic [31:0] foo_i,") == ["foo_i"]
assert _PORT.findall("  output logic bar_o") == ["bar_o"]
assert _PORT.findall("  output logic [W-1:0] baz_o,  // trailing note") == ["baz_o"]
assert _PORT.findall("  parameter int unsigned NOPE = 4,") == []
assert _PORT.findall("  localparam logic thing_i = 1;") == []

if __name__ == "__main__":
    sys.exit(main())
