"""An OUTPUT PORT WITH NO DRIVER reads as a clean zero. Find them.

WHY THIS EXISTS
---------------
P0-C, 2026-09-08. Replacing the texture island's fragment robot with the v3
owner block left FOUR output ports undriven across two rounds:

    fr_tmu_valid (planner), the AUX valid, cnt_fragments_o,
    cnt_fragrob_id_errors_o

None failed elaboration. None produced a Verilator lint diagnostic in this
tree's configuration. Two of them cost a debugging round each, because an
undriven counter reads ZERO and a counter reading zero is indistinguishable
from a stage that is simply quiet. The first two were found by a stall; the
second two only because the first two prompted a sweep.

This is the repository's own law applied to a port list: a broken instrument
lies in ONE direction, and a missing connection always lies toward "nothing
happened".

It is most valuable during a RESTRUCTURE, where a top is seeded from an older
one: every port the old design drove is still declared, and the ones the new
design forgot to reconnect are silent.

WHAT IT DOES NOT DO
-------------------
It does not parse SystemVerilog. It is a lexical sweep, deliberately -- a real
parser here would be a second implementation of a thing Verilator already does
better, and its failure mode would be silent. This one's failure mode is
FALSE POSITIVES, which are loud and cheap to dismiss, and that asymmetry is the
whole design.
"""

import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# THE PORT NAME IS THE LAST IDENTIFIER, NOT THE FIRST.
#
# `output var logic signed [31:0] req_u_o` puts `signed` where a naive
# pattern expects the name, and `gen_prod_top.py` had this EXACT bug and
# had it fixed days earlier -- so the first version of this file
# reproduced a defect that was already understood, in a tree that already
# contained the fix. Anchoring on the trailing comma or paren instead of
# guessing the leading keywords is what makes it robust.
PORT = re.compile(r"^\s*output\b(?P<decl>[^;]*?)(?P<name>\w+)\s*(?:,|$)")
SKIP = ("signed", "unsigned", "var", "logic", "wire", "reg", "bit", "output")
TERM = re.compile(r"^\s*\);\s*$")
MODULE = re.compile(r"^\s*module\s+(\w+)")


def scan(path):
    """[(module, port)] for every declared output with no visible driver."""
    text = io.open(path, encoding="utf-8", errors="replace").read()
    lines = text.split(chr(10))
    mi = [i for i, l in enumerate(lines) if MODULE.match(l)]
    if not mi:
        return []
    start = mi[0]
    name = MODULE.match(lines[start]).group(1)
    end = None
    for i in range(start, len(lines)):
        if TERM.match(lines[i]):
            end = i
            break
    if end is None:
        return []
    outs = []
    for l in lines[start:end]:
        m = PORT.match(l)
        if m and m.group("name") not in SKIP:
            outs.append(m.group("name"))
    body_text = chr(10).join(lines[end:])
    dead = []
    for o in outs:
        if re.search(r"assign\s+" + o + r"\b", body_text):
            continue
        if re.search(r"\b" + o + r"\s*(<=|=[^=])", body_text):
            continue
        if re.search(r"\.\w+\s*\(\s*" + o + r"\s*\)", body_text):
            continue
        dead.append((name, o))
    return dead, len(outs)


# A KNOWN-BAD MODULE, checked on every run. Three outputs: one driven by an
# assign, one by an instance connection, one by nothing at all.
_FIRE = """
module zhao_fire_test (
    input  var logic        clk,
    output var logic [31:0] driven_by_assign_o,
    output var logic        driven_by_instance_o,
    output var logic [31:0] driven_by_nothing_o,
    output var logic signed [31:0] signed_and_driven_o
);
  assign driven_by_assign_o = 32'd7;
  assign signed_and_driven_o = 32'sd1;
  some_block u_x (.clk(clk), .out_o(driven_by_instance_o));
endmodule
"""


def self_fire_test(tmp):
    p = os.path.join(tmp, "_undriven_fire.sv")
    io.open(p, "w", encoding="utf-8", newline=chr(10)).write(_FIRE)
    try:
        dead, n = scan(p)
    finally:
        try:
            os.unlink(p)
        except OSError:
            pass
    # Exactly one dead port, and it must be the right one -- a rule that flagged
    # all three would "fire" while being useless.
    # FOUR ports now, one of them `signed` and driven -- so a pattern that
    # captured `signed` as the name would fail this test rather than pass.
    return n == 4 and len(dead) == 1 and dead[0][1] == "driven_by_nothing_o"


def main(argv):
    targets = argv[1:]
    if not targets:
        print("usage: undriven_outputs.py <file.sv> [...]")
        return 2

    tmp = os.environ.get("TEMP") or os.environ.get("TMP") or "."
    if not self_fire_test(tmp):
        print("UNDRIVEN-OUTPUT SWEEP BROKEN: it no longer isolates the one "
              "undriven port in a known-bad module. Refusing to report a pass.")
        return 2

    total_ports, findings = 0, []
    for t in targets:
        p = t if os.path.isabs(t) else os.path.join(ROOT, t)
        if not os.path.exists(p):
            print("missing: %s" % t)
            return 2
        dead, n = scan(p)
        total_ports += n
        findings.extend([(t, m, o) for (m, o) in dead])

    print("undriven-output sweep: %d files, %d output ports"
          % (len(targets), total_ports))
    if total_ports == 0:
        print("SWEEP VACUOUS: zero output ports parsed. A clean result from a "
              "parser that found nothing is not a clean result.")
        return 2
    if findings:
        print("OUTPUTS WITH NO VISIBLE DRIVER: %d" % len(findings))
        for (f, m, o) in findings:
            print("  - %s: %s.%s" % (f, m, o))
        print("These read as a constant zero. If one is genuinely intended to "
              "be tied off, tie it off EXPLICITLY so the intent is in the "
              "source rather than in its absence.")
        return 1
    print("every declared output has a driver")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
