#!/usr/bin/env python3
"""Build the fit-evidence bundle the 110-115 MHz spec section 3.1 requires.

The spec lists what every fit MUST archive:

    path startpoint-type histogram
    M10K-launched path list
    seed and Quartus version
    source and elaborated-instance manifest

    python3 tools/quartus/fit_evidence.py <characterization-dir> <out-dir>

Reads `setup_paths.rpt` and writes:

    startpoint_types.txt   histogram by launch-register PHYSICAL resource
    m10k_launched.txt      every path launching at an M10K
    dsp_launched.txt       every path launching at a DSP output register
    owners.txt             destination-block histogram
    manifest.txt           tool version, seed, device, hash of every RTL source
    evidence.json          the same, machine-readable

--------------------------------------------------------------------------
CLASSIFY BY THE `Location` COLUMN, NEVER BY THE REGISTER'S NAME.

The first version matched the logical node name for substrings like `~mac` or
`DSP_X`, and reported for both round 9 and round 10:

    startpoints: fabric_ff=200

Both numbers in that line are wrong, and bro caught it.

WRONG CLASSIFICATION. Quartus packs `cross_r` into the output register of the
DSP that feeds it, so its launch is a DSP register carrying a DSP's
clock-to-out. The summary node name is merely `cross_r[47]` and carries no
hint of that. The detail says it plainly:

    6.394 ; 0.000 ; uTco ; DSP_X20_Y45_N0     ; ...u_edgewalk|cross_r
    7.099 ; 0.705 ; CELL ; DSP_X20_Y45_N0     ; ...Mult1~mac|resulta[47]
    8.076 ; 0.977 ; IC   ; LABCELL_X23_Y45_N54; ...cxf[11]~4|datac

Round 10's true split is 52 fabric FF and 48 DSP, and those 48 are exactly the
`cross_r -> cross_r` paths. Inferring a physical property from a logical name
is the same error as deriving a 3D radius from a 2D drawing: the name is a
projection of the placement, not the placement.

WRONG COUNT. `^; -` also matched each path block's own `; Slack ; -1.563` row,
so 100 paths were counted as 200. The report states its own total in the
header -- "Found 100 setup paths" -- and that is now asserted against.

Exactly one `uTco` row per path block IS the launch register, and its
`Location` is physical fact from the fitter. That is what gets parsed.
--------------------------------------------------------------------------

Exit 0 unless the report is unreadable or self-inconsistent. This is evidence
collection, not a gate -- but it FAILS LOUDLY on a parse mismatch, because an
empty M10K list is the GOAL state and a silent parse failure looks identical
to success.
"""
import hashlib
import io
import json
import os
import re
import sys

# Physical resource, from the fitter's own Location column. A Cyclone V launch
# register lives in exactly one of these.
LOCATION_KIND = [
    ("m10k",      re.compile(r"^M10K", re.I)),
    ("dsp",       re.compile(r"^DSP", re.I)),
    ("mlab",      re.compile(r"^MLABCELL", re.I)),
    ("io",        re.compile(r"^(IOIBUF|IOOBUF|DDIO|PLL)", re.I)),
    ("fabric_ff", re.compile(r"^(FF|LABCELL)", re.I)),
]

PATH_RE  = re.compile(r"^Path #(\d+): Setup slack is (-?[\d.]+)")
FROM_RE  = re.compile(r"^;\s*From Node\s*;\s*(.+?)\s*;")
TO_RE    = re.compile(r"^;\s*To Node\s*;\s*(.+?)\s*;")
UTCO_RE  = re.compile(r"^;[^;]*;[^;]*;[^;]*;\s*uTco\s*;[^;]*;\s*(\S+)\s*;\s*(.+?)\s*;?\s*$")
TOTAL_RE = re.compile(r"Found (\d+) setup paths")


def kind_of(location):
    for name, rx in LOCATION_KIND:
        if rx.match(location):
            return name
    return "other:" + location.split("_")[0]


def block_of(node):
    m = re.findall(r"(zhao_[a-z0-9_]+)", node)
    return m[-1] if m else "?"


