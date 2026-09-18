#!/usr/bin/env python3
"""Read a Quartus map report's Compilation Hierarchy Node table BY HEADER.

Why this exists, written the day it was needed:

On 2026-09-18 an hour of whole-machine analysis read that table by column
POSITION and got the offsets wrong — the DSP column taken for memory, the Pins
column taken for DSP. Four conclusions followed and were published before the
arithmetic gave it away (the whole machine cannot use 123 M10K while the shell
alone fits in 136). The same day, a `.setup.rpt` was read with a regex that
matched three different tables and reported 1,600 paths where there are 200.

Both were positional assumptions about a text table. Both survived several
derived conclusions. Neither was caught by review.

So this module refuses to index by position. It reads the header row, maps
column NAMES to indices, and raises if a name it was asked for is absent — which
turns "the layout moved" from a silently wrong number into a loud failure.

The header, for reference, and it is not assumed anywhere below:

    ; Compilation Hierarchy Node ; Combinational ALUTs ; Dedicated Logic
      Registers ; Block Memory Bits ; DSP Blocks ; Pins ; Virtual Pins ;
      Full Hierarchy Name

Note in particular: **memory is reported in BITS, not in M9K/M10K blocks.** A
map report has no block count. Divide by the device's bits, or read a fitted
row's `ramBlocks`.
"""

from __future__ import annotations

import re

HIER = "Compilation Hierarchy Node"

# Canonical short names for the columns anyone actually wants, mapped to the
# exact header text. Asking for a name not in here is a KeyError at the call
# site rather than a plausible wrong number.
COLUMNS = {
    "node":      "Compilation Hierarchy Node",
    "alut":      "Combinational ALUTs",
    "registers": "Dedicated Logic Registers",
    "bits":      "Block Memory Bits",
    "dsp":       "DSP Blocks",
    "pins":      "Pins",
    "vpins":     "Virtual Pins",
    "full":      "Full Hierarchy Name",
}

_OWN = re.compile(r"\(\s*([0-9][0-9,]*)\s*\)")
_NUM = re.compile(r"^-?[\d,]+")


def _cells(line):
    """`; a ; b ; c ;` -> ['a', 'b', 'c'] with the outer empties dropped."""
    parts = line.split(";")
    if parts and parts[0].strip() == "":
        parts = parts[1:]
    if parts and parts[-1].strip() == "":
        parts = parts[:-1]
    return parts


def _lead(cell):
    """The leading count of `123 (45)`, or None. Commas tolerated.

    THIS IS THE SUBTREE TOTAL, not the entity''s own cost. Quartus writes
    `TOTAL (OWN)` in the hierarchy table, where TOTAL includes every descendant.
    Ranking by it is the leaf-versus-census error in a new costume: on
    2026-09-18 it reported `zhao_geom_wcache` at 1,480,718 ALUTs, which is larger
    than any Cyclone V and is in fact its whole subtree. Use `_own` to attribute
    cost to a module, and `_lead` only when you want the subtree.
    """
    m = _NUM.match(cell.strip())
    if not m:
        return None
    return int(m.group(0).replace(",", ""))


def _own(cell):
    """The parenthesised OWN count of `123 (45)`, or None.

    This is the number that attributes cost TO THIS ENTITY and to nothing below
    it, so a column of them sums to the design without counting a child twice.
    A cell with no parenthesis (the memory-bits and DSP columns) has no separate
    own figure; the caller falls back to the leading value.
    """
    m = _OWN.search(cell)
    if not m:
        return None
    return int(m.group(1).replace(",", ""))


