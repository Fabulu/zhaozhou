#!/usr/bin/env python3
"""Score the tree against the ALM Liberation Roadmap's EIGHT DOMAIN ALLOCATIONS.

Written 2026-09-10. The DSP campaign had a scoreboard -- one number, <= 94 -- and
it got all the attention for a day. The ALM campaign has a BETTER one and nobody
was reading it: `Zhaozhou_ALM_Liberation_Roadmap_2026-09-09.txt` section 2 sets
an inclusive per-domain allocation, and section 2.1 explains how to use a MISS.
This file makes that table executable.

    Domain                              ALM      DSP     M10K
    Shell/raster/video/memory          8,000       8       80
    Texture                            7,000      12       96
    Projection and result arenas       4,500       9       48
    Geometry and lighting              5,200      24       64
    Terrain, forge and maintenance     3,500       9       64
    Complete FIELD                     4,500      12       64
    2D/particles/surfaces/post         1,800       4       24
    Integration and remaining support  1,500      10       24
    ---------------------------------------------------------
    TOTAL                             36,000      88      464

TWO THINGS THIS SURFACED IMMEDIATELY, both of which had been missed:

**The DSP allocation totals 88, not 94.** The owner cap is <= 94; the roadmap's
own allocation sums to 88 and says "DSP remaining under owner cap: 6". A day was
spent steering at 94. Hitting 94 is not hitting the plan.

**The M10K envelope is 464, not 553.** "Spend memory like Monopoly money" is
bounded: 464 inclusive, 89 remaining. And the roadmap is explicit that these are
REPLACEMENT allocations -- "Do not add these 464 M10Ks to the historical 147."

WHAT THE ROADMAP SAYS ABOUT MISSING A ROW, and it is the reason this tool prints
misses without editorialising (section 2.1):

    "A 5.8k projection candidate misses its 4.5k objective but might still be a
     large improvement over two engines. Retain it as a measured frontier point,
     try the next structural change, and expose any proposed reallocation. Do not
     reject a useful saving merely because one ambitious target was not reached.
     Equally, do not call the whole plan closed because one row succeeded."

And on the totals themselves: "That number is a set of allocations TO EARN, not a
forecast calculated by subtracting hoped-for savings from a speculative baseline.
Failure to earn an allocation remains a real failure; a table of targets does not
establish closure."

THE COVERAGE PROBLEM IS THE POINT. The audit found 21 roots with fitted ALM, 11
map-only, and **34 unpriced**. So most rows will read UNKNOWN, and that is the
honest output -- a scoreboard that printed a confident total from a third of the
evidence would be the broken instrument this repo has a law about. Every number
is tagged FIT / MAP / UNPRICED and the unpriced are counted, never zeroed.

REUSED, NOT REIMPLEMENTED: `dsp_census.load_evidence` owns "what has been
measured" and `blocks.yml` owns which subsystem a block belongs to. This file
computes neither.
"""
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "quartus"))
sys.path.insert(0, HERE)

from dsp_census import (load_evidence, build_bill, load_profiles)  # noqa: E402

BLOCKS = os.path.join("design", "blocks.yml")
MANIFEST = os.path.join("design", "prod_manifest.yml")

# The roadmap's section 2 table, transcribed. Inclusive allocations.
ALLOCATION = [
    ("Shell/raster/video/memory",          8000,  8,  80),
    ("Texture",                            7000, 12,  96),
    ("Projection and result arenas",       4500,  9,  48),
    ("Geometry and lighting",              5200, 24,  64),
    ("Terrain, forge and maintenance",     3500,  9,  64),
    ("Complete FIELD",                     4500, 12,  64),
    ("2D/particles/surfaces/post",         1800,  4,  24),
    ("Integration and remaining support",  1500, 10,  24),
]
DEVICE = (41910, 112, 553)

# blocks.yml `subsystem:` -> the roadmap's domain. The roadmap names domains in
# prose, not by subsystem key, so this mapping is MINE and is the tool's main
# assumption -- it is printed at the end of every run so a reader can disagree
# with it rather than having to reverse-engineer it.
SUBSYSTEM_DOMAIN = {
    "raster": "Shell/raster/video/memory",
    "video": "Shell/raster/video/memory",
    "memory": "Shell/raster/video/memory",
    "command": "Shell/raster/video/memory",
    "platform": "Shell/raster/video/memory",
    "audio": "Shell/raster/video/memory",
    "texture": "Texture",
    "geometry": "Geometry and lighting",
    "terrain": "Terrain, forge and maintenance",
    "forge": "Terrain, forge and maintenance",
    "field": "Complete FIELD",
    "particles": "2D/particles/surfaces/post",
    "surface": "2D/particles/surfaces/post",
    "compositor": "2D/particles/surfaces/post",
    "debug": "Integration and remaining support",
    "measure": "Integration and remaining support",
    "input": "Integration and remaining support",
    "sw": None,        # software contracts, not silicon
}

