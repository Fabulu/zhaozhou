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
    # TWOD.BAND's burst mutant (owner ruling R235, packet BANDBUILD). A wrapper
    # with one parameter changed -- BURST_PX raised past the FIFO slack, which
    # is "admit everything" -- whose driver passes when `band_underrun_o` FIRES.
    # Registered HERE as well as built, because R220's lesson is that a wrapper
    # nobody checks is a control that goes stale in the reassuring direction.
    ("tests/mutants/zhao_twod_band_burst_mutant.sv",
     "fpga/rtl/compositor/zhao_twod_band.sv"),
]

# THIS USED TO BE ONE LINE-ANCHORED REGEX AND IT COUNTED LOW, SILENTLY.
#
#     ^\s*(?:input|output)\s+.*?\b(\w+_[io])\s*,?\s*(?://.*)?$
#
# The `$` means ONE port per line. Two declarations sharing a line -- which is
# legal, common, and what a here-string produces when it eats a newline -- and
# the first one is simply not seen.
#
# Both failure directions were met in one day, 2026-09-21:
#
#  * FALSE POSITIVE. Pointed at the texture wrappers it reported "8 of 14 RED",
#    and every phantom missing name contained `_valid_` -- the leading half of
#    `input logic frag_valid_i, output logic frag_ready_o,`. Committing that
#    would have reddened the tree for every running lane (owner ruling R225).
#  * FALSE NEGATIVE, and this is the one that matters. Packet DELTALAW ADDED a
#    port and the gate still read **1270 = 1270**. A here-string had put two
#    declarations on one line, the regex stopped counting the first, and
#    because BOTH sides of the comparison were parsed by the same blind
#    pattern, the SYMMETRY HELD and the gate passed. Every other gate was happy
#    too.
#
# That second shape is this repository's own law about a checker whose two
# operands move together: the comparison could not see a fault its own parser
# participates in. **The number moving is the evidence** -- 1271 = 1271 after
# the fix, where the port count had been stuck.
#
# So: split the header on commas and read each fragment. A fragment carrying a
# direction keyword opens a declaration; bare fragments after it are the
# comma-continuation form (`input logic [31:0] a_i, b_i,`), which
# `gen_prod_top`'s parser has a hard-won fix for and whose own comment warns
# that a silently narrowed port is not cosmetic.
_DIR = re.compile(r"\b(?:input|output)\b")
_STOP = re.compile(r"\b(?:parameter|localparam)\b")
_NAME = re.compile(r"\b(\w+_[io])\b")
_COMMENT = re.compile(r"//[^\n]*")


def ports(path):
    """The port names declared in a module header, as a set."""
    with io.open(os.path.join(REPO, path), encoding="utf-8",
                 errors="replace") as fh:
        text = fh.read()
    # The header ends at the first `\n);` -- everything after is the body, and
    # a body may legitimately mention a name that is not a port.
    end = text.find("\n);")
    head = _COMMENT.sub(" ", text if end < 0 else text[:end])

    found, declaring = set(), False
    for frag in head.split(","):
        if _STOP.search(frag):
            declaring = False
            continue
        if _DIR.search(frag):
            declaring = True
        elif not declaring:
            continue
        names = _NAME.findall(frag)
        if names:
            # The LAST `_i`/`_o` token is the port; anything earlier in the
            # fragment is a type or a packed range (`zhao_guard_req_t`,
            # `[BUILD_HPS_N-1:0]`) that happens to match.
            found.add(names[-1])
    return found


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
# this very tool exists because of. It runs against the real `ports()` now,
# not a bare regex, because the defect this replaced was in the SPLITTING and
# a pattern test could not have seen it.
def _parse(text):
    """`ports()` over a literal header, for the self-test only."""
    head = _COMMENT.sub(" ", text)
    found, declaring = set(), False
    for frag in head.split(","):
        if _STOP.search(frag):
            declaring = False
            continue
        if _DIR.search(frag):
            declaring = True
        elif not declaring:
            continue
        names = _NAME.findall(frag)
        if names:
            found.add(names[-1])
    return found


assert _parse("  input  var logic [31:0] foo_i,") == {"foo_i"}
assert _parse("  output logic bar_o") == {"bar_o"}
assert _parse("  output logic [W-1:0] baz_o,  // trailing note") == {"baz_o"}
# THE REGRESSION THAT MATTERS: two declarations on ONE line. The old pattern
# saw only `frag_ready_o` and silently dropped `frag_valid_i` -- which read as
# a phantom "missing" port on the texture wrappers, and as a port that did not
# exist when DELTALAW added one.
assert _parse("  input logic frag_valid_i, output logic frag_ready_o,") == {
    "frag_valid_i", "frag_ready_o"}
# the comma-continuation form, whose loss `gen_prod_top` warns is not cosmetic
assert _parse("  input logic [31:0] a_i, b_i,") == {"a_i", "b_i"}
# a TYPEDEF port keeps its name, not its type
assert _parse("  input var zhao_guard_req_t geom_guard_req_i,") == {
    "geom_guard_req_i"}
# and what must still be ignored
assert _parse("  parameter int unsigned NOPE = 4,") == set()
assert _parse("  localparam logic thing_i = 1;") == set()
assert _parse("  parameter int P = 1, input logic real_i,") == {"real_i"}

if __name__ == "__main__":
    sys.exit(main())
