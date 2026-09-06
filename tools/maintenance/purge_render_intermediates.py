#!/usr/bin/env python3
"""purge_render_intermediates.py -- reclaim raw render frames from finished runs.

WHY THIS EXISTS
---------------
On 2026-09-06 the machine's C: drive hit ZERO bytes free, 952 GB of 952 GB.
Quartus died with "Es steht nicht genug Speicherplatz auf dem Datentraeger",
concurrent Verilator builds died with "No space left on device", a full fit was
killed 55 minutes into placement, and -- the part that was nearly expensive --
the disk filled DURING a fire-test mutation and left a ZERO-BYTE backup file.
A moment later and the repository would have held deliberately broken RTL with
a truncated backup beside it.

About 33 GB of that was ours: roughly 129,000 `.rgb` files, the uncompressed
intermediate frame buffers the render pipeline writes and never removes.

THE HALF-SOLVED PROBLEM, WHICH IS THE ACTUAL LESSON
---------------------------------------------------
`.gitignore` has covered `*.rgb` since 2026-08-28, added after an over-broad
`git add` swept 867 raw frames into three commits and the owner had to
authorise a history rewrite. Its comment is exactly right about evidence:

    THE EVIDENCE IS THE PNG CONTACT SHEET, NOT THE FRAMES IT WAS MADE FROM.

So "never committed" was solved thoroughly, and "never accumulates" was never
solved at all -- the frames simply stopped being visible to git while
continuing to fill the disk. A rule that makes waste invisible to your tooling
is not a rule that removes it, and the failure took eight days and a dead
toolchain to surface.

WHAT IT KEEPS
-------------
Only regenerable intermediates go. The OUTPUTS stay, always:

  *.webm   the encoded clips -- the thing anyone actually looks at
  *.png    contact sheets and stills -- the committed evidence
  *.md     every log, report and finding
  *.pack   git object packs, which are not render data at all

DRY RUN BY DEFAULT. It prints what it would free and touches nothing until
`--apply`. A tool that deletes 33 GB on an accidental invocation is a worse
problem than the disk it was cleaning.

ACTIVE RUNS ARE PROTECTED. Anything modified within `--keep-hours` (default 48)
is left alone, so a run still being worked on cannot have its frames pulled out
from under it mid-pass. `--keep-newest` additionally spares the N most recently
touched run folders whatever their age.
"""

import argparse
import os
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Extensions that are pure intermediates: reproducible by re-running the
# pipeline, and never the evidence for anything.
PURGE_EXT = (".rgb",)

# Directory names whose entire contents are working data. Named rather than
# pattern-matched so that adding one is a deliberate act.
PURGE_DIRS = ("renders-baseline", "scratch")

KEEP_EXT = (".webm", ".png", ".md", ".pack", ".json", ".yml", ".sv", ".cpp", ".py")


def human(n):
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if abs(n) < 1024.0:
            return "%.2f %s" % (n, unit)
        n /= 1024.0
    return "%.2f PB" % n


def scan(root, keep_seconds, keep_newest):
    """Return (victims, skipped_recent, by_dir) without deleting anything."""
    now = time.time()

    # Newest run folders, spared wholesale.
    runs_root = os.path.join(root, "runs", "CLAUDE-RUNS")
    spared = set()
    if keep_newest and os.path.isdir(runs_root):
        entries = []
        for name in os.listdir(runs_root):
            p = os.path.join(runs_root, name)
            if os.path.isdir(p):
                try:
                    entries.append((os.path.getmtime(p), p))
                except OSError:
                    pass
        entries.sort(reverse=True)
        spared = {p for _m, p in entries[:keep_newest]}

    victims = []
    skipped_recent = 0
    by_dir = {}

    for dirpath, dirnames, filenames in os.walk(root):
        # Never walk into git internals or the build tree.
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "node_modules")]

        if any(dirpath.startswith(s) for s in spared):
            continue

        purge_whole_dir = os.path.basename(dirpath) in PURGE_DIRS

        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if not purge_whole_dir and ext not in PURGE_EXT:
                continue
            if purge_whole_dir and ext in KEEP_EXT:
                continue
            full = os.path.join(dirpath, fn)
            try:
                st = os.stat(full)
            except OSError:
                continue
            if (now - st.st_mtime) < keep_seconds:
                skipped_recent += 1
                continue
            victims.append((full, st.st_size))
            by_dir[dirpath] = by_dir.get(dirpath, 0) + st.st_size

    return victims, skipped_recent, by_dir


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=os.path.dirname(REPO),
                    help="tree to sweep (default: the zencrifice root, so sibling "
                         "creature working directories are covered -- 15 of the "
                         "33 GB sat OUTSIDE zhaozhou)")
    ap.add_argument("--apply", action="store_true",
                    help="actually delete; without this nothing is touched")
    ap.add_argument("--keep-hours", type=float, default=48.0,
                    help="spare anything modified this recently (default 48)")
    ap.add_argument("--keep-newest", type=int, default=3,
                    help="spare the N most recently touched run folders entirely")
    args = ap.parse_args(argv[1:])

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("purge: %s is not a directory" % root)
        return 2

    victims, skipped, by_dir = scan(root, args.keep_hours * 3600.0, args.keep_newest)
    total = sum(sz for _f, sz in victims)

    print("purge_render_intermediates: %s" % root)
    print("  candidates : %d files, %s" % (len(victims), human(total)))
    print("  spared     : %d modified within %.0f h, plus the %d newest run folders"
          % (skipped, args.keep_hours, args.keep_newest))

    if by_dir:
        print("  largest holders:")
        for d, sz in sorted(by_dir.items(), key=lambda kv: kv[1], reverse=True)[:8]:
            print("    %10s  %s" % (human(sz), os.path.relpath(d, root)))

    if not victims:
        print("  nothing to do.")
        return 0

    if not args.apply:
        print()
        print("  DRY RUN -- nothing deleted. Re-run with --apply to reclaim %s."
              % human(total))
        return 0

    freed = 0
    failed = 0
    for full, sz in victims:
        try:
            os.remove(full)
            freed += sz
        except OSError:
            failed += 1
    print()
    print("  deleted %d files, reclaimed %s%s"
          % (len(victims) - failed, human(freed),
             "" if not failed else "  (%d could not be removed, likely locked)" % failed))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
