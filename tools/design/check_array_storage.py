#!/usr/bin/env python3
r"""check_array_storage.py -- which declared arrays did NOT become memory?

WHY THIS EXISTS (D19m)
----------------------
`zhao_texture_tmu_pipe` declares

    logic [255:0] pal_val_r [PAL_SLOTS];        //  16 x 256      =  4,096 bits
    logic [15:0]  pal_dat_r [PAL_SLOTS][256];   //  16 x 256 x 16 = 65,536 bits

and synthesised to **72,824 registers against 256 block-memory bits**. Nearly
70 Kbit of palette cache went into flip-flops -- about 87% of every register on
the device -- where as memory it is 7 M10K out of 553.

That was found by reading one fit log. The same question can be asked of every
block from the SOURCE, against the fit results already committed, without
running Quartus at all:

    declared array bits  >>  measured blockMemoryBits   ->  it is in flops

WHAT IT IS AND IS NOT
---------------------
It is a COMPARISON-SIDE tool. It reads what was authored and what was measured
and reports the gap. It never edits RTL, never edits `fit_targets.yml` (the fit
queue polls that file and a non-atomic rewrite of it has broken a run before),
and never launches a fit.

It is a HEURISTIC in one specific way: **not every array should be a memory.**
A four-entry pipeline register file, a small lookup indexed combinationally on
several ports, a shift register -- all are correctly flops, and all appear here
as a gap. So the output is sorted by size and the threshold is stated, because
the interesting cases are large and the small ones are noise.

It also cannot see through parameters it cannot resolve. `logic [W-1:0] x [N]`
with N set by a `parameter` is read when the parameter has a literal default in
the same file, and skipped otherwise -- skipped, not guessed. A guessed width
would be a measurement that decides a value, which this repository does not do.

HOW WELL IT ACTUALLY PREDICTS, MEASURED
---------------------------------------
Checked against blocks whose answer is already known, in both directions --
because a detector that only ever fires is as useless as one that never does.

    block                     declared    measured        verdict
    zhao_texture_tmu_pipe       72,544    72,824 REGISTERS  flagged  (0.4% off)
    zhao_texture_fragrob         6,608     6,464 MEM BITS   silent   (2% off)
    zhao_texture_cache           2,304     8,192 MEM BITS   silent
    zhao_field_v2_front          2,688   266,513 MEM BITS   silent

The two that matter are the first two. `tmu_pipe` put its arrays in flops and
the declared total predicts the REGISTER count to 0.4%. `fragrob` put its arrays
in memory and the same declared total predicts the MEMORY BIT count to 2%. The
arithmetic is reading real storage in both cases; only its destination differs,
which is exactly the question this tool asks.

**AND IT MUST NOT COMPARE ACROSS TIME.** The first version reported
`zhao_texture_tmu` as 9,762 declared bits against 0 memory and only 350
registers, with a note guessing "deleted by a leaf fit whose outputs are
unconnected". That was wrong twice over: the block's current source reads its
array straight into a flop -- the shape that DOES infer -- and the row it was
compared against was measured 2026-08-23 against source last changed 2026-08-30.

**Declared bits come from today's file; measured bits come from whenever the fit
ran.** When those differ, the subtraction is not a weaker check, it is a
different question. Rows older than their source are now reported separately and
never as gaps.

I walked into that trap while writing this tool, and the trap is filed as D19o
-- which was itself found from this tool's output. A finding and its own
counterexample can come from the same afternoon.

**Its weakness is UNDER-counting**, visible in `zhao_field_v2_front`: 2,688
declared against 266,513 measured, because most of that block's arrays are sized
by expressions this cannot resolve and are skipped rather than guessed. That is
the safe direction -- it misses defects, it does not invent them -- but it means
a small declared total is not evidence of a small block. Read the `skipped`
count in the header line.

READ IT WITH THE FIT ROW, NOT INSTEAD OF IT
--------------------------------------------
A gap here is a QUESTION. The answer is in the fit: a block with 13 M10K and
6 Kbit measured (`zhao_texture_fragrob`) inferred its arrays fine even though
they are multidimensional, which is why the multidimensional-array folklore is
not a sufficient explanation on its own. Being multidimensional AND large AND
indexed on both axes is what actually broke inference in tmu_pipe.
"""
from __future__ import annotations