def parse_hierarchy(text):
    """[{name, depth, alut, registers, bits, dsp, pins, vpins, full}], top first.

    `depth` is 0 for the top and counts indentation levels below it, so a
    caller can select direct children without pattern-matching a path.
    """
    # Scan LINES rather than slicing at `text.find(HIER)`. That slice starts in
    # the MIDDLE of the header line -- the table's title is the first column's
    # name -- so the header row arrives truncated and unparseable. Caught by
    # this module's own control on its first run, which is the point of having
    # one.
    if HIER not in text:
        return None                      # no table: abstain, never accuse

    lines = text.splitlines()
    start = None
    for n, ln in enumerate(lines):
        if ln.lstrip().startswith(";") and HIER in ln and COLUMNS["alut"] in ln:
            start = n
            break
    if start is None:
        raise ValueError("hierarchy table found but its header row was not; "
                         "the report layout moved and reading by position "
                         "would now be silently wrong")
    header = _cells(lines[start])
    lines = lines[start:]

    names = [c.strip() for c in header]
    idx = {}
    for short, full in COLUMNS.items():
        if full in names:
            idx[short] = names.index(full)
    for required in ("node", "alut"):
        if required not in idx:
            raise ValueError("hierarchy header lacks %r: %r" % (COLUMNS[required], names))

    # Indentation is three spaces per level in Quartus 17, on top of a
    # one-space baseline that the top row also carries. BOTH are measured from
    # the data rather than assumed: the baseline from the first row, the step
    # from the first row that differs. A hard-coded 3 would have reported the
    # top at depth 1 and its children at depth 4.
    base = None
    step = None
    rows = []
    for ln in lines:
        if not ln.lstrip().startswith(";"):
            continue
        cells = _cells(ln)
        if len(cells) < len(names):
            continue
        node = cells[idx["node"]]
        stripped = node.strip()
        if not (stripped.startswith("|") or stripped.startswith("|")):
            continue
        if _lead(cells[idx["alut"]]) is None:
            continue                      # the header itself, or a rule line
        ind = len(node) - len(node.lstrip())
        if base is None:
            base = ind
        if step is None and ind > base:
            step = ind - base
        row = {
            "name": stripped.strip("|").split(":")[0],
            "depth": 0 if ind == base else ((ind - base) // step if step else 1),
            "full": cells[idx["full"]].strip() if "full" in idx else "",
        }
        for short in ("alut", "registers", "bits", "dsp", "pins", "vpins"):
            row[short] = _lead(cells[idx[short]]) if short in idx else None
            # ...and the OWN figure beside it. The unsuffixed key stays the
            # subtree TOTAL so no existing caller changes meaning silently;
            # `<key>_own` is the per-entity attribution, and a column of those
            # sums to the design without counting a child under every parent.
            if short in idx:
                own = _own(cells[idx[short]])
                row[short + "_own"] = own if own is not None else row[short]
            else:
                row[short + "_own"] = None
        rows.append(row)
    return rows


def children(rows, depth=1):
    """Rows at one depth — `depth=1` is the top's direct children."""
    return [r for r in rows if r["depth"] == depth]


def self_test():
    sample = (
        "noise before\n"
        "; " + HIER + " ; Combinational ALUTs ; Dedicated Logic Registers ;"
        " Block Memory Bits ; DSP Blocks ; Pins ; Virtual Pins ; Full Hierarchy Name ;\n"
        "; |zhao_top ; 158887 (7182) ; 105818 (4049) ; 1029005 ; 123 ; 0 ; 4 ; |zhao_top ;\n"
        ";    |zhao_a:u_a| ; 8715 (110) ; 3855 (506) ; 119808 ; 0 ; 0 ; 0 ; |zhao_top|zhao_a:u_a ;\n"
        ";       |zhao_b:u_b| ; 100 (100) ; 7 (7) ; 0 ; 33 ; 0 ; 0 ; |zhao_top|zhao_a:u_a|zhao_b:u_b ;\n"
    )
    rows = parse_hierarchy(sample)
    assert rows is not None and len(rows) == 3, "row count wrong: %r" % (rows,)

    top, a, b = rows
    # THE EXACT CONFUSION THIS MODULE EXISTS TO PREVENT: 123 is DSP, not memory,
    # and 1,029,005 is bits, not a block count.
    assert top["dsp"] == 123, "DSP column misread"
    assert top["bits"] == 1029005, "memory column misread"
    assert top["pins"] == 0 and top["vpins"] == 4, "pins/vpins misread"
    assert top["alut"] == 158887 and top["registers"] == 105818

    # And the block that a positional read called memoryless is not.
    assert a["bits"] == 119808 and a["dsp"] == 0, "child columns misread"
    assert b["dsp"] == 33 and b["bits"] == 0

    assert [r["depth"] for r in rows] == [0, 1, 2], "depth wrong: %r" % ([r["depth"] for r in rows],)
    assert [r["name"] for r in children(rows)] == ["zhao_a"]

    # A report with no table must abstain rather than report an empty design.
    assert parse_hierarchy("nothing here") is None

    # And a table whose header moved must be LOUD.
    broken = "noise\n" + HIER + "\n; |zhao_top ; 1 ; 2 ; 3 ; 4 ; 5 ; 6 ; x ;\n"
    try:
        parse_hierarchy(broken)
    except ValueError:
        pass
    else:
        raise AssertionError("a missing header row did not raise; a positional "
                             "fallback would be exactly the bug this prevents")
    return True


if not __debug__:
    raise RuntimeError("map_report refuses Python -O: it removes the controls "
                       "that pin every column to its header name")
if not self_test():
    raise AssertionError("map_report self-test failed")


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print(__doc__)
        raise SystemExit(2)
    with open(sys.argv[1], "r", errors="replace") as fh:
        rows = parse_hierarchy(fh.read())
    if rows is None:
        print("no Compilation Hierarchy Node table in that report")
        raise SystemExit(0)
    kids = children(rows)
    print("%d rows, %d direct children" % (len(rows), len(kids)))
    print("ranked by OWN cost. Quartus writes TOTAL (OWN); TOTAL includes every")
    print("descendant, so ranking by it counts a child under each of its parents.")
    print("%9s %9s %9s %10s %5s  module"
          % ("ALUT_own", "ALUT_tot", "REG_own", "BITS", "DSP"))
    for r in sorted(kids, key=lambda r: -(r["alut_own"] or 0))[:20]:
        print("%9s %9s %9s %10s %5s  %s"
              % (r["alut_own"], r["alut"], r["registers_own"], r["bits"],
                 r["dsp"], r["name"]))
    own_sum = sum(r["alut_own"] or 0 for r in rows)
    top_tot = rows[0]["alut"] or 0
    print("\nsum of every row's OWN ALUTs : %d" % own_sum)
    print("top row's TOTAL ALUTs        : %d" % top_tot)
    print("a large gap between these means the table was misread, not that the")
    print("design changed -- they are two ways of counting the same silicon.")
