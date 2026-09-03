#!/usr/bin/env python3
"""Who instantiates whom, across every .sv in fpga/rtl.

Written 2026-09-03 for the owner's production-only resource count. The count
it exists to protect is "counted ONCE": if a chosen module already instantiates
another chosen module, putting both in the resource top counts the inner one
twice and inflates the very number the report is supposed to settle.

It is a text scan, not an elaboration, so it is deliberately GENEROUS about
what looks like an instantiation -- a false edge makes the top smaller and is
visible in the manifest, a missed edge silently double-counts.
"""
import collections
import io
import os
import re
import sys

SEP = os.sep


def load(rtl_dir):
    files = {}
    for root, _dirs, names in os.walk(rtl_dir):
        for n in names:
            if n.endswith(".sv"):
                p = os.path.join(root, n).replace(SEP, "/")
                files[p] = io.open(p, encoding="utf-8", errors="replace").read()
    return files


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def build(rtl_dir="fpga/rtl"):
    files = load(rtl_dir)
    decl = {}
    for p, s in files.items():
        for m in re.finditer(r"^\s*module\s+(\w+)", s, re.M):
            decl[m.group(1)] = p
    names = set(decl)
    inst = collections.defaultdict(set)
    for p, s in files.items():
        body = strip_comments(s)
        for mod in names:
            if decl[mod] == p:
                continue
            # The trailing boundary is not optional: without it
            # `zhao_terrain_bake` matches inside `zhao_terrain_bake_delta` and
            # invents a cycle between a module and its own child.
            pat = (r"(?<![\w.])" + mod + r"(?![\w$])"
                   r"\s*(?:#\s*\((?:[^;]*?)\)\s*)?[A-Za-z_]\w*\s*\(")
            if re.search(pat, body):
                inst[p].add(mod)
    return decl, inst


def main():
    decl, inst = build()
    instantiated = set()
    for ms in inst.values():
        instantiated |= ms
    roots = sorted(m for m in decl if m not in instantiated)
    print("modules: %d   roots: %d" % (len(decl), len(roots)))
    if "--edges" in sys.argv:
        for p in sorted(inst):
            print(p.replace("fpga/rtl/", "") + " -> " + ", ".join(sorted(inst[p])))
        return
    for m in roots:
        print("  ROOT  %-38s %s" % (m, decl[m].replace("fpga/rtl/", "")))


if __name__ == "__main__":
    main()
