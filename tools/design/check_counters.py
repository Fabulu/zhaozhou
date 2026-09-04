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

PORT_RE = re.compile(r"^\s*output\s+(?:var\s+)?(?:[\w:]+\s+)*?(?:\[[^\]]*\]\s*)*(\w+)\s*(?:\[[^\]]*\]\s*)*(?:,|\)|;)?\s*(?://.*)?$")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def modules():
    out = {}
    for root, _d, files in os.walk("fpga/rtl"):
        for f in files:
            if f.endswith(".sv"):
                out[f[:-3]] = os.path.join(root, f)
    return out


# Every line that declares an output, however it is written. If PORT_RE matches
# fewer lines than this does, the parser is DROPPING ports -- which is exactly
# how this tool reported "0 counters match their port" three separate times.
OUTPUT_LINE_RE = re.compile(r"^\s*output\s")

# The self-check has to be self-checked. Its first version was written with a
# `\b` that a shell heredoc turned into a literal BACKSPACE (0x08), so the
# pattern demanded a backspace after "output", matched nothing, and cheerfully
# printed "no silent drops" while the parser was dropping 17 ports. A check
# that can never fire is worse than no check, because it reassures. This
# asserts at import that the pattern still matches an ordinary declaration.
assert OUTPUT_LINE_RE.match("    output var logic [31:0] x_o,"), "dead self-check"

UNPARSED = []


def outputs_of(path):
    """Every output port of the FIRST module in the file.

    Also records any line that declares an output but that PORT_RE could not
    read. Three bugs -- width brackets `[31:0]`, scoped types `pkg::type_t`,
    and the final port carrying no trailing comma -- each silently dropped
    ports and each made this tool report a SMALLER, more alarming number. A
    parser that drops what it cannot match will always report progress, so it
    has to say out loud how much it dropped."""
    names, started = [], False
    for line in read(path).splitlines():
        if not started:
            if re.match(r"^\s*module\s+\w+", line):
                started = True
            continue
        m = PORT_RE.match(line)
        if m:
            names.append(m.group(1))
        elif OUTPUT_LINE_RE.match(line):
            UNPARSED.append((path, line.strip()))
        if re.match(r"^\s*\);\s*$", line):
            break
    return names


COUNTER_SHAPE = re.compile(r"^\s*output\s+(?:var\s+)?(?:[\w:]+\s+)*?\[31:0\]\s*(\w+)")


def counter_shaped_ports(path):
    """Output ports declared `[31:0]` -- the shape a counter has here.

    Suggestion material only. spec/counters.md 3 makes a counter a 32-bit local
    register presented on request, so a 32-bit output is a CANDIDATE and nothing
    more: `dma_bytes_consumed_o` is 32 bits and is a payload, not a counter.
    Offered so the remaining rows can be worked by eye instead of by grep.
    """
    return [m.group(1) for m in COUNTER_SHAPE.finditer(read(path))]


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
        cand = counter_shaped_ports(mods[mod])
        # spec/counters.md 3: a block may instead own its counters locally and
        # present them on a D9 SNAP CHANNEL as a zhao_counter_snap_t. Those have
        # no <counter>_o port and are not supposed to -- so an unresolved row on
        # such a block wants a MAPPING, while an unresolved row on a block with
        # neither form is a counter with no visible presentation path at all.
        # Reporting the two as one list is what made this look like 82 defects.
        has_snap = "zhao_counter_snap_t" in read(mods[mod])
        for n in names:
            if n in mapping:
                if mapping[n] in ports:
                    by_mapping.append((bid, n, mapping[n]))
                else:
                    unresolved.append((bid, n, "mapped to %s, which is not a port" % mapping[n],
                                       has_snap))
            elif n + "_o" in ports:
                by_default.append((bid, n))
            else:
                why = "no %s_o and no mapping" % n
                if cand and "--suggest" in sys.argv:
                    why += "   candidates: " + " ".join(sorted(set(cand))[:6])
                unresolved.append((bid, n, why, has_snap))

    total = len(by_default) + len(by_mapping) + len(unresolved)
    print("counters: %d declared on blocks with a module; %d resolve by the "
          "default <name>_o, %d by an explicit mapping, %d UNRESOLVED. "
          "(%d block(s) have counters but no module file yet.)"
          % (total, len(by_default), len(by_mapping), len(unresolved), len(no_module)))

    snapped = [u for u in unresolved if u[3]]
    bare = [u for u in unresolved if not u[3]]

    if snapped:
        print("\nUNRESOLVED, BUT THE BLOCK HAS A SNAP CHANNEL (%d) -- the D9 "
              "form (spec/counters.md 3). The counter is owned locally and "
              "presented as a zhao_counter_snap_t, so there is no <counter>_o "
              "port and there is NOT MEANT TO BE ONE. These want a "
              "`counter_ports:` entry, not an implementation:" % len(snapped))
        cur = None
        for bid, n, why, _ in snapped:
            if bid != cur:
                print("  %s" % bid)
                cur = bid
            print("      %-28s %s" % (n, why))

    if bare:
        print("\nUNRESOLVED, AND NO SNAP CHANNEL EITHER (%d) -- the block has "
              "neither a <counter>_o port nor a zhao_counter_snap_t, so this "
              "counter has no visible presentation path at all. THIS is the "
              "list that is about missing work:" % len(bare))
        cur = None
        for bid, n, why, _ in bare:
            if bid != cur:
                print("  %s" % bid)
                cur = bid
            print("      %-28s %s" % (n, why))

    if by_mapping and "--quiet" not in sys.argv:
        print("\nRESOLVED BY EXPLICIT MAPPING (%d):" % len(by_mapping))
        for bid, n, p in by_mapping:
            print("  %-22s %-28s -> %s" % (bid, n, p))

    if UNPARSED:
        print("\nPARSER DROPPED %d OUTPUT DECLARATION(S) it could not read. "
              "Every one is a port this tool is BLIND to, so every number above "
              "is a LOWER BOUND. Fix the pattern before believing them:"
              % len(UNPARSED))
        for path, line in UNPARSED[:12]:
            print("  %-30s %s" % (os.path.basename(path), line[:64]))
        if len(UNPARSED) > 12:
            print("  ... and %d more" % (len(UNPARSED) - 12))
    else:
        print("\nparser read every `output` line it met -- no silent drops.")

    print("\nNOTE: this checks that a PORT EXISTS. It does not check that the "
          "counter counts the right thing (spec/counters.md) or that anything "
          "reads it (check_port_coverage.py). It REPORTS; it does not gate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
