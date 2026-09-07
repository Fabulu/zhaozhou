"""Record every block's GATING PATH IDENTITY, so a later comparison can check it.

WHY THIS EXISTS
---------------
Docket M6, 2026-09-07: a fit-to-fit Fmax delta is attributable to a change only
if the WORST PATH FAMILY is the same on both sides. The island's reported Fmax
rose 66.77 -> 78.80 with one file changed, and the cause was that the gating
family stopped being PALETTE and became RCP — a placement effect, not the
change's doing.

Applying that rule needs the OLD fit's worst path, and that is exactly what does
not survive: `blockpaths/<module>.setup.rpt` is overwritten by the next fit of
the same module. The T2 owner comparison could only be checked because an
unrelated report happened to quote its baseline path in prose. That is
archaeology, not a record.

So this walks every setup report present and writes the gating path — slack,
launch, capture — into a sidecar index keyed by module. Run it after a fit and
the identity is preserved even though the report is not.

WHAT IT DOES NOT DO
-------------------
It does not judge. It records launch and capture node names and the slack, and a
human or a later tool compares them. A checker that decided for itself whether
two families are "the same" would be inventing a similarity rule nobody ratified
— and the whole point of M6 is that the identity has to be LOOKED AT.
"""

import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join(ROOT, "reports", "synthesis", "blockpaths")
INDEX = os.path.join(ROOT, "reports", "synthesis", "worst_path_index.json")

# A summarised setup row: "; <slack> ; <from> ; <to> ; <launch clk> ; ..."
ROW = re.compile(r"^;\s*(-?\d+\.\d+)\s*;\s*([^;]+?)\s*;\s*([^;]+?)\s*;")


def worst_row(path):
    """(slack, from, to) of the worst summarised path, or None."""
    try:
        text = io.open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    best = None
    for line in text.split("\n"):
        m = ROW.match(line.strip())
        if not m:
            continue
        try:
            slack = float(m.group(1))
        except ValueError:
            continue
        src, dst = m.group(2).strip(), m.group(3).strip()
        # Header rows repeat the column names; a real row has node-shaped ends.
        if src.lower() in ("from node", "from") or dst.lower() in ("to node", "to"):
            continue
        if best is None or slack < best[0]:
            best = (slack, src, dst)
    return best


def main(argv):
    if not os.path.isdir(BLOCKPATHS):
        print("no blockpaths directory: %s" % BLOCKPATHS)
        return 2

    index = {}
    if os.path.exists(INDEX):
        try:
            index = json.load(io.open(INDEX, encoding="utf-8"))
        except ValueError:
            index = {}

    added, updated, skipped = 0, 0, 0
    for name in sorted(os.listdir(BLOCKPATHS)):
        if not name.endswith(".setup.rpt"):
            continue
        module = name[: -len(".setup.rpt")]
        row = worst_row(os.path.join(BLOCKPATHS, name))
        if row is None:
            skipped += 1
            continue
        rec = {"slackNs": row[0], "from": row[1], "to": row[2]}
        if module not in index:
            index[module] = rec
            added += 1
        elif index[module] != rec:
            # The report has been overwritten by a newer fit. Keep BOTH: the
            # previous identity is the thing M6 needs and the thing that
            # otherwise disappears.
            hist = index[module].get("previous", [])
            prev = dict(index[module])
            prev.pop("previous", None)
            rec["previous"] = ([prev] + hist)[:8]
            index[module] = rec
            updated += 1

    io.open(INDEX, "w", encoding="utf-8", newline="\n").write(
        json.dumps(index, indent=2, sort_keys=True) + "\n"
    )
    print("worst-path index: %d modules (%d new, %d updated, %d unreadable)"
          % (len(index), added, updated, skipped))
    print("wrote %s" % os.path.relpath(INDEX, ROOT))

    # SELF-CHECK. A parser that matches nothing writes an empty index and prints
    # a reassuring line, which is precisely the failure this repository keeps
    # finding in its own tools.
    if len(index) < 3:
        print("WORST-PATH INDEX BROKEN: fewer than three modules parsed -- "
              "refusing to present this as a record")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