import io
import json
import os
import re
import subprocess
import sys

RESULTS = "reports/synthesis/zhao_block_fit.json"

# `logic [15:0] name [A][B];` / `logic [255:0] name [N];` / `logic name [0:3];`
ARRAY_RE = re.compile(
    r"^\s*(?:var\s+)?(?:logic|reg|bit)\s*"
    r"(?:signed\s*)?(\[[^\];]*\]\s*)?"            # packed width, optional
    r"(\w+)\s*"                                   # name
    r"((?:\[[^\];]*\]\s*)+);",                    # one or more unpacked dims
    re.M,   # `^` must mean start-of-LINE. Without this the pattern matches only
            # at offset 0 of the file and the tool reports a confident ZERO --
            # which is how the first run of this file found nothing at all, on
            # a tree containing the 65,536-bit array it was written to catch.
)

PARAM_RE = re.compile(r"^\s*(?:parameter|localparam)\s+(?:int\s+)?(?:unsigned\s+)?"
                      r"(?:int\s+)?(\w+)\s*=\s*(\d+)")


# Never ship a detector that has not been shown to fire. This is the exact shape
# from D19m; if the pattern stops matching it, this tool is broken, not the tree.
assert ARRAY_RE.search("  logic [15:0]  pal_dat_r [PAL_SLOTS][256];"), (
    "ARRAY_RE no longer matches the declaration this tool exists to find")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def _git_date(*args):
    r = subprocess.run(["git", "log", "-1", "--format=%cI"] + list(args),
                       capture_output=True, text=True)
    return r.stdout.strip() or None


def row_predates_source(row, path):
    """Is the measurement OLDER than the file it is being compared against?

    This tool compares bits declared in TODAY'S source against memory measured
    in some past fit. When the source has changed since, that comparison is
    meaningless -- and it produces confident nonsense: `zhao_texture_tmu` was
    reported as having 9,762 declared bits against 0 memory and only 350
    registers, and the note attached to it guessed "deleted by a leaf fit".
    Wrong. The row was measured 2026-08-23 against source last changed
    2026-08-30, and the current code reads the array straight into a flop --
    the shape that DOES infer (QUARTUS_GOTCHAS 14).

    I walked into exactly the trap that is filed as D19o, while writing the
    tool whose output produced D19o. Comparing a current file to a stale
    measurement is not a weaker version of the check; it is a different
    question with the same shape.
    """
    measured = _git_date(row.get("sourceCommit") or "HEAD")
    changed = _git_date("--", path)
    return bool(measured and changed and changed > measured)


def dim_size(expr, params):
    """Elements in one `[...]` dimension. `[N]` is N, `[a:b]` is |a-b|+1.

    Returns None when the expression uses anything this cannot resolve -- an
    unknown identifier, arithmetic, a function call. None means SKIP, never a
    guess: a tool that invents a size reports a defect that may not exist.
    """
    e = expr.strip()[1:-1].strip()
    if ":" in e:
        a, b = e.split(":", 1)
        va, vb = resolve(a, params), resolve(b, params)
        if va is None or vb is None:
            return None
        return abs(va - vb) + 1
    return resolve(e, params)


def resolve(tok, params):
    t = tok.strip()
    if re.fullmatch(r"\d+", t):
        return int(t)
    if t in params:
        return params[t]
    m = re.fullmatch(r"(\w+)\s*-\s*1", t)
    if m:
        v = params.get(m.group(1))
        return None if v is None else v - 1
    return None


def bits_of(path):
    """Total declared bits across every resolvable unpacked array in the file."""
    s = read(path)
    params = {}
    for line in s.splitlines():
        m = PARAM_RE.match(line)
        if m:
            params[m.group(1)] = int(m.group(2))

    total, biggest, skipped = 0, [], 0
    for m in ARRAY_RE.finditer(s):
        packed, name, unpacked = m.group(1), m.group(2), m.group(3)
        w = 1
        if packed:
            w = dim_size(packed, params)
            if w is None:
                skipped += 1
                continue
        n = 1
        ok = True
        for d in re.findall(r"\[[^\]]*\]", unpacked):
            v = dim_size(d, params)
            if v is None:
                ok = False
                break
            n *= v
        if not ok:
            skipped += 1
            continue
        b = w * n
        total += b
        biggest.append((b, name, "%s%s" % (packed or "", unpacked.strip())))
    biggest.sort(reverse=True)
    return total, biggest[:3], skipped


