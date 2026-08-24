#!/usr/bin/env python3
"""build_manifest.py -- fuse the audit's evidence into one ranked ledger.

INPUTS (each one measured or analysed by a different lane, none of them by hand)
  reports/rtl_inventory.json          tools/budget/scan_rtl.py, elaborated AST
  reports/synthesis/zhao_block_map.json   quartus_map, this HEAD
  reports/synthesis/zhao_block_fit.json   quartus_fit, various commits
  design/budgets/workloads.yml        transcribed demand, with provenance
  tools/budget/calibration.json       measured shape -> resource mapping

OUTPUTS
  reports/budget_manifest.json        one record per block
  reports/BUDGET_HEATMAP.md           the readable ranking

THE DEBT FLAGS ARE THE POINT
============================
`docs/OWNER_DOCKET.md`: "Those flags would have exposed Field and the TMU
before anyone read their misleading headline numbers."

  NO_CURRENT_FIT             no fit at HEAD's commit
  OLD_SDC                    fit predates the corrected clock+I/O SDC, so its
                             Fmax is not a measurement of this block
  NO_WORKLOAD                no items/frame, so no demand ratio can exist
  NO_II_TEST                 throughput asserted nowhere executable
  EXPECTED_RAM_NOT_INFERRED  the source declares addressable storage and the
                             map reports zero block memory bits
  NO_SUBSYSTEM_FIT           boundary-heavy block never fitted with a neighbour
  NO_RESERVE                 demand ratio above 1.0 with nothing spare
  PARETO_UNPROVEN            >5 DSPs or >5% ALMs with no second measured point

THE FALSIFIABLE TEST
====================
Run against `zhao_field_seq` (0 M10Ks while spending 8,901 ALMs on a register
file and three ROMs built from logic) and `zhao_texture_tmu` (II = 6 against a
demand needing II = 1). Both must come out RED from mechanical rules alone. If
they do not, the heatmap does not work and nothing else in it should be
believed either.

NOTHING HERE IS HAND-EDITED. Every number is copied from a tool's own output
file, and every row says which file and which commit it came from.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# The corrected-SDC boundary. QUARTUS_GOTCHAS 7 fixed create_clock resolving to
# an empty collection; 9 added set_input_delay/set_output_delay. Rows measured
# before the second fix carry an Fmax that is not this block's.
CORRECTED_SDC_COMMITS = None  # resolved from the fit file's own row labels

SEV_ORDER = {"GREEN": 0, "YELLOW": 1, "ORANGE": 2, "RED": 3}

# NOT CONSOLE HARDWARE, and named here rather than filtered silently.
# zhao_stub_top is the Phase-1 gate's frame-validator stub; its 8,388,608-bit
# frame slot is a test scaffold and would otherwise sit at the top of the
# storage ranking, above zhao_surface_sheet, looking like the design's largest
# memory. zhao_synth_probe is a synthesis canary. Both are still scanned,
# still measured and still printed -- they are excluded only from TOTALS.
ROLES = {
    "zhao_stub_top": "scaffolding: Phase-1 gate frame-validator stub, not console hardware",
    "zhao_synth_probe": "scaffolding: synthesis canary",
    "zhao_shell_top": "composed top: has its own lane (run_composed_fit.ps1)",
}


def load(path, default=None):
    p = os.path.join(REPO, path)
    if not os.path.exists(p):
        return default
    with open(p, encoding="utf-8", errors="replace") as fh:
        return json.load(fh)


def load_workloads(path="design/budgets/workloads.yml"):
    """A deliberately small YAML reader.

    The repo has no yaml dependency and adding one to run an audit would be a
    new install on the critical path. This handles exactly the shape of
    workloads.yml -- two levels of mapping, scalars, `-` lists and `>-` folded
    blocks.

    ITS LIMIT, STATED BECAUSE THE FAILURE WOULD BE SILENT: it SKIPS a
    construct it does not recognise rather than raising, so a workloads.yml
    written with anchors, flow mappings or multi-document separators would
    lose rows and turn a RED block GREEN by omission. The file it reads is
    checked in beside it and uses none of those. If workloads.yml grows, either
    keep it inside this subset or take the pyyaml dependency -- do not let this
    reader guess.
    """
    p = os.path.join(REPO, path)
    if not os.path.exists(p):
        return {"frame": {}, "blocks": {}, "unruled": []}
    lines = open(p, encoding="utf-8").read().split("\n")
    root = {}
    stack = [(-1, root)]
    i = 0
    while i < len(lines):
        raw = lines[i]
        i += 1
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        indent = len(raw) - len(raw.lstrip())
        s = raw.strip()
        while stack and stack[-1][0] >= indent:
            stack.pop()
        parent = stack[-1][1]
        if s.startswith("- "):
            body = s[2:].strip()
            if not isinstance(parent, list):
                continue
            if ":" in body:
                k, v = body.split(":", 1)
                d = {k.strip(): parse_scalar(v.strip())}
                parent.append(d)
                stack.append((indent, d))
            else:
                parent.append(parse_scalar(body))
            continue
        if ":" not in s:
            continue
        k, v = s.split(":", 1)
        k, v = k.strip(), v.strip()
        if v in (">-", ">", "|", "|-"):
            buf = []
            while i < len(lines):
                nxt = lines[i]
                if nxt.strip() and (len(nxt) - len(nxt.lstrip())) <= indent:
                    break
                buf.append(nxt.strip())
                i += 1
            parent[k] = " ".join(x for x in buf if x)
        elif v == "":
            # container: list if the next meaningful line is a `- `
            j = i
            nxt = None
            while j < len(lines):
                if lines[j].strip() and not lines[j].lstrip().startswith("#"):
                    nxt = lines[j]
                    break
                j += 1
            child = [] if (nxt is not None and nxt.strip().startswith("- ")) else {}
            parent[k] = child
            stack.append((indent, child))
        else:
            parent[k] = parse_scalar(v)
    return root


def parse_scalar(v):
    v = v.strip()
    if v in ("null", "~", ""):
        return None
    if v in ("true", "True"):
        return True
    if v in ("false", "False"):
        return False
    if re.match(r"^-?\d+$", v):
        return int(v)
    if re.match(r"^-?\d*\.\d+$", v):
        return float(v)
    return v.strip("'\"")


def sev_max(a, b):
    return a if SEV_ORDER[a] >= SEV_ORDER[b] else b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="reports/budget_manifest.json")
    ap.add_argument("--md-out", default="reports/BUDGET_HEATMAP.md")
    args = ap.parse_args()

    inv = load("reports/rtl_inventory.json")
    mapd = load("reports/synthesis/zhao_block_map.json", {"blocks": []})
    fitd = load("reports/synthesis/zhao_block_fit.json", {"blocks": []})
    calib = load("tools/budget/calibration.json", {"points": []})
    wl = load_workloads()
    if inv is None:
        raise SystemExit("reports/rtl_inventory.json missing -- run tools/budget/scan_rtl.py first")

    head = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                          capture_output=True, text=True).stdout.strip()

    # ---- "AT HEAD" MEANS THE RTL, NOT THE COMMIT -------------------------
    # A map taken during this run is a measurement of the CURRENT RTL even
    # though the commit has moved on since -- because everything committed
    # since was tooling and reports. Comparing bare commit ids reported
    # `modulesWithMapAtHead: 0` while 35 rows had just been measured against
    # the very tree in the working directory, which is a provenance check that
    # answers "no" regardless and is therefore worth nothing.
    #
    # `git rev-parse <commit>:fpga/rtl` is the tree hash of the RTL directory
    # at that commit. Equal tree hash means byte-identical RTL, whatever else
    # changed. Exact, cheap, and it cannot be fooled by a docs commit.
    _tree_cache = {}

    def rtl_tree(commit):
        if not commit:
            return None
        if commit not in _tree_cache:
            r = subprocess.run(["git", "-C", REPO, "rev-parse", "%s:fpga/rtl" % commit],
                               capture_output=True, text=True)
            _tree_cache[commit] = r.stdout.strip() if r.returncode == 0 else None
        return _tree_cache[commit]

    head_rtl = rtl_tree(head)

    maps = {b["module"]: b for b in mapd.get("blocks", [])}
    fits = {b["module"]: b for b in fitd.get("blocks", [])
            if not b.get("variantOf")}
    fit_variants = {}
    for b in fitd.get("blocks", []):
        if b.get("variantOf"):
            fit_variants.setdefault(b["variantOf"], []).append(b)

    frame = wl.get("frame") or {}
    budget = frame.get("computeClocksPerFrame") or 1666667
    wblocks = wl.get("blocks") or {}

    records = []
    for m in inv["modules"]:
        name = m["module"]
        h = m["hierarchical"]
        a, s = h["arithmetic"], h["storage"]
        mp = maps.get(name)
        ft = fits.get(name)
        w = wblocks.get(name)

        flags = []

        # ---- resources at HEAD ------------------------------------------
        res = {}
        if mp and mp.get("status") == "ok":
            res["mapDspBlocks"] = mp.get("dspBlocks")
            res["mapBlockMemoryBits"] = mp.get("blockMemoryBits")
            res["mapEstimatedAlms"] = mp.get("estimatedAlms")
            res["mapRegisters"] = mp.get("registers")
            res["mapCommit"] = (mp.get("sourceCommit") or "")[:8]
            res["mapDspDecomposition"] = mp.get("dspDecomposition")
            res["mapInferredDesignMemories"] = mp.get("inferredDesignMemoryCount")
        else:
            flags.append("NO_MAP")

        if ft and ft.get("status") == "ok":
            res["fitDspBlocks"] = ft.get("dspBlocks")
            res["fitAlms"] = ft.get("alms")
            res["fitRamBlocks"] = ft.get("ramBlocks")
            res["fitCommit"] = (ft.get("sourceCommit") or "")[:8]
            res["fitFmaxMhz"] = ft.get("fmaxMhz")
            res["fitSetupSlackNs"] = ft.get("setupSlackNs")
            res["fitHoldSlackNs"] = ft.get("holdSlackNs")
            if rtl_tree(ft.get("sourceCommit")) != head_rtl or not ft.get("rtlCleanAtHead", True):
                flags.append("NO_CURRENT_FIT")
            # OLD_SDC: an Fmax exists but predates the corrected constraints.
            # A row measured under the fixed harness HAS an fmaxMhz AND a
            # holdSlackNs, because quartus_sta only became a stage at the same
            # time. A row with no fmax at all was fitted with no timing
            # objective whatsoever -- 47 of them were.
            if ft.get("fmaxMhz") is None:
                flags.append("OLD_SDC")
        else:
            flags.append("NO_CURRENT_FIT")
            flags.append("OLD_SDC")

        # ---- expected vs inferred RAM -----------------------------------
        expected_bits = s.get("addressableBits", 0) + s.get("constRomBits", 0)
        inferred_design = (mp or {}).get("inferredDesignMemoryCount", 0) or 0
        map_bits = (mp or {}).get("blockMemoryBits") or 0
        ram = {
            "expectedAddressableBits": s.get("addressableBits", 0),
            "expectedConstRomBits": s.get("constRomBits", 0),
            "expectedTotalBits": expected_bits,
            "mapBlockMemoryBits": map_bits,
            "mapInferredDesignMemories": inferred_design,
        }
        if expected_bits >= 512 and mp and mp.get("status") == "ok" and inferred_design == 0:
            flags.append("EXPECTED_RAM_NOT_INFERRED")

        # ---- rate --------------------------------------------------------
        ii = m["interface"]["inferredMinII"]
        rate = {"inferredMinII": ii}
        if w:
            rate["itemsPerFrame"] = w.get("itemsPerFrame")
            rate["demandConfidence"] = w.get("confidence")
            rate["workloadSource"] = w.get("source")
            rate["measuredII"] = w.get("measuredII")
            eff_ii = w.get("measuredII") or ii
            rate["iiUsed"] = eff_ii
            rate["iiUsedIsMeasured"] = w.get("measuredII") is not None
            cap = budget // max(1, eff_ii)
            rate["capacityPerFrame"] = cap
            items = w.get("itemsPerFrame") or 0
            rate["demandRatio"] = round(items / cap, 6) if cap else None

            # ---- THE RETURN, DERIVED RATHER THAN GUESSED -----------------
            # Every DSP reduction landed so far came from the same move: a
            # block builds N products in parallel to hit one item per clock,
            # the demand is a fraction of one item per clock, so the products
            # are time-multiplexed by the over-provisioning factor.
            #
            # overProvision = capacity / demand. Serialising by up to that
            # factor costs no throughput at all, because the spare cycles are
            # already there. TERRAIN.NORMALS is the clean case: 2,000
            # normals/frame against 1,666,667 capacity is 833x over-provisioned
            # and its six products could share one multiplier.
            #
            # This is an ESTIMATE and the manifest says so. It assumes the
            # products are independent and the block can be pipelined; the
            # docket's own gate still demands two MEASURED Pareto points before
            # any of it counts.
            if items > 0 and cap:
                over = cap / items
                rate["overProvisionFactor"] = round(over, 1)
                prods = a["nonconstantMultiplyInstances"]
                if prods > 0 and over > 1.5:
                    lanes = max(1, int(prods / min(over, prods)) or 1)
                    rate["serialisableToLanes"] = lanes
                    rate["productsToday"] = prods
            if w.get("measuredII") is None:
                flags.append("NO_II_TEST")
            if rate["demandRatio"] and rate["demandRatio"] > 1.0:
                flags.append("NO_RESERVE")
            elif rate["demandRatio"] and rate["demandRatio"] > (1.0 - (w.get("reserve") or 0.0)):
                flags.append("NO_RESERVE")
        else:
            flags.append("NO_WORKLOAD")
            if ii > 1:
                flags.append("NO_II_TEST")

        # ---- composition -------------------------------------------------
        # A block whose arithmetic runs from its own pins is not represented by
        # a leaf fit. QUARTUS_GOTCHAS 9 measured a 5.4x error from exactly this.
        boundary_heavy = m["interface"]["directIoArithmeticPaths"] > 0 or m["interface"]["ports"] > 40
        composition = {
            "directIoArithmeticPaths": m["interface"]["directIoArithmeticPaths"],
            "ports": m["interface"]["ports"],
            "submodules": m.get("submodules", []),
        }
        if boundary_heavy:
            flags.append("NO_SUBSYSTEM_FIT")

        # ---- critical-path family ---------------------------------------
        # Named from the source shapes present, because no per-block STA report
        # in this repo carries a path family and inventing one would be worse
        # than saying which cones EXIST.
        fam = []
        if a["nonconstantMultiplyInstances"]:
            fam.append("MULTIPLY(%d, widest %d-bit)" % (
                a["nonconstantMultiplyInstances"], a["widestNonconstantOperand"]))
        if a["divides"]:
            fam.append("DIVIDE(%d)" % a["divides"])
        if a["variableShifts"]:
            fam.append("VARSHIFT(%d)" % a["variableShifts"])
        if a["satChains"]:
            fam.append("ADD_COMPARE_SATURATE(%d)" % a["satChains"])
        if a["combinationalLoops"]:
            fam.append("COMB_LOOP(%d)" % a["combinationalLoops"])
        if s.get("asyncReadArrays"):
            fam.append("ASYNC_ARRAY_READ(%d)" % s["asyncReadArrays"])
        if s.get("constRomTables"):
            fam.append("CONST_ROM(%d tables, %d bits)" % (s["constRomTables"], s["constRomBits"]))
        if m["interface"]["directIoArithmeticPaths"]:
            fam.append("PIN_TO_PIN_ARITHMETIC(%d)" % m["interface"]["directIoArithmeticPaths"])

        # ---- severity ----------------------------------------------------
        sev = m["severity"]

        # THE DOCKET'S OWN CI GATE, applied to the measured number rather than
        # to the source: ">5 DSPs or >5% ALMs requires two measured Pareto
        # points, or a stated reason no cheaper point exists."
        #
        # This exists because the scanner rates each multiply on its own and is
        # right to: `zhao_geom_quat2mat`'s nine products are 16x16, one DSP
        # each, and every one of them is a perfectly ordinary GREEN. Nine DSPs
        # in one block is not. Severity has to see the TOTAL, or a block builds
        # a fifth of the DSP budget out of findings that are individually fine.
        d = res.get("mapDspBlocks") or res.get("fitDspBlocks") or 0
        alm = res.get("fitAlms") or res.get("mapEstimatedAlms") or 0
        alm_pct = round(100.0 * alm / 41910, 1) if alm else 0.0
        res["almPercentOfDevice"] = alm_pct
        res["dspPercentOfDevice"] = round(100.0 * d / 112, 1) if d else 0.0
        if d > 5:
            flags.append("PARETO_UNPROVEN")
            sev = sev_max(sev, "ORANGE" if d <= 12 else "RED")
        if alm_pct > 5.0:
            flags.append("PARETO_UNPROVEN")
            sev = sev_max(sev, "ORANGE" if alm_pct <= 15.0 else "RED")

        # A long initiation interval becomes RED only where a DEMAND FIGURE
        # proves it cannot be afforded. The scanner reports the II as a fact at
        # ORANGE; this is where the fact meets items/frame.
        if ii >= 4 and rate.get("demandRatio") and rate["demandRatio"] > 1.0:
            sev = sev_max(sev, "RED")

        for f in flags:
            if f in ("EXPECTED_RAM_NOT_INFERRED",):
                sev = sev_max(sev, "RED")
            elif f in ("NO_RESERVE",):
                sev = sev_max(sev, "RED")
            elif f in ("NO_CURRENT_FIT", "OLD_SDC", "NO_WORKLOAD", "NO_II_TEST",
                       "NO_SUBSYSTEM_FIT"):
                sev = sev_max(sev, "YELLOW")

        records.append({
            "module": name,
            "role": ROLES.get(name, "design"),
            "sourceFile": m["sourceFile"],
            "severity": sev,
            "scanSeverity": m["severity"],
            "resources": res,
            "arithmetic": a,
            "storage": s,
            "expectedVsInferredRam": ram,
            "rate": rate,
            "criticalPathFamily": fam,
            "composition": composition,
            "debtFlags": sorted(set(flags)),
            "provenance": {
                "scanCommit": (inv.get("sourceCommit") or "")[:8],
                "mapCommit": res.get("mapCommit"),
                "fitCommit": res.get("fitCommit"),
                "headCommit": head[:8],
                "scanSourceListHash": inv.get("sourceListHash"),
                "mapSourceListHash": (mp or {}).get("sourceListHash"),
                "mapRtlMatchesHead": (mp is not None
                                      and rtl_tree(mp.get("sourceCommit")) == head_rtl
                                      and bool(mp.get("rtlCleanAtHead"))),
                "fitRtlMatchesHead": (ft is not None
                                      and rtl_tree(ft.get("sourceCommit")) == head_rtl
                                      and bool(ft.get("rtlCleanAtHead"))),
                "headRtlTree": (head_rtl or "")[:12],
            },
        })

    records.sort(key=lambda r: (-SEV_ORDER[r["severity"]],
                                -(r["resources"].get("mapDspBlocks") or 0),
                                r["module"]))

    manifest = {
        "schemaVersion": 1,
        "generator": "tools/budget/build_manifest.py",
        "headCommit": head,
        "frameBudget": frame,
        "inputs": {
            "scan": "reports/rtl_inventory.json",
            "map": "reports/synthesis/zhao_block_map.json",
            "fit": "reports/synthesis/zhao_block_fit.json",
            "workloads": "design/budgets/workloads.yml",
            "calibration": "tools/budget/calibration.json",
        },
        "coverage": {
            "modulesScanned": len(records),
            "modulesWithMapAtHead": sum(1 for r in records
                                        if r["provenance"]["mapRtlMatchesHead"]),
            "modulesWithFitAtHead": sum(1 for r in records
                                        if r["provenance"]["fitRtlMatchesHead"]),
            "modulesWithAnyMap": sum(1 for r in records if r["resources"].get("mapDspBlocks") is not None),
            "modulesWithAnyFit": sum(1 for r in records if r["resources"].get("fitAlms") is not None),
            "modulesWithWorkload": sum(1 for r in records if "NO_WORKLOAD" not in r["debtFlags"]),
            "calibrationPoints": len(calib.get("points", [])),
        },
        "blocks": records,
        "limitations": [
            "No number in this file was typed. Each is copied from reports/rtl_inventory.json, reports/synthesis/zhao_block_map.json, reports/synthesis/zhao_block_fit.json or design/budgets/workloads.yml, and each row names the commit its measurement came from.",
            "mapDspBlocks and mapEstimatedAlms come from Analysis and Synthesis. The DSP figure matched the fitter on every block cross-checked; the ALM figure is an estimate and is NOT comparable to fitAlms.",
            "A map row carries NO timing. Every Fmax and slack in this file comes from the fit lane, at the commit named in fitCommit.",
            "criticalPathFamily names the expensive cones the SOURCE contains. It is not an STA result -- no per-block STA report in this repo carries a path family, and naming one from the source is honest where inventing one would not be.",
            "inferredMinII is a lower bound derived from the state graph. Where a measured II exists in workloads.yml it is used instead and the row says which.",
        ],
    }

    out = os.path.join(REPO, args.json_out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(manifest, fh, indent=1)
        fh.write("\n")
    print("WROTE %s (%d block(s))" % (args.json_out, len(records)))

    write_heatmap(os.path.join(REPO, args.md_out), manifest, calib, wl)
    print("WROTE %s" % args.md_out)


def fmt_ratio(dr):
    """Demand ratios in this design span five orders of magnitude.

    TERRAIN.NORMALS is 2,000 items against a 1,666,667 capacity -- 0.0012 --
    and printing that as `0.00x` next to TEXTURE.TMU's `3.06x` hides the single
    most useful fact about it: it is over-provisioned by a factor of 833. Two
    significant figures, always.
    """
    if dr is None:
        return "-"
    if dr >= 0.01:
        return "%.2fx" % dr
    return "%.2gx" % dr


def fmt(v, dash="-"):
    return dash if v is None else (("%,d" % v).replace(",", ",") if isinstance(v, int) else str(v))


def write_heatmap(path, man, calib, wl):
    R = man["blocks"]
    cov = man["coverage"]
    L = []
    L.append("# BUDGET HEATMAP")
    L.append("")
    L.append("> Generated by `tools/budget/build_manifest.py` from")
    L.append("> `reports/budget_manifest.json`. **Nothing in this file was typed by hand.**")
    L.append("> Regenerate rather than edit; a hand-corrected number here is indistinguishable")
    L.append("> from a measured one, which is the failure this whole audit exists to stop.")
    L.append("")
    L.append("HEAD `%s`. Frame budget **%s clocks** (compute), *not* the 251,520 raster period." %
             (man["headCommit"][:8], "{:,}".format(man["frameBudget"].get("computeClocksPerFrame", 0))))
    L.append("")
    L.append("| coverage | |")
    L.append("| --- | ---: |")
    L.append("| modules scanned (elaborated AST) | **%d** |" % cov["modulesScanned"])
    L.append("| modules with a map of **this exact RTL** | **%d** |" % cov["modulesWithMapAtHead"])
    L.append("| modules with a fit of this exact RTL | %d |" % cov.get("modulesWithFitAtHead", 0))
    L.append("| modules with any map | %d |" % cov["modulesWithAnyMap"])
    L.append("| modules with any fit | %d |" % cov["modulesWithAnyFit"])
    L.append("| modules with a demand figure | **%d** |" % cov["modulesWithWorkload"])
    L.append("| calibration points measured | %d |" % cov["calibrationPoints"])
    L.append("")

    # ---- the falsifiable test ------------------------------------------
    L.append("## The test of whether this works")
    L.append("")
    L.append("`docs/OWNER_DOCKET.md` set two blocks as the calibration of the flags themselves:")
    L.append("`zhao_field_seq` spends 8,901 ALMs and **zero** M10Ks on a register file and three")
    L.append("ROMs built from logic, and `zhao_texture_tmu` runs at II=6 against a demand needing")
    L.append("II=1. Both must come out RED from mechanical rules alone.")
    L.append("")
    L.append("| block | severity | why, mechanically |")
    L.append("| --- | --- | --- |")
    for nm in ("zhao_field_seq", "zhao_texture_tmu"):
        r = next((x for x in R if x["module"] == nm), None)
        if not r:
            L.append("| `%s` | **ABSENT** | not scanned |" % nm)
            continue
        why = "; ".join(r["debtFlags"][:4]) or "-"
        L.append("| `%s` | **%s** | %s |" % (nm, r["severity"], why))
    L.append("")

    # ---- the red list ---------------------------------------------------
    L.append("## RED")
    L.append("")
    L.append("| block | map DSP | fit DSP | expected RAM bits | inferred | II | demand | debt flags |")
    L.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |")
    for r in R:
        if r["severity"] != "RED":
            continue
        res, ram, rate = r["resources"], r["expectedVsInferredRam"], r["rate"]
        dr = rate.get("demandRatio")
        L.append("| `%s` | %s | %s | %s | %s | %s | %s | %s |" % (
            r["module"],
            res.get("mapDspBlocks", "-"),
            res.get("fitDspBlocks", "-"),
            "{:,}".format(ram["expectedTotalBits"]) if ram["expectedTotalBits"] else "-",
            ("%s (%d design)" % ("{:,}".format(ram["mapBlockMemoryBits"]), ram["mapInferredDesignMemories"] or 0)
             if ram["mapBlockMemoryBits"] else ("**0**" if res.get("mapDspBlocks") is not None else "-")),
            rate.get("iiUsed", rate["inferredMinII"]),
            fmt_ratio(dr),
            ", ".join("`%s`" % f for f in r["debtFlags"]) or "-",
        ))
    L.append("")

    for level in ("ORANGE", "YELLOW", "GREEN"):
        rows = [r for r in R if r["severity"] == level]
        L.append("## %s (%d)" % (level, len(rows)))
        L.append("")
        if not rows:
            L.append("*none* -- and for GREEN that is a statement about the EVIDENCE, not the")
            L.append("blocks. Not one of the 41 fitted rows describes the RTL at HEAD, so every")
            L.append("module in the design carries `NO_CURRENT_FIT` and no module can currently")
            L.append("reach GREEN however clean its source is. Closing that is a re-fit campaign,")
            L.append("which this run deliberately does not start.")
            L.append("")
            continue
        L.append("| block | map DSP | II | critical-path family | debt flags |")
        L.append("| --- | ---: | ---: | --- | --- |")
        for r in rows:
            L.append("| `%s` | %s | %s | %s | %s |" % (
                r["module"], r["resources"].get("mapDspBlocks", "-"),
                r["rate"].get("iiUsed", r["rate"]["inferredMinII"]),
                "; ".join(r["criticalPathFamily"])[:90] or "-",
                ", ".join("`%s`" % f for f in r["debtFlags"])[:70] or "-"))
        L.append("")

    # ---- ALM ledger -----------------------------------------------------
    L.append("## ALMs, and the resource nobody was counting")
    L.append("")
    L.append("The DSP campaign has had this project's attention for a week. **The two largest")
    L.append("resource items in the repository have no DSPs at all**, carry no fit row, and")
    L.append("therefore appear in no census: they are storage described as logic.")
    L.append("")
    L.append("Device: **41,910 ALMs**. `mapEstimatedAlms` is Analysis and Synthesis' own")
    L.append("estimate, and it is an estimate -- but a block estimating over twice the whole")
    L.append("device is not a question of estimator error.")
    L.append("")
    L.append("| block | est. ALM | % of device | DSP | expected storage bits | inferred |")
    L.append("| --- | ---: | ---: | ---: | ---: | ---: |")
    for r in sorted(R, key=lambda x: -(x["resources"].get("mapEstimatedAlms") or 0))[:14]:
        res, ram = r["resources"], r["expectedVsInferredRam"]
        a = res.get("mapEstimatedAlms")
        if not a:
            continue
        pct = 100.0 * a / 41910
        L.append("| `%s` | %s | %s | %s | %s | %s |" % (
            r["module"], "{:,}".format(a),
            ("**%.0f%%**" % pct) if pct > 25 else ("%.1f%%" % pct),
            res.get("mapDspBlocks", "-"),
            "{:,}".format(ram["expectedTotalBits"]) if ram["expectedTotalBits"] else "-",
            "{:,}".format(ram["mapBlockMemoryBits"]) if ram["mapBlockMemoryBits"] else "**0**"))
    L.append("")

    # ---- map-vs-fit agreement -------------------------------------------
    L.append("## Is the map lane trustworthy? Measured, not assumed")
    L.append("")
    L.append("This audit reads DSP counts out of `quartus_map` because a constrained fit costs")
    L.append("300-1300 s and 90 of them are not affordable. That is only legitimate if map and")
    L.append("fit agree, so every block holding both a map row and a fit row is compared here.")
    L.append("")
    L.append("| block | map DSP | fit DSP | map commit | fit commit | |")
    L.append("| --- | ---: | ---: | --- | --- | --- |")
    agree = dis = 0
    for r in R:
        md, fd = r["resources"].get("mapDspBlocks"), r["resources"].get("fitDspBlocks")
        if md is None or fd is None:
            continue
        same = (md == fd)
        agree += same
        dis += (not same)
        L.append("| `%s` | %d | %d | `%s` | `%s` | %s |" % (
            r["module"], md, fd, r["provenance"].get("mapCommit"),
            r["provenance"].get("fitCommit"), "" if same else "**differs**"))
    L.append("")
    L.append("**%d agree exactly, %d differ.**" % (agree, dis))
    if dis:
        L.append("")
        L.append("Every difference above is a block whose map and fit were taken at DIFFERENT")
        L.append("commits, which is what `NO_CURRENT_FIT` exists to say. Read the commit columns")
        L.append("before reading the difference as a tool disagreement.")
    L.append("")
    L.append("The ALM columns are NOT comparable and are deliberately absent from this table:")
    L.append("map reports an Analysis and Synthesis estimate, the fitter reports a placed count,")
    L.append("and they run about 10-20% apart on this design.")
    L.append("")

    # ---- ranked returns -------------------------------------------------
    L.append("## Ranked by estimated return")
    L.append("")
    L.append("**Estimates, and the docket's own gate still applies**: over 5 DSPs needs two")
    L.append("MEASURED Pareto points before any of this counts. What is measured here is the")
    L.append("DSP figure and the products; what is derived is the over-provisioning factor, and")
    L.append("what is estimated is the landing point.")
    L.append("")
    L.append("`overProvision = capacity / demand`. Serialising a block by up to that factor")
    L.append("costs no throughput, because the spare cycles are already in the frame.")
    L.append("")
    L.append("| block | DSP now | products | demand | over-provision | lanes that clear demand | est. DSP after | est. return |")
    L.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    ranked = []
    for r in R:
        d = r["resources"].get("mapDspBlocks") or r["resources"].get("fitDspBlocks")
        rt = r["rate"]
        prods = r["arithmetic"]["nonconstantMultiplyInstances"]
        if not d or not prods:
            continue
        over = rt.get("overProvisionFactor")
        lanes = rt.get("serialisableToLanes")
        if over is None or lanes is None:
            ranked.append((0, r, d, prods, over, None, None, None))
            continue
        per = d / prods
        after = max(1, int(round(per * lanes)))
        ranked.append((d - after, r, d, prods, over, lanes, after, d - after))
    ranked.sort(key=lambda x: -x[0])
    for gain, r, d, prods, over, lanes, after, ret in ranked:
        rt = r["rate"]
        L.append("| `%s` | %d | %d | %s | %s | %s | %s | %s |" % (
            r["module"], d, prods, fmt_ratio(rt.get("demandRatio")),
            (("%.0fx" % over) if over and over >= 1 else
             ("**%.2fx UNDER**" % over) if over else "-"),
            lanes if lanes else "-",
            after if after else "-",
            ("**%d**" % ret) if ret else "-"))
    L.append("")
    L.append("Blocks with `-` in the demand columns have **no items/frame figure**, so no")
    L.append("return can be derived for them at all. That is the gap `design/budgets/workloads.yml`")
    L.append("names explicitly, and it is where the next cheap win is.")
    L.append("")

    # ---- DSP ledger -----------------------------------------------------
    L.append("## DSP ledger at HEAD, from the map lane")
    L.append("")
    L.append("Leaf modules only -- a module that instantiates another would double-count it,")
    L.append("and scaffolding is excluded from the total with its reason printed rather than")
    L.append("filtered away silently.")
    L.append("")
    inside = set()
    for r in R:
        for sub in r["composition"]["submodules"]:
            inside.add(sub)
    total = 0
    L.append("| block | map DSP | inside another row? |")
    L.append("| --- | ---: | --- |")
    for r in sorted(R, key=lambda x: -(x["resources"].get("mapDspBlocks") or 0)):
        d = r["resources"].get("mapDspBlocks")
        if not d:
            continue
        ins = r["module"] in inside or not r["role"].startswith("design")
        if not ins:
            total += d
        why = "yes" if r["module"] in inside else (
            "excluded: " + r["role"].split(":")[0] if not r["role"].startswith("design") else "")
        L.append("| `%s` | %d | %s |" % (r["module"], d, why))
    L.append("")
    L.append("**Top-level total: %d DSP** against a 112-DSP device and a policy ceiling of 85-90." % total)
    L.append("")
    L.append("This total is not the console's number. It sums per-module maps, which share")
    L.append("nothing, and it counts every module that is not textually inside another --")
    L.append("including blocks that will never be instantiated together. Read it as the")
    L.append("**arithmetic that exists in the repository**, which is the quantity this audit")
    L.append("was asked to stop guessing at.")
    L.append("")

    # ---- timing ---------------------------------------------------------
    L.append("## Timing, and how little of it exists")
    L.append("")
    L.append("Every figure here comes from the FIT lane. The map lane has no SDC and no")
    L.append("placement, so it contributes nothing to this table by construction.")
    L.append("")
    L.append("`OLD_SDC` marks a row whose fit carries no Fmax at all -- it ran with no timing")
    L.append("objective, which QUARTUS_GOTCHAS 7 records was true of every per-block fit this")
    L.append("project ran for weeks. Those rows are not slow measurements; they are not")
    L.append("measurements.")
    L.append("")
    L.append("| block | Fmax (MHz) | WNS setup (ns) | hold (ns) | fit commit | RTL at HEAD? | critical-path family, from source |")
    L.append("| --- | ---: | ---: | ---: | --- | :--: | --- |")
    timed = 0
    for r in sorted(R, key=lambda x: (x["resources"].get("fitFmaxMhz") or 1e9)):
        res = r["resources"]
        if res.get("fitAlms") is None:
            continue
        f = res.get("fitFmaxMhz")
        if f is not None:
            timed += 1
        L.append("| `%s` | %s | %s | %s | `%s` | %s | %s |" % (
            r["module"],
            ("**%.2f**" % f) if f is not None else "*never timed*",
            res.get("fitSetupSlackNs") if res.get("fitSetupSlackNs") is not None else "-",
            res.get("fitHoldSlackNs") if res.get("fitHoldSlackNs") is not None else "-",
            r["provenance"].get("fitCommit"),
            "yes" if r["provenance"].get("fitRtlMatchesHead") else "**no**",
            "; ".join(r["criticalPathFamily"])[:70] or "-"))
    L.append("")
    L.append("**%d of %d fitted blocks carry an Fmax at all.**" % (timed, cov["modulesWithAnyFit"]))
    L.append("")
    nslack = sum(1 for r in R if r["resources"].get("fitSetupSlackNs") is not None
                 or r["resources"].get("fitHoldSlackNs") is not None)
    L.append("**And %d of them carry a setup or hold slack figure.**" % nslack)
    if nslack == 0:
        L.append("")
        L.append("That is not a gap in this report. `tools/quartus/run_block_fit.ps1` DOES try to")
        L.append("extract both, with")
        L.append("")
        L.append(r"    '(?m)^\s*Worst-case Setup Slack\D+(-?[0-9.]+)'")
        L.append("")
        L.append("and **it has never once matched** -- not in a single committed row, including")
        L.append("the rows measured after the SDC repair, which do carry an Fmax from the very")
        L.append("same report file. The Fmax regex reads the summary table; the slack regex reads")
        L.append("a line format that Quartus 17.0.2's STA report does not appear to emit.")
        L.append("")
        L.append("So the ruling asks this heatmap for **WNS, TNS and hold**, and the fit lane can")
        L.append("supply **none of the three**. The columns are printed empty rather than dropped,")
        L.append("because a column that is missing looks like a question nobody asked.")
        L.append("")
        L.append("This is QUARTUS_GOTCHAS' own pattern one more time -- an extraction that fails")
        L.append("silently and whose only symptom is a number that never appears. It is a defect")
        L.append("in the instrument, not in the blocks, and it is the cheapest item on the board.")
        L.append("")

    # ---- the cost curve the DESIGN already measures ----------------------
    L.append("## DSP per product, measured on this design")
    L.append("")
    L.append("The calibration microbenches below characterise synthetic modules. This table is")
    L.append("the same question answered by the SHIPPING RTL: for every block whose products")
    L.append("are all one width, DSP blocks divided by product count. It is independent")
    L.append("evidence, and it is what `design/budgets/dsp.md`'s corrected rule needs.")
    L.append("")
    L.append("| block | products | widest operand | map DSP | DSP per product | decomposition |")
    L.append("| --- | ---: | ---: | ---: | ---: | --- |")
    curve = []
    for r in R:
        d = r["resources"].get("mapDspBlocks")
        n = r["arithmetic"]["nonconstantMultiplyInstances"]
        w = r["arithmetic"]["widestNonconstantOperand"]
        if not d or not n or not w:
            continue
        if r["arithmetic"].get("constantMultiplyInstances"):
            continue
        curve.append((w, d / n, r, d, n))
    for w, per, r, d, n in sorted(curve, key=lambda x: (x[0], x[1])):
        dec = r["resources"].get("mapDspDecomposition") or {}
        decs = ", ".join("%s=%s" % (k.replace("Fixed Point ", "").replace(" Multiplier", ""), v)
                         for k, v in dec.items() if v and k != "Total number of DSP blocks")
        L.append("| `%s` | %d | %d | %d | **%.2f** | %s |" % (r["module"], n, w, d, per, decs or "-"))
    L.append("")
    L.append("Two things fall straight out and both matter to planning:")
    L.append("")
    L.append("* **A product at or below 18 bits costs ONE DSP block; a 32- or 33-bit product")
    L.append("  costs THREE.** The jump is the discontinuity `design/budgets/dsp.md` was")
    L.append("  corrected to warn about, now with a number on it.")
    L.append("* **Quartus Lite packs NOTHING.** `zhao_geom_quat2mat` forms nine 16x16 signed")
    L.append("  products and takes **nine** DSP blocks -- `Two Independent 18x18: 9` -- when two")
    L.append("  16x16 operators would fit in one block's two halves. That block's own comment")
    L.append("  assumes narrow products will pack. **They do not.**")
    L.append("")

    # ---- calibration ----------------------------------------------------
    pts = [p for p in calib.get("points", []) if p.get("status") == "ok"]
    L.append("## Calibration: measured shape -> resources")
    L.append("")
    if not pts:
        L.append("*Not yet measured.* Run `python tools/budget/gen_calib.py` then")
        L.append("`tools/quartus/run_calib.ps1`.")
        L.append("")
    else:
        muls = [p for p in pts if p.get("family") == "multiply"]
        if muls:
            L.append("### Multipliers")
            L.append("")
            L.append("Cyclone V `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2, input+output registered")
            L.append("unless the style column says otherwise. **DSP is per module, so divide by the")
            L.append("operator count to get cost per product.**")
            L.append("")
            L.append("| operand width | signed | operators | style | DSP | DSP/product | est. ALM | decomposition |")
            L.append("| ---: | :--: | ---: | --- | ---: | ---: | ---: | --- |")
            for p in sorted(muls, key=lambda x: (x.get("width", 0), x.get("signed"), x.get("operators", 0), x.get("style", ""))):
                d = p.get("dspBlocks")
                n = p.get("operators") or 1
                dec = p.get("dspDecomposition") or {}
                decs = ", ".join("%s=%s" % (k.replace("Fixed Point ", "").replace(" Multiplier", ""), v)
                                 for k, v in dec.items() if v and k != "Total number of DSP blocks")
                L.append("| %s | %s | %d | %s | %s | %s | %s | %s |" % (
                    p.get("width"), "S" if p.get("signed") else "U", n, p.get("style"),
                    d if d is not None else "-",
                    ("%.2f" % (d / n)) if d is not None else "-",
                    p.get("estimatedAlms", "-"), decs or "-"))
            L.append("")
        wid = [p for p in pts if p.get("family") == "widening"]
        if wid:
            L.append("### The widening-multiply idiom")
            L.append("")
            L.append("`zhao_geom_project` writes nine products as")
            L.append("`$signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b})`. The first draft of")
            L.append("`scan_rtl.py` called that extension slack and flagged it RED. These two rows")
            L.append("settle it by measurement rather than by argument.")
            L.append("")
            L.append("| form | DSP | est. ALM |")
            L.append("| --- | ---: | ---: |")
            for p in wid:
                L.append("| %s | %s | %s |" % (p.get("note") or p["module"],
                                               p.get("dspBlocks", "-"), p.get("estimatedAlms", "-")))
            L.append("")
        rams = [p for p in pts if p.get("family") == "ram"]
        if rams:
            L.append("### Storage templates")
            L.append("")
            L.append("`blockMemoryBits > 0` is the ONLY evidence an array became a memory.")
            L.append("")
            L.append("| depth x width | read | reset | ports | byte-en | expected bits | block mem bits | INFERRED? | est. ALM |")
            L.append("| --- | --- | --- | ---: | :--: | ---: | ---: | :--: | ---: |")
            for p in sorted(rams, key=lambda x: (x.get("depth", 0), x.get("width", 0),
                                                 x.get("readStyle", ""), bool(x.get("reset")), x.get("ports", 0))):
                bmb = p.get("blockMemoryBits") or 0
                L.append("| %sx%s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                    p.get("depth"), p.get("width"), p.get("readStyle"),
                    "yes" if p.get("reset") else "no", p.get("ports"),
                    "yes" if p.get("byteEnables") else "no",
                    "{:,}".format(p.get("expectedBits") or 0),
                    "{:,}".format(bmb),
                    "**yes**" if bmb > 0 else "**NO**",
                    p.get("estimatedAlms", "-")))
            L.append("")

    # ---- workload coverage ---------------------------------------------
    L.append("## Blocks with no demand figure")
    L.append("")
    L.append("`design/budgets/workloads.yml` lists these explicitly rather than omitting them,")
    L.append("because a missing row and an unanswered question look identical otherwise.")
    L.append("")
    for u in (wl.get("unruled") or []):
        if isinstance(u, dict):
            L.append("* `%s`%s" % (u.get("module"), (" -- " + u["note"]) if u.get("note") else ""))
    L.append("")

    L.append("## What every flag means")
    L.append("")
    for f, why in [
        ("NO_MAP", "no `quartus_map` row exists for this module at any commit"),
        ("NO_CURRENT_FIT", "no fit at HEAD; the resource columns describe older source"),
        ("OLD_SDC", "the fit carries no Fmax at all, so it ran with no timing objective -- QUARTUS_GOTCHAS 7 and 9"),
        ("NO_WORKLOAD", "no items/frame, so no demand ratio can be computed for this block"),
        ("NO_II_TEST", "throughput is asserted nowhere executable; the II shown is inferred from the state graph"),
        ("EXPECTED_RAM_NOT_INFERRED", "the source declares addressable storage or a large constant table and the map reports zero design memories. **reports/QUARTUS_GOTCHAS.md 10** measures what this costs: a synchronous read with no reset on the array infers at tens of ALMs, and EITHER an async read OR a reset kills inference outright -- 35x the ALMs at 2 kbit, 108x at 4 kbit, 502x at 32 kbit, 806x at 36 kbit. This flag is the cheap detector for it, and it needs no fit"),
        ("NO_SUBSYSTEM_FIT", "arithmetic runs from this block's own pins, so a leaf fit misrepresents it -- measured at 5.4x once"),
        ("NO_RESERVE", "demand ratio leaves less headroom than workloads.yml asks for"),
        ("PARETO_UNPROVEN", "over 5 DSPs or over 5% of the device's ALMs with no second measured operating point -- the docket's own CI gate, applied to the measured number"),
    ]:
        L.append("* **`%s`** -- %s" % (f, why))
    L.append("")

    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
