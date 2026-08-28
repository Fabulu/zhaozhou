#!/usr/bin/env python3
"""Every mutant table's anchors must still resolve against the CURRENT RTL.

WHY THIS EXISTS
---------------
On 2026-08-28 the FIELD.V3.EXEC sweep passed 31/31 at 07:31. At 10:36 a commit
added the executor's long-op path and, in doing so, moved two of the lines that
sweep's mutants anchor on. Nothing said anything.

The sweep driver DOES notice -- it exits 3 on an unresolvable anchor -- but only
if somebody runs it. Nobody did, and for the rest of the day "FIELD.V3.EXEC
31/31" was carried in the closed-block table as though it still described the
executor. It described the executor as it stood at 07:31, before the long-op
path existed.

That is the same failure as the ledger gate that was red for eight hours and
the fast lane that was never run: a result that was true when measured, carried
forward as though still true, with nothing checking that the thing it described
had changed underneath it.

This is the cheap check that closes it. It resolves every anchor in every table
against the file that mutant actually names, and it takes under a second, so it
can run after any RTL edit rather than only before a sweep.

    python tools/sweep_anchors_check.py            # every table
    python tools/sweep_anchors_check.py exec rot   # only matching names

WHAT IT DOES NOT DO
-------------------
It does NOT claim the sweep would pass. An anchor that resolves says the mutant
can still be APPLIED, not that it would still be caught -- a mutant can survive
perfectly well against RTL that has moved on. Re-scoring is the sweep's job.
What this rules out is the narrower and more embarrassing case: a table that
cannot even be run against the code it claims to score.

A table whose anchors resolve is therefore necessary and nowhere near
sufficient, and saying so here is the point -- a guard that oversells itself is
how a green check comes to mean less than nobody thinks it does.
"""

import glob
import importlib.util
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _mutate(gold, old, new):
    """The house anchor rule, for tables too old to carry their own `mutate`.

    Kept byte-identical in behaviour to the copies inside the tables: try CRLF
    first, then LF, and demand exactly one match. A guard that resolved anchors
    MORE leniently than the driver does would pass tables the driver then
    refuses, which is worse than not checking at all.
    """
    for nl in ("\r\n", "\n"):
        o = old.replace("\n", nl)
        n = new.replace("\n", nl)
        c = gold.count(o)
        if c == 1:
            if o == n:
                raise ValueError("mutant identical to base")
            return gold.replace(o, n, 1)
        if c > 1:
            raise ValueError("anchor matches %d times" % c)
    raise ValueError("anchor matches 0 times (tried CRLF and LF)")


def load(path):
    name = "mut_" + os.path.basename(path)[:-3]
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main(argv):
    pats = argv[1:]
    tables = sorted(glob.glob(os.path.join(ROOT, "tools", "sweep_*_mutants.py")))
    if pats:
        tables = [t for t in tables if any(p in os.path.basename(t) for p in pats)]
    if not tables:
        sys.stderr.write("no mutant tables matched\n")
        return 2

    files = {}
    total = 0
    broken = []

    for t in tables:
        try:
            mod = load(t)
        except Exception as exc:                      # noqa: BLE001
            broken.append((os.path.basename(t), "-", "table will not import: %s" % exc))
            continue
        entries = getattr(mod, "MUTANTS", None) or getattr(mod, "MUTS", None)
        if entries is None:
            broken.append((os.path.basename(t), "-", "no MUTANTS table"))
            continue
        rtl = getattr(mod, "RTL", None)
        ok = 0
        for e in entries:
            rel = e[1] if len(e) == 4 else rtl
            total += 1
            if rel is None:
                broken.append((os.path.basename(t), e[0], "no file named"))
                continue
            path = os.path.join(ROOT, rel)
            if path not in files:
                try:
                    files[path] = io.open(path, encoding="utf-8", newline="").read()
                except OSError as exc:
                    files[path] = None
                    broken.append((os.path.basename(t), e[0], "unreadable: %s" % exc))
            gold = files[path]
            if gold is None:
                continue
            try:
                getattr(mod, "mutate", _mutate)(gold, e[-2], e[-1])
                ok += 1
            except ValueError as exc:
                broken.append((os.path.basename(t), e[0], str(exc)))
        print("  %-44s %3d/%-3d resolve" % (os.path.basename(t), ok, len(entries)))

    print("")
    if broken:
        print("STALE ANCHORS -- these tables cannot be run against today's RTL:")
        for tbl, name, why in broken:
            print("  %s" % tbl)
            print("      %s" % name)
            print("      %s" % why)
        print("")
        print("An anchor stops resolving when the RTL it names is edited. The sweep")
        print("driver would exit 3 rather than score anything, so the block's last")
        print("tally describes code that no longer exists. Repair the anchor against")
        print("the current file and RE-RUN THAT SWEEP -- repairing alone only makes")
        print("the table runnable again, it does not re-score anything.")
        return 1

    print("all %d anchors across %d table(s) resolve" % (total, len(tables)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
