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

PREFER MUTATING A VALUE OVER DELETING A USE -- AND MOVING A USE COUNTS AS
DELETING ONE. That second half was learned here, on the first run of this
table, when TEN of twenty-five mutants were refused at once.

The rule had been stated as "do not tie a port to a constant, it orphans what
fed it". Every mutant refused here obeyed that and was refused anyway, because
a wiring mutant does not delete a read -- it MOVES one, and the thing it moved
off is now unread:

    bank_req_ready[1] -> [0]      orphans bit 1. Verilator tracks per-bit use.
    nz_b[l] -> nz_a[l]            orphans nz_b entirely.
    drain_data -> alu_wb_data_i   orphans drain_data.

Which is obvious once seen and was not obvious before: this is the first table
whose mutants are almost all redirections, so it is the first place the second
half of the rule could bite.

The repairs keep BOTH ends live and are not mechanical -- each had to be chosen
for what it claims:

  * coupled handshakes (V01-V04, V10, V11): `a & b` rather than `b`, which is
    what somebody actually writes when they think ANDing two readys is safe;
  * mirrored operands (V08): because the OBVIOUS repair -- swap A and B -- is
    equivalent, multiplication commuting;
  * crossed payloads (V12-V14): two claimants' fields exchanged, which is what
    a mis-ordered port map really produces.

