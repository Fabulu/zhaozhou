#!/usr/bin/env python3
"""Separate a leaf fit's INTERNAL timing from its virtual-pin boundary.

    python3 tools/quartus/internal_paths.py [reports/synthesis/blockpaths]

---------------------------------------------------------------------------
WHY THIS EXISTS
---------------------------------------------------------------------------
`run_block_fit.ps1` fits one leaf with every port a VIRTUAL PIN, and its SDC
declares `set_input_delay 0` / `set_output_delay 0` -- a "same clock, no
external budget" model. Its own comment is honest about what that is:

    optimistic about inter-block routing and exact about the logic inside
    the block

The optimism is real and so is the exactness. What the comment does not say is
that the model can go the OTHER way, and on this island it did. From
`zhao_texture_aux_pipe` after its input boundary was registered:

    From Node  req_wx_i[3]        (Type iExt -- an external input)
    To Node    u_div|LessThan0~26
    Data path  18.944 ns, of which the FIRST INTERCONNECT HOP IS 9.985 ns

Nine hundred picoseconds short of the entire clock period, in one wire, before
any logic. That is not the block's arithmetic. It is a virtual pin the fitter
placed wherever it liked, in a design with no neighbours to place it near. In
the composed machine that signal arrives from an adjacent block's register.

So a leaf's reported Fmax can be set by a hop that will not exist. The fix is
NOT to stop constraining the boundary -- unconstrained ports are how this lane
previously reported 195 MHz for a block whose arithmetic ran at 48.9. The fix
is to report BOTH numbers and say which is which.

---------------------------------------------------------------------------
WHAT IT MEASURES
---------------------------------------------------------------------------
Every path in a `.setup.rpt` is classified from the report's OWN path-type
markers rather than by matching node names, which is the mistake `fit_evidence`
already made once and had corrected:

    iExt in the arrival path   -> launched by an external input
    oExt anywhere in the path  -> captured by an external output
    neither                    -> INTERNAL, register to register

The internal worst slack is the block's own logic. The boundary worst slack is
the boundary. Reporting one without the other is how a block gets rewritten for
a wire, or shipped because a wire hid its arithmetic.

This is a COMPARISON-side tool. It reads archived reports and re-fits nothing,
so every number it prints can be recomputed from what is already committed.
"""
import io
import os
import re
import sys

PERIOD_NS = 10.0


PORT_RE = re.compile(r"^\s*(?:input|output|inout)\s+(?:var\s+)?(?:logic|wire|reg)?\s*"
                     r"(?:signed\s*)?(?:\[[^\]]*\]\s*)*([A-Za-z_]\w*)", re.M)


def module_ports(module, rtl_root="fpga/rtl"):
    """Top-level port names of `module`, from its own .sv.

    The summary report gives node names and no path types, so a summary row is
    classified by whether its endpoint IS a port. That is a name match, which
    is the classification `fit_evidence` once got wrong by matching substrings
    -- so this matches the WHOLE identifier, with an optional bit index, and
    nothing else.
    """
    base = module.split("@")[0]
    for suffix in ("", "seed2", "seed3"):
        if suffix and base.endswith(suffix):
            base = base[: -len(suffix)]
            break
    for dirpath, _dirs, files in os.walk(rtl_root):
        if base + ".sv" in files:
            txt = io.open(os.path.join(dirpath, base + ".sv"),
                          encoding="utf-8", errors="replace").read()
            # Anchor at the MODULE, not at the start of the file. Cutting at
            # the first ");" found anywhere returned ZERO ports for
            # zhao_texture_tmu_plan, because its header comment quotes a line
            # of SystemVerilog that contains one -- and a classifier that
            # silently finds no ports calls every path internal.
            mstart = txt.find("module " + base)
            if mstart < 0:
                mstart = 0
            tail = txt[mstart:]
            end = tail.find(");")
            head = tail[:end] if end > 0 else tail
            return set(PORT_RE.findall(head))
    return set()


def strip_index(node):
    """`req_wx_i[3]` -> `req_wx_i`; leaves hierarchical names alone."""
    n = node.split("|")[-1]
    if "[" in n:
        n = n[: n.index("[")]
    return n


