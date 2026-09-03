#!/usr/bin/env python3
"""Every module under fpga/rtl is counted once, or declared absent with a reason.

The failure this guards is not "a wrong total". It is a block that is silently
NEITHER counted nor declared missing -- which reads as a healthy number and is
actually an incomplete one. So the check is exhaustive by construction: top +
inside + excluded must equal the module list exactly, in both directions.

`inside` is VERIFIED against the instantiation graph rather than trusted. A
module declared inside something that does not instantiate it would be dropped
from the count while looking accounted for.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from module_graph import build  # noqa: E402

MANIFEST = "design/prod_manifest.yml"


def read_manifest(path=MANIFEST):
    tops, excluded = [], {}
    section = None
    for raw in io.open(path, encoding="utf-8"):
        line = raw.split("#")[0].rstrip()
        if not line.strip():
            continue
        if re.match(r"^\w[\w_]*:", line):
            section = line.split(":")[0]
            continue
        m = re.match(r"^\s*-\s*(\S+)\s*$", line)
        if m and section == "top":
            tops.append(m.group(1))
            continue
        m = re.match(r"^\s*-\s*(\S+)\s*:\s*(\S+)\s*(.*)$", line)
        if m and section == "excluded":
            excluded[m.group(1)] = (m.group(2), m.group(3).strip())
    return tops, excluded


def module_edges():
    decl, inst = build()
    fmods = {}
    for m, p in decl.items():
        fmods.setdefault(p, []).append(m)
    edges = {}
    for p, ms in inst.items():
        for owner in fmods[p]:
            edges.setdefault(owner, set()).update(ms)
    return decl, edges


def closure(edges, root):
    seen, stack = set(), [root]
    while stack:
        for y in edges.get(stack.pop(), ()):
            if y not in seen:
                seen.add(y)
                stack.append(y)
    return seen


def main():
    decl, edges = module_edges()
    tops, excluded = read_manifest()
    errors = []

    for t in tops:
        if t not in decl:
            errors.append("top '%s' is not a module under fpga/rtl" % t)
    for e in excluded:
        if e not in decl:
            errors.append("excluded '%s' is not a module under fpga/rtl" % e)

    inside = {}
    for t in tops:
        if t not in decl:
            continue
        for m in closure(edges, t):
            inside.setdefault(m, []).append(t)

    # A top that is also inside another top would be counted twice -- once
    # standalone and once within its parent. That is the specific arithmetic
    # error this whole exercise exists to remove.
    for t in tops:
        if t in inside:
            errors.append(
                "DOUBLE COUNT: top '%s' is already instantiated by %s"
                % (t, ", ".join(inside[t]))
            )

    for e, (reason, _d) in excluded.items():
        if e in inside:
            errors.append(
                "excluded '%s' (%s) is nevertheless instantiated by %s -- it is "
                "in the machine whatever the manifest says"
                % (e, reason, ", ".join(inside[e]))
            )

    accounted = set(tops) | set(inside) | set(excluded)
    for m in sorted(set(decl) - accounted):
        errors.append(
            "UNACCOUNTED: %s (%s) is neither counted nor declared absent"
            % (m, decl[m].replace("fpga/rtl/", ""))
        )

    print(
        "prod manifest: %d modules, %d tops, %d inside, %d excluded"
        % (len(decl), len(tops), len(inside), len(excluded))
    )
    if errors:
        print("\nMANIFEST CHECK FAILED -- %d error(s)" % len(errors))
        for e in errors:
            print("  - " + e)
        return 1
    print("manifest check OK -- every module counted once or declared absent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