Twenty-two orphan refusals across this family now, and the pattern has held
every time: the reshape the linter forces is a better mutant than the one first
written.

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
    # THE SHAPE OF EVERY DEFECT THIS FILE EXISTS TO CATCH: a claimant whose
    # handshake is wired to the wrong part of a vector it shares with somebody
    # else. The block it lives in cannot see the mistake, because the index is
    # chosen HERE.
    #
    # RESHAPED, ALL FOUR, AND THE REASON IS A NEW FORM OF THE ORPHAN RULE.
    # These first read `bank_req_ready[0]` where the design reads `[1]`, which
    # is the more obvious way to say it -- and unbuildable, per the docstring.
    # These read `bank_req_ready[0]` where the design reads `[1]`. That is not
    # deleting a use -- it is MOVING one -- and it orphans the bit it moved
    # off: Verilator tracks per-bit usage and refuses the file with "Bits of
    # signal are not used: 'bank_req_ready'[1]".
    #
    # Coupling the two handshakes keeps BOTH bits live and states a defect that
    # is at least as realistic as the redirect: somebody ANDs two ready lines
    # thinking it is the safe thing to do. Each claimant then cannot proceed
    # unless the OTHER one also could, which under contention is a deadlock and
    # with an idle rival is invisible -- so it is caught by exactly the
    # sections that drive the rival, and by nothing else.
    ("V01 the service's grant is coupled to the RIVAL's",
     "  assign nz_mul_ready  = bank_req_ready[1];",
     "  assign nz_mul_ready  = bank_req_ready[1] & bank_req_ready[0];"),
    ("V02 the service's product is coupled to the RIVAL's",
     "  assign nz_mul_valid  = bank_rsp_valid[1];",
     "  assign nz_mul_valid  = bank_rsp_valid[1] & bank_rsp_valid[0];"),
    ("V03 the rival's grant is coupled to the SERVICE's",
     "  assign rival_grant_o = bank_req_ready[0];",
     "  assign rival_grant_o = bank_req_ready[0] & bank_req_ready[1];"),
    ("V04 the rival's reply is coupled to the SERVICE's",
     "  assign rival_rsp_o   = bank_rsp_valid[0];",
     "  assign rival_rsp_o   = bank_rsp_valid[0] & bank_rsp_valid[1];"),
    ("V05 the two bank claimants' request-valid lines are crossed",
     "    bank_req_valid[0] = rival_req_i;\n"
     "    bank_req_valid[1] = nz_mul_issue;",
     "    bank_req_valid[0] = nz_mul_issue;\n"
     "    bank_req_valid[1] = rival_req_i;"),
    # The bank's fixed priority is a REQUIREMENT where it is defined. Whether
    # this composition's test can see it is a different question, and one this
    # mutant asks rather than assumes.
    ("V06 the bank puts the LANES above the service",
     "      .CLAIMANTS(3), .PRIO_SERVICES_FIRST(1'b1), .TAGW(TAGW)",
     "      .CLAIMANTS(3), .PRIO_SERVICES_FIRST(1'b0), .TAGW(TAGW)"),
    ("V07 the service's A operands are broadcast from point 0",
     "      bank_a[1][l] = nz_a[l];",
     "      bank_a[1][l] = nz_a[0];"),
    # Reshaped: pointing B at A orphans nz_b entirely. Swapping A and B would
    # keep both live and be EQUIVALENT -- multiplication commutes -- so the
    # obvious repair is the wrong one here. Mirroring B across the four points
    # keeps every element used and states a real defect: each point multiplied
    # against another point's operand.
    ("V08 the service's B operands are mirrored across the four points",
     "      bank_b[1][l] = nz_b[l];",
     "      bank_b[1][l] = nz_b[3 - l];"),
    ("V09 both bank claimants carry the same tag",
     "    bank_tag[1] = 8'd1;",
     "    bank_tag[1] = 8'd0;"),

    # ---- the write port's two slots ----------------------------------------
    # Same reshape, same reason: redirecting the slot orphans the bit left
    # behind. Coupled here is if anything MORE plausible than at the bank --
    # a single write port with two claimants is exactly where somebody reaches
    # for an AND.
    ("V10 the ALU's ready is coupled to the drain's",
     "  assign alu_wb_ready_o = wb_req_ready[0];",
     "  assign alu_wb_ready_o = wb_req_ready[0] & wb_req_ready[1];"),
    ("V11 the drain's ready is coupled to the ALU's",
     "  assign drain_ready    = wb_req_ready[1];",
     "  assign drain_ready    = wb_req_ready[1] & wb_req_ready[0];"),
    # Reshaped: overwriting the drain's field with the ALU's orphans
    # drain_data / drain_ctx / drain_reg, each of which is read in exactly one
    # place. CROSSING the two claimants keeps both live and is the more
    # realistic composition defect anyway -- the payloads of two claimants
    # swapped is what a mis-ordered port map actually produces.
    #
    # The anchors SPAN from claimant 0's field to claimant 1's, because the
    # mutation has to change both and the table's own rule is that an anchor
    # must span every place a mutation touches.
    ("V12 the two claimants' write DATA are crossed",
     "    wb_data[0] = alu_wb_data_i;\n"
     "    wb_ctx[1]  = drain_ctx;\n"
     "    wb_reg[1]  = drain_reg;\n"
     "    wb_data[1] = drain_data;",
     "    wb_data[0] = drain_data;\n"
     "    wb_ctx[1]  = drain_ctx;\n"
     "    wb_reg[1]  = drain_reg;\n"
     "    wb_data[1] = alu_wb_data_i;"),
    ("V13 the two claimants' write CONTEXTS are crossed",
     "    wb_ctx[0]  = alu_wb_ctx_i;\n"
     "    wb_reg[0]  = alu_wb_reg_i;\n"
     "    wb_data[0] = alu_wb_data_i;\n"
     "    wb_ctx[1]  = drain_ctx;",
     "    wb_ctx[0]  = drain_ctx;\n"
     "    wb_reg[0]  = alu_wb_reg_i;\n"
     "    wb_data[0] = alu_wb_data_i;\n"
     "    wb_ctx[1]  = alu_wb_ctx_i;"),
    ("V14 the two claimants' write REGISTERS are crossed",
     "    wb_reg[0]  = alu_wb_reg_i;\n"
     "    wb_data[0] = alu_wb_data_i;\n"
     "    wb_ctx[1]  = drain_ctx;\n"
     "    wb_reg[1]  = drain_reg;",
     "    wb_reg[0]  = drain_reg;\n"
     "    wb_data[0] = alu_wb_data_i;\n"
     "    wb_ctx[1]  = drain_ctx;\n"
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
     "      rsp_r2[l] = 32'sd0;",
     "      rsp_r2[l] = 32'sd1;"),
    ("V23 the wrong-op detector fires on the ops that ARE implemented",
     "                 (svc_op != OP_NOISE2) && (svc_op != OP_RIDGE) &&",
     "                 (svc_op == OP_NOISE2) || (svc_op == OP_RIDGE) ||"),
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
    # ---- the second service, added 2026-08-29 -----------------------------
    # The twenty-five above were scored against a path with ONE service. None
    # of them can reach the request routing, the response arbitration or the
    # third bank claimant, because none of that existed. Three of the
    # twenty-five (V06, V22, V23) also had to be re-anchored, which is the
    # same argument in its sharpest form: they were scored CAUGHT and had
    # stopped being applicable at all.
    ("W01 curve ops are also handed to the noise unit",
     "      .v_valid_i(svc_valid && !is_curve_c), .v_ready_o(nz_v_ready),",
     "      .v_valid_i(svc_valid), .v_ready_o(nz_v_ready),"),

    ("W02 the handshake is taken from the service that was NOT asked",
     "  assign svc_ready = is_curve_c ? cv_req_ready : nz_v_ready;",
     "  assign svc_ready = is_curve_c ? nz_v_ready : cv_req_ready;"),

    ("W03 the response mux publishes the other service's value",
     "      rsp_r0[l] = cv_rsp_valid ? cv_r0[l] : nz_r0[l];",
     "      rsp_r0[l] = cv_rsp_valid ? nz_r0[l] : cv_r0[l];"),

    # THE ONE THAT LOSES A VALUE RATHER THAN TIME. Without the guard the noise
    # unit sees its response accepted on a cycle the mux published the curve
    # service's, and its answer is gone -- not late, gone.
    ("W04 the losing service's response is dropped instead of held",
     "  assign nz_rsp_ready = rsp_ready && !cv_rsp_valid;",
     "  assign nz_rsp_ready = rsp_ready;"),

    ("W05 the response carries the other service's tag",
     "  assign rsp_tag   = cv_rsp_valid ? cv_rsp_tag : nz_rsp_tag;",
     "  assign rsp_tag   = cv_rsp_valid ? nz_rsp_tag : cv_rsp_tag;"),

    ("W06 a width-1 answer leaves the second register unzeroed",
     "      rsp_r1[l] = cv_rsp_valid ? 32'sd0 : nz_r1[l];",
     "      rsp_r1[l] = nz_r1[l];"),

    ("W07 DCURVE is served as CURVE",
     "  assign cv_mode_c = (svc_op == OP_DCURVE) ? 2'd1",
     "  assign cv_mode_c = (svc_op == OP_DCURVE) ? 2'd0"),

    ("W08 SPLINE is served as DCURVE",
     "                   : (svc_op == OP_SPLINE) ? 2'd2 : 2'd0;",
     "                   : (svc_op == OP_SPLINE) ? 2'd1 : 2'd0;"),

    ("W09 the table index is read from the wrong bits of the immediate",
     "      .req_mode_i(cv_mode_c), .req_tbl_i(svc_imm[1:0]),",
     "      .req_mode_i(cv_mode_c), .req_tbl_i(svc_imm[3:2]),"),

    # RESHAPED AFTER THE PREFLIGHT REFUSED THEM. Pointing the curve service at
    # claimant 1 leaves bank_req_ready[2] read by nobody, and Verilator tracks
    # usage per BIT -- so the mutant orphans a signal and cannot build. That is
    # the preflight doing its job: a mutant that will not elaborate would be
    # scored CAUGHT by a compile failure, the most flattering way to be wrong.
    #
    # Crossing the two claimants keeps every bit read and states the same
    # claim from both sides at once: each service is answered by the other's
    # slot.
    ("W10 the two services' bank grants are crossed",
     "  assign nz_mul_ready  = bank_req_ready[1];\n"
     "  assign nz_mul_valid  = bank_rsp_valid[1];\n"
     "  assign cv_mul_ready  = bank_req_ready[2];",
     "  assign nz_mul_ready  = bank_req_ready[2];\n"
     "  assign nz_mul_valid  = bank_rsp_valid[1];\n"
     "  assign cv_mul_ready  = bank_req_ready[1];"),

    ("W11 the two services' bank products are crossed",
     "  assign nz_mul_valid  = bank_rsp_valid[1];\n"
     "  assign cv_mul_ready  = bank_req_ready[2];\n"
     "  assign cv_mul_valid  = bank_rsp_valid[2];",
     "  assign nz_mul_valid  = bank_rsp_valid[2];\n"
     "  assign cv_mul_ready  = bank_req_ready[2];\n"
     "  assign cv_mul_valid  = bank_rsp_valid[1];"),

    ("W12 the wrong-op detector fires on a SPLINE it does serve",
     "                 (svc_op != OP_SPLINE)) begin",
     "                 (svc_op != OP_NOISE2)) begin"),
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
EQUIVALENT = {
    # DECLARED 2026-08-29, after the run that showed it surviving.
    "W06": (
        "EQUIVALENT, AND CHECKED IN THE DISPATCHER RATHER THAN ARGUED. An "
        "unzeroed rsp_r1 cannot be observed for a width-1 op: the drain "
        "selects r1_r only when d_memb_r == 1, and d_memb_r counts 0 .. "
        "width-1, so a width-1 op never reaches that branch. CURVE, DCURVE "
        "and SPLINE are all width 1 in zhao_field_ops_pkg. The tie is "
        "DEFENSIVE, exactly like f_spl_offered in the curve service: it makes "
        "a width the dispatcher should never ask for drain a defined value, "
        "wrong the same way twice instead of differently each run. "
        "RE-SCORE IF: any op routed to the curve service is ever given a "
        "field_long_width above 1, or the drain reads r1_r anywhere else."),
    "V08": (
        "THE NOISE UNIT'S B OPERAND IS PER STEP, NOT PER POINT, so mirroring "
        "it across the four points is the IDENTITY. Every branch of "
        "zhao_field_v3_noise's operand mux assigns mul_b[l] a CONSTANT -- C_X, "
        "C_Y, C_LCG_M, C_XSM, or zero -- with no dependence on l whatsoever. "
        "So mul_b[0] == mul_b[1] == mul_b[2] == mul_b[3] at every instant and "
        "nz_b[3 - l] == nz_b[l] identically. Exhaustive by inspection of the "
        "mux, not measured over a sample. "
        "THE ASYMMETRY IS THE POINT: mul_a[l] IS per point (ix[l], iy[l], "
        "s_reg[l], rxs_in[l]), which is why V07 -- broadcasting A from point 0 "
        "-- is caught while this is not. A four-wide bank request is cheap for "
        "this service precisely because one side of every product is shared. "
        "RE-SCORE THIS IF ANY mul_b BRANCH EVER INDEXES BY LANE. The lane salt "
        "in S_LCG is the near miss to watch: it is added to mul_a, and moving "
        "it to mul_b would break the equivalence on that step alone."
    ),
    "V09": (
        "THE BANK'S REQUEST TAG IS DEAD IN THIS COMPOSITION, and the model "
        "hash proves it rather than merely suggesting it: the mutant "
        "elaborated to a BYTE-IDENTICAL model, which means Verilator folded "
        "the tag away entirely. Nothing here consumes it -- bank_rsp_tag is "
        "declared with an explicit UNUSEDSIGNAL waiver, and the bank's "
        "desync_o does not depend on the request tags' VALUES. "
        "This is not a hole in the test: the bank's tag is the BANK's claim, "
        "and it is checked where it is made, by the mulbank sweep. What this "
        "file can be wrong about is which claimant gets which slot, and that "
        "is V01-V05, all caught. "
        "RE-SCORE THIS THE MOMENT bank_rsp_tag IS READ -- the waiver comment "
        "on it is the tripwire."
    ),
    "V22": (
        "THE THIRD RESULT CANNOT BE DRAINED, so its value cannot be observed. "
        "This service implements NOISE2 and RIDGE only, and the dispatcher's "
        "dst_width_of gives them 2 and 1. The drain writes exactly `width` "
        "registers per point, so index 2 is never read for either op -- not "
        "rarely, never. "
        "THE TIE-OFF STAYS, and its own comment already says why: it exists so "
        "that a width-3 op routed here by mistake drains a DEFINED value, "
        "wrong the same way twice rather than differently each run. That is a "
        "statement about a case this composition makes unreachable, which is "
        "exactly what wrong_op_o is for -- and V23, which breaks that "
        "detector, is caught. "
        "RE-SCORE THIS IF A WIDTH-3 OP IS EVER ROUTED TO THIS SERVICE, which "
        "is the same event that would make wrong_op_o fire."
    ),
    "V25": (
        "NOTHING CAN READ THE RIVAL'S PRODUCT, so its operands cannot matter. "
        "The bank returns products on a SHARED rsp_p bus with a per-claimant "
        "valid; the noise unit samples it only under bank_rsp_valid[1], and no "
        "other reader exists. rival_rsp_o carries the VALID, not the value. "
        "So RIVAL_A is unobservable by construction. "
        "AND THIS CORRECTS THE HEADER THAT MOTIVATED IT. That header says the "
        "constants are 'recognisable, so a routing bug into an unused lane "
        "looks wrong rather than convincing'. The recognisability earns "
        "nothing: V02 -- the service taking the rival's product -- is caught "
        "because the service's ANSWER is wrong, and it would be wrong for any "
        "rival operands at all, including zero. The constants are a debugging "
        "convenience when reading a waveform, which is worth having and is not "
        "what the header claimed. "
        "RE-SCORE THIS IF THE BANK EVER GIVES EACH CLAIMANT ITS OWN PRODUCT "
        "BUS, or if a check reads the rival's product directly."
    ),
}


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