def parse_paths(text):
    """One record per `Path #N` block, with the PHYSICAL launch resource."""
    paths, cur = [], None
    for line in text.splitlines():
        m = PATH_RE.match(line)
        if m:
            if cur:
                paths.append(cur)
            cur = {"n": int(m.group(1)), "slack": float(m.group(2)),
                   "from": "?", "to": "?", "loc": "", "launch": ""}
            continue
        if cur is None:
            continue
        m = FROM_RE.match(line)
        if m and cur["from"] == "?":
            cur["from"] = m.group(1)
            continue
        m = TO_RE.match(line)
        if m and cur["to"] == "?":
            cur["to"] = m.group(1)
            continue
        if not cur["loc"]:
            m = UTCO_RE.match(line)
            if m:
                cur["loc"], cur["launch"] = m.group(1), m.group(2)
    if cur:
        paths.append(cur)
    return paths


def provenance(chdir, repo):
    """Tool version, seed, device and a hash per RTL source (spec 3.1).

    The commit alone is not enough: a fit can be launched from a dirty tree,
    and then the number belongs to sources no commit records. Hashing the
    actual files is the only form of this that cannot lie.
    """
    p = {}
    try:
        head = io.open(os.path.join(chdir, "zhao_shell_fit.sta.rpt"),
                       encoding="utf-8", errors="replace").read(4096)
        m = re.search(r"Quartus Prime Version ([^\r\n]+)", head)
        if m:
            p["quartus"] = m.group(1).strip()
    except OSError:
        pass

    qsf = os.path.join(repo, "fpga", "quartus", "shell_fit", "zhao_shell_fit.qsf")
    try:
        for line in io.open(qsf, encoding="utf-8", errors="replace"):
            m = re.match(r"\s*set_global_assignment\s+-name\s+"
                         r"(SEED|DEVICE|FAMILY)\s+(.+)", line)
            if m:
                p[m.group(1).lower()] = m.group(2).strip().strip('"')
    except OSError:
        pass

    srcs = []
    for root, _, files in os.walk(os.path.join(repo, "fpga", "rtl")):
        for f in sorted(files):
            if f.endswith((".sv", ".v", ".svh")):
                full = os.path.join(root, f)
                try:
                    h = hashlib.sha256(io.open(full, "rb").read()).hexdigest()[:16]
                except OSError:
                    continue
                srcs.append((os.path.relpath(full, repo).replace(os.sep, "/"), h))
    p["sources"] = srcs
    return p


