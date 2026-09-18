#!/usr/bin/env python3
"""Which DECLARED sources of a fit actually elaborate into its synthesis?

A fit target's `sources:` list in `design/fit_targets.yml` is read by two
audiences with different expectations. Quartus reads it as "analyse these
files"; a human reads it as "this is what the number covers". Those are not the
same list, and nothing was checking the gap.

Found 2026-09-18 on `zhao_shell_top_v2`: its closure declares 97 files, and
EIGHT of them -- `zhao_geom_project`, `zhao_geom_setup`, `zhao_geom_meshfetch`,
`zhao_geom_assemble`, `zhao_geom_vdecode`, `zhao_geom_assetfetch`,
`zhao_geom_clip`, `zhao_geom_depthquant` -- are instantiated nowhere in the
resulting hierarchy. The composed shell has no geometry front end, and
`zhao_geom_project.sv` sitting in its source list is exactly what would persuade
a reader otherwise. 13,329 ALUT of blocks that the receipt appears to cover and
does not.

The structural proof that it is real rather than a reporting artefact:
`zhao_project_core.sv` is NOT in that closure, and `zhao_geom_project`
instantiates it. Had the block elaborated, the fit would have failed
`MODMISSING`. It fit clean three times.

Two costs, and the second is the one that bites during a campaign:

  1. **The receipt overstates its own scope.** Every comparison of that fit's
     ALM against a budget is a comparison against a part.
  2. **It is NOT a freeze, whatever it feels like.** This paragraph first
     claimed a dead entry locks its file for the duration of a fit. It does
     not: `run_block_fit.ps1` snapshots every declared source into the
     workspace and points the QSF at the copies, printing "the live tree
     cannot reach this fit" as it goes, and `QUARTUS_GOTCHAS.md` §11 has
     carried a supersession box saying so since 2026-09-03. The claim is
     struck; cost 1 above stands on its own and never needed it.

Method: this compares the declared list against the `Info (12128): Elaborating
entity "X"` lines of that receipt's own `.map.rpt`, unioned with the instance
names in its Compilation Hierarchy Node table. The Info lines are authoritative
and the table is corroboration -- the union can only ever call a module MORE
alive, which is the safe direction for a check whose false positives accuse a
live block. The first version used the table alone and did exactly that, to
`zhao_texture_mod255`, on its first run.

Its one limitation, stated up front: it reports on the arrangement AS FITTED. A
file dead because a parameter deselected it is reported dead, correctly, for
that receipt -- and a different parameterisation may light it up. The tool names
the receipt it read. Several rows it prints are deliberate: a V1 sibling kept in
the list beside its V2, or an unselected arithmetic variant. Read the list, do
not act on the count.

Usage:
    python tools/budget/closure_liveness.py            # every receipt with a map report
    python tools/budget/closure_liveness.py <label>    # one, verbosely
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join(ROOT, "reports", "synthesis", "blockpaths")

# THE AUTHORITATIVE SIGNAL. Quartus emits one of these per elaborated entity:
#
#   Info (12128): Elaborating entity "zhao_texture_mod255" for hierarchy "..."
#
# It is not the same as "Found entity 1: zhao_x", which only means the file was
# ANALYSED. That distinction is the whole measurement, and it is the one the
# eight dead geometry blocks fail: analysed, never elaborated.
ELABORATING = re.compile(r'Elaborating entity "(zhao_[a-z0-9_]+)"')

# The hierarchy table's instance form, kept as CORROBORATION only.
#
# The first version of this tool used it as the primary signal and produced a
# false positive immediately: `zhao_texture_mod255` is elaborated (one Info
# 12128 line, inside zhao_texture_mosaic_v2) and does NOT get its own row in
# the Compilation Hierarchy Node table, because Quartus folds small entities
# into the parent's row. Reading absence from that table as absence from the
# design accuses a live module.
#
# The error ran in the ACCUSING direction, which is why it was caught in one
# run rather than trusted for weeks -- an audit that reports too many dead
# entries gets checked. Recorded because the same tool reading the same table
# the other way round would have been silent and wrong.
INSTANCE = re.compile(r"\|(zhao_[a-z0-9_]+):")
HIER = "Compilation Hierarchy Node"

# Packages declare types and instantiate nothing. Reporting them would bury the
# signal under five guaranteed rows per receipt.
def is_package(module):
    return module.endswith("_pkg")


def elaborated(map_text):
    """Modules Quartus elaborated, or None if the report cannot say.

    The Info 12128 lines are the answer. The hierarchy table is unioned in as
    corroboration -- it can only ADD names, never remove one, so a table that
    folds an entity away cannot turn a live module dead.
    """
    live = set(ELABORATING.findall(map_text))
    i = map_text.find(HIER)
    if i < 0 and not live:
        return None                  # neither signal present: abstain
    if i >= 0:
        live |= set(INSTANCE.findall(map_text[i:i + 900000]))
    return live


def declared(sha_text):
    """Module basenames from a receipt's own `.sources.sha256` sidecar.

    The sidecar is used rather than fit_targets.yml on purpose: it is what the
    fit ACTUALLY snapshotted, hash by hash, so the comparison cannot drift with
    a later edit to the declaration.
    """
    return [t[:-3] for t in sha_text.split() if t.endswith(".sv")]


def receipts():
    if not os.path.isdir(BLOCKPATHS):
        return []
    out = []
    for name in sorted(os.listdir(BLOCKPATHS)):
        if name.endswith(".map.rpt"):
            label = name[: -len(".map.rpt")]
            sha = os.path.join(BLOCKPATHS, label + ".sources.sha256")
            if os.path.exists(sha):
                out.append(label)
    return out


def audit(label):
    with open(os.path.join(BLOCKPATHS, label + ".map.rpt"), "r", errors="replace") as fh:
        live = elaborated(fh.read())
    if live is None:
        return None
    with open(os.path.join(BLOCKPATHS, label + ".sources.sha256"), "r", errors="replace") as fh:
        decl = declared(fh.read())
    top = label.split("@")[0]
    dead = [m for m in decl
            if m not in live and not is_package(m) and m != top
            and not top.startswith(m)]
    return {"label": label, "declared": len(decl), "live": len(live), "dead": sorted(dead)}


def self_test():
    """Both polarities, on the shape that produced the real finding."""
    sample_map = (
        "noise\n" + HIER + "\n"
        "; |zhao_top|   ; 10 (1) ;\n"
        "; |zhao_alive:u_a| ; 5 (5) ;  |zhao_top|zhao_alive:u_a\n"
        "; |zhao_alive_child:u_b| ; 2 (2) ; |zhao_top|zhao_alive:u_a|zhao_alive_child:u_b\n"
    )
    live = elaborated(sample_map)
    assert live == {"zhao_alive", "zhao_alive_child"}, "instance extraction wrong: %r" % (live,)

    # A module named ONLY in an analysis line must not count as elaborated --
    # that is precisely how zhao_geom_project looked, and reading the whole
    # file instead of the hierarchy table would have called it live.
    analysed_only = "Info (12023): Found entity 1: zhao_dead\n" + sample_map
    assert "zhao_dead" not in elaborated(analysed_only), (
        "an ANALYSED-only module was counted as elaborated -- the hierarchy "
        "table offset is wrong and every receipt would read clean")

    # And the offset must actually be doing work: text BEFORE the table is
    # excluded even when it carries the instance form.
    before = "; |zhao_ghost:u_z| ;\n" + sample_map
    assert "zhao_ghost" not in elaborated(before), "hierarchy table offset not applied"

    # THE FALSE POSITIVE THAT THE FIRST VERSION SHIPPED. A module Quartus
    # elaborated but folded into its parent's hierarchy row has an Info 12128
    # line and no table row of its own. It must read LIVE.
    folded = ('Info (12128): Elaborating entity "zhao_folded" for hierarchy "x|y"\n'
              + sample_map)
    assert "zhao_folded" in elaborated(folded), (
        "a folded-but-elaborated module was called dead -- the accusing direction")

    # And the converse must still hold: ANALYSED-only is not elaborated.
    assert "zhao_dead" not in elaborated(analysed_only)

    assert declared("aa  zhao_a.sv\nbb  zhao_b.sv\ncc  notes.txt\n") == ["zhao_a", "zhao_b"]
    assert is_package("zhao_pkg") and not is_package("zhao_geom_project")

    # A map report with no hierarchy table (a failed or map-only run) must
    # return None rather than an empty set -- an empty set would report every
    # declared source as dead, which is loud, wrong, and would get the tool
    # switched off on its first encounter with a partial receipt.
    assert elaborated("no table here") is None, "a tableless report must abstain, not accuse"
    return True


if not __debug__:
    raise RuntimeError("closure_liveness refuses Python -O: it removes the "
                       "assertion-based controls that keep the parser honest")
if not self_test():
    raise AssertionError("closure_liveness self-test failed")


def main(argv):
    labels = [argv[1]] if len(argv) > 1 else receipts()
    if not labels:
        print("no receipts with both a .map.rpt and a .sources.sha256")
        return 0
    rows = []
    for label in labels:
        r = audit(label)
        if r is None:
            if len(argv) > 1:
                print("%s: no Compilation Hierarchy Node table (map-only or "
                      "failed run) -- abstaining" % label)
            continue
        rows.append(r)
    rows.sort(key=lambda r: -len(r["dead"]))
    worst = 0
    for r in rows:
        if not r["dead"]:
            continue
        worst = max(worst, len(r["dead"]))
        print("%-58s %3d declared, %3d dead"
              % (r["label"], r["declared"], len(r["dead"])))
        if len(argv) > 1 or len(r["dead"]) >= 5:
            for m in r["dead"]:
                print("        %s" % m)
    clean = sum(1 for r in rows if not r["dead"])
    print("\n%d receipt(s) audited; %d declare only what they elaborate."
          % (len(rows), clean))
    print("A dead entry overstates the receipt's scope AND freezes its file for")
    print("the duration of every fit that declares it. Remove it, or mark it in")
    print("design/fit_targets.yml so the list stops reading as a measurement.")
    print("\nReported AS FITTED: a source deselected by a parameter is dead for")
    print("that receipt and may be live in another.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