def main() -> int:
    thresh = 8192
    for a in sys.argv[1:]:
        if a.startswith("--min="):
            thresh = int(a.split("=", 1)[1])

    rows = {}
    if os.path.exists(RESULTS):
        d = json.loads(read(RESULTS))
        rs = d if isinstance(d, list) else d.get("blocks", d)
        if isinstance(rs, dict):
            rs = list(rs.values())
        rows = {r.get("module"): r for r in rs if r.get("module")}

    found, unmeasured, outdated, skipped_total = [], [], [], 0
    for root, _dirs, files in os.walk("fpga/rtl"):
        for f in files:
            if not f.endswith(".sv"):
                continue
            mod = f[:-3]
            total, biggest, skipped = bits_of(os.path.join(root, f))
            skipped_total += skipped
            if total < thresh:
                continue
            row = rows.get(mod)
            if row is None:
                unmeasured.append((total, mod, biggest))
                continue
            mem, reg = row.get("blockMemoryBits"), row.get("registers")
            # A row that TIMED OUT or errored carries no numbers. Reading those
            # as zero turns "not measured" into "zero memory" and invents a
            # gap -- `zhao_forge_cliff` (status: timeout) was reported that way,
            # with 119,808 declared bits against 0 memory AND 0 registers, a
            # combination no real block can have.
            if mem is None or reg is None:
                unmeasured.append((total, mod, biggest))
                continue
            # A measurement older than the file it is compared against answers
            # a different question. See row_predates_source().
            if row_predates_source(row, os.path.join(root, f)):
                outdated.append((total, mod, biggest))
                continue
            if mem < total // 2:
                # A leaf fit with unconnected outputs lets synthesis DELETE
                # storage nothing observes, so few registers and no memory can
                # mean "optimised away" rather than "in flops".
                # `zhao_texture_tmu`: 9,762 declared bits, 350 registers, 0
                # memory -- 350 flops cannot hold 9,762 bits, so the array is
                # not there at all. Flagged, with the reason named.
                gone = reg * 2 < total // 2
                found.append((total - mem, total, mem, reg, mod, biggest, gone))

    found.sort(reverse=True)
    unmeasured.sort(reverse=True)

    outdated.sort(reverse=True)
    print("array storage: %d measured block(s) declare >= %d array bits with "
          "less than half of it in block memory; %d further block(s) declare "
          "that much but have no fit row yet; %d whose row PREDATES the source "
          "and cannot be compared; %d declaration(s) skipped as unresolvable"
          % (len(found), thresh, len(unmeasured), len(outdated), skipped_total))

    if found:
        print("\nDECLARED BUT NOT IN MEMORY -- largest gap first. A gap is a "
              "QUESTION, not a verdict: a small array indexed on many ports is "
              "correctly flops. Read each against its fit row:")
        for gap, total, mem, reg, mod, biggest, gone in found:
            note = ("   <- too few registers to HOLD it either: likely deleted "
                    "by a leaf fit whose outputs are unconnected" if gone else "")
            print("  %-34s declared %8d bits, memory %7d, registers %6d%s"
                  % (mod, total, mem, reg, note))
            for b, name, shape in biggest:
                print("        %8d bits  %-22s %s" % (b, name, shape))

    if unmeasured:
        print("\nNO FIT ROW (%d) -- declares the bits, nothing has measured "
              "where they went:" % len(unmeasured))
        for total, mod, biggest in unmeasured[:10]:
            print("  %-34s declared %8d bits   largest: %s"
                  % (mod, total, biggest[0][1] if biggest else "-"))

    print("\nNOTE: heuristic. Not every array should be a memory, and a "
          "declaration whose size cannot be resolved from literals and "
          "same-file parameter defaults is SKIPPED rather than guessed. "
          "REPORTS; never gates, never edits RTL or fit_targets.yml.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
