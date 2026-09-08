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

AMENDED 2026-09-08 BY THE OWNER BRIEF -- AND THE AMENDMENT MATTERS
-----------------------------------------------------------------
Docket M6 said a Fmax delta is attributable only if the worst path FAMILY is
the same on both sides. The brief rejects that as a rule, and it is right:

    "matching the worst-path family is neither necessary nor sufficient for
     attribution: a successful repair often SHOULD change which path is worst."

That is obviously true once stated. A change that removes the gating cone will
of course be followed by a different cone gating. Requiring the families to
match would reject exactly the repairs that worked.

So this tool KEEPS ITS JOB and LOSES ITS INFERENCE. Recording which path gated a
fit is still worth doing -- that evidence is destroyed by the next fit of the
same module and cannot be recovered afterwards. What must not be done is to
conclude "same family, therefore attributable" or "different family, therefore
not". Attribution needs the separate structural, measured, repeatability and
causal claims the brief sets out; this file supplies one input to that and
decides nothing.

Related, and also conceded: the +10.90 MHz reseed figure was reported here too
confidently. Two post-change seeds are encouraging; their 1.13 MHz spread is
not a bound on seed variation.

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

# A summarised setup row, which has EIGHT columns:
#   ; Slack ; From Node ; To Node ; Launch Clock ; Latch Clock ; Relationship ;
#   Clock Skew ; Data Delay ;
#
# Anchoring on the full width matters. The first version matched a three-column
# prefix, which also matches rows in the PER-PATH DETAIL tables further down the
# same report -- two-column rows like "; Slack ; 1.623 ;" and interconnect rows
# whose second and third fields are blank. On this tool's first real use it
# therefore recorded slack 0.000 with empty node names for a block whose true
# worst path is +1.623 ns: a number that is both wrong and, being a clean zero,
# unremarkable enough to be believed. That is this repository's own law -- "a
# number that is exactly zero is a broken instrument until proven otherwise" --
# committed by the person who had just written the law down.
ROW = None  # superseded: see split_row(). Kept out of the module namespace so
            # nobody reaches for a regex here again.

# EIGHT columns is the summary row's signature:
#   ; Slack ; From Node ; To Node ; Launch Clock ; Latch Clock ; Relationship ;
#   Clock Skew ; Data Delay ;
#
# This is a SPLIT and not a regular expression, deliberately. The first version
# matched a three-column prefix, which also matches rows in the PER-PATH DETAIL
# tables further down the same report -- two-column rows like
# "; Slack ; 1.623 ;" and interconnect rows whose node fields are blank. On this
# tool's first real use it recorded slack 0.000 with empty node names for a
# block whose true worst path is +1.623 ns: a number that is both wrong and,
# being a clean zero, unremarkable enough to be believed. That is this
# repository's own law -- "a number that is exactly zero is a broken instrument
# until proven otherwise" -- committed by the person who had just written the
# law down.
#
# The SECOND attempt at a fix was an eight-group lazy regex, which backtracked
# catastrophically and hung on a real report. Two wrong parsers in a row is the
# argument for not parsing a fixed-width table with a pattern at all.
NCOLS = 8


def split_row(line):
    """(slack, from, to) if `line` is a summary row, else None."""
    line = line.strip()
    if not line.startswith(";") or not line.endswith(";"):
        return None
    f = [c.strip() for c in line[1:-1].split(";")]
    if len(f) != NCOLS:
        return None
    src, dst = f[1], f[2]
    # Header rows repeat the column names; a real row has node-shaped ends.
    if src.lower() in ("from node", "from") or dst.lower() in ("to node", "to"):
        return None
    # A path with no endpoints is not a path. This is the guard that would have
    # caught the 0.000 row.
    if not src or not dst:
        return None
    try:
        return (float(f[0]), src, dst)
    except ValueError:
        return None


# A port-origin path starts at a bare identifier: no hierarchy separator, no
# synthesised-node tilde. In a BLOCK fit those are virtual pins.
_PORTISH = re.compile(r"^[a-z_][a-z0-9_]*(\[\d+\])?$")


def is_port_origin(name):
    return ("|" not in name) and ("~" not in name) and bool(_PORTISH.match(name))


