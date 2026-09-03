#!/usr/bin/env python3
"""Will these arrays infer as memory? Answered statically, in a second.

WHY THIS EXISTS
---------------
On 2026-09-03 a texture cache rebuild was reported as a PASS at 98.66 MHz while
9,728 bits of array sat in flip-flops. The cause was one construct: the arrays
were written from an `always_ff @(posedge clk or negedge rst_n)`, and an M10K
has no reset port, so an array touched by an asynchronously-reset process
cannot be one.

It cost an 85-minute fit to find, and the same defect was already documented in
a comment inside the block being replaced -- `zhao_texture_cache.sv:495-523`,
which records the A/B measurement and says the async reset was the blocker.

This check is that knowledge made mechanical. It is not a substitute for the
fit; only the fitter knows what the tool did. It is a way to stop spending an
hour discovering something a grep can see.

WHAT IT LOOKS FOR, and why each one
-----------------------------------
1. AN ARRAY WRITTEN FROM AN ASYNC-RESET PROCESS. The killer above.
2. AN ARRAY READ COMBINATIONALLY through a dynamic index. Forces a per-bit mux
   the width of the array and pins it in flops whatever the writes do. This is
   TEXJOIN's second defect: `srgb_q[head_q][0]` in an `always_comb`.
3. TWO DYNAMIC WRITE ADDRESSES into one array, which the island brief's S5.3
   forbids by name -- it asks for a two-write-port memory, which the device
   does not have at that shape.
4. A MULTIDIMENSIONAL unpacked array, `[LANES][N]`. Added 2026-09-03 after this
   checker called `zhao_texture_cache_pipe` clean and an 88-minute fit came
   back with 2 M10K and 128 memory bits anyway. Synthesis was explicit:

       EDA Netlist Writer cannot regroup multidimensional array "data_r"

   with no "Inferred RAM" line for it at all. Quartus cannot map a memory whose
   OUTER selection is dynamic; it builds a mux across every element of the
   outer dimension and the whole array falls into flip-flops, however correct
   the writes are. The fix is one flat array per lane inside a `generate`, with
   the outer index a genvar.

   This rule is the reason the checker exists at all: it was blind to the exact
   construct that cost the fit it was written to prevent.

It is deliberately CONSERVATIVE about what counts as an array: only unpacked
declarations with a depth, since those are what become memories. A packed
vector is a register file by construction and is not the subject.
"""
import io
import os
import re
import sys

# `logic [W-1:0] name [DEPTH];` or `logic name [A][B];` -- an UNPACKED array.
DECL = re.compile(
    r"^\s*(?:logic|reg|bit)\s*(?:signed\s+)?(?:\[[^\]]*\]\s*)*"
    r"([A-Za-z_]\w*)\s*((?:\[[^\]]*\]\s*)+);", re.M)

PROC = re.compile(r"always_(ff|comb|latch)\s*(?:@\(([^)]*)\))?")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def processes(text):
    """(start, kind, has_async_reset) for every always_* block, in order."""
    out = []
    for m in PROC.finditer(text):
        sens = m.group(2) or ""
        out.append((m.start(), m.group(1), "negedge" in sens or "posedge rst" in sens))
    return out


def owner(procs, pos):
    """Which process contains `pos`? None means module scope."""
    best = None
    for p in procs:
        if p[0] < pos:
            best = p
        else:
            break
    return best


