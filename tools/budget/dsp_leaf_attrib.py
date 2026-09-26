#!/usr/bin/env python3
"""Leaf-level DSP attribution out of a Quartus map report's entity table.

map_entity_attrib.py prints SUBTREE totals, which is the right thing for the
ALUT question and the wrong thing for this one: a subtree's DSP is the sum of
its children's, so the top of that table is always the top of the hierarchy.
This prints each entity's OWN DSP -- its subtree total minus the sum of its
immediate children -- so the number lands on the module that actually wrote
the multiply.

Comparison side only. Every repair is decided by a -MapOnly row, never by this.

USAGE
    python gz-dsphunt-dsp-leaves.py <path-to.map.rpt> [min-own-dsp]
"""
import sys

PATH = sys.argv[1]
MIN = int(sys.argv[2]) if len(sys.argv) > 2 else 2

hdr = None
rows = []
with open(PATH, encoding="utf-8", errors="replace") as fh:
    for line in fh:
        if not line.startswith(";"):
            if hdr is not None and rows:
                if line.startswith("+---") and len(rows) > 5:
                    break
            continue
        cells = [c.strip() for c in line.rstrip("\n").strip().strip(";").split(";")]
        if hdr is None:
            if cells and cells[0].startswith("Compilation Hierarchy Node"):
                hdr = cells
            continue
        if len(cells) < len(hdr) - 1:
            continue
        rows.append(cells)

if hdr is None:
    print("entity table not found")
    sys.exit(2)

i_dsp = next(i for i, c in enumerate(hdr) if "DSP" in c)
i_full = next(i for i, c in enumerate(hdr) if c.startswith("Full Hierarchy"))
i_ent = next(i for i, c in enumerate(hdr) if c.startswith("Entity Name"))


def num(s):
    s = s.split("(")[0].strip().replace(",", "")
    try:
        return int(float(s))
    except ValueError:
        return 0


tree = {}
for cells in rows:
    full = cells[i_full]
    if not full:
        continue
    tree[full] = (cells[i_ent] or cells[0], num(cells[i_dsp]))

own = []
for full, (ent, dsp) in tree.items():
    kids = 0
    plen = len(full) + 1
    for other, (_, odsp) in tree.items():
        if other.startswith(full + "|") and "|" not in other[plen:]:
            kids += odsp
    own.append((full, ent, dsp, dsp - kids))

out = [r for r in own if r[3] >= MIN]
out.sort(key=lambda r: -r[3])
print("rows parsed: %d" % len(tree))
print("%-40s %8s %6s  %s" % ("entity", "subtree", "OWN", "hierarchy"))
tot = 0
for full, ent, sub, o in out:
    tot += o
    print("%-40s %8d %6d  %s" % (ent[:40], sub, o, full[:100]))
print("\nsum of OWN listed (>=%d): %d" % (MIN, tot))
print("sum of ALL own: %d" % sum(r[3] for r in own))