def path_census(path):
    """Distilled facts the raw report is otherwise the only source of.

    2026-09-08: the V3 island's worst path was a virtual pin at -2.134 ns, and
    the worst path starting INSIDE the design was -2.093 -- so the whole port
    boundary was worth 41 picoseconds. That conclusion needed all 200 summarised
    rows, not the single worst one, and the raw setup report is 2 MB and is
    overwritten by the next fit of the same module.

    Recording the census here is what makes the raw report redundant rather than
    merely large. Data delay is included because slack alone misleads: on that
    same fit the deepest cone (15.4 ns) was FLATTERED by 3.3 ns of favourable
    clock skew into looking tied with an 11.5 ns one.
    """
    try:
        text = io.open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    rows = []
    for line in text.split(chr(10)):
        line = line.strip()
        if not (line.startswith(";") and line.endswith(";")):
            continue
        f = [c.strip() for c in line[1:-1].split(";")]
        if len(f) != NCOLS:
            continue
        try:
            slack, skew, delay = float(f[0]), float(f[6]), float(f[7])
        except ValueError:
            continue
        if not f[1] or not f[2]:
            continue
        rows.append((slack, f[1], f[2], skew, delay))
    if not rows:
        return None
    ports = [r for r in rows if is_port_origin(r[1])]
    inside = [r for r in rows if not is_port_origin(r[1])]
    out = {"summarisedPaths": len(rows),
           "portOriginPaths": len(ports),
           "internalPaths": len(inside)}
    if ports:
        out["worstPortSlackNs"] = min(r[0] for r in ports)
    if inside:
        b = min(inside, key=lambda r: r[0])
        out["worstInternalSlackNs"] = b[0]
        out["worstInternalFrom"] = b[1]
        out["worstInternalTo"] = b[2]
    if ports and inside:
        out["boundaryWorthNs"] = round(out["worstInternalSlackNs"] -
                                       out["worstPortSlackNs"], 4)
    deepest = max(rows, key=lambda r: r[4])
    out["deepestDataDelayNs"] = deepest[4]
    out["deepestFrom"] = deepest[1]
    out["deepestTo"] = deepest[2]
    out["deepestSlackNs"] = deepest[0]
    out["deepestClockSkewNs"] = deepest[3]
    return out


def worst_row(path):
    """(slack, from, to) of the worst summarised path, or None."""
    try:
        text = io.open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    best = None
    for line in text.split(chr(10)):
        row = split_row(line)
        if row is None:
            continue
        if best is None or row[0] < best[0]:
            best = row
    return best


# THE KNOWN-BAD REPORT, checked on every run.
#
# Two rows the old regex accepted and the new one must reject, plus the real
# summary row it must find. A detector that has not been shown to FIRE has not
# been tested, and this one shipped untested against real input.
_FIRE = """
; Slack ; From Node ; To Node ; Launch Clock ; Latch Clock ; Relationship ; Clock Skew ; Data Delay ;
; 1.623 ; cur_q.count[0] ; iss_tmu_valid_o ; clk ; clk ; 10.000 ; -6.286 ; 2.031 ;
; 2.680 ; cur_q.owner[12] ; req_src_id_o[14] ; clk ; clk ; 10.000 ; -6.301 ; 0.959 ;
; 0.000 ;  ;  ;
; Slack              ; 1.623           ;
"""


def self_fire_test():
    """True if the parser still picks the summary row and rejects the debris."""
    rows = [r for r in (split_row(l) for l in _FIRE.split(chr(10))) if r]
    if len(rows) != 2:
        return False
    worst = min(rows, key=lambda r: r[0])
    # The right answer is the 1.623 ns path, NOT the 0.000 row and NOT the
    # -6.301 clock-skew column that a column miscount would reach for.
    return abs(worst[0] - 1.623) < 1e-9 and worst[1] == "cur_q.count[0]"


def main(argv):
    if not self_fire_test():
        print("WORST-PATH PARSER BROKEN: it no longer selects the summary row, "
              "or has started accepting endpoint-less debris again. Refusing to "
              "write an index from a parser that cannot be trusted.")
        return 2

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
        census = path_census(os.path.join(BLOCKPATHS, name))
        if census:
            rec.update(census)
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
