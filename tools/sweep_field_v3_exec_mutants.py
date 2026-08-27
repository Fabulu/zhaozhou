#!/usr/bin/env python3
"""The mutant table for zhao_probe_v3_exec.sv (FIELD.V3.EXEC).

WHY THIS SWEEP EXISTS
---------------------
This is the datapath every Earth point passes through. Its differential runs
440 real plans against `zfield::execute_point`, which is strong evidence that
it computes the right values -- and no evidence at all that the tests would
NOTICE if it stopped. That second question is this file.

WHAT THIS BLOCK'S FAILURES LOOK LIKE
-------------------------------------
Almost nothing here fails loudly. The pipeline keeps running, contexts keep
retiring, the counters keep counting, and the arithmetic is quietly attached
to the wrong instruction or the wrong context. That is exactly what happened
for real: the first version put the ALU at S3, one clock before the product
landed, so every multiply consumed the PREVIOUS instruction's product. Every
context still finished. `desync_o` is the only reason it was caught, and X01
below is that defect kept as a permanent regression test.

The three claims worth attacking:

  * THE PRODUCT BELONGS TO THIS INSTRUCTION. zhao_field_mul is two clocks
    deep and the operands are carried to meet it. Off by one in either
    direction is a wrong answer, not a slow one.
  * ONE INSTRUCTION IN FLIGHT PER CONTEXT. `inflight` is what stops a context
    issuing again before its write lands. Clear it early and a program reads
    a register its own previous instruction has not yet written.
  * AN OP THIS INCREMENT OMITS IS REFUSED. DOT2/DOT3 have no products
    computed for them, so answering with a zero is worse than refusing.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/synth/zhao_probe_v3_exec.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the product must belong to THIS instruction -----------------------
    # THE DEFECT THAT ACTUALLY HAPPENED -- the whole ALU one stage early,
    # consuming the previous instruction's product -- IS NOT IN THIS TABLE,
    # and the reason is structural rather than an oversight.
    #
    # Every pipeline stage register here is read in exactly ONE place. So any
    # single-line mutation that makes a stage read from somewhere else leaves
    # the original register write-only, which orphans it and fails the LINTER
    # instead of the tests. Three shapes were tried (all three operands back
    # to S3; a0 alone back to S3; the S3->S4 carry sourced from the live RF
    # output) and every one orphaned something.
    #
    # The claim is not left untested. X02 attacks the same alignment from the
    # other end -- issuing the multiplier a stage early -- so the product
    # reaching the ALU belongs to a different instruction, which is exactly
    # the failure mode. X05 attacks the context-release timing that would let
    # the same disagreement arise through re-issue.
    #
    # Recorded here rather than silently omitted: a claim with no mutant
    # against it looks identical to a claim nobody thought of.
    ("X02 the multiplier is issued a stage early, on operands not yet read",
     "      .issue_i(s2_v_r),",
     "      .issue_i(s1_v_r),"),
    ("X03 the desync guard is inverted, so alignment is never reported",
     "      if (s4_v_r != prod_valid) desync_o <= 1'b1;",
     "      if (s4_v_r == prod_valid) desync_o <= 1'b1;"),
    ("X04 the multiplier zero-extends its operands instead of sign-extending",
     "      .a_i    (33'(rf_a0)),\n"
     "      .b_i    (33'(rf_b0)),",
     "      .a_i    (33'($unsigned(rf_a0))),\n"
     "      .b_i    (33'($unsigned(rf_b0))),"),

    # ---- one instruction in flight per context -----------------------------
    ("X05 a context is released for re-issue a stage before its write lands",
     "      if (s4_v_r) begin\n"
     "        inflight_r[s4_ctx_r] <= 1'b0;",
     "      if (s3_v_r) inflight_r[s3_ctx_r] <= 1'b0;\n"
     "      if (s4_v_r) begin"),
    # Reshaped: dropping the term orphaned inflight_r. ORing it keeps the
    # operand and keeps the defect -- an in-flight context is offered again
    # before its own write has landed.
    ("X06 an in-flight context is offered for issue again",
     "    ready_c = active_r & ~inflight_r;",
     "    ready_c = active_r | inflight_r;"),
    ("X07 the context is never released, so each runs one instruction only",
     "        inflight_r[s4_ctx_r] <= 1'b0;",
     "        inflight_r[s4_ctx_r] <= 1'b1;"),

    # ---- the program counter -----------------------------------------------
    ("X08 the pc advances past END as well",
     "        if (alu_is_end) begin\n"
     "          active_r[s4_ctx_r] <= 1'b0;\n"
     "        end else begin\n"
     "          pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(1);\n"
     "        end",
     "        if (alu_is_end) active_r[s4_ctx_r] <= 1'b0;\n"
     "        pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(1);"),
    ("X09 the pc advances by two, so every other instruction is skipped",
     "          pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(1);",
     "          pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(2);"),
    # X10 is DEGENERATE while PLAN == REGS and is declared equivalent below.
    # X20 states the same defect as a LITERAL so the indexing is actually
    # scored: a mutant whose meaning depends on two parameters happening to be
    # equal is not coverage.
    ("X10 the uop store is indexed by register count rather than plan length",
     "        s1_uop_r              <= store[(int'(issue_ctx_c) * PLAN) + int'(pc_r[issue_ctx_c])];",
     "        s1_uop_r              <= store[(int'(issue_ctx_c) * REGS) + int'(pc_r[issue_ctx_c])];"),
    ("X20 the uop store stride is 16 rather than the plan length",
     "        s1_uop_r              <= store[(int'(issue_ctx_c) * PLAN) + int'(pc_r[issue_ctx_c])];",
     "        s1_uop_r              <= store[(int'(issue_ctx_c) * 16) + int'(pc_r[issue_ctx_c])];"),

    # ---- writeback ----------------------------------------------------------
    # The write enable grew a `!dot_here_c` term on 2026-08-28 -- the fix for
    # the defect X11 exposed -- so these two anchors moved with it.
    ("X11 every op writes, including the ones that do not",
     "      rf_we_c    = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;",
     "      rf_we_c    = s4_v_r && !alu_is_end && !dot_here_c;"),
    ("X12 END writes its result over a register",
     "      rf_we_c    = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;",
     "      rf_we_c    = s4_v_r && alu_writes && !dot_here_c;"),
    ("X13 the writeback lands in the wrong context",
     "      rf_wctx_c  = s4_ctx_r;",
     "      rf_wctx_c  = s4_ctx_r + CW'(1);"),
    ("X14 the observed writeback stream disagrees with what the file was told",
     "  assign wb_reg_o   = s4_dst_r;",
     "  assign wb_reg_o   = s4_dst_r + RW'(1);"),

    # ---- refusal and the ledger --------------------------------------------
    # Reshaped: dropping the term orphaned dot_here_c. AND keeps the operand
    # and still lets a DOT through, because the ALU KNOWS the DOT opcodes and
    # does not itself call them unsupported -- their products were simply
    # never computed.
    ("X15 a DOT op is refused only if the ALU also objects, which it does not",
     "        if (alu_unsupported || dot_here_c) unsupported_o <= 1'b1;",
     "        if (alu_unsupported && dot_here_c) unsupported_o <= 1'b1;"),
    # Reshaped: the straight swap orphaned alu_sat_add.
    ("X16 the ADD saturation flag also latches on a MUL clamp",
     "        if (alu_sat_add) sat_add_o <= 1'b1;",
     "        if (alu_sat_add || alu_sat_mul) sat_add_o <= 1'b1;"),
    # Reshaped: latching unconditionally orphaned alu_sat_mul.
    ("X17 the MUL saturation flag also latches on a rescale clamp",
     "        if (alu_sat_mul) sat_mul_o <= 1'b1;",
     "        if (alu_sat_mul || alu_sat_rescale) sat_mul_o <= 1'b1;"),

    # ---- issue order and commutativity: both expected to be EQUIVALENT -----
    # Kept because an equivalence that is PROVEN is evidence about the block,
    # while an equivalence that is merely absent from the table is a hole
    # nobody looked at.
    ("X18 the ready scan picks the highest-numbered context rather than the lowest",
     "    for (int i = CTX - 1; i >= 0; i--) if (ready_c[i]) issue_ctx_c = CW'(i);",
     "    for (int i = 0; i < CTX; i++) if (ready_c[i]) issue_ctx_c = CW'(i);"),
    ("X19 the multiplier's operands are swapped",
     "      .a_i    (33'(rf_a0)),\n"
     "      .b_i    (33'(rf_b0)),",
     "      .a_i    (33'(rf_b0)),\n"
     "      .b_i    (33'(rf_a0)),"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
    nl = "\r\n" if "\r\n" in gold else "\n"
    o = old.replace("\n", nl)
    n = new.replace("\n", nl)
    count = gold.count(o)
    if count != 1:
        raise ValueError("anchor matches %d times" % count)
    if o == n:
        raise ValueError("mutant identical to base")
    return gold.replace(o, n, 1)


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives --
# writing a proof before the evidence is how a hole acquires a note.
EQUIVALENT = {
    "X05":
        "CORRECTNESS-EQUIVALENT, TIMING-VISIBLE -- and it is the TIMING that "
        "now catches it. Releasing a context at S3 instead of S4 cannot "
        "corrupt a value: the released context can issue no earlier than the "
        "next clock, its uop reaches S1 the clock after that, and the "
        "register-file read address is driven only then -- by which time the "
        "S4 write has already landed. So no read can observe the stale "
        "register. "
        "What it DOES change is occupancy, and nothing was looking: the only "
        "timing check was a loose inequality. The barrel test now PINS the "
        "measured cycle counts (65 for one context, 126 for eight), so this "
        "mutant is caught as the throughput change it is. Listed here for the "
        "record of why it once survived, not as a standing exemption.",
    "X08":
        "DEAD STATE. Advancing the pc past END writes a value nobody reads: "
        "the same arm clears active_r, so the context never issues again, and "
        "the only path back is start_i -- which resets pc_r to zero. So the "
        "post-END pc is unobservable through any port. "
        "RE-SCORE THIS IF A CONTEXT IS EVER RESUMED WITHOUT start_i, or if "
        "start_i stops resetting the pc. Either change makes the value live.",
    "X10":
        "DEGENERATE WHILE PLAN == REGS. Both are 32 in this probe, so "
        "`ctx * PLAN` and `ctx * REGS` are the same expression and Verilator "
        "emits a byte-identical model -- the binary-hash guard reported it as "
        "a discard, which is that guard working. The defect it was meant to "
        "probe IS scored, by X20, which uses a literal 16. "
        "RE-SCORE THIS THE MOMENT PLAN != REGS.",
    # X18 WAS DECLARED EQUIVALENT HERE AND THAT WAS WRONG. The claim was
    # that permuting which ready context issues cannot change the clock count.
    # It can, and the re-score CAUGHT the mutant: the eight contexts are
    # STARTED ONE AT A TIME, so during the staggered start "highest ready" and
    # "lowest ready" are different contexts, and the interleaving -- and with
    # it the pinned cycle count -- differs.
    #
    # The values argument still holds (a context's own program order is
    # preserved), but values were never the whole observable. Left as a caught
    # mutant with no proof, which is what it is. Recorded rather than deleted
    # because a proof that turned out to be false is worth remembering.
    "X19":
        "MULTIPLICATION IS COMMUTATIVE. The swap reaches only the multiplier's "
        "two operand ports; the ALU still receives a0_i and b0_i unswapped, so "
        "the ONLY affected value is prod_ab = a*b versus b*a. Both operands are "
        "sign-extended to 33 bits by the same expression before the multiply, "
        "so the products are bit-identical, not merely numerically equal. "
        "RE-SCORE THIS IF THE MULTIPLIER EVER TREATS ITS PORTS ASYMMETRICALLY "
        "-- different widths, a signed/unsigned split, or an accumulate that "
        "reads one port twice.",
    "X12":
        "REDUNDANT TERM, PROVEN FROM THE ALU'S OWN DECODE. The write enable "
        "is `s4_v_r && alu_writes && !alu_is_end`, and X12 drops the last "
        "term. In zhao_field_alu exactly ONE op sets is_end_o -- OP_END -- "
        "and that same arm also sets writes_o = 1'b0. So alu_is_end implies "
        "!alu_writes, which makes `alu_writes && !alu_is_end` identical to "
        "`alu_writes` for every op the ALU can decode. No stimulus can "
        "distinguish them. "
        "The term is KEPT in the shipped RTL as defence in depth: it costs "
        "one gate and it states the intent at the write port rather than "
        "relying on a property of a different module's decode. "
        "RE-SCORE THIS THE MOMENT ANY OP SETS BOTH is_end_o AND writes_o. "
        "That combination -- an instruction that both ends the program and "
        "writes a result -- is what makes the term load-bearing, and the ALU's "
        "decode, not this note, is the thing to watch."
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
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
