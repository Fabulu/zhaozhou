#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_svcpath.sv (FIELD.V3.SVCPATH).

THIS TABLE MUTATES WIRING, ALMOST NOTHING ELSE, AND THAT IS THE POINT
---------------------------------------------------------------------
Every block inside this file is swept on its own and every one of them closed:
DISPATCH 28, NOISE 23, WBARB 17, and the bank before them. Sweeping their
arithmetic again here would re-ask questions that already have answers.

The four defects this project has actually paid for lived in NONE of those
sweeps:

    the executor's open-loop DOT      no mul_ready port to refuse it
    the curve service's hang          no mul_ready port at all
    the dispatcher's missing imm      no port to carry it
    the dispatcher's missing s3       no port to carry it

A sweep cannot mutate a port that does not exist. So the composition is where
the composition's mutants belong: crossed ready lines, a claimant reading its
neighbour's slot, a decode that names the wrong op, a control that never
arrives. Each of those is a one-token edit in a port map and none is visible
from inside either module.

TWO KINDS OF MUTANT ARE HERE ON PURPOSE
---------------------------------------
Most state a defect that must be caught. A few -- V06, V09, V22, V25 -- state
a claim this file MAKES but that no check may currently observe. If they
survive, that is a finding about the test and not a blemish on the table, and
the survivor is either closed or proven equivalent WITH ITS PROOF. Nothing is
declared equivalent before the run.

PREFER MUTATING A VALUE OVER DELETING A USE. Eight preflight refusals across
the dispatch, ring and noise tables came from ignoring that: tying a port to a
constant orphans the signal that fed it, Verilator refuses the file, and an
unbuildable mutant is a DISCARD rather than evidence. Hence flush qualified by
long_valid_i rather than tied low, and a seed perturbed rather than zeroed.

WHAT IS DELIBERATELY NOT MUTATED
--------------------------------
long_s2_i and long_s3_i. The directed test drives both as zero for every
context, so any mutation of them is equivalent BY CONSTRUCTION rather than by
argument -- it would score as a survivor and teach nothing. They are the
dispatcher's to carry and the dispatcher's sweep is where they are checked.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
newlines and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_svcpath.sv"

