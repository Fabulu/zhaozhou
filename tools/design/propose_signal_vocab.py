"""Propose the vocabulary mapping, from the ledger's own edges.

For every input with no declared producer, look at what its declared upstreams
actually emit and score the candidates by shared word-stems. This does not
decide anything -- it produces the shortlist a human would otherwise build by
hand, with the evidence attached.
"""
import io
import re

txt = io.open("design/blocks.yml", encoding="utf-8", errors="replace").read()

blocks, cur = {}, None
for line in txt.split("\n"):
    m = re.match(r"^  - id:\s*(\S+)", line)
    if m:
        cur = m.group(1)
        blocks[cur] = {"up": [], "in": [], "out": []}
        continue
    if cur is None:
        continue
    for key, field in (("upstream", "up"), ("inputs", "in"), ("outputs", "out")):
        m = re.match(r"^\s+%s:\s*\[(.*)\]" % key, line)
        if m:
            blocks[cur][field] = [x.strip() for x in m.group(1).split(",") if x.strip()]


def stems(name):
    return set(w for w in re.split(r"[_\s]+", name.lower()) if w)


pairs = {}
for k, v in blocks.items():
    produced = set()
    for u in v["up"]:
        if u in blocks:
            produced |= set(blocks[u]["out"])
    for sig in v["in"]:
        if sig in produced:
            continue
        best, score = None, 0
        for cand in produced:
            sh = len(stems(sig) & stems(cand))
            if sh > score:
                best, score = cand, sh
        if best:
            pairs.setdefault((best, sig), []).append(k)

print("PROPOSED RENAMES — producer name -> consumer name, by how many rows it fixes")
print()
ranked = sorted(pairs.items(), key=lambda x: -len(x[1]))
fixed = 0
for (prod, cons), rows in ranked:
    fixed += len(rows)
    print("  %-22s == %-24s  fixes %d  (%s)"
          % (prod, cons, len(rows), ", ".join(sorted(rows)[:4])))
print()
print("%d of the unproduced inputs are explained by %d name pairs."
      % (fixed, len(ranked)))
print()
print("A PAIR APPEARING HERE IS NOT A RENAME. This scores shared word-stems and")
print("nothing else, so it offers `meshlet_stream == index_stream` (the index")
print("stream is asset memory) and `tile_write == tile_read` (opposite")
print("directions). Counting occurrences does not rescue it either: a name used")
print("exactly once is not thereby the WRONG name.")
print()
print("What licensed the renames made on 2026-09-04 was evidence of a different")
print("kind, one per case -- a consumer contract naming the port, a name that")
print("already had a producer AND another correct consumer, or a collision with")
print("an entry in the counter catalog. Use this list to find candidates; use")
print("the contracts to decide them.")
