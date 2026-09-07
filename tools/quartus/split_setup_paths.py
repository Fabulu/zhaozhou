#!/usr/bin/env python3
"""split_setup_paths.py -- what is a block's Fmax actually limited BY?

WHY THIS EXISTS
---------------
`reports/synthesis/zhao_block_fit.json` records one `fmaxMhz` per block. Read
straight, it says 43 of the 51 rows that have one MISS THE 100 MHz PRODUCT
CLOCK, several of them by a factor of three. Read straight, it is also close to
useless, because a leaf fit wraps its block in VIRTUAL PINS and the reported
number is whatever the single worst path says -- including paths that begin or
end at an imaginary pad with an imaginary clock network attached to it.

CLAUDE.md has this exact lesson from the composed island, where every one of the
twelve worst paths appeared to start at a virtual pin and the comfortable
conclusion was "measurement artefact". Splitting all 2,000 summarised paths by
ORIGIN showed 1,595 started inside the design. The artefact was real and almost
irrelevant. This tool is that split, written down so it stops being redone by
hand and thrown away.

WHAT IT SPLITS, AND WHY THREE WAYS AND NOT TWO
----------------------------------------------
Each summarised path is classified by BOTH endpoints:

  * `port`  -- a top-level pin of the block under fit. In a leaf fit this is
               virtual: it has routing and clock skew that no assembled design
               will ever have. Reported skew on these runs to -6.3 ns against
               -0.5 ns internally.
  * `reset` -- launched from `rst_n`. A recovery/removal path, not a data path.
               It is a real constraint but a DIFFERENT one, with a different
               fix (synchronise the deassertion, or constrain it), and mixing
               it into the data number hides both.
  * `core`  -- everything else, including anything hierarchical (`a:b|c`),
               which is by construction inside a submodule.

The number that matters is the worst path with `core` at BOTH ends. Nothing
about the leaf boundary can excuse it.

THIS TOOL CHANGED ITS OWN ANSWER TWICE WHILE BEING WRITTEN, which is the whole
argument for it existing:

  * ends-only split: 7 blocks looked limited by an internal path;
  * both-ends split: `zhao_texture_aux_pipe` jumped 63.63 -> 120.37 MHz, because
    its worst path STARTED at an input pin and ended in a submodule;
  * reset split: `zhao_texture_aux_div6` went 87.45 -> 103.00, `rcp24_v3`
    90.54 -> 129.18, because their worst non-boundary path was launched from
    `rst_n`.

A single-number `fmaxMhz` cannot say any of that.

WHAT IT DOES NOT DO
-------------------
It reads the SUMMARY reports, which hold the top ~2,000 paths. A block whose
summary is entirely boundary paths reports `--` for the data column rather than
a made-up number: this tool could not answer, which is not the same as the
answer being good. It REPORTS; it does not gate.
"""

from __future__ import annotations

import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKPATHS = os.path.join(REPO, "reports", "synthesis", "blockpaths")

ROW_RE = re.compile(r"^;\s*(-?[\d.]+)\s*;\s*(\S.*?)\s*;\s*(\S.*?)\s*;\s*\S+\s*;")


def classify(node: str) -> str:
    """port | reset | core -- see the module docstring."""
    if "|" in node:
        return "core"  # hierarchical name: inside a submodule, by construction
    if node == "clk" or node == "rst_n" or node.startswith("rst_n") or node.startswith("clk"):
        return "reset"
    if re.search(r"_(o|i)(\[|\.|~|$)", node) or node.endswith("_o") or node.endswith("_i"):
        return "port"
    return "core"


# A DETECTOR THAT HAS NOT BEEN SHOWN TO FIRE HAS NOT BEEN TESTED (CLAUDE.md).
# These assert at import, against real node names taken from the reports, so a
# classifier that silently stops matching cannot print a reassuring table.
assert classify("v_base_o[7]") == "port", "port: bare output bit"
assert classify("guard_req_o.addr[26]") == "port", "port: struct field"
assert classify("req_wx_i[3]") == "port", "port: input bit"
assert classify("rst_n") == "reset", "reset: bare"
assert classify("zhao_texture_aux_div6:u_div|LessThan0~26") == "core", "core: hierarchical"
assert classify("Add46~41_OTERM2248_OTERM2772") == "core", "core: synthesised term"
assert classify("buf_q[1][334]") == "core", "core: array register"
assert classify("s0_set[6]~DUPLICATE") == "core", "core: duplicated register"


def fmax(slack: float, period_ns: float) -> float:
    return 1000.0 / (period_ns - slack)


def read_rows(path: str) -> list[tuple[float, str, str]]:
    out: list[tuple[float, str, str]] = []
    for line in io.open(path, encoding="utf-8", errors="replace"):
        m = ROW_RE.match(line)
        if not m:
            continue
        try:
            out.append((float(m.group(1)), m.group(2).strip(), m.group(3).strip()))
        except ValueError:
            pass
    return out


def main(argv: list[str]) -> int:
    period = 10.0  # the 100 MHz product clock; every block fit is constrained to it
    files = sorted(glob.glob(os.path.join(BLOCKPATHS, "*.setup.summary.rpt")))
    if not files:
        print("no *.setup.summary.rpt under %s" % BLOCKPATHS)
        return 0

    results = []
    for f in files:
        name = os.path.basename(f).replace(".setup.summary.rpt", "")
        rows = read_rows(f)
        if not rows:
            continue
        data = [r for r in rows if classify(r[1]) == "core" and classify(r[2]) == "core"]
        rst = [r for r in rows if classify(r[1]) == "reset"]
        worst_all = min(r[0] for r in rows)
        results.append({
            "name": name,
            "reported": fmax(worst_all, period),
            "data": fmax(min(r[0] for r in data), period) if data else None,
            "reset": fmax(min(r[0] for r in rst), period) if rst else None,
            "worst": min(data) if data else None,
            "n": len(rows),
            "n_data": len(data),
        })

    results.sort(key=lambda r: (r["data"] is None, r["data"] or 0.0))

    print("setup paths split by endpoint class, %.0f MHz product clock, %d block(s)"
          % (1000.0 / period, len(results)))
    print()
    print("%-36s %9s %9s %9s   %s" % ("block", "reported", "DATA", "reset", "worst core->core path"))
    print("-" * 116)
    short = []
    for r in results:
        d = "%9.2f" % r["data"] if r["data"] else "       --"
        s = "%9.2f" % r["reset"] if r["reset"] else "       --"
        w = "%s -> %s" % (r["worst"][1][:24], r["worst"][2][:24]) if r["worst"] else ""
        flag = ""
        if r["data"] and r["data"] < 1000.0 / period:
            flag = "  <-- BELOW THE PRODUCT CLOCK"
            short.append(r)
        print("%-36s %9.2f %s %s   %s%s" % (r["name"][:36], r["reported"], d, s, w, flag))

    print()
    print("%d of %d block(s) miss the product clock on a path with NO boundary to blame."
          % (len(short), len(results)))
    for r in short:
        print("   %-36s %6.2f MHz  (reported %6.2f)" % (r["name"][:36], r["data"], r["reported"]))
    print()
    print("NOTE: this REPORTS, it does not gate. `--` in the DATA column means every")
    print("summarised path touched the boundary or reset, so this tool could not answer --")
    print("which is a different thing from the answer being good.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
