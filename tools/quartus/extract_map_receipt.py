#!/usr/bin/env python3
"""Pull the numbers that matter out of a `.map.rpt` into a SMALL COMMITTED file.

WHY THIS EXISTS
---------------
`reports/synthesis/blockpaths/*.map.rpt` is a megabyte per run and is IGNORED
by a size rule, so it is not in history. On 2026-09-25 the attribution report
that an entire ALM lever list had been derived from -- `zhao_console_core@
diag-map-attrib.map.rpt` -- was found GONE from disk, with only a totals-only
summary surviving. The lever list could no longer be checked against anything.

The committed `.map.summary` beside it is not enough either, and the gap is
specific: it carries `Logic utilization (in ALMs) : N/A`, `Total registers` and
`Total block memory bits`, and it does NOT carry the two numbers an area
argument is actually made of -- `Estimate of Logic utilization (ALMs needed)`
and `Combinational ALUT usage for logic`. Those live only in the ignored file.

This repo's rule is "commit the probe". The gap that rule left is the RECEIPT.

WHAT IT KEEPS APART, AND WHY THAT IS THE POINT
----------------------------------------------
`OWNER_VACATION_DIRECTIVE_2026-09-23.txt` section 8: "Separate ALUTs,
registers, estimated ALMs, placed ALMs and capacity bounds. Do not add parent
and child resource totals or add register and logic bounds as though ALMs could
not share them."

So the five are emitted as five named fields and never summed:

  * combinationalALUTs   -- ALUTs, from Analysis & Synthesis
  * dedicatedRegisters   -- registers, ditto
  * estimatedALMs        -- the MAP's ESTIMATE. Not a placement.
  * placedALMs           -- `null` after a map. A map does not place, and
                            writing the estimate here would be the exact
                            conflation the directive forbids.
  * blockMemoryBits / ramSummary -- the memory question, which is the one a
                            `-MapOnly` run is for.

AND IT KEEPS THE RAM SUMMARY VERBATIM, because that table NAMES each array that
inferred and how it was packed. Section 8 again: "exact M10K counts require
legal width/depth/port packing, not payload bits divided by 10,240." A bit
count cannot be divided into blocks after the fact; the packing has to be read
off the tool that chose it.

READ THE SILENCE THE RIGHT WAY ROUND. Quartus prints "uninferred due to ..."
when it CONSIDERED an array and refused. It prints NOTHING when the array never
presented as a RAM candidate at all -- which is the commoner and worse case.
So this tool records `ramSummaryPresent: false` explicitly rather than emitting
an empty list, because an absent table and an empty one are different findings.
"""
import argparse
import io
import json
import os
import re
import sys

# `; Label ; Value ;` rows in Quartus' box-drawing tables.
ROW = re.compile(r"^;\s*(.+?)\s*;\s*(.+?)\s*;\s*$")

WANTED = {
    "Estimate of Logic utilization (ALMs needed)": "estimatedALMs",
    "Combinational ALUT usage for logic": "combinationalALUTs",
    "Total registers": "dedicatedRegisters",
    "Total block memory bits": "blockMemoryBits",
    "Total DSP Blocks": "dspBlocks",
    "Total virtual pins": "virtualPins",
    "Total pins": "physicalPins",
    "Family": "family",
    "Device": "device",
    "Top-level Entity Name": "topLevelEntity",
    "Quartus Prime Version": "toolVersion",
}


def to_num(s):
    t = s.replace(",", "").strip()
    if re.fullmatch(r"-?\d+", t):
        return int(t)
    return s.strip()


def parse(path):
    with io.open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    out = {}
    for line in text.splitlines():
        m = ROW.match(line)
        if not m:
            continue
        label, value = m.group(1), m.group(2)
        key = WANTED.get(label)
        # First occurrence wins: the summary table comes before the per-entity
        # breakdown, and a later hierarchy row with the same label would
        # otherwise overwrite the whole-design number with one child's.
        if key and key not in out:
            out[key] = to_num(value)

    # The RAM summary, verbatim, so the PACKING is preserved and not re-derived.
    #
    # ANCHOR ON THE TABLE'S COLUMN HEADER, NOT ON ITS TITLE. The first version
    # of this matched the string "RAM Summary" and found the table of CONTENTS
    # ("8. Analysis & Synthesis RAM Summary") twenty lines into the file, then
    # collected whatever `;`-rows came next -- and emitted "; Legal Notice ;"
    # as the packing. It was caught only because the value was printed and read.
    # A tool that reports a plausible wrong row is this repo's broken-instrument
    # law wearing the receipt's clothes, so the anchor is now the header row
    # that only the real table has, and there is a self-check below.
    lines = text.splitlines()
    hdr = re.compile(r"^;\s*Name\s*;\s*Type\s*;\s*Mode\s*;")
    ram = []
    present = False
    for i, line in enumerate(lines):
        if not hdr.match(line):
            continue
        present = True
        for l in lines[i + 1:]:
            if l.startswith("+"):
                if ram:
                    break
                continue
            if l.startswith(";"):
                ram.append(l.rstrip())
            if len(ram) > 400:
                break
        break
    out["ramSummaryPresent"] = present
    out["ramSummary"] = ram

    # SELF-CHECK: a present table whose rows name no memory primitive means the
    # anchor slipped again. Fail loudly rather than emit a reassuring receipt.
    if present and not any(
            k in r for r in ram for k in ("M10K", "MLAB", "M20K", "M9K", "LUTRAM")):
        raise SystemExit(
            "extract_map_receipt: RAM summary matched but no row names a memory "
            "primitive -- the anchor slipped. Rows: %r" % (ram[:5],))

    # Every "uninferred" complaint Quartus made, which is the OTHER half: a
    # refusal it explains is a different finding from an array it never looked
    # at, and only one of the two produces text.
    out["uninferredNotes"] = sorted(set(
        l.strip() for l in text.splitlines()
        if "uninferred" in l.lower() or "cannot regroup" in l.lower()))[:40]

    # A map does not place. This is null on purpose; see the header.
    out["placedALMs"] = None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rpt", action="append", required=True,
                    metavar="LABEL=PATH",
                    help="a map report to read, named, e.g. before=path.map.rpt")
    ap.add_argument("--out", required=True, help="the committed JSON receipt")
    ap.add_argument("--note", default="", help="one line describing the pair")
    a = ap.parse_args()

    rows = {}
    for spec in a.rpt:
        if "=" not in spec:
            sys.stderr.write("--rpt needs LABEL=PATH, got %r\n" % spec)
            return 2
        label, path = spec.split("=", 1)
        if not os.path.isfile(path):
            sys.stderr.write("no such report: %s\n" % path)
            return 2
        rows[label] = parse(path)
        print("read %-8s %s" % (label, path))

    doc = {
        "schema": "zhao_map_receipt/1",
        "note": a.note,
        "capacityBound": {
            "device": "5CSEBA6U23I7",
            "alms": 41910,
            "m10k": 553,
            "dsp": 112,
            "note": "the shipping target; a leaf map's utilisation is not a "
                    "closure claim and these are recorded as the BOUND only",
        },
        "rows": rows,
    }
    with io.open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(doc, indent=2) + "\n")
    print("wrote %s" % a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