# Projection is a CROSS-CUTTING domain in the roadmap -- it is not a subsystem in
# blocks.yml, so its members are named. Each of these lives under geometry/ or
# terrain/ but the roadmap gives arenas and projection their own 4,500 ALM row.
PROJECTION_MODULES = {
    "zhao_project_core", "zhao_project_service", "zhao_proj_arena3",
    "zhao_geom_project", "zhao_terrain_project",
    "zhao_vertex_arena", "zhao_geom_wcache",
}
PROJECTION = "Projection and result arenas"


def load_block_subsystems(path=BLOCKS):
    """{BLOCK.ID: subsystem} and {BLOCK.ID: rtl module name} from blocks.yml."""
    sub, mod = {}, {}
    try:
        text = io.open(os.path.join(ROOT, path), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return sub, mod
    cur = None
    for line in text.splitlines():
        m = re.match(r"^\s*-\s+id:\s*([A-Za-z0-9._]+)\s*$", line)
        if m:
            cur = m.group(1)
            continue
        if not cur:
            continue
        s = re.search(r"^\s*subsystem:\s*(\w+)", line)
        if s:
            sub[cur] = s.group(1)
        r = re.search(r"^\s*rtl:\s*\S*?(zhao_\w+)\.sv", line)
        if r:
            mod[cur] = r.group(1)
    return sub, mod


# zhao_shell_top is a ROOT of the machine and is not in the production
# manifest's `top:` list, so blocks.yml gives it no subsystem. dsp_census adds
# it explicitly for the same reason; without it the DSP total reads 176 against
# the census's 192, and a scoreboard that does not reconcile with the authority
# it derives from is a broken instrument.
NAMED_DOMAIN = {"zhao_shell_top": "Shell/raster/video/memory"}


def module_domain(module, subsystem):
    if module in NAMED_DOMAIN:
        return NAMED_DOMAIN[module]
    if module in PROJECTION_MODULES:
        return PROJECTION
    return SUBSYSTEM_DOMAIN.get(subsystem)


def main():
    os.chdir(ROOT)

    # USE THE CENSUS'S OWN SELECTOR. Summing load_evidence() directly gives
    # 113,478 ALM and 386 DSP against the audit's 58,359 and 192, because the
    # ledgers hold every LABELLED variant (@lanes1, @pre-rearch, @g2-prod ...)
    # and every probe and superseded block. build_bill picks ONE row per
    # SELECTED production root, which is the whole reason it exists.
    #
    # I made exactly this mistake once already in this campaign -- a DSP scan
    # read 430 instead of 192 -- and made it again writing the first version of
    # this file. The rule that survives: never reimplement a selector, and if a
    # total comes out roughly double the authority's, suspect the selector
    # before suspecting the authority.
    from check_prod_manifest import read_manifest
    tops, _excluded = read_manifest()
    ev = load_evidence()
    roots = list(tops)
    if "zhao_shell_top" in ev and "zhao_shell_top" not in roots:
        roots.append("zhao_shell_top")
    try:
        targets_text = io.open(os.path.join("design", "fit_targets.yml"),
                               encoding="utf-8", errors="replace").read()
    except OSError:
        targets_text = ""
    rows = build_bill(roots, ev, targets_text, load_profiles())
    sub, mod = load_block_subsystems()

    # module -> subsystem, via blocks.yml's rtl: field where present, else by
    # the module's own directory, which is how the tree is actually organised.
    mod_sub = {}
    for bid, m in mod.items():
        if bid in sub:
            mod_sub[m] = sub[bid]
    for root, _d, names in os.walk(os.path.join(ROOT, "fpga", "rtl")):
        for n in names:
            if n.endswith(".sv"):
                m = n[:-3]
                mod_sub.setdefault(m, os.path.basename(root))

    agg = {}
    for name, alm, dsp, m10k in ALLOCATION:
        agg[name] = dict(alm=0, dsp=0, m10k=0, fit=0, mapo=0, unpriced=0,
                         unpriced_names=[])

    unmapped = []
    for row in sorted(rows, key=lambda r: r.get("module", "")):
        module = row.get("module", "")
        d = module_domain(module, mod_sub.get(module))
        if d is None:
            unmapped.append(module)
            continue
        a, p, k = row.get("alm"), row.get("dsp"), row.get("m10k")
        g = agg[d]
        if a is not None:
            g["alm"] += a
            g["fit"] += 1
        elif p is not None:
            g["mapo"] += 1
            g["unpriced_names"].append(module + " (map-only: ALM UNKNOWN)")
        else:
            g["unpriced"] += 1
            g["unpriced_names"].append(module)
        if p is not None:
            g["dsp"] += p
        if k is not None:
            g["m10k"] += k

    print("DOMAIN SCOREBOARD -- the ALM Liberation Roadmap section 2 allocations")
    print("Every ALM figure below is a SUM OF FITTED ROWS ONLY. Map-only and")
    print("unpriced blocks contribute 0 ALM and are counted separately: their")
    print("cost is UNKNOWN, never zero.\n")
    hdr = "%-34s %7s %7s   %5s %4s   %5s %4s   %s"
    print(hdr % ("domain", "ALM", "/objv", "DSP", "/al", "M10K", "/al", "evidence"))
    print("-" * 104)
    tA = tD = tK = 0
    oA = oD = oK = 0
    for name, alm, dsp, m10k in ALLOCATION:
        g = agg[name]
        tA += g["alm"]; tD += g["dsp"]; tK += g["m10k"]
        oA += alm; oD += dsp; oK += m10k
        flag = "**OVER**" if g["alm"] > alm else ""
        ev_s = "%d fit" % g["fit"]
        if g["mapo"]:
            ev_s += ", %d map" % g["mapo"]
        if g["unpriced"]:
            ev_s += ", %d UNPRICED" % g["unpriced"]
        print(hdr % (name, "{:,}".format(g["alm"]), "{:,}".format(alm),
                     g["dsp"], dsp, g["m10k"], m10k, ev_s) + " " + flag)
    print("-" * 104)
    print(hdr % ("TOTAL", "{:,}".format(tA), "{:,}".format(oA),
                 tD, oD, tK, oK, "fitted rows only"))
    print(hdr % ("DEVICE", "{:,}".format(DEVICE[0]), "", DEVICE[1], "",
                 DEVICE[2], "", ""))
    print()
    # RECONCILE AGAINST THE AUTHORITY. This scoreboard re-buckets the census's
    # own bill; if the buckets do not sum back to it, the bucketing dropped
    # something and every row above is suspect.
    from dsp_census import totals as census_totals
    ct = census_totals(rows)
    ok = (tD == ct["dsp"] and tA == ct["alm"] and tK == ct["m10k"])
    print("  RECONCILIATION vs dsp_census.totals on the SAME bill:")
    print("    DSP  %5d vs %5d    ALM %7d vs %7d    M10K %4d vs %4d    %s"
          % (tD, ct["dsp"], tA, ct["alm"], tK, ct["m10k"],
             "OK" if ok else "*** MISMATCH -- the domain mapping drops rows ***"))
    print()
    print("  The roadmap's DSP allocation totals %d, NOT the owner cap of 94."
          % oD)
    print("  It says so itself: 'DSP remaining under owner cap: 6'. Steering at")
    print("  94 is steering 6 DSP past the plan.")
    print()
    print("  The M10K envelope is %d, not 553, and the roadmap is explicit that"
          % oK)
    print("  these are REPLACEMENT allocations: 'Do not add these 464 M10Ks to")
    print("  the historical 147.' Spending memory is bounded, not free.")
    print()
    print("== WHERE THE EVIDENCE IS MISSING, which is most of it ==")
    for name, _a, _d, _k in ALLOCATION:
        g = agg[name]
        if g["unpriced_names"]:
            print("  %s -- %d without a fitted ALM:" % (name, len(g["unpriced_names"])))
            for n in sorted(g["unpriced_names"])[:8]:
                print("       %s" % n)
            if len(g["unpriced_names"]) > 8:
                print("       ... and %d more" % (len(g["unpriced_names"]) - 8))
    if unmapped:
        print("\n  NOT MAPPED TO ANY DOMAIN (%d) -- the mapping below is the")
        print("  tool's assumption and these fell through it:")
        for n in sorted(unmapped)[:12]:
            print("       %s" % n)
    print()
    print("== THE MAPPING THIS TOOL ASSUMED, printed so it can be argued with ==")
    print("  The roadmap names its domains in prose, not by blocks.yml subsystem")
    print("  key, so this correspondence is the tool author's and is the main")
    print("  thing to check before trusting a row:")
    seen = {}
    for k, v in sorted(SUBSYSTEM_DOMAIN.items()):
        seen.setdefault(v, []).append(k)
    for dname, keys in sorted(seen.items()):
        print("    %-34s <- %s" % (dname or "(excluded: software)", ", ".join(keys)))
    print("    %-34s <- named modules: %s" % (PROJECTION,
          ", ".join(sorted(PROJECTION_MODULES))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