def check_file(path):
    raw = io.open(path, encoding="utf-8", errors="replace").read()
    text = strip_comments(raw)
    procs = processes(text)
    findings = []

    arrays = {}
    for m in DECL.finditer(text):
        name = m.group(1)
        # Only unpacked dimensions with a real depth are memory candidates.
        if m.group(2).strip():
            # Remember the declaration's SPAN, not just its start: the read
            # scan below would otherwise match the declaration itself --
            # `logic arr_q [DEPTH];` looks exactly like a read indexed by
            # `DEPTH`. A tool with obvious false positives teaches people to
            # ignore its real findings.
            arrays[name] = (m.start(), m.end())

    for name in sorted(arrays):
        # Writes: `name [i] <=` or `name [i][j] <=`, whitespace tolerated,
        # because a spaced index is the same construct and missing it would
        # make this check quietly useless.
        wpat = re.compile(r"\b" + name + r"\s*(?:\[[^\]]*\]\s*)+<=")
        writes = [mm.start() for mm in wpat.finditer(text)]
        if not writes:
            continue

        async_writes = 0
        write_addrs = set()
        for w in writes:
            o = owner(procs, w)
            if o and o[1] == "ff" and o[2]:
                async_writes += 1
            frag = text[w:w + 200]
            idx = re.match(r"\b" + name + r"\s*((?:\[[^\]]*\]\s*)+)<=", frag)
            if idx:
                write_addrs.add(idx.group(1).strip())

        if async_writes:
            findings.append(
                (name, "written from an ASYNC-RESET process (%d site%s) -- an "
                       "M10K has no reset port, so this array cannot be one"
                 % (async_writes, "" if async_writes == 1 else "s")))

        # Combinational read through a dynamic index.
        rpat = re.compile(r"\b" + name + r"\s*\[\s*([A-Za-z_]\w*)")
        decl_lo, decl_hi = arrays[name]
        for mm in rpat.finditer(text):
            if decl_lo <= mm.start() < decl_hi:
                continue  # the declaration, not a read
            o = owner(procs, mm.start())
            after = text[mm.start():mm.start() + 300]
            is_write = re.match(r"\b" + name + r"\s*(?:\[[^\]]*\]\s*)+<=", after)
            if is_write:
                continue
            if o and o[1] == "comb":
                findings.append(
                    (name, "read COMBINATIONALLY through dynamic index `%s` -- "
                           "forces a per-bit mux the width of the array"
                     % mm.group(1)))
                break
            if o is None:
                findings.append(
                    (name, "read at MODULE SCOPE through dynamic index `%s` "
                           "(a continuous assignment is combinational)"
                     % mm.group(1)))
                break

        # 4. Multidimensional unpacked array.
        #
        # Read from the DECLARATION rather than from use, because that is where
        # the shape is: `logic [15:0] m [LANES][N];` has two unpacked
        # dimensions and cannot be a memory whatever the accesses look like.
        decl_text = text[arrays[name][0]:arrays[name][1]]
        unpacked_dims = re.findall(r"\[[^\]]*\]", decl_text[decl_text.index(name) + len(name):])
        if len(unpacked_dims) > 1:
            findings.append(
                (name, "MULTIDIMENSIONAL unpacked array %s -- Quartus cannot "
                       "regroup this into a memory; it muxes across the outer "
                       "dimension and the whole array becomes flip-flops. One "
                       "flat array per element inside a generate, outer index a "
                       "genvar." % "".join(unpacked_dims)))

        if len(write_addrs) > 1:
            findings.append(
                (name, "TWO OR MORE distinct write addresses %s -- island brief "
                       "S5.3 forbids this by name; it asks for a memory shape "
                       "the device does not have"
                 % sorted(write_addrs)))

    return findings


def main():
    targets = sys.argv[1:]
    if not targets:
        targets = []
        for root, _d, names in os.walk("fpga/rtl"):
            for n in names:
                if n.endswith(".sv"):
                    targets.append(os.path.join(root, n).replace(os.sep, "/"))

    total = 0
    for path in sorted(targets):
        found = check_file(path)
        if not found:
            continue
        print(path.replace("fpga/rtl/", ""))
        for name, why in found:
            print("    %-16s %s" % (name, why))
        total += len(found)

    print()
    if total:
        print("%d structural finding(s). Not all are defects -- a small array "
              "in flops can be deliberate -- but each one is a reason an array "
              "will NOT become an M10K." % total)
    else:
        print("no structural obstacles to memory inference found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