MUTANTS = [
    # ---- the bank's two slots ----------------------------------------------
    # THE SHAPE OF EVERY DEFECT THIS FILE EXISTS TO CATCH: a claimant reading
    # the wrong index of a vector it shares with somebody else. Verilator is
    # perfectly happy, both signals exist, and the block it lives in cannot
    # see the mistake because the index is chosen HERE.
    ("V01 the service reads the RIVAL's grant",
     "  assign nz_mul_ready  = bank_req_ready[1];",
     "  assign nz_mul_ready  = bank_req_ready[0];"),
    ("V02 the service takes the RIVAL's product",
     "  assign nz_mul_valid  = bank_rsp_valid[1];",
     "  assign nz_mul_valid  = bank_rsp_valid[0];"),
    ("V03 the rival's grant is reported from the service's slot",
     "  assign rival_grant_o = bank_req_ready[0];",
     "  assign rival_grant_o = bank_req_ready[1];"),
    ("V04 the rival's reply is read from the service's slot",
     "  assign rival_rsp_o   = bank_rsp_valid[0];",
     "  assign rival_rsp_o   = bank_rsp_valid[1];"),
    ("V05 the two bank claimants' request-valid lines are crossed",
     "    bank_req_valid[0] = rival_req_i;\n"
     "    bank_req_valid[1] = nz_mul_issue;",
     "    bank_req_valid[0] = nz_mul_issue;\n"
     "    bank_req_valid[1] = rival_req_i;"),
    # The bank's fixed priority is a REQUIREMENT where it is defined. Whether
    # this composition's test can see it is a different question, and one this
    # mutant asks rather than assumes.
    ("V06 the bank puts the LANES above the service",
     "      .CLAIMANTS(2), .PRIO_SERVICES_FIRST(1'b1), .TAGW(TAGW)",
     "      .CLAIMANTS(2), .PRIO_SERVICES_FIRST(1'b0), .TAGW(TAGW)"),
    ("V07 the service's A operands are broadcast from point 0",
     "      bank_a[1][l] = nz_a[l];",
     "      bank_a[1][l] = nz_a[0];"),
    ("V08 the service's B operand is its own A",
     "      bank_b[1][l] = nz_b[l];",
     "      bank_b[1][l] = nz_a[l];"),
    ("V09 both bank claimants carry the same tag",
     "    bank_tag[1] = 8'd1;",
     "    bank_tag[1] = 8'd0;"),

    # ---- the write port's two slots ----------------------------------------
    ("V10 the ALU's ready comes from the drain's slot",
     "  assign alu_wb_ready_o = wb_req_ready[0];",
     "  assign alu_wb_ready_o = wb_req_ready[1];"),
    ("V11 the drain's ready comes from the ALU's slot",
     "  assign drain_ready    = wb_req_ready[1];",
     "  assign drain_ready    = wb_req_ready[0];"),
    ("V12 the drain's write carries the ALU's data",
     "    wb_data[1] = drain_data;",
     "    wb_data[1] = alu_wb_data_i;"),
    ("V13 the drain's write lands in the ALU's context",
     "    wb_ctx[1]  = drain_ctx;",
     "    wb_ctx[1]  = alu_wb_ctx_i;"),
    ("V14 the drain's write lands in the ALU's register",
     "    wb_reg[1]  = drain_reg;",
     "    wb_reg[1]  = alu_wb_reg_i;"),
    ("V15 the two writeback claimants' request-valid lines are crossed",
     "    wb_req_valid[0] = alu_wb_valid_i;\n"
     "    wb_req_valid[1] = drain_valid;",
     "    wb_req_valid[0] = drain_valid;\n"
     "    wb_req_valid[1] = alu_wb_valid_i;"),
    # Reshaped from a policy tied to zero, which orphans wb_policy_i. Losing
    # the top bit is the same claim -- the policy the test asked for is not the
    # policy the arbiter runs -- and it keeps the signal live: round robin
    # silently becomes ALU-first, which section 5 has already MEASURED to
    # starve the drain forever.
    ("V16 the writeback policy loses its top bit",
     "      .policy_i(wb_policy_i),",
     "      .policy_i(wb_policy_i & 2'd1),"),

    # ---- the decode --------------------------------------------------------
    ("V17 RIDGE is decoded as NOISE2",
     "      .is_ridge_i(svc_op == OP_RIDGE),",
     "      .is_ridge_i(svc_op == OP_NOISE2),"),
    # An opcode constant is used TWICE -- to steer the service and to decide
    # what counts as a wrong op -- so one edit moves both, which is exactly how
    # a real opcode mistake behaves.
    ("V18 the RIDGE opcode is off by one",
     "  localparam logic [7:0] OP_RIDGE  = 8'h22;",
     "  localparam logic [7:0] OP_RIDGE  = 8'h23;"),
    ("V19 the NOISE2 opcode is off by one",
     "  localparam logic [7:0] OP_NOISE2 = 8'h1C;",
     "  localparam logic [7:0] OP_NOISE2 = 8'h1D;"),
    ("V20 the seed handed to the service is perturbed",
     "      .seed_i(svc_imm), .tag_i(svc_tag),",
     "      .seed_i(svc_imm ^ 32'd1), .tag_i(svc_tag),"),
    ("V21 the tag handed to the service is not the one the dispatcher expects",
     "      .seed_i(svc_imm), .tag_i(svc_tag),",
     "      .seed_i(svc_imm), .tag_i(svc_tag ^ 8'd1),"),

    # ---- the tie-offs ------------------------------------------------------
    # NOISE2 and RIDGE write at most two registers per point, so the third
    # result may never be drained at all. This asks whether the tie-off is
    # checked or merely present.
    ("V22 the third result is not zero",
     "    for (int l = 0; l < 4; l++) rsp_r2[l] = 32'sd0;",
     "    for (int l = 0; l < 4; l++) rsp_r2[l] = 32'sd1;"),
    ("V23 the wrong-op detector fires on the ops that ARE implemented",
     "                 (svc_op != OP_NOISE2) && (svc_op != OP_RIDGE)) begin",
     "                 (svc_op == OP_NOISE2) || (svc_op == OP_RIDGE)) begin"),
    # Reshaped from flush tied low, which orphans flush_i. Qualifying it with
    # long_valid_i is the sharper defect anyway: flush is RAISED after the last
    # context is offered and long_valid_i is low by then, so a partial group
    # never learns it will get no fourth point.
    ("V24 flush reaches the dispatcher only while an op is being offered",
     "      .long_imm_i(long_imm_i), .flush_i(flush_i),",
     "      .long_imm_i(long_imm_i), .flush_i(flush_i & long_valid_i),"),
    # The rival's operands are documented as "recognisable, so a routing bug
    # into an unused lane looks wrong rather than convincing". That claim is
    # only worth the words if something can tell 15 from any other number.
    ("V25 the rival's multiplicand is changed",
     "  localparam logic signed [32:0] RIVAL_A = 33'sd3;",
     "  localparam logic signed [32:0] RIVAL_A = 33'sd7;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
    for nl in ("\r\n", "\n"):
        o = old.replace("\n", nl)
        n = new.replace("\n", nl)
        count = gold.count(o)
        if count == 1:
            if o == n:
                raise ValueError("mutant identical to base")
            return gold.replace(o, n, 1)
        if count > 1:
            raise ValueError("anchor matches %d times" % count)
    raise ValueError("anchor matches 0 times (tried CRLF and LF)")


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. NOTHING IS DECLARED PREDICTIVELY.
EQUIVALENT = {}


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--equiv":
        proof = EQUIVALENT.get(argv[2])
        if proof is None:
            return 1
        print(proof)
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        entry = MUTANTS[int(argv[2])]
        name = entry[0]
        path = entry[1] if len(entry) == 4 else RTL
        old, new = entry[-2], entry[-1]
        try:
            write_rtl(mutate(read_rtl(path), old, new), path)
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
