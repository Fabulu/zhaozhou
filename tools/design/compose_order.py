"""Derive the geometry composition order from the ledger's own declarations.

D22 says nineteen geometry blocks are not wired. This asks the ledger what the
order WOULD be, and where the declared seams do not meet -- so the composition
is a plan rather than a discovery made one block at a time.
"""
import io
import re

txt = io.open("design/blocks.yml", encoding="utf-8", errors="replace").read()

blocks = {}
cur = None
for line in txt.split("\n"):
    m = re.match(r"^  - id:\s*(\S+)", line)
    if m:
        cur = m.group(1)
        blocks[cur] = {"up": [], "down": [], "maturity": "?", "sub": "?",
                       "in_sig": [], "out_sig": []}
        continue
    if cur is None:
        continue
    m = re.match(r"^\s+upstream:\s*\[(.*)\]", line)
    if m:
        blocks[cur]["up"] = [x.strip() for x in m.group(1).split(",") if x.strip()]
    m = re.match(r"^\s+downstream:\s*\[(.*)\]", line)
    if m:
        blocks[cur]["down"] = [x.strip() for x in m.group(1).split(",") if x.strip()]
    m = re.match(r"^\s+inputs:\s*\[(.*)\]", line)
    if m:
        blocks[cur]["in_sig"] = [x.strip() for x in m.group(1).split(",") if x.strip()]
    m = re.match(r"^\s+outputs:\s*\[(.*)\]", line)
    if m:
        blocks[cur]["out_sig"] = [x.strip() for x in m.group(1).split(",") if x.strip()]
    m = re.match(r"^\s+maturity:\s*(\S+)", line)
    if m and blocks[cur]["maturity"] == "?":
        blocks[cur]["maturity"] = m.group(1)
    m = re.match(r"^\s+subsystem:\s*(\S+)", line)
    if m:
        blocks[cur]["sub"] = m.group(1)

geo = {k: v for k, v in blocks.items() if v["sub"] == "geometry"}
print("geometry blocks in the ledger:", len(geo))
print()

# Topological order over the declared edges, restricted to geometry.
indeg = {k: 0 for k in geo}
for k, v in geo.items():
    for d in v["down"]:
        if d in indeg:
            indeg[d] += 1
order, ready = [], sorted([k for k, n in indeg.items() if n == 0])
while ready:
    n = ready.pop(0)
    order.append(n)
    for d in sorted(geo[n]["down"]):
        if d in indeg:
            indeg[d] -= 1
            if indeg[d] == 0:
                ready.append(d)
                ready.sort()

print("DECLARED ORDER (topological over the ledger's own upstream/downstream):")
for i, n in enumerate(order, 1):
    print("  %2d. %-24s %s" % (i, n, geo[n]["maturity"]))

cyc = [k for k in geo if k not in order]
if cyc:
    print()
    print("NOT ORDERABLE (a declared cycle, or an edge into a cycle):")
    for k in sorted(cyc):
        print("   ", k, geo[k]["maturity"], "up=", geo[k]["up"], "down=", geo[k]["down"])

# Where do declared edges disagree with each other?
# ---------------------------------------------------------------------------
# EDGES THAT NOBODY CARRIES A SIGNAL ACROSS
# ---------------------------------------------------------------------------
# Symmetry is not agreement. On 2026-09-04 this probe reported "every declared
# seam agrees" for GEOM.MESHFETCH -> GEOM.ASSEMBLE, and the edge was wrong:
# MESHFETCH emits a 32-bit BYTE OFFSET into the vertex stream and ASSEMBLE
# consumes a 16-bit PER-VIEW PROJECTED-VERTEX ID. Both rows named each other,
# so the symmetry check passed, and the derived order put ASSEMBLE at position
# 2 -- before the projection that creates its input.
#
# The ledger already had the evidence: MESHFETCH's `outputs` is
# [meshlet_stream] and ASSEMBLE's `inputs` is [meshlet_descriptor,
# index_stream]. NO SHARED NAME. An edge across which no declared signal
# travels is an edge nobody has checked.
#
# This is deliberately a WARNING and not a verdict. The vocabularies are prose
# and two rows may legitimately name one thing differently. What it buys is
# that such a pair has to be looked at once, rather than being derived into an
# order and believed.
print()
print("EDGES WITH NO SHARED SIGNAL NAME (symmetry held; nothing travels):")
unjustified = 0
for k, v in sorted(geo.items()):
    for d in sorted(v["down"]):
        if d not in blocks:
            continue
        shared = set(v["out_sig"]) & set(blocks[d]["in_sig"])
        if not shared:
            unjustified += 1
            print("    %s -> %s" % (k, d))
            print("        %s emits   %s" % (k, v["out_sig"] or ["-"]))
            print("        %s takes   %s" % (d, blocks[d]["in_sig"] or ["-"]))
if not unjustified:
    print("    none")

print()
print("DECLARED INPUTS WITH NO DECLARED PRODUCER UPSTREAM:")
orphan = 0
for k, v in sorted(geo.items()):
    produced = set()
    for u in v["up"]:
        if u in blocks:
            produced |= set(blocks[u]["out_sig"])
    for sig in v["in_sig"]:
        if sig not in produced:
            orphan += 1
            print("    %-22s takes %-22s no upstream emits it" % (k, sig))
if not orphan:
    print("    none")

print()
print("SEAMS THAT DO NOT AGREE (A names B downstream, B does not name A upstream):")
bad = 0
for k, v in geo.items():
    for d in v["down"]:
        if d in blocks and k not in blocks[d]["up"]:
            print("    %s -> %s  (%s's upstream is %s)" % (k, d, d, blocks[d]["up"]))
            bad += 1
if not bad:
    print("    none")
