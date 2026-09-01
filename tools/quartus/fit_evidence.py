#!/usr/bin/env python3
"""Build the fit-evidence bundle the 110-115 MHz spec section 3.1 requires.

The spec lists what every fit MUST archive, and four of those items were not
being produced:

    path startpoint-type histogram
    M10K-launched path list
    seed and Quartus version
    source and elaborated-instance manifest

All four are produced now. The last two are provenance rather than analysis,
and they exist because a number without the RTL that produced it is not
evidence -- round 7's 66.78 MHz only became a usable lesson once it could be
tied to the exact source that inferred twelve DSPs.

The first two matter most, and they are the systematic form of a finding that
was made by hand twice. Rounds 3 and 6 both recovered time by removing a
RAM-launched combinational path, and the reason is in spec section 2.2: an
M10K's register sits about 2 ns deeper inside the block than a fabric flop, so
a path that LAUNCHES at a RAM output pays that before any logic runs. Counting
those paths by hand is how they get missed.

    python3 tools/quartus/fit_evidence.py <characterization-dir> <out-dir>

Reads `setup_paths.rpt` and writes:

    startpoint_types.txt   histogram by launch-register kind
    m10k_launched.txt      every worst-100 path launching at an M10K
    owners.txt             destination-block histogram
    manifest.txt           tool version, seed, device and a hash of every RTL
                           source at fit time, so the number is reproducible
    evidence.json          the same, machine-readable

Exit 0 always unless the report is unreadable: this is evidence collection, not
a gate. A gate that refuses to record is worse than no gate.
"""
import hashlib
import io
import json
import os
import re
import sys

# A path's launch node tells you what KIND of register it started at, and the
# kinds have very different clock-to-out costs on this device.
KINDS = [
    ("m10k",    re.compile(r"altsyncram|ram_block", re.I)),
    ("dsp",     re.compile(r"~mac|DSP_X", re.I)),
    ("mlab",    re.compile(r"MLABCELL|\bmlab\b", re.I)),
    ("io",      re.compile(r"~padout|~IO_|IOIBUF|IOOBUF", re.I)),
]


def classify(node):
    for name, rx in KINDS:
        if rx.search(node):
            return name
    return "fabric_ff"


def block_of(node):
    m = re.findall(r"(zhao_[a-z0-9_]+)", node)
    return m[-1] if m else "?"


def provenance(chdir, repo):
    """Tool version, seed, device and a hash per RTL source (spec 3.1).

    The commit alone is not enough: a fit can be launched from a dirty tree, and
    then the number belongs to sources no commit records. Hashing the actual
    files is the only form of this that cannot lie.
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

    rows = []
    for line in text.splitlines():
        if not line.startswith("; -") and not line.startswith("; 0"):
            continue
        parts = [p.strip() for p in line.split(" ; ")]
        if len(parts) < 3:
            continue
        try:
            slack = float(parts[0].lstrip("; ").strip())
        except ValueError:
            continue
        rows.append({"slack": slack, "from": parts[1], "to": parts[2]})

    if not rows:
        sys.stderr.write("zhao: no summary rows parsed from %s -- the report "
                         "format may have changed, and a silent empty bundle "
                         "would look like a clean design\n" % src)
        return 2

    starts, owners, m10k = {}, {}, []
    for r in rows:
        k = classify(r["from"])
        starts[k] = starts.get(k, 0) + 1
        b = block_of(r["to"])
        owners[b] = owners.get(b, 0) + 1
        if k == "m10k":
            m10k.append(r)

    def dump(name, lines):
        io.open(os.path.join(out, name), "w", encoding="utf-8",
                newline="\n").write("\n".join(lines) + "\n")

    dump("startpoint_types.txt",
         ["# worst-%d setup paths by LAUNCH register kind" % len(rows),
          "# spec 2.2: an M10K launch pays ~2 ns of clock-to-out that a",
          "# fabric flop does not, so this histogram is the first thing to",
          "# read after the Fmax.", ""] +
         ["%-12s %4d" % (k, n) for k, n in sorted(starts.items(), key=lambda x: -x[1])])

    dump("owners.txt",
         ["# worst-%d setup paths by DESTINATION block" % len(rows), ""] +
         ["%-28s %4d" % (b, n) for b, n in sorted(owners.items(), key=lambda x: -x[1])])

    dump("m10k_launched.txt",
         ["# every worst-path launching at an M10K output.",
          "# EMPTY IS THE GOAL. Rounds 3 and 6 each recovered time by removing",
          "# one of these; if this file is long, that is the next lever.",
          "# count: %d of %d" % (len(m10k), len(rows)), ""] +
         ["%8.3f  %s\n          -> %s" % (r["slack"], r["from"], r["to"])
          for r in m10k[:40]])

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
                {"paths": len(rows),
                 "worst_slack_ns": min(r["slack"] for r in rows),
                 "startpoint_types": starts,
                 "owners": owners,
                 "m10k_launched": len(m10k),
                 "quartus": prov.get("quartus"),
                 "device": prov.get("device"),
                 "seed": prov.get("seed"),
                 "source_count": len(prov["sources"]),
                 "sources": dict((f, h) for f, h in prov["sources"])},
                indent=2) + "\n")

    print("paths %d   worst %.3f ns" % (len(rows), min(r["slack"] for r in rows)))
    print("startpoints: " + ", ".join("%s=%d" % kv for kv in
                                      sorted(starts.items(), key=lambda x: -x[1])))
    print("M10K-launched: %d of %d" % (len(m10k), len(rows)))
    print("quartus %s  device %s  seed %s  sources %d"
          % (prov.get("quartus", "?"), prov.get("device", "?"),
             prov.get("seed", "?"), len(prov["sources"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
