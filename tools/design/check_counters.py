#!/usr/bin/env python3
"""check_counters.py -- does the ledger's `counters:` list name real RTL ports?

WHY THIS EXISTS (D19c)
----------------------
`design/blocks.yml` declares a `counters:` list per block and `spec/counters.md`
governs their behaviour. V12 checks each name is in `counter_catalog`. Nothing
checked that the RTL implements one, so a row like

    counters: [meshlets_fetched, triangles_culled]

on a block whose ports are `meshlets_considered_o` and `descriptors_fetched_o`
is documentation that reads like a claim.

THE RULING BEHIND IT
--------------------
Two conventions were possible: rename every port to `<counter>_o` (mechanical,
60+ blocks, and a port rename sweep during an active fit campaign is how you
discover a `PINMISSING` at hour three), or let a row declare its own mapping.
**The mapping wins**, because it is checkable today, touches no RTL, and does
not foreclose the rename later -- a block whose ports already follow the
default needs no mapping at all, so adopting the rename simply deletes entries.

So resolution is, in order:

  1. an explicit `counter_ports:` entry on the block, `name: port_o`
  2. the default `<name>_o`
  3. otherwise UNRESOLVED -- reported, not tolerated silently

WHAT IT DOES NOT DO
-------------------
It does not check that the counter COUNTS the right thing, or that anything
reads it. `spec/counters.md` governs the first and `check_port_coverage.py`
speaks to the second. A port existing is the weakest of the three claims and
the only one that can be checked from names alone -- which is exactly why the
earlier attempt to answer this question from names alone reported a headline
that was wrong. See the note at the bottom of the output.
"""
from __future__ import annotations

import io
import os
import re
import sys

PORT_RE = re.compile(r"^\s*output\s+(?:var\s+)?(?:\w+\s+)*?(?:\[[^\]]*\]\s*)*(\w+)\s*(?:,|\)|;)\s*(?://.*)?$")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def modules():
    out = {}
    for root, _d, files in os.walk("fpga/rtl"):
        for f in files:
            if f.endswith(".sv"):
                out[f[:-3]] = os.path.join(root, f)
    return out


def outputs_of(path):
    """Every output port of the FIRST module in the file."""
    names, started = [], False
    for line in read(path).splitlines():
        if not started:
            if re.match(r"^\s*module\s+\w+", line):
                started = True
            continue
        m = PORT_RE.match(line)
        if m:
            names.append(m.group(1))
        if re.match(r"^\s*\);\s*$", line):
            break
    return names


def blocks():
    s = read("design/blocks.yml")
    out = []
    for chunk in re.split(r"\n  - id: ", s)[1:]:
        bid = chunk.split("\n", 1)[0].strip()
        m = re.search(r"^    counters: \[(.*?)\]", chunk, re.M)
        if not m:
            continue
        names = [x.strip() for x in m.group(1).split(",") if x.strip()]
        mapping = {}
        mm = re.search(r"^    counter_ports:\n((?:      \w+: \w+\n)+)", chunk, re.M)
        if mm:
            for line in mm.group(1).strip().splitlines():
                k, v = line.strip().split(":", 1)
                mapping[k.strip()] = v.strip()
        out.append((bid, names, mapping))
    return out


def main() -> int:
    mods = modules()
    by_default, by_mapping, unresolved, no_module = [], [], [], []

    for bid, names, mapping in blocks():
        mod = "zhao_" + bid.lower().replace(".", "_")
        if mod not in mods:
            no_module.append((bid, len(names)))
            continue
        ports = set(outputs_of(mods[mod]))
        for n in names:
            if n in mapping:
                if mapping[n] in ports:
                    by_mapping.append((bid, n, mapping[n]))
                else:
                    unresolved.append((bid, n, "mapped to %s, which is not a port" % mapping[n]))
            elif n + "_o" in ports:
                by_default.append((bid, n))
            else:
                unresolved.append((bid, n, "no %s_o and no mapping" % n))

    total = len(by_default) + len(by_mapping) + len(unresolved)
    print("counters: %d declared on blocks with a module; %d resolve by the "
          "default <name>_o, %d by an explicit mapping, %d UNRESOLVED. "
          "(%d block(s) have counters but no module file yet.)"
          % (total, len(by_default), len(by_mapping), len(unresolved), len(no_module)))

    if unresolved:
        print("\nUNRESOLVED -- the ledger names a counter the RTL does not have "
              "under that name. Either the port is named differently (add a "
              "`counter_ports:` entry) or the counter is not implemented:")
        cur = None
        for bid, n, why in unresolved:
            if bid != cur:
                print("  %s" % bid)
                cur = bid
            print("      %-28s %s" % (n, why))

    if by_mapping and "--quiet" not in sys.argv:
        print("\nRESOLVED BY EXPLICIT MAPPING (%d):" % len(by_mapping))
        for bid, n, p in by_mapping:
            print("  %-22s %-28s -> %s" % (bid, n, p))

    print("\nNOTE: this checks that a PORT EXISTS. It does not check that the "
          "counter counts the right thing (spec/counters.md) or that anything "
          "reads it (check_port_coverage.py). It REPORTS; it does not gate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
