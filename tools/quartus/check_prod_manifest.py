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



def check_fit_sources(decl, edges=None):
    """Every module in the GENERATED production top's INSTANTIATION CLOSURE must
    appear in zhao_prod_top's source list in design/fit_targets.yml.

    CLOSURE, not direct instances -- and that word is the 2026-09-09 repair.

    This function used to scan only what `zhao_prod_top.sv` instantiates by name.
    It therefore could not see `zhao_skid2`, which is instantiated by
    `zhao_raster_tile_pipe`, one level down. The production source list was
    missing it, the fit would have died at elaboration, and this checker reported
    OK. Verilator found it instead, which is the wrong tool discovering the thing
    this one exists for.

    A gate that inspects one level of a hierarchy is not a gate on the
    hierarchy -- and it reads exactly like one, which is the expensive part.
    """
    import os
    import re

    out = []
    top_path = "fpga/rtl/prod/zhao_prod_top.sv"
    yml = "design/fit_targets.yml"
    if not (os.path.exists(top_path) and os.path.exists(yml)):
        return out

    y = io.open(yml, encoding="utf-8", errors="replace").read()
    i = y.find("- top: zhao_prod_top")
    if i < 0:
        return ["design/fit_targets.yml has no zhao_prod_top target, so the "
                "production fit cannot be run at all"]
    seg = y[i:]
    j = seg.find("\n  - top:")
    if j > 0:
        seg = seg[:j]
    listed = set(re.findall(r"-\s+(fpga/rtl/\S+\.sv)", seg))

    top = io.open(top_path, encoding="utf-8", errors="replace").read()
    direct = set(re.findall(r"^\s{2}(zhao_\w+)\s+u\d+_i\s*\(", top, re.M))

    # Expand to the full closure. Every module reachable from anything the top
    # instantiates has to be compiled too, or elaboration stops -- and Quartus
    # reports that as a missing module, which reads like a typo rather than a
    # source-list gap.
    inst = set(direct)
    if edges:
        for m in direct:
            inst |= closure(edges, m)

    for m in sorted(inst):
        src = decl.get(m)
        if src is None:
            out.append(
                "the generated production top instantiates '%s' and no such "
                "module file exists" % m)
        elif src not in listed:
            # Say WHERE it is instantiated. "instantiated by the generated
            # production top" was the old message, and for a transitively-reached
            # module it is simply false -- zhao_skid2 is instantiated by
            # zhao_raster_tile_pipe. A gate that misreports the location sends
            # the reader to the wrong file.
            how = ("instantiated directly by the generated production top"
                   if m in direct else
                   "reachable from the generated production top (instantiated by "
                   "a submodule, not by the top itself)")
            out.append(
                "'%s' (%s) is %s but is NOT in zhao_prod_top's source list in "
                "design/fit_targets.yml -- the fit would die at elaboration"
                % (m, src, how))
    return out


def check_top_fresh():
    """zhao_prod_top.sv must match what its generator produces RIGHT NOW.

    The other half of the 2026-09-09 repair. Every check in this file reads the
    generated top, so all of them silently describe whatever design the top was
    generated from -- and on 2026-09-09 that was 2,888 lines out of date, with one
    instance pointing at a different module. This checker reported OK the whole
    time.

    `gen_prod_top.py --check` compares without writing, so running the gate can
    never itself modify the tree -- a checker with a side effect is a checker
    people stop trusting.
    """
    import subprocess
    import sys as _sys
    gen = os.path.join("tools", "quartus", "gen_prod_top.py")
    if not os.path.exists(gen):
        return []
    try:
        r = subprocess.run([_sys.executable, gen, "--check"],
                           capture_output=True, text=True)
    except OSError as exc:
        return ["could not run %s --check (%s)" % (gen, exc)]
    if r.returncode == 3:
        return ["fpga/rtl/prod/zhao_prod_top.sv is STALE -- it does not match "
                "tools/quartus/gen_prod_top.py's output, so every check in this "
                "file is describing an older design. Run: python %s" % gen]
    return []


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

    # ---- the SECOND place a new block has to be registered -----------------
    # A block being in the manifest is not enough for the production fit to
    # elaborate: design/fit_targets.yml carries zhao_prod_top's own flat source
    # list, and nothing connected the two. On 2026-09-04 zhao_geom_assemble and
    # zhao_geom_depthquant were caught here as UNACCOUNTED, added to the
    # manifest, regenerated into the top -- and the fit would STILL have died at
    # elaboration, because neither was in that source list.
    #
    # The manifest gate and the fit source list were one step apart and only
    # the first of them was mechanical. This closes the gap: whatever the
    # generated top instantiates must be compilable.
    errors.extend(check_fit_sources(decl, edges))
    errors.extend(check_top_fresh())

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
