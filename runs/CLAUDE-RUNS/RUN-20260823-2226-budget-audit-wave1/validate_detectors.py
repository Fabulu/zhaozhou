#!/usr/bin/env python3
"""Positive controls for scan_rtl.py's detectors.

WHY THIS EXISTS
===============
The full scan reported **zero** mux-before-multiply candidates across all 91
modules.  That is either a real finding -- the one place the docket names
(`zhao_raster_blend`) already carries the fix at HEAD, and its header says so
in capitals -- or a detector that never fires.  A detector that never fires and
a repository with no instances are indistinguishable from the output alone,
and the difference decides whether `RASTER.FRAGMENT`'s predicted 6 -> 3 saving
is already banked or still owed.

So each detector gets a SYNTHETIC POSITIVE it must find and, where the
distinction matters, a synthetic NEGATIVE it must not flag.  This is the same
rule QUARTUS_GOTCHAS states for tool directives -- measure that it did what you
asked -- applied to the audit's own instrument.

It writes nothing into fpga/rtl.  Everything lives in a scratch directory, so
it is safe to run while a Quartus job holds the RTL tree.
"""

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

spec = importlib.util.spec_from_file_location(
    "scan_rtl", os.path.join(REPO, "tools", "budget", "scan_rtl.py"))
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)


CASES = {
    # POSITIVE: two products under mutually exclusive arms of one condition,
    # sharing an operand. This is RASTER.BLEND as its own header describes the
    # form it USED to have -- "this used to compute BOTH unconditionally in two
    # always_comb blocks, so every channel carried two multipliers to use one".
    "tc_mux_positive": """
module tc_mux_positive (
    input  logic signed [17:0] src_i,
    input  logic signed [17:0] dst_i,
    input  logic signed [17:0] a_i,
    input  logic               mode_i,
    output logic signed [17:0] y_o
);
  logic signed [17:0] p_alpha, p_addmod;
  always_comb p_alpha  = (src_i - dst_i) * a_i;
  always_comb p_addmod = src_i * a_i;
  assign y_o = mode_i ? p_alpha : p_addmod;
endmodule
""",
    # NEGATIVE: the SHIPPING form -- one product, left operand muxed first.
    # Must NOT be flagged, or the detector would demand a fix that is done.
    "tc_mux_negative": """
module tc_mux_negative (
    input  logic signed [17:0] src_i,
    input  logic signed [17:0] dst_i,
    input  logic signed [17:0] a_i,
    input  logic               mode_i,
    output logic signed [17:0] y_o
);
  logic signed [17:0] mul_left;
  always_comb mul_left = mode_i ? (src_i - dst_i) : src_i;
  assign y_o = mul_left * a_i;
endmodule
""",
    # POSITIVE: a function called several times is several multipliers.
    "tc_func_instances": """
module tc_func_instances (
    input  logic signed [31:0] a_i, b_i, c_i,
    output logic signed [63:0] y_o
);
  function automatic logic signed [63:0] m32(input logic signed [31:0] x,
                                             input logic signed [31:0] y);
    m32 = $signed({{32{x[31]}}, x}) * $signed({{32{y[31]}}, y});
  endfunction
  assign y_o = m32(a_i, b_i) + m32(b_i, c_i) + m32(a_i, c_i);
endmodule
""",
    # POSITIVE: async-read addressable storage -- zhao_field_seq's rf.
    "tc_async_ram": """
module tc_async_ram (
    input  logic clk,
    input  logic we_i,
    input  logic [5:0] waddr_i,
    input  logic [31:0] wdata_i,
    input  logic [5:0] raddr_i,
    output logic [31:0] rdata_o
);
  logic [31:0] mem [0:63];
  always_ff @(posedge clk) if (we_i) mem[waddr_i] <= wdata_i;
  assign rdata_o = mem[raddr_i];
endmodule
""",
    # NEGATIVE: the same storage read synchronously. Must NOT be RED.
    "tc_sync_ram": """
module tc_sync_ram (
    input  logic clk,
    input  logic we_i,
    input  logic [5:0] waddr_i,
    input  logic [31:0] wdata_i,
    input  logic [5:0] raddr_i,
    output logic [31:0] rdata_o
);
  logic [31:0] mem [0:63];
  always_ff @(posedge clk) begin
    if (we_i) mem[waddr_i] <= wdata_i;
    rdata_o <= mem[raddr_i];
  end
endmodule
""",
    # POSITIVE: ready gated on one state, five-state walk -- TEXTURE.TMU's shape,
    # written with localparams so the state names vanish at elaboration.
    "tc_idle_ready": """
module tc_idle_ready (
    input  logic clk,
    input  logic rst_n,
    input  logic req_valid_i,
    output logic req_ready_o,
    output logic [2:0] st_o
);
  localparam logic [2:0] A = 3'd0, B = 3'd1, C = 3'd2, D = 3'd3, E = 3'd4;
  logic [2:0] st_r;
  assign req_ready_o = (st_r == A);
  assign st_o = st_r;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) st_r <= A;
    else case (st_r)
      A: if (req_valid_i) st_r <= B;
      B: st_r <= C;
      C: st_r <= D;
      D: st_r <= E;
      default: st_r <= A;
    endcase
  end
endmodule
""",
}

EXPECT = {
    "tc_mux_positive":   ("muxBeforeMultiplyCandidates", lambda v: v >= 1),
    "tc_mux_negative":   ("muxBeforeMultiplyCandidates", lambda v: v == 0),
    "tc_func_instances": ("nonconstantMultiplyInstances", lambda v: v == 3),
    "tc_async_ram":      ("__redArrays", lambda v: v >= 1),
    "tc_sync_ram":       ("__redArrays", lambda v: v == 0),
    "tc_idle_ready":     ("__ii", lambda v: v >= 5),
}


def main():
    work = tempfile.mkdtemp(prefix="zhao-detector-check-")
    ok = True
    results = []
    try:
        srcdir = os.path.join(work, "src")
        os.makedirs(srcdir)
        paths = []
        for name, src in CASES.items():
            p = os.path.join(srcdir, name + ".sv")
            with open(p, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(src.lstrip())
            paths.append(p.replace(os.sep, "/"))

        for name in CASES:
            outdir = os.path.join(work, "ast", name)
            tree, meta, err = S.elaborate(name, outdir, paths)
            if tree is None:
                results.append((name, "ELABORATION FAILED", err[:300], False))
                ok = False
                continue
            root = json.load(open(tree, encoding="utf-8", errors="replace"))
            types = S.Types(root)
            mnode = S.collect_module_nodes(root).get(name)
            rec = S.analyse_module(name, mnode, types, S.read_meta_files(meta))
            key, pred = EXPECT[name]
            if key == "__redArrays":
                val = sum(1 for f in rec["findings"]
                          if f["kind"] == "array" and f["severity"] == "RED")
            elif key == "__ii":
                val = rec["interface"]["inferredMinII"]
            else:
                val = rec["arithmetic"][key]
            good = pred(val)
            ok = ok and good
            results.append((name, key, val, good))
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print("%-22s %-32s %-8s %s" % ("case", "measure", "value", "verdict"))
    for name, key, val, good in results:
        print("%-22s %-32s %-8s %s" % (name, key, val, "PASS" if good else "**FAIL**"))
    print()
    print("ALL PASS" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
