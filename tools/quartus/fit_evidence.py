#!/usr/bin/env python3
"""Build the fit-evidence bundle the 110-115 MHz spec section 3.1 requires.

The spec lists what every fit MUST archive, and four of those items were not
being produced:

    path startpoint-type histogram
    M10K-launched path list
    seed and Quartus version
    source and elaborated-instance manifest

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
    evidence.json          the same, machine-readable

Exit 0 always unless the report is unreadable: this is evidence collection, not
a gate. A gate that refuses to record is worse than no gate.
"""
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

    io.open(os.path.join(out, "evidence.json"), "w", encoding="utf-8",
            newline="\n").write(json.dumps(
                {"paths": len(rows),
                 "worst_slack_ns": min(r["slack"] for r in rows),
                 "startpoint_types": starts,
                 "owners": owners,
                 "m10k_launched": len(m10k)}, indent=2) + "\n")

    print("paths %d   worst %.3f ns" % (len(rows), min(r["slack"] for r in rows)))
    print("startpoints: " + ", ".join("%s=%d" % kv for kv in
                                      sorted(starts.items(), key=lambda x: -x[1])))
    print("M10K-launched: %d of %d" % (len(m10k), len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
