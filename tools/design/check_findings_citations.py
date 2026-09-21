r"""Every `FINDINGS-<lane>.md` cited in the tree must actually exist.

WHY THIS EXISTS. The harness refuses to let a packet write its own findings
file, so transcribing lane findings is the coordinator's job -- and a job that
depends on somebody remembering is a job that silently does not get done. On
2026-09-21 packet GOURAUDLOOK found `zhao_geom_attrpack.sv` and
`zhao_raster_tile_pipe_v2.sv` BOTH citing `FINDINGS-attrlane.md` for the cost
working behind a +24 DSP decision, and the file was in no commit. The numbers
survived only in a ruling and in those two comments.

Sweeping for the general case found EIGHT more, none of them in any of the 88
run folders, and four of them in PRODUCTION RTL:

    fieldw1    fpga/rtl/geometry/zhao_geom_warp.sv, tests/CMakeLists.txt
    geom4      tests/prod/tb_zhao_console_core_smoke.sv
    post3      fpga/rtl/prod/zhao_console_board.sv, zhao_console_core.sv
    post3b     fpga/rtl/prod/zhao_console_board.sv, zhao_console_core.sv
    projadopt  fpga/rtl/raster/zhao_raster_attrdiv.sv, OWNER-RULINGS-...md
    terrain4   design/console_inventory.yml
    terrain6   fpga/rtl/prod/zhao_console_core.sv
    warp       design/contracts/GEOM.WARP.md, OWNER-RATIFICATION-...-WARP.md

A reader chasing one of those lands on nothing. That is worse than no citation,
because a citation is a promise that the reasoning was written down somewhere.

WHY THE KNOWN EIGHT ARE GRANDFATHERED RATHER THAN FIXED. Their content is in
lanes that closed in earlier sessions; the files cannot be written now without
inventing them, and inventing a findings document is worse than lacking one.
But the debt is NOT hidden: this tool PRINTS the grandfathered list on every
run, pass or fail. An allowlist that goes quiet is how a debt becomes
invisible, which is the failure this repository has a whole chapter about.

NEW dangling citations FAIL. That is the point -- the class stops growing.
"""
from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAN_ROOTS = ("fpga", "design", "reports", "tests", "tools")
RUNS = os.path.join(REPO, "runs")

_CITE = re.compile(r"FINDINGS-([A-Za-z0-9_]+)\.md")

# Cited before this check existed, in lanes that have since closed. Each entry
# is a DEBT, not an exemption -- see the module docstring. Removing a name from
# here requires the file to exist, which is the only way an entry should leave.
GRANDFATHERED = {
    "fieldw1", "geom4", "post3", "post3b",
    "projadopt", "terrain4", "terrain6", "warp",
}


def existing_findings() -> set:
    """Every FINDINGS-<lane>.md present under runs/, across ALL run folders.

    Scoped to `runs/` and NOT to the current run on purpose. A lane's findings
    live with the run that produced them, and checking only the newest folder
    would report every older lane as dangling -- a scoped search that reads as
    rigorous while being wrong (packet PROJOUT, 2026-09-21).
    """
    names = set()
    for dirpath, _dirnames, filenames in os.walk(RUNS):
        for fn in filenames:
            m = _CITE.fullmatch(fn)
            if m:
                names.add(m.group(1))
    return names


def citations() -> dict:
    """{lane: [citing file, ...]} across the scanned roots."""
    out = {}
    for root in SCAN_ROOTS:
        base = os.path.join(REPO, root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d != ".git"]
            for fn in filenames:
                if not fn.endswith((".sv", ".svh", ".v", ".md", ".yml",
                                    ".yaml", ".py", ".txt", ".cpp", ".hpp")):
                    continue
                full = os.path.join(dirpath, fn)
                try:
                    with io.open(full, encoding="utf-8", errors="replace") as fh:
                        text = fh.read()
                except OSError:
                    continue
                for m in _CITE.finditer(text):
                    rel = os.path.relpath(full, REPO).replace(os.sep, "/")
                    out.setdefault(m.group(1), set()).add(rel)
    return {k: sorted(v) for k, v in out.items()}


def main() -> int:
    have = existing_findings()
    cited = citations()
    dangling = {k: v for k, v in cited.items() if k not in have}

    new = {k: v for k, v in dangling.items() if k not in GRANDFATHERED}
    old = sorted(k for k in dangling if k in GRANDFATHERED)

    print("findings citations: %d cited, %d present under runs/"
          % (len(cited), len(have)))

    # PRINTED EVERY RUN, pass or fail. An allowlist that goes quiet is how a
    # debt becomes invisible.
    if old:
        print("  %d GRANDFATHERED dangling citation(s) -- a standing debt, not "
              "an exemption:" % len(old))
        for k in old:
            print("      FINDINGS-%s.md  <- %s" % (k, ", ".join(cited[k][:3])))
    stale_allow = sorted(GRANDFATHERED - set(dangling))
    if stale_allow:
        print("  %d allowlist entry/entries no longer dangling -- REMOVE them "
              "from GRANDFATHERED: %s" % (len(stale_allow), ", ".join(stale_allow)))

    if not new:
        print("no NEW dangling findings citations.")
        return 0

    print("\n%d NEW DANGLING CITATION(S). A citation is a promise that the "
          "reasoning was written down; these point at nothing:" % len(new))
    for k in sorted(new):
        print("  FINDINGS-%s.md" % k)
        for f in new[k]:
            print("      cited by %s" % f)
    print("\nEither transcribe the lane's findings to "
          "runs/CLAUDE-RUNS/<run>/FINDINGS-%s.md, or remove the citation. Do "
          "NOT add it to GRANDFATHERED: that list is closed." % sorted(new)[0])
    return 1


# SELF-TEST AT IMPORT. A detector that has not been shown to FIRE has not been
# tested, and this one's whole job is to notice an absence.
assert _CITE.findall("see FINDINGS-attrlane.md for the cost") == ["attrlane"]
assert _CITE.findall("runs/x/FINDINGS-post3b.md and FINDINGS-warp.md") == [
    "post3b", "warp"]
assert _CITE.fullmatch("FINDINGS-terrcomp.md") is not None
assert _CITE.fullmatch("NOTES-terrcomp.md") is None
assert _CITE.findall("no citation here") == []

if __name__ == "__main__":
    sys.exit(main())
