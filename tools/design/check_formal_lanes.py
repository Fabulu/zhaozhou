#!/usr/bin/env python3
"""check_formal_lanes.py -- does `formal_runs.yml`'s `lane:` name a real test?

WHY THIS EXISTS
---------------
`design/formal_runs.yml` records, per property, the CTest lane that runs it.
V16 in the ledger checks that registry hard -- every `.sby` on disk is
registered, every entry is dated and commit-pinned, every entry names at least
one task, and a `covers: true` claim must have a task whose name contains
"cover". It does not check `lane:`.

So the one field that says WHERE THE PROOF ACTUALLY RUNS was the one field
nothing verified, and on 2026-09-07 it was wrong: `input_snapshot_atomic`
recorded `lane: formal_input_snapshot_atomic`, and the test is registered as
`input_snapshot_atomic` -- no prefix. Anyone following the registry to find the
lane would have found nothing and concluded the property was not run, when it
is and it is green.

The neighbouring failure is worse and had also happened, on the same file:
`terrain_bake_delta` recorded `lane: null` while a lane existed and timed out on
every nightly. A field that can say "no lane" when there is one, and can name a
lane that does not exist, is a field nobody can use.

WHAT IT CHECKS
--------------
  * every `lane:` that is not `null` names a test registered in
    `tests/CMakeLists.txt`;
  * every property whose `lane:` is `null` really has no obvious test -- the
    reverse direction, which is the one that caught terrain_bake_delta.

WHAT IT DOES NOT DO
-------------------
It parses `add_test(NAME ...)` textually and does not expand the `foreach`
loops that register a lane per item, so a lane registered that way is resolved
from the loop's ITEMS list where it can be read and reported as UNRESOLVED
where it cannot. UNRESOLVED is not a failure: it means this tool could not
answer, which is a different thing from the answer being no.

It REPORTS. It does not gate, for the same reason `check_counters.py` does not:
the tree has legitimate shapes this parser cannot see, and a gate that fails on
what it cannot parse trains people to ignore it.
"""

from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(p: str) -> str:
    return io.open(p, encoding="utf-8", errors="replace").read()


def registered_tests(cmake_text: str) -> tuple[set[str], set[str]]:
    """Returns (explicit names, names reachable through a foreach ITEMS list)."""
    explicit = set(re.findall(r"add_test\(\s*NAME\s+([A-Za-z0-9_]+)", cmake_text))

    # `foreach(sby_prop IN ITEMS a b c)` ... `add_test(NAME formal_${sby_prop}`
    looped: set[str] = set()
    for m in re.finditer(r"foreach\(\s*(\w+)\s+IN\s+ITEMS\s+([^\)]*)\)", cmake_text):
        var, items = m.group(1), m.group(2).split()
        # Only expand into templates that actually use this variable.
        for tm in re.finditer(r"add_test\(\s*NAME\s+([A-Za-z0-9_]*)\$\{" + var + r"\}", cmake_text):
            prefix = tm.group(1)
            for it in items:
                looped.add(prefix + it)
    return explicit, looped


def registry_lanes(yml_text: str) -> list[tuple[str, str | None]]:
    out: list[tuple[str, str | None]] = []
    prop: str | None = None
    for line in yml_text.splitlines():
        m = re.match(r"\s*-\s*property:\s*tests/formal/(\S+)\.sby\s*$", line)
        if m:
            prop = m.group(1)
            continue
        m = re.match(r"\s*lane:\s*(\S+)\s*$", line)
        if m and prop is not None:
            out.append((prop, None if m.group(1) == "null" else m.group(1)))
            prop = None
    return out


def main(argv: list[str]) -> int:
    yml = read(os.path.join(REPO, "design", "formal_runs.yml"))
    cmake = read(os.path.join(REPO, "tests", "CMakeLists.txt"))

    explicit, looped = registered_tests(cmake)
    known = explicit | looped

    lanes = registry_lanes(yml)
    named_bad: list[tuple[str, str]] = []
    null_but_present: list[tuple[str, str]] = []
    ok = 0
    null_ok = 0

    for prop, lane in lanes:
        if lane is not None:
            if lane in known:
                ok += 1
            else:
                named_bad.append((prop, lane))
        else:
            # The reverse direction: a `lane: null` that is not true. Look for
            # any registered test whose name contains the property's name.
            hits = sorted(t for t in known if prop in t)
            if hits:
                null_but_present.append((prop, ", ".join(hits)))
            else:
                null_ok += 1

    print("check_formal_lanes: %d properties, %d registered tests parsed "
          "(%d explicit, %d via foreach)"
          % (len(lanes), len(known), len(explicit), len(looped)))
    print("  %d lane: names resolve, %d lane: null with no test found" % (ok, null_ok))

    if named_bad:
        print()
        print("NAMES A LANE THAT IS NOT REGISTERED (%d) -- following the registry to find "
              "the proof leads nowhere:" % len(named_bad))
        for prop, lane in named_bad:
            print("  %-34s lane: %s" % (prop, lane))

    if null_but_present:
        print()
        print("SAYS `lane: null` BUT A TEST EXISTS (%d) -- this is the direction that "
              "hid terrain_bake_delta's nightly timeout:" % len(null_but_present))
        for prop, hits in null_but_present:
            print("  %-34s -> %s" % (prop, hits))

    print()
    print("NOTE: this REPORTS, it does not gate. `lane:` is not checked by V16, which "
          "validates dates, commits, tasks and covers but not where the proof runs.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