def main(argv):
    if len(argv) < 3:
        sys.stderr.write(__doc__)
        return 2
    src = os.path.join(argv[1], "setup_paths.rpt")
    out = argv[2]
    os.makedirs(out, exist_ok=True)

    try:
        text = io.open(src, encoding="utf-8", errors="replace").read()
    except OSError as e:
        sys.stderr.write("zhao: cannot read %s: %s\n" % (src, e))
        return 2

    paths = parse_paths(text)
    if not paths:
        sys.stderr.write("zhao: no path blocks parsed from %s -- the report "
                         "format may have changed, and a silent empty bundle "
                         "would look like a clean design\n" % src)
        return 2

    # The report states its own total. Assert against it: the previous version
    # silently counted 100 paths as 200 and nothing noticed.
    m = TOTAL_RE.search(text)
    if m and int(m.group(1)) != len(paths):
        sys.stderr.write("zhao: report says %s setup paths, parsed %d -- "
                         "refusing to write a bundle that miscounts\n"
                         % (m.group(1), len(paths)))
        return 2

    unlocated = [p for p in paths if not p["loc"]]
    if unlocated:
        sys.stderr.write("zhao: %d of %d paths have no uTco launch row; the "
                         "report needs `-detail full_path`\n"
                         % (len(unlocated), len(paths)))
        return 2

    starts, owners, worst_by = {}, {}, {}
    for p in paths:
        p["kind"] = kind_of(p["loc"])
        starts[p["kind"]] = starts.get(p["kind"], 0) + 1
        b = block_of(p["to"])
        owners[b] = owners.get(b, 0) + 1
        if b not in worst_by or p["slack"] < worst_by[b]:
            worst_by[b] = p["slack"]

    m10k = [p for p in paths if p["kind"] == "m10k"]
    dsp = [p for p in paths if p["kind"] == "dsp"]
    worst = min(p["slack"] for p in paths)

    def dump(name, lines):
        io.open(os.path.join(out, name), "w", encoding="utf-8",
                newline="\n").write("\n".join(lines) + "\n")

    def listing(head, rows):
        return (head + ["# count: %d of %d" % (len(rows), len(paths)), ""] +
                ["%8.3f  %-22s %s" % (r["slack"], r["loc"], r["launch"])
                 for r in rows[:40]])

    dump("startpoint_types.txt",
         ["# worst-%d setup paths by launch-register PHYSICAL resource," % len(paths),
          "# read from the fitter's own Location column -- NOT from the node",
          "# name, which does not say where Quartus put the register.",
          "#",
          "# spec 2.2: an M10K launch pays ~2 ns of clock-to-out a fabric flop",
          "# does not, and a DSP-packed launch pays its own. Read this right",
          "# after the Fmax.", ""] +
         ["%-12s %4d" % (k, n) for k, n in sorted(starts.items(), key=lambda x: -x[1])])

    # RANKED BY WORST SLACK, NOT BY COUNT. Count is what the eye reaches for
    # and it is the wrong ranking: in the seed-3 fit Early-Z owned 78 of 100
    # paths at -0.258 while zhao_vram_arbiter owned FOUR at -0.425. Early-Z
    # looked like the limiter by a factor of twenty and had 0.167 ns of slack
    # in hand -- fixing it would have bought nothing at all. The block that
    # sets Fmax is the one holding the worst path, however few it owns.
    dump("owners.txt",
         ["# worst-%d setup paths by DESTINATION block," % len(paths),
          "# RANKED BY WORST SLACK -- count is a decoy. The block holding the",
          "# worst path sets Fmax however few paths it owns; a block owning",
          "# most of the list with slack in hand is not the limiter.",
          "",
          "# %-26s %6s  %5s" % ("block", "worst", "paths"),
          ""] +
         ["%-28s %6.3f  %5d" % (b, worst_by[b], owners[b])
          for b in sorted(owners, key=lambda x: worst_by[x])])

    dump("m10k_launched.txt",
         listing(["# paths launching at an M10K output.",
                  "# EMPTY IS THE GOAL. Rounds 3 and 6 each recovered time by",
                  "# removing one; a long list here is the next lever."], m10k))

    dump("dsp_launched.txt",
         listing(["# paths launching at a DSP output register.",
                  "# Quartus packs a register into the DSP that feeds it, so",
                  "# these do NOT look like DSPs by name -- which is exactly how",
                  "# the first version of this tool missed all 48 of them."], dsp))

    repo = os.path.abspath(os.path.join(argv[1], os.pardir, os.pardir))
    prov = provenance(argv[1], repo)
    dump("manifest.txt",
         ["# fit provenance (spec 3.1). A number without the RTL that produced",
          "# it is not evidence.",
          "",
          "quartus  %s" % prov.get("quartus", "?"),
          "family   %s" % prov.get("family", "?"),
          "device   %s" % prov.get("device", "?"),
          "seed     %s" % prov.get("seed", "?"),
          "",
          "# sha256[:16] of every RTL source in the tree at fit time",
          ""] +
         ["%s  %s" % (h, f) for f, h in prov["sources"]])

    io.open(os.path.join(out, "evidence.json"), "w", encoding="utf-8",
            newline="\n").write(json.dumps(
                {"paths": len(paths),
                 "worst_slack_ns": worst,
                 "startpoint_types": starts,
                 "owners": owners,
                 "worst_slack_by_owner": worst_by,
                 "m10k_launched": len(m10k),
                 "dsp_launched": len(dsp),
                 "quartus": prov.get("quartus"),
                 "device": prov.get("device"),
                 "seed": prov.get("seed"),
                 "source_count": len(prov["sources"]),
                 "sources": dict((f, h) for f, h in prov["sources"])},
                indent=2) + "\n")

    print("paths %d   worst %.3f ns" % (len(paths), worst))
    print("startpoints: " + ", ".join("%s=%d" % kv for kv in
                                      sorted(starts.items(), key=lambda x: -x[1])))
    print("M10K-launched: %d   DSP-launched: %d" % (len(m10k), len(dsp)))
    lim = sorted(owners, key=lambda x: worst_by[x])[:3]
    print("limiter: " + ", ".join("%s %.3f(%d)" % (b, worst_by[b], owners[b])
                                  for b in lim))
    print("quartus %s  device %s  seed %s  sources %d"
          % (prov.get("quartus", "?"), prov.get("device", "?"),
             prov.get("seed", "?"), len(prov["sources"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
