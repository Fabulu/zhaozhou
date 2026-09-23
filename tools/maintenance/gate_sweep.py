#!/usr/bin/env python3
"""Run EVERY `check_*.py` gate in the tree, bare, and compare to a baseline.

WHY THIS EXISTS (2026-09-23)
----------------------------
The coordinator's standing error in this campaign was recorded in
`reports/HANDOVER-20260919.md` §15.9 as a sentence:

    "I told two lanes the tree is FULLY GREEN when I meant every gate on my
     list is green. DISCOVER THE GATES; do not run a remembered list."

Advisory prose loses to the pull of reporting a status -- CLAUDE.md says so
about a different rule, which was then violated twice in the same session and
had to be moved into a hook. So this is the tool.

Sweeping all 26 `check_*.py` on 2026-09-23 found THREE reds nobody was running:

  * `check_counter_ids`  -- 50 names inserted inside an append-only list, so
    `ZHAO_CNT_CMD_DMA_COMMANDS = 198` was reading catalog index 244;
  * `check_v3_banks`     -- DARK FOR NINE DAYS on `rc=2 CANNOT PARSE`, its own
    self-test included, and it is not registered as a ctest at all;
  * `check_git_autocrlf_guard` -- 9 of its 15 findings were prose, and it was
    blind to the shell fit's own dirty-tree gate.

AND CI CANNOT BE RELIED ON TO CATCH THE NEXT ONE. Of the last 60 runs, 39 were
CANCELLED and 19 failed and NOT ONE succeeded: `concurrency: cancel-in-progress`
plus frequent pushes kills the slow `ctest (fast)` job before it finishes, while
the fast npm and format jobs live long enough to go red. `counter_ids_append_only`
IS a registered ctest and it never ran on the commit that broke it.

THE BASELINE IS THE POINT, NOT THE RUNNING
------------------------------------------
Several gates in this tree are LEGITIMATELY red on inherited debt that is
recorded and triaged -- `check_counter_ids` on 32 counters declared by blocks
and never catalogued, `check_v3_banks` on findings triaged in `3524f6ba`. A
sweep that just prints them teaches people to skim past red, which is the
cry-wolf failure this repository keeps writing down.

So the exit code answers ONE question: **did any gate's status change from the
committed baseline?** A gate that goes red is loud. A gate that goes GREEN is
also reported, because an inherited red being fixed must be recorded rather
than silently absorbed -- and because a gate that starts returning 0 by going
BLIND looks exactly like a repair. `check_v3_banks` returning `rc=2` for nine
days is that failure in its other form.

A gate this sweep has never seen is NOT silently accepted either: it is
reported as NEW and fails the run until someone writes its baseline down. A new
checker landing already-red and being absorbed into the noise is how the list
rotted in the first place.

Usage:
    python tools/maintenance/gate_sweep.py            # compare to baseline
    python tools/maintenance/gate_sweep.py --update   # accept current as baseline
    python tools/maintenance/gate_sweep.py --list     # just show what it found
"""
from __future__ import annotations

import io
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BASELINE = os.path.join(REPO, "design", "gate_baseline.json")
TIMEOUT_S = 300

# Gates that REQUIRE arguments and answer a different question without them.
# Running them bare returns rc=2 usage errors, which is noise, not a finding.
# Named rather than pattern-matched so adding one is a deliberate act.
NEEDS_ARGS = {
    "tools/budget/check_dual18_map.py",
    "tools/budget/check_dual18_atom_routes.py",
}

# A DISCOVERY RULE IS ONLY AS GOOD AS ITS PREDICATE, and this one's predicate is
# a FILENAME. Added 2026-09-23, after "gate_sweep RC 0, every gate matching the
# baseline" was quoted three times in one afternoon while `mutant_copy_drift`
# was RED -- on a regression introduced by that same afternoon's merge. The
# sweep could not see it for one reason: the file is not called `check_*.py`.
#
# So these are named DELIBERATELY, each with what it watches and why its verdict
# is trustworthy. The bar for entry is narrow and it is the important part:
# **the tool's EXIT CODE must carry the verdict.** A tool that prints a finding
# and returns 0 has nothing for a status baseline to compare -- baselining it
# would add a number that cannot move and call it coverage, which is the exact
# reassurance this sweep exists to refuse. `closure_liveness.py` and
# `mutant_drivers.py` were both considered and REFUSED on that test: both print
# real debt lists, both always return 0.
#
# AND TWO OF THESE CARRY THEIR VERDICT BEHIND A FLAG, which is its own trap:
# run bare, `uncashed_cheques.py` and `refmodel_liveness.py` return 0 ALWAYS --
# the checking is inside `if "--gate" in sys.argv`. A sweep that ran everything
# bare "for safety" would have listed them green forever. They are run WITH the
# flag here, and the argv is part of the baseline key so it cannot drift.
EXTRA_GATES = {
    "tools/budget/mutant_copy_drift.py": [],
    # a committed mutant is a COPY; a copy older than its production module is
    # a positive control for a block that no longer exists. Returns 1 on drift.
    "tools/design/wrapper_port_parity.py": [],
    # a `.*` wrapper mutant cannot bind a port the real module gained or lost,
    # and the failure READS LIKE A BROKEN CORE. Returns 1 on either.
    "tools/budget/uncashed_cheques.py": ["--gate"],
    # a module built and installed nowhere, or a fit row gone stale, with no
    # written disposition. BARE THIS RETURNS 0 WHATEVER IT FINDS.
    "tools/budget/refmodel_liveness.py": ["--gate"],
    # a block declaring a reference model with no differential test to hold it
    # to. BARE THIS RETURNS 0 WHATEVER IT FINDS.
}


