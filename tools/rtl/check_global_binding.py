#!/usr/bin/env python3
"""check_global_binding.py -- a global pin may not be a per-fragment binding.

THE DEFECT THIS EXISTS FOR, AND THE GATE IT PATCHES
---------------------------------------------------
`zhao_texture_island_top` declares `bind_base_i` and `bind_mode_i` as
module-global inputs and wires them straight into the TMU planner's
`req_base_i` / `req_mode_i`.  The planner registers them at its own T0
handshake, so a reader hunting for an unregistered pin finds nothing wrong.

The defect is SCOPE, not combinational depth.  The captured word has no
correspondence to WHICH fragment is being planned: a mode cannot travel per
fragment, which is why the composed fixture must run fully drained phases, one
per mode, and why a single frame cannot mix a CLUT sheet with a direct-colour
one.  V3 section 0 point F states the law -- "Resolve actual bindings rather
than using late global base/mode pins."

`check_ingress_capture.py` cannot see it.  That gate watches an ingress PREFIX
for reads outside the capture region, and the island contract watches `frag_`
only.  Adding a `req_` prefix contract to it was tried and is the wrong shape:
it counts fifteen "offences" that are mostly port declarations plus the
planner's own legitimate T0 capture, and a baseline pinned to that number would
be an instrument measuring noise.  The rule here is structural and needs one
line of evidence, not a prefix sweep.

  RESULT: a V3 that keeps a global mode/base pin would PASS the gate that
  exists to prevent exactly that.  An unwatched known defect had become a route
  for its own replacement to be wrong.

WHY IT IS PINNED RATHER THAN SIMPLY FAILING
-------------------------------------------
The defect is real and its repair is a work item, so this gate would be red
from the day it is written until the day the descriptor lands -- and a gate
that is red for months is one people learn to ignore, which is worse than the
hole.  So the current count is PINNED.

The gate fails if the count goes UP, which is the obvious half.  It also fails
if the count goes DOWN, which is the half that makes it work rather than rot:
a baseline that only fails upward outlives its defect and then silently guards
nothing, whereas one that fails downward forces whoever repairs the defect to
retire the baseline in the same change.  That is how this reaches zero and goes
green for a reason instead of by neglect.

SELF-TEST AT IMPORT
-------------------
CLAUDE.md records nine measuring tools found wrong in one session, every one
failing in the direction that made the answer look better -- a regex with no
provision for a width bracket reporting "0 counters match", a `^` without
re.MULTILINE reporting "0 arrays declared".  Precision at zero is a tell.  So
the patterns below are asserted against a known-bad and a known-good example at
import: if the matcher ever stops matching, this file refuses to load rather
than reporting a reassuring clean.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# One rule: a global binding pin connected into a per-fragment consumer's port.
# Matched on the instantiation connection itself -- `.req_mode_i(bind_mode_i)`
# -- because that single line IS the defect, and it is the line that has to
# change when the descriptor lands.
CONNECT = re.compile(
    r"\.\s*(req_(?:base|mode)_i)\s*\(\s*(bind_(?:base|mode)_i)\s*\)")

# --- the self-test, run at import ------------------------------------------
_KNOWN_BAD = "      .req_base_i(bind_base_i), .req_mode_i(bind_mode_i),"
_KNOWN_GOOD = "      .req_base_i(desc_base_c), .req_mode_i(desc_mode_c),"
assert len(CONNECT.findall(_KNOWN_BAD)) == 2, (
    "check_global_binding: the connection pattern no longer matches its own "
    "known-bad example, so it would report clean forever")
assert not CONNECT.findall(_KNOWN_GOOD), (
    "check_global_binding: the connection pattern matches a per-fragment "
    "descriptor, so it would report a false offence and be ignored")

RULES = [
    {
        "path": "fpga/rtl/texture/zhao_texture_island_top.sv",
        "known_offences": None,   # measured on first run; see main()
        "why": ("V3 section 0 point F -- resolve real bindings, not late global "
                "base/mode pins. A per-fragment binding is ALREADY plumbed "
                "eight stages (frag_binding_i -> tmu_binding_o -> "
                "fr_tmu_binding) and consumed by nothing, so the repair is "
                "largely connection rather than new state."),
    },
]

# The pinned baseline. Kept beside the rule rather than inside it so that
# changing it is a visible, deliberate edit rather than a tweak buried in a
# dict literal.
BASELINE = {
    "fpga/rtl/texture/zhao_texture_island_top.sv": 2,
}


def scan(path):
    full = os.path.join(REPO, path)
    if not os.path.exists(full):
        return None, "file does not exist"
    with open(full, encoding="utf-8", errors="replace") as f:
        text = f.read()
    hits = []
    for n, raw in enumerate(text.split("\n"), start=1):
        line = raw.split("//", 1)[0]
        for m in CONNECT.finditer(line):
            hits.append((n, m.group(1), m.group(2), raw.strip()))
    return hits, None


def main(argv):
    bad = 0
    for rule in RULES:
        path = rule["path"]
        hits, err = scan(path)
        if hits is None:
            print("check_global_binding: cannot read %s (%s) -- refusing to "
                  "report clean" % (path, err))
            return 2
        pinned = BASELINE.get(path)
        if pinned is None:
            print("check_global_binding: %s has no pinned baseline; measured "
                  "%d. Add it to BASELINE." % (path, len(hits)))
            return 2
        if len(hits) == pinned:
            print("check_global_binding: %s -- %d PINNED global binding "
                  "connection(s), unchanged." % (path, pinned))
            for n, port, pin, src in hits:
                print("    (pinned) %s:%d  %s <- %s" % (path, n, port, pin))
            print("    %s" % rule["why"])
            continue
        bad += 1
        if len(hits) > pinned:
            print("check_global_binding: REGRESSION in %s -- %d global binding "
                  "connection(s), baseline %d" % (path, len(hits), pinned))
        else:
            print("check_global_binding: %s now has %d against a baseline of "
                  "%d -- BETTER THAN PINNED, and this is a failure on purpose. "
                  "Lower or retire the baseline in this file, or it will "
                  "outlive the defect and guard nothing."
                  % (path, len(hits), pinned))
        for n, port, pin, src in hits:
            print("    %s:%d  %s <- %s" % (path, n, port, pin))
            print("        %s" % src[:100])
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
