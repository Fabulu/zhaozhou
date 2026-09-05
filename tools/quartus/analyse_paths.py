#!/usr/bin/env python3
"""Classify a block fit's timing paths by where they START.

WHY THIS IS A COMMITTED TOOL AND NOT A ONE-LINER
------------------------------------------------
This analysis was written inline twice on 2026-09-05 and was WRONG the second
time, in the direction that would have made the design look innocent.

The first use produced the composed island's decisive finding: 405 of its 2,000
summarised paths start at a virtual pin (worst slack -4.482 ns) and 1,595 start
inside the design (worst -3.63 ns), so removing the boundary entirely would buy
about 4 MHz of the 36 needed and the limit is `zhao_raster_rcp24_svc`, not the
pins.

The second use, on `zhao_texture_material_combine_v1`, classified ALL 1,820 of
its paths as virtual-pin starts. It keyed on `:` or `|` appearing in the node
name -- hierarchy punctuation -- and a SINGLE-MODULE fit has no hierarchy, so
every internal node looked like a pin. Had it not been obvious that `Decoder5`
and `Add46` are not ports, the conclusion would have been "this block's timing
is all boundary artefact", which is false: the path is a decode-and-add chain in
the block's own counters.

CLAUDE.md's rule is explicit about this shape:

    A probe that does this was written once and thrown away, so its numbers are
    unreproducible -- commit the probe.

So the classifier no longer guesses from punctuation. It reads the TOP MODULE'S
PORT LIST out of the RTL and asks whether the path's start node IS one of those
ports. That is the actual question, and it works whether or not the fit has
hierarchy.

USAGE
-----
    python tools/quartus/analyse_paths.py <setup.summary.rpt> <top.sv> [top_module]

Prints the split, the worst slack per class, and the most common start nodes.
Reports; never gates.
"""

import os
import re
import sys

# A summary row: "; slack ; from ; to ; ..."
ROW = re.compile(r"^;\s*(-?[0-9.]+)\s*;\s*([^;]+?)\s*;\s*([^;]+?)\s*;", re.M)

# A port declaration, in the same shape check_port_coverage.py had to learn:
# optional `var`, optional net/data type, optional package-qualified type,
# optional signedness, any number of packed dimensions, then the name.
PORT = re.compile(
    r"^\s*(input|output|inout)\s+"
    r"(?:var\s+)?(?:wire\s+|logic\s+|reg\s+)?"
    r"(?:[A-Za-z_]\w*::[A-Za-z_]\w*\s+)?"
    r"(?:signed\s+|unsigned\s+)?"
    r"(?:\[[^\]]*\]\s*)*"
    r"([A-Za-z_]\w*)",
    re.M,
)

# SELF-CHECK. Both patterns are asserted against known-good lines at import,
# because a regex that matches nothing reports a confident, empty answer -- and
# this file exists precisely because a silent classifier lied once already.
assert PORT.match("    input  var logic [23:0] frag_depth_i,"), "port regex is dead"
assert PORT.match("  output logic o_valid_o,"), "port regex is dead"
assert PORT.match("    input  var logic signed [31:0] vx_i,"), "port regex is dead"
assert ROW.match("; -4.482 ; frag_depth_i[19] ; u_rcp|c_x[7][8] ; clk ;"), "row regex is dead"


def top_ports(sv_path, top_module=None):
    """Every port name of the top module, as a set."""
    with open(sv_path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    start = 0
    if top_module:
        m = re.search(r"^\s*module\s+" + re.escape(top_module) + r"\b", text, re.M)
        if m:
            start = m.start()
    end = text.find(");", start)
    if end < 0:
        end = len(text)
    return {m.group(2) for m in PORT.finditer(text[start:end])}


def base_name(node):
    """`frag_depth_i[19]` -> `frag_depth_i`; leaves hierarchical names alone."""
    return re.sub(r"\[[^\]]*\]$", "", node.strip())


def classify(node, ports):
    """PIN if the start node is a top-level port; otherwise INTERNAL.

    Hierarchy punctuation is NOT consulted. That was the defect: it answers
    "does this name look nested", which is a different question and is
    meaningless for a single-module fit.
    """
    if base_name(node) in ports:
        return "pin"
    return "internal"


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    rpt, sv = argv[1], argv[2]
    top = argv[3] if len(argv) > 3 else os.path.splitext(os.path.basename(sv))[0]

    ports = top_ports(sv, top)
    if not ports:
        print("analyse_paths: parsed NO ports from %s (top=%s) -- refusing to "
              "classify, because with an empty port set every path would be "
              "called internal and the answer would look clean" % (sv, top))
        return 1

    with open(rpt, encoding="utf-8", errors="replace") as f:
        text = f.read()
    rows = [(float(s), f_, t) for s, f_, t in ROW.findall(text)
            if f_ and not f_.lower().startswith("slack")]
    if not rows:
        print("analyse_paths: parsed NO paths from %s -- a precise zero is a "
              "broken instrument until proven otherwise" % rpt)
        return 1

    buckets = {"pin": [], "internal": []}
    for slack, frm, to in rows:
        buckets[classify(frm, ports)].append((slack, frm, to))

    print("analyse_paths: %s" % os.path.basename(rpt))
    print("  top module %s -- %d ports parsed" % (top, len(ports)))
    print("  %d paths summarised" % len(rows))
    for name in ("pin", "internal"):
        b = buckets[name]
        label = "start at a TOP-LEVEL PORT" if name == "pin" else "start INSIDE the design"
        if not b:
            print("  %-26s none" % label)
            continue
        worst = min(b, key=lambda r: r[0])
        print("  %-26s %5d paths, worst slack %8.3f  %s -> %s"
              % (label, len(b), worst[0], worst[1][:34], worst[2][:34]))

    # What removing the boundary could be worth, which is the question the
    # island's finding turned on. Stated as a bound, not a prediction.
    pin_b, int_b = buckets["pin"], buckets["internal"]
    if pin_b and int_b:
        wp = min(r[0] for r in pin_b)
        wi = min(r[0] for r in int_b)
        print("  boundary headroom: %.3f ns -- what perfect boundary timing "
              "could recover before the internal paths bind" % (wi - wp))
        if wi - wp < 0:
            print("  (negative: the internal paths are already worse than the "
                  "pin paths, so the boundary is not the limit at all)")

    from collections import Counter
    tally = Counter(base_name(r[1]) for r in rows)
    print("  most common start nodes:")
    for node, n in tally.most_common(5):
        print("    %6d  %s  [%s]" % (n, node[:52], classify(node, ports)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