def gate_key(rel: str, argv: list[str]) -> str:
    """The baseline key. Argv is part of it: the same file run two ways is two
    different questions, and a flag quietly disappearing must read as NEW."""
    return rel if not argv else rel + " " + " ".join(argv)


def discover() -> list[tuple[str, list[str]]]:
    """Every `check_*.py` under tools/, found rather than remembered -- plus the
    deliberately named EXTRA_GATES above, which the filename rule cannot see."""
    out = []
    for dirpath, _dirs, files in os.walk(os.path.join(REPO, "tools")):
        for fn in files:
            if not (fn.startswith("check_") and fn.endswith(".py")):
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), REPO).replace(os.sep, "/")
            if rel in NEEDS_ARGS:
                continue
            out.append((rel, []))
    for rel, argv in EXTRA_GATES.items():
        if os.path.exists(os.path.join(REPO, rel)):
            out.append((rel, list(argv)))
    uniq = {(rel, tuple(argv)) for rel, argv in out}
    return sorted(((rel, list(argv)) for rel, argv in uniq),
                  key=lambda ra: gate_key(ra[0], ra[1]))


def run_gate(rel: str, argv: list[str]) -> tuple[int, str]:
    """Run one gate BARE and return its OWN exit code.

    Bare matters. `cmd | tail` reports tail's status -- a trap this repository
    has paid for more than once, including on the day it was written into every
    brief. So: no pipe, no shell, and the code is read straight off the child.
    ("Bare" is about the PIPE, not the arguments: an EXTRA_GATES flag that turns
    the checking on is part of asking the question at all.)
    """
    try:
        p = subprocess.run([sys.executable, os.path.join(REPO, rel), *argv],
                           cwd=REPO, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return 124, "TIMEOUT after %ds" % TIMEOUT_S
    except OSError as exc:
        return 125, "could not run: %s" % exc
    text = (p.stdout or "") + (p.stderr or "")
    first = ""
    for line in text.splitlines():
        low = line.lower()
        if "fail" in low or "error" in low or "unguarded" in low:
            first = line.strip()
            break
    return p.returncode, first


def load_baseline() -> dict:
    if not os.path.exists(BASELINE):
        return {}
    try:
        return json.load(io.open(BASELINE, encoding="utf-8")).get("gates", {})
    except (OSError, ValueError):
        return {}


def main(argv: list[str]) -> int:
    gates = discover()
    if "--list" in argv:
        for rel, gargv in gates:
            print("  " + gate_key(rel, gargv))
        print("\n%d gate(s) discovered" % len(gates))
        return 0

    base = load_baseline()
    results: dict[str, int] = {}
    changed: list[str] = []
    new: list[str] = []

    print("gate sweep: %d gate(s), each run BARE" % len(gates))
    for rel, gargv in gates:
        g = gate_key(rel, gargv)
        rc, first = run_gate(rel, gargv)
        results[g] = rc
        was = base.get(g)
        if was is None:
            new.append(g)
            mark = "NEW  "
        elif was != rc:
            changed.append(g)
            mark = "MOVED"
        else:
            mark = "     "
        print("  %s RC=%-3d %s" % (mark, rc, g))
        if first and (rc != 0):
            print("          %s" % first[:130])

    if "--update" in argv:
        io.open(BASELINE, "w", encoding="utf-8", newline="\n").write(
            json.dumps({
                "_comment": "Committed status of every check_*.py gate. A gate that MOVES "
                            "from this -- in either direction -- fails the sweep, because an "
                            "inherited red being fixed must be recorded and a gate that starts "
                            "returning 0 by going blind looks exactly like a repair.",
                "gates": dict(sorted(results.items())),
            }, indent=2) + "\n")
        print("\nbaseline written: %s (%d gates)" % (
            os.path.relpath(BASELINE, REPO).replace(os.sep, "/"), len(results)))
        return 0

    print("")
    if new:
        print("%d gate(s) NOT IN THE BASELINE -- write them down deliberately:" % len(new))
        for g in new:
            print("    %s  (RC=%d)" % (g, results[g]))
    if changed:
        print("%d gate(s) MOVED from the baseline:" % len(changed))
        for g in changed:
            print("    %s  %d -> %d" % (g, base[g], results[g]))
    if not new and not changed:
        print("every gate matches the committed baseline (%d gate(s))." % len(results))
        print("NOTE: that is NOT 'the tree is green'. Several gates are")
        print("legitimately red on recorded, triaged, inherited debt. This says")
        print("nothing moved -- which is the only thing a baseline can say.")
        return 0
    print("")
    print("Run with --update to accept the current status, ONCE you have read")
    print("why each one moved. Accepting a red without reading it is how the")
    print("hand list rotted in the first place.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
