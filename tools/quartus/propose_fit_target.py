#!/usr/bin/env python3
"""Emit a design/fit_targets.yml target for a module, with its SOURCE CLOSURE computed.

WHY THIS EXISTS
---------------
2026-09-09: the census found that 43 of the 45 unmeasured production blocks have
no `- top:` entry in design/fit_targets.yml at all. They are not queued behind the
toolchain -- `run_block_fit` refuses them at preflight, so nobody can measure them
until a target is authored. Two thirds of the intended machine has no way to be
measured, which is the real reason the DSP and ALM figures are floors.

Authoring a target is two jobs of very different character:

  * THE SOURCE CLOSURE is mechanical, tedious, and the part that goes wrong. Today
    alone, three modules were missing from zhao_prod_top's list and one of them
    (zhao_skid2) was reachable only through a submodule. Quartus reports a missing
    source as a missing MODULE, which reads like a typo rather than a list gap.
    This tool computes it.

  * THE RULES are judgement and this tool DELIBERATELY DOES NOT GUESS THEM. It
    emits them commented out with a reminder, because CLAUDE.md's law is that a
    rule written after the fit it governs reports a pass. A generated
    `max_alms: <whatever it measured>` would be exactly that, wearing a tool's
    authority. State the budget you expect BEFORE the fit, then let it disagree.

Usage:
    python tools/quartus/propose_fit_target.py zhao_field_v3_len
    python tools/quartus/propose_fit_target.py --missing      # list what has no target
    python tools/quartus/propose_fit_target.py --self-test
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from module_graph import build          # noqa: E402
from check_prod_manifest import read_manifest, module_edges, closure  # noqa: E402

YML = os.path.join("design", "fit_targets.yml")


def existing_targets():
    try:
        y = io.open(YML, encoding="utf-8", errors="replace").read()
    except OSError:
        return set(), ""
    return set(re.findall(r"^\s*-\s*top:\s*(\S+)\s*$", y, re.M)), y


def sources_for(mod, decl, edges):
    """Every file needed to elaborate `mod`: its own, plus its whole closure's.

    Packages come FIRST. Quartus elaborates the whole project so order is not
    load-bearing for the fit, but a sequential reader (Verilator, used by every
    lint gate in tests/CMakeLists.txt) needs a package before its first use --
    and putting the top first is what made prod_top's lint report
    'Reference to zhao_guard_req_t before declaration' today.
    """
    mods = {mod} | closure(edges, mod)
    paths = []
    for m in sorted(mods):
        p = decl.get(m)
        if p and p not in paths:
            paths.append(p)
    pkgs = [p for p in paths if p.endswith("_pkg.sv")]
    own = decl.get(mod)
    rest = [p for p in paths if p not in pkgs and p != own]
    return pkgs + ([own] if own else []) + rest, sorted(mods - {mod})


def propose(mod, decl, edges):
    paths, deps = sources_for(mod, decl, edges)
    if not decl.get(mod):
        return None, "no file declares module '%s'" % mod
    L = []
    L.append("  - top: %s" % mod)
    L.append("    sources:")
    for p in paths:
        L.append("      - %s" % p.replace(os.sep, "/"))
    L.append("    # RULES ARE NOT GENERATED, ON PURPOSE.")
    L.append("    #")
    L.append("    # A rule written after the fit it governs reports a pass, so")
    L.append("    # emitting `max_alms: <whatever it measured>` would be that")
    L.append("    # mistake wearing a tool's authority. State what this block is")
    L.append("    # BUDGETED for, from its contract or its architecture section,")
    L.append("    # then let the fit disagree with you.")
    L.append("    #")
    L.append("    # rules:")
    L.append("    #   max_alms: ?")
    L.append("    #   max_dsp: ?          # S3.4 rejects DSP > 2 without a reason")
    L.append("    #   max_m10k: ?")
    L.append("    #   max_registers: ?")
    return "\n".join(L), ("%d source file(s), %d dependency module(s)%s"
                          % (len(paths), len(deps),
                             (": " + ", ".join(deps[:6])) if deps else ""))


# ---------------------------------------------------------------------------
# SELF-FIRE TEST. A closure computer that quietly returns only the module's own
# file would produce targets that look complete and die at elaboration -- the
# exact failure this tool exists to prevent, so it may not report before it has
# been shown to follow at least one edge.
# ---------------------------------------------------------------------------
def self_test():
    decl, edges = module_edges()
    # zhao_raster_tile_pipe instantiates zhao_skid2. That edge is the one whose
    # absence cost a MODMISSING today, so it is the fixture.
    if "zhao_raster_tile_pipe" not in decl:
        return True                     # not this tree; nothing to assert
    paths, deps = sources_for("zhao_raster_tile_pipe", decl, edges)
    assert len(paths) > 1, \
        "closure returned only the module's own file -- it is not following edges"
    assert "zhao_skid2" in deps, \
        ("zhao_skid2 not in the closure of zhao_raster_tile_pipe, which "
         "instantiates it -- the edge walk is broken")
    own = decl["zhao_raster_tile_pipe"].replace(os.sep, "/")
    assert any(p.replace(os.sep, "/") == own for p in paths), \
        "the module's own file is missing from its source list"
    # packages must precede the module's own file
    pk = [n for n, p in enumerate(paths) if p.endswith("_pkg.sv")]
    ow = [n for n, p in enumerate(paths) if p.replace(os.sep, "/") == own]
    if pk and ow:
        assert max(pk) < min(ow), "a package is listed after the module that uses it"
    return True


def main():
    self_test()
    args = sys.argv[1:]
    if "--self-test" in args:
        print("propose_fit_target self-test: the closure follows edges "
              "(zhao_raster_tile_pipe -> zhao_skid2) and packages come first.")
        return 0

    decl, edges = module_edges()
    have, _y = existing_targets()

    if "--missing" in args:
        tops, _ex = read_manifest()
        missing = sorted(m for m in tops if m not in have)
        print("%d intended production block(s) have NO fit target, so nobody can "
              "measure them:" % len(missing))
        for m in missing:
            paths, deps = sources_for(m, decl, edges)
            print("   %-38s %2d source(s), %2d dep(s)" % (m, len(paths), len(deps)))
        return 0

    if not args:
        print(__doc__)
        return 2
    rc = 0
    for mod in args:
        if mod in have:
            print("# %s ALREADY has a target in %s -- not proposing one" % (mod, YML))
            continue
        text, note = propose(mod, decl, edges)
        if text is None:
            print("# %s: %s" % (mod, note), file=sys.stderr)
            rc = 1
            continue
        print("# %s -- %s" % (mod, note))
        print(text)
        print()
    return rc


if __name__ == "__main__":
    sys.exit(main())
