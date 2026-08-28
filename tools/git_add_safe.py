#!/usr/bin/env python3
"""`git add`, refused if a running sweep can mutate any of the named files.

WHY THIS IS A SCRIPT AND NOT A RULE IN A DOCUMENT
--------------------------------------------------
On 2026-08-28 a mutant reached a commit because `git add` read a file while its
own sweep had a mutation applied. The sweep restored the file afterwards, so
the working tree was clean at every moment anything looked at it, and the
defect lived on in the commit.

The rule written that hour -- never stage a file a running sweep can mutate --
was violated within the hour, on a different file, and survived only because
the timing happened to land between two mutations. A rule that depends on
remembering it at the exact moment of a two-word command is not a guard. This
is.

    python tools/git_add_safe.py <path> [path...]

Refuses, loudly, if any path is in the mutation set of a RUNNING sweep, and
otherwise runs `git add` on all of them.

"Running" is a log with no `EXIT:` line that has ALSO been written in the last
twenty minutes. The second half is not decoration: this repository carries logs
from killed and renamed sweeps going back days, none of which has an EXIT line,
and the first version of this script refused to stage anything at all because
of four of them. A guard that always says no is a guard nobody runs.

A sweep killed minutes ago will therefore still block, which is right -- it may
have stranded a mutant on disk, and `tools/sweep_check_clean.py` is what
settles that. Twenty minutes later it stops blocking, and the same check is
still the thing to run.
"""

import glob
import importlib.util
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# A sweep writes its log continuously -- a line per mutant, and the rebuild it
# drives touches the tree constantly. So a log that has not been written for
# this long is not a running sweep, whatever its contents say.
#
# The threshold is generous because the slow step is the cmake RECONFIGURE the
# driver runs before every rebuild, measured at several minutes on this tree.
# Twenty leaves room for the worst case and still rules out old logs.
#
# WITHOUT THIS THE CHECK IS USELESS: the repo carries logs from killed and
# renamed sweeps going back days, none with an EXIT line, and the first version
# refused to stage ANYTHING because of four of them.
STALE_SECONDS = 20 * 60


def live_sweeps():
    """(sweep name, log path) for every sweep that is actually still running."""
    out = []
    now = time.time()
    pattern = os.path.join(ROOT, "runs", "CLAUDE-RUNS", "*", "*_sweep.log")
    for log in glob.glob(pattern):
        try:
            if now - os.path.getmtime(log) > STALE_SECONDS:
                continue
            with open(log, encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        if "\nEXIT:" in text or text.startswith("EXIT:"):
            continue
        name = os.path.basename(log)[: -len("_sweep.log")]
        out.append((name, log))
    return out


def files_of(name):
    """Every file that sweep's mutant table can touch."""
    table = os.path.join(ROOT, "tools", "sweep_%s_mutants.py" % name)
    if not os.path.exists(table):
        # A live sweep whose table cannot be found is not a reason to relax:
        # say so and let the caller decide, rather than quietly allowing it.
        return None
    spec = importlib.util.spec_from_file_location("mut_" + name, table)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    entries = getattr(mod, "MUTANTS", None)
    if entries is None:
        entries = getattr(mod, "MUTS", None)
    if entries is None:
        return None
    rtl = getattr(mod, "RTL", None)
    paths = set()
    for e in entries:
        paths.add(e[1] if len(e) == 4 else rtl)
    return {os.path.normpath(p) for p in paths if p}


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: git_add_safe.py <path> [path...]\n")
        sys.stderr.write("       git_add_safe.py --check\n")
        return 2

    # --check answers "is a sweep running at all", which is a DIFFERENT
    # question from "may I stage this file" and has its own reason to exist.
    #
    # A sweep re-runs `cmake` over the WHOLE PROJECT before every rebuild, so
    # ANY file that breaks the configure breaks the sweep -- including files the
    # sweep never elaborates. A port change that spans two files has a window,
    # however short, in which the project does not configure.
    #
    # On 2026-08-28 that window cost a 23-mutant RING sweep: seventeen mutants
    # DISCARDED with "model or exe absent after rebuild", and the cause was a
    # missing pin in a file the sweep had nothing to do with. The guard was
    # right -- it discarded rather than scoring -- but the run was lost.
    #
    # So ask this BEFORE starting a multi-file RTL edit, not only before
    # staging one.
    if argv[1] == "--check":
        live = live_sweeps()
        if not live:
            print("no sweep is running")
            return 0
        for name, log in live:
            print("RUNNING: %s  (%s)" % (name, os.path.relpath(log, ROOT)))
        print("\nA sweep configures the WHOLE project on every rebuild, so an edit\n"
              "that leaves the project un-configurable for even a moment breaks it --\n"
              "in a file the sweep never touches. Finish the edit first, or wait.")
        return 1

    wanted = {os.path.normpath(p) for p in argv[1:]}

    blocked = []
    unknown = []
    for name, log in live_sweeps():
        owned = files_of(name)
        if owned is None:
            unknown.append((name, log))
            continue
        for p in sorted(wanted & owned):
            blocked.append((p, name))

    if unknown:
        for name, log in unknown:
            sys.stderr.write(
                "REFUSING: a sweep named %r is running (%s) and its mutant table\n"
                "          could not be read, so what it can mutate is unknown.\n"
                % (name, os.path.relpath(log, ROOT)))
        return 1

    if blocked:
        sys.stderr.write("REFUSING TO STAGE -- a running sweep can mutate these:\n")
        for p, name in blocked:
            sys.stderr.write("    %s   (sweep: %s)\n" % (p, name))
        sys.stderr.write(
            "\nStaging one of these freezes whatever the file says AT THAT INSTANT.\n"
            "A mutant reached a commit that way on 2026-08-28: the sweep restored\n"
            "the file afterwards, so the working tree was clean every time anything\n"
            "looked, and the defect shipped in the commit.\n"
            "\nWait for the sweep, or stage the other files only.\n")
        return 1

    r = subprocess.run(["git", "add"] + argv[1:], cwd=ROOT)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv))