def parse_summary(path, ports):
    """Classify the 2000-path summary rows by whether an endpoint is a port."""
    internal, inb, outb = [], [], []
    for line in io.open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"^;\s*(-?[\d.]+)\s*;\s*([^;]+?)\s*;\s*([^;]+?)\s*;", line)
        if not m:
            continue
        try:
            slack = float(m.group(1))
        except ValueError:
            continue
        frm, to = m.group(2).strip(), m.group(3).strip()
        rec = (slack, frm, to)
        if strip_index(to) in ports:
            outb.append(rec)
        elif strip_index(frm) in ports:
            inb.append(rec)
        else:
            internal.append(rec)
    return internal, inb, outb


def parse_report(path):
    """Return (internal, in_boundary, out_boundary) lists of (slack, frm, to)."""
    text = io.open(path, encoding="utf-8", errors="replace").read()
    # Each detailed path begins with "Path #N:" and carries its own summary.
    chunks = re.split(r"\nPath #\d+:", text)[1:]
    internal, inb, outb = [], [], []
    for c in chunks:
        m = re.search(r"Setup slack is (-?[\d.]+)", c)
        if not m:
            continue
        slack = float(m.group(1))
        frm = re.search(r"; From Node\s+;\s*([^\s;]+)", c)
        to = re.search(r"; To Node\s+;\s*([^\s;]+)", c)
        frm = frm.group(1) if frm else "?"
        to = to.group(1) if to else "?"
        rec = (slack, frm, to)
        if "oExt" in c:
            outb.append(rec)
        elif "iExt" in c:
            inb.append(rec)
        else:
            internal.append(rec)
    return internal, inb, outb


def fmax(slack):
    """Fmax implied by a worst slack against the constrained period."""
    if slack is None:
        return None
    return 1000.0 / (PERIOD_NS - slack)


def main(argv):
    d = argv[1] if len(argv) > 1 else "reports/synthesis/blockpaths"
    files = sorted(f for f in os.listdir(d)
                if f.endswith(".setup.rpt") and not f.endswith(".setup.summary.rpt"))
    if not files:
        print("no .setup.rpt found in " + d)
        return 1

    rows = []
    for f in files:
        mod = f[: -len(".setup.rpt")]
        # Prefer the 2000-path SUMMARY when one exists: the 200-path detailed
        # report is often entirely boundary for a leaf with many virtual pins,
        # and an absent internal path is not a fast internal path.
        summary = os.path.join(d, mod + ".setup.summary.rpt")
        if os.path.exists(summary):
            ports = module_ports(mod)
            if not ports:
                # A classifier that finds no ports calls EVERY path internal,
                # which is the most flattering possible answer and completely
                # wrong. Refuse rather than report it.
                print("%-34s   PORT LIST NOT FOUND -- refusing to classify. "
                      "Every path would read as internal." % mod)
                continue
            internal, inb, outb = parse_summary(summary, ports)
        else:
            internal, inb, outb = parse_report(os.path.join(d, f))
        wi = min((x[0] for x in internal), default=None)
        wb = min((x[0] for x in inb + outb), default=None)
        rows.append((mod, wi, wb, len(internal), len(inb), len(outb),
                     min(internal)[1:] if internal else ("-", "-")))

    print("%-34s %9s %9s %9s %9s   %s"
          % ("module", "internal", "boundary", "int Fmax", "rpt Fmax", "worst internal path"))
    print("-" * 130)
    for mod, wi, wb, ni, nin, nout, worst in sorted(
            rows, key=lambda r: (r[1] is None, r[1] if r[1] is not None else 0)):
        fi = fmax(wi)
        fb = fmax(wb)
        rpt = min(x for x in (wi, wb) if x is not None) if (wi is not None or wb is not None) else None
        print("%-34s %9s %9s %9s %9s   %s -> %s"
              % (mod,
                 ("%.3f" % wi) if wi is not None else "none",
                 ("%.3f" % wb) if wb is not None else "none",
                 ("%.2f" % fi) if fi is not None else "-",
                 ("%.2f" % fmax(rpt)) if rpt is not None else "-",
                 worst[0], worst[1]))
        if ni == 0:
            print("%-34s   NO INTERNAL PATH IN THE TOP 200. Every timed path in this"
                  " report touches a port," % "")
            print("%-34s   so the reported Fmax says nothing about the block's own"
                  " logic." % "")
    print()
    print("internal = worst register-to-register slack, ns, against a %.1f ns period"
          % PERIOD_NS)
    print("boundary = worst slack on a path launched by an input or captured by an output")
    print("A report only samples its top 200 paths, so 'none' means no internal path was")
    print("among the worst 200 -- not that the block has none.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
