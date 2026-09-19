#!/usr/bin/env python3
"""check_counter_ids.py -- counter_id = catalog index, and the index never moves.

WHY THIS EXISTS (2026-09-19, cmdmem packet, owner ruling R19)
------------------------------------------------------------
`spec/counters.md` 2 makes a counter's id its ZERO-BASED POSITION in
`design/blocks.yml`'s `counter_catalog`, and makes that list APPEND-ONLY: "an
existing index is never renumbered and never reused". The ledger's V15 checks
the list for duplicates and nothing else -- so twenty-three commits PREPENDED
new entries at the top, and `frame_cycles`, whose id is 0 in
`zhao_pkg::ZHAO_CNT_FRAME_CYCLES`, in every wave-2 capture's COUNTERS section
and in DEBUG.COUNTERS, had become catalog index 150. Every phase-2 id the RTL
emits disagreed with the catalog, and every tool that checks the catalog said
nothing, because none of them compares the catalog with anything but itself.
(V15 could not have caught it even in principle; it also never ran, because
the ledger's schema stage fails first and the rule stage is skipped.)

So this checks the two things that make an id mean something:

  1. APPEND-ONLY against `design/counter_ids.lock` -- the catalog, in id order,
     as last accepted. The catalog must BEGIN with the lock exactly; new names
     may only follow it. Adding a counter is: append it to the catalog, run
     `--update-lock`, commit both.
  2. THE RTL AGREES. Every `ZHAO_CNT_*` localparam in `zhao_pkg.sv` carries its
     catalog name in its trailing comment, and its value must be that name's
     catalog index.

Plus V15's own content, restated so it cannot be skipped: no duplicates, and
every name a block declares in `counters:` is in the catalog.

It asserts at import that its own parsers read a known-good example, and
`--self-test` plants a prepend and a wrong RTL id and requires both to FIRE.
"""
from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKS = os.path.join(REPO, "design", "blocks.yml")
LOCK = os.path.join(REPO, "design", "counter_ids.lock")
PKG = os.path.join(REPO, "fpga", "rtl", "common", "zhao_pkg.sv")

ENTRY_RE = re.compile(r"^  - (\S+)\s*(#.*)?$")
CNT_RE = re.compile(
    r"localparam\s+logic\s*\[15:0\]\s*(ZHAO_CNT_\w+)\s*=\s*16'd(\d+)\s*;\s*//\s*(\w+)")
COUNTERS_RE = re.compile(r"^    counters:\s*\[(.*)\]\s*$", re.M)


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read().replace("\r\n", "\n")


def catalog(text):
    lines = text.split("\n")
    i = lines.index("counter_catalog:") + 1
    out = []
    while i < len(lines) and not re.match(r"^\S", lines[i]):
        m = ENTRY_RE.match(lines[i])
        if m:
            out.append(m.group(1))
        i += 1
    return out


def lock_names(text):
    return [l.strip() for l in text.split("\n") if l.strip() and not l.lstrip().startswith("#")]


def rtl_ids(text):
    return [(m.group(1), int(m.group(2)), m.group(3)) for m in CNT_RE.finditer(text)]


def declared(text):
    names = set()
    for m in COUNTERS_RE.finditer(text):
        names.update(x.strip() for x in m.group(1).split(",") if x.strip())
    return names


# The parsers must read what they are pointed at, or every check below is silent.
assert catalog("counter_catalog:\n  - a\n  # c\n  - b  # x\nblocks:\n") == ["a", "b"], "dead catalog parser"
assert rtl_ids("localparam logic [15:0] ZHAO_CNT_X = 16'd7; // x_name") == [("ZHAO_CNT_X", 7, "x_name")], \
    "dead RTL-id parser"


def check(cat, lock, rtl, decl):
    errs = []
    seen = set()
    for i, n in enumerate(cat):
        if n in seen:
            errs.append("duplicate catalog entry %r at index %d" % (n, i))
        seen.add(n)
    if cat[:len(lock)] != lock:
        k = next((i for i in range(min(len(cat), len(lock))) if cat[i] != lock[i]), min(len(cat), len(lock)))
        errs.append("NOT APPEND-ONLY: catalog index %d is %r, the lock says %r -- an existing id moved"
                    % (k, cat[k] if k < len(cat) else None, lock[k] if k < len(lock) else None))
    for const, val, name in rtl:
        if name not in cat:
            errs.append("%s names %r, which is not in the catalog" % (const, name))
        elif cat.index(name) != val:
            errs.append("%s = %d but %r is catalog index %d" % (const, val, name, cat.index(name)))
    for n in sorted(decl - seen):
        errs.append("a block declares counter %r, which is not in the catalog" % n)
    return errs


def self_test(cat, lock, rtl, decl):
    ok = True
    planted = ["planted_prepend"] + cat
    if not any("NOT APPEND-ONLY" in e for e in check(planted, lock, [], set())):
        print("SELF-TEST FAILED: a prepended entry did not fire")
        ok = False
    if rtl:
        c, v, n = rtl[0]
        if not any(c in e for e in check(cat, lock, [(c, v + 1, n)], set())):
            print("SELF-TEST FAILED: a wrong RTL id did not fire")
            ok = False
    if not any("duplicate" in e for e in check(cat + [cat[0]], lock, [], set())):
        print("SELF-TEST FAILED: a duplicate did not fire")
        ok = False
    if not any("not in the catalog" in e for e in check(cat, lock, [], {"planted_undeclared"})):
        print("SELF-TEST FAILED: an undeclared block counter did not fire")
        ok = False
    return ok


def main(argv):
    text = read(BLOCKS)
    cat = catalog(text)
    if "--update-lock" in argv:
        old = lock_names(read(LOCK)) if os.path.exists(LOCK) else []
        if cat[:len(old)] != old:
            print("refusing to update the lock: the catalog is not an append of it")
            return 1
        hdr = ("# design/counter_ids.lock -- counter_id = line number (0-based) of each name,\n"
               "# in the order the ids were ASSIGNED. Append-only: tools/design/check_counter_ids.py\n"
               "# fails if design/blocks.yml's counter_catalog does not begin with this list.\n"
               "# Extend it with `python tools/design/check_counter_ids.py --update-lock`.\n")
        io.open(LOCK, "w", encoding="utf-8", newline="\n").write(hdr + "\n".join(cat) + "\n")
        print("lock: %d ids (%d new)" % (len(cat), len(cat) - len(old)))
        return 0
    lock = lock_names(read(LOCK))
    rtl = rtl_ids(read(PKG))
    decl = declared(text)
    if not self_test(cat, lock, rtl, decl):
        return 2
    errs = check(cat, lock, rtl, decl)
    print("counter ids: %d catalog entries, %d locked, %d RTL ids, %d declared names; self-test fired 4/4"
          % (len(cat), len(lock), len(rtl), len(decl)))
    if errs:
        for e in errs:
            print("  FAIL: " + e)
        return 1
    print("OK -- append-only against the lock, and every ZHAO_CNT_* id is its name's index")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
