# validate_scan_rtl_fixes.py — positive controls for the two scan_rtl.py defects
# fixed in RUN-20260824-0317.
#
# RUN-20260823-2226's fifth disclosed failure was a detector that reported ZERO
# across all 91 modules because it could never fire, and whose silence was about
# to be published as a result. Both changes here are detectors. Neither is
# believed until it has been shown to say the right thing about a case whose
# answer is already known from reading the source.
#
#   DEFECT 1  `resetTouched` walked the whole IF node, so the ELSE branch — the
#             block's normal operating logic — counted as "written from a reset
#             branch". Every array in any always_ff with an async reset was
#             reported reset-touched.
#
#   DEFECT 2  there was NO byte-enable / partial-write detector at all, so the
#             array that cost 229 % of the device was reported healthy.
#
# The controls are BOTH directions. A detector that says "no partial write"
# about everything passes a one-sided test just as well as a correct one.
#
# Usage:  python runs/CLAUDE-RUNS/RUN-.../validate_scan_rtl_fixes.py
import json
import os
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
PRE = "991f13c3"  # the commit whose zhao_surface_sheet still has byte enables

# (module, array, field, expected, why the answer is known without the tool)
CASES = [
    # ---- DEFECT 2: partial writes, both directions ------------------------
    ("zhao_forge_cliff", "edge_span_r", "partialWriteSites", 0,
     "split at the field boundary this run; both writes are whole-word"),
    ("zhao_forge_cliff", "edge_key_r", "partialWriteSites", 0,
     "written whole by StEnum and never partially"),
    ("zhao_forge_cliff", "prio_mem_r", "partialWriteSites", 0,
     "one write site, whole word — and it INFERRED, at 65,536 bits"),
    ("zhao_forge_cliff", "run_mem_r", "partialWriteSites", 0,
     "two write sites, both whole word — and it INFERRED, at 17,408 bits"),
    ("zhao_surface_sheet", "mem_tag", "partialWriteSites", 0,
     "split into byte planes this run; written whole"),
    ("zhao_surface_sheet", "mem_str", "partialWriteSites", 0,
     "split into byte planes this run; written whole"),
    # ---- DEFECT 1: resetTouched, both directions --------------------------
    ("zhao_forge_cliff", "prio_mem_r", "resetTouched", False,
     "the reset branch assigns 31 scalars and no array element"),
    ("zhao_forge_cliff", "run_mem_r", "resetTouched", False, "same reset branch"),
    ("zhao_forge_cliff", "edge_key_r", "resetTouched", False, "same reset branch"),
    ("zhao_surface_sheet", "mem_tag", "resetTouched", False,
     "declared outside any reset process, deliberately"),
    ("zhao_surface_sheet", "dir_handle", "resetTouched", True,
     "`for (s...) dir_handle[s] <= 32'd0;` IS inside `if (!rst_n)` — the "
     "detector must still FIRE here or it has been broken, not fixed"),
    ("zhao_field_seq", "rf", "resetTouched", True,
     "GOTCHAS section 10 retro-explains this block as '64x32, read "
     "asynchronously, written from a reset branch'"),
]

# The same two arrays as they were BEFORE this run, from git. If the partial
# -write detector cannot see the defect it was written for, it is decoration.
PRE_CASES = [
    ("zhao_surface_sheet", "mem", "partialWriteSites", 2,
     "`if (mem_be[1]) mem[wr_addr][15:8] <= ...` and its [7:0] sibling"),
    ("zhao_forge_cliff", "edge_mem_r", "partialWriteSites", 1,
     "`edge_mem_r[mhead_r][5:0] <= mtake_r` in StMdead"),
]


def scan(modules, workdir=None):
    out = os.path.join(tempfile.gettempdir(), "scan_validate.json")
    cmd = [sys.executable, "tools/budget/scan_rtl.py", "--modules"] + modules + \
          ["--out", out, "--quiet"]
    r = subprocess.run(cmd, cwd=workdir or REPO, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("scan_rtl failed:\n" + r.stdout + r.stderr)
    with open(out) as fh:
        d = json.load(fh)
    arrays = {}
    for m in d["modules"]:
        for f in m.get("findings", []):
            if f.get("kind") == "array":
                arrays[(m["module"], f["name"])] = f
    return arrays


def check(arrays, cases, label):
    bad = 0
    print("== %s" % label)
    for mod, arr, field, want, why in cases:
        rec = arrays.get((mod, arr))
        if rec is None:
            print("   MISSING  %-20s %-14s (%s)" % (mod, arr, why))
            bad += 1
            continue
        got = rec.get(field)
        ok = (got == want)
        print("   %-4s %-20s %-13s %-18s got=%-6s want=%-6s  %s"
              % ("ok" if ok else "FAIL", mod, arr, field, got, want, why))
        if not ok:
            bad += 1
    return bad


bad = check(scan(["zhao_forge_cliff", "zhao_surface_sheet", "zhao_field_seq"]),
            CASES, "working tree")

# The pre-change RTL, in a scratch checkout. `git show`, NOT
# `git checkout <rev> -- <path>` — the latter STAGES, and this repository has
# already lost a rearchitecture to that exact call.
tmp = os.path.join(tempfile.gettempdir(), "scan_validate_pre")
os.makedirs(tmp, exist_ok=True)
for rel in ("fpga/rtl/surface/zhao_surface_sheet.sv", "fpga/rtl/forge/zhao_forge_cliff.sv"):
    dst = os.path.join(REPO, rel)
    bak = os.path.join(tmp, os.path.basename(rel))
    with open(dst, "rb") as fh:
        open(bak, "wb").write(fh.read())
try:
    for rel in ("fpga/rtl/surface/zhao_surface_sheet.sv", "fpga/rtl/forge/zhao_forge_cliff.sv"):
        blob = subprocess.run(["git", "show", "%s:%s" % (PRE, rel)], cwd=REPO,
                              capture_output=True)
        if blob.returncode != 0:
            sys.exit("could not read %s from %s" % (rel, PRE))
        open(os.path.join(REPO, rel), "wb").write(blob.stdout)
    bad += check(scan(["zhao_forge_cliff", "zhao_surface_sheet"]), PRE_CASES,
                 "the PRE-change RTL at %s — the defect the detector exists for" % PRE)
finally:
    for rel in ("fpga/rtl/surface/zhao_surface_sheet.sv", "fpga/rtl/forge/zhao_forge_cliff.sv"):
        bak = os.path.join(tmp, os.path.basename(rel))
        with open(bak, "rb") as fh:
            open(os.path.join(REPO, rel), "wb").write(fh.read())

print("----")
print("%d of %d controls failed" % (bad, len(CASES) + len(PRE_CASES)))
sys.exit(1 if bad else 0)
