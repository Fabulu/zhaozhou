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
RF = "fpga/rtl/field/zhao_field_v3_rf.sv"

# Entries are (name, old, new) against RTL, or (name, path, old, new) when the
# mutation lands in another file of the cone. The register file is a separate
# module and its group addressing is exactly the thing that hid behind 440
# passing programs, so it is swept here rather than left to a sweep of its own
# that does not exist yet.

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
    # Re-pointed 2026-08-28: the issue moved into the operand mux.
    # Re-pointed AGAIN 2026-08-28 06:41: the DOT fix moved the whole sequence
    # to S4 and rewrote the desync guard, so EIGHT anchors in this table died.
    # They are re-aimed at the SAME claims against the new shape, not dropped.
    # A claim whose mutant stopped applying is a claim that silently stopped
    # being tested, and it looks exactly like a claim nobody thought of.
    ("X02 the multiplier is issued a stage early, on operands not yet read",
     "    mul_issue_c = s2_v_r && !is_dot(s2_op_r);",
     "    mul_issue_c = s1_v_r && !is_dot(s2_op_r);"),
    # The guard now checks THE MULTIPLIER'S CONTRACT -- a product arrives
    # exactly two clocks after a granted issue -- through a two-deep shadow.
    # Inverting it outright is a constant alarm; WIDENING the window to "one
    # or two clocks" keeps both shadow stages live and is the real defect,
    # because a product arriving on the wrong clock then passes unreported.
    ("X03 the desync guard accepts a product a clock early, so a shifted product passes",
     "      if (prod_valid != issued_s2_r) desync_o <= 1'b1;",
     "      if (prod_valid != (issued_s2_r || issued_s1_r)) desync_o <= 1'b1;"),
    # Re-pointed with X19, same reason.
    ("X04 the multiplier zero-extends its operands instead of sign-extending",
     "    mul_a_c     = 33'(use_a0_c);\n"
     "    mul_b_c     = 33'(use_b0_c);",
     "    mul_a_c     = 33'($unsigned(use_a0_c));\n"
     "    mul_b_c     = 33'($unsigned(use_b0_c));"),

    # ---- one instruction in flight per context -----------------------------
    ("X05 a context is released for re-issue a stage before its write lands",
     "      end else if (s4_v_r) begin\n"
     "        inflight_r[s4_ctx_r] <= 1'b0;",
     "      end else if (s4_v_r) begin\n"
     "        inflight_r[s3_ctx_r] <= 1'b0;"),
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
    # RESHAPED BY MY OWN REFACTOR. This dropped `alu_writes` entirely, which
    # was buildable while the signal had TWO readers -- rf_we_c and
    # wb_valid_o. Consolidating those into one `wb_req_c` left it with one,
    # so dropping it now orphans it and Verilator refuses the file.
    #
    # Inverting keeps it live and is the sharper claim anyway: not "the
    # write enable is missing" but "the write enable is backwards", so every
    # op that should write is silent and every op that should not writes
    # garbage. Both halves are wrong at once, and the second half is the one
    # that corrupts a register rather than merely losing a result.
    ("X11 the write enable is INVERTED -- only the ops that do not write, write",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c;",
     "  assign wb_req_c   = s4_v_r && !alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c;"),
    ("X12 END writes its result over a register",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c;",
     "  assign wb_req_c   = s4_v_r && alu_writes && !dot_here_c &&\n"
     "                      !retire_hold_c;"),
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

    # ---- the DOT sequencer (landed 2026-08-28) -----------------------------
    # Every claim in reports/FIELD_V3_DOT_SEQUENCING.md gets a mutant. The
    # sequencer's failures are quiet: the pipeline keeps running and a dot
    # product comes out slightly wrong, or right for the wrong reason.
    # The hold length is now ONE expression -- `dot_need_c` -- so the three
    # mutants that used to attack the two-term form all point at it.
    ("X21 a DOT2 also issues the third product, which it does not have",
     "  assign dot_need_c  = dot3_at_s4_c ? 2'd3 : 2'd2;",
     "  assign dot_need_c  = dot_at_s4_c ? 2'd3 : 2'd2;"),
    ("X22 the hold ends one product early, dropping the last one",
     "  assign retire_hold_c = (dot_at_s4_c && (dot_cnt_r < dot_need_c)) || long_hold_c;",
     "  assign retire_hold_c = (dot_at_s4_c && (dot_cnt_r < (dot_need_c - 2'd1))) ||\n"
     "                         long_hold_c;"),
    ("X23 the DOT2 and DOT3 product counts are swapped",
     "  assign dot_need_c  = dot3_at_s4_c ? 2'd3 : 2'd2;",
     "  assign dot_need_c  = dot3_at_s4_c ? 2'd2 : 2'd3;"),
    # Re-aimed at the RETIRE clear, which is where the accumulator is zeroed
    # now. Self-assignment rather than deletion, so the mutant reads as a
    # deliberate defect and orphans nothing.
    ("X24 the accumulator is not cleared between instructions",
     "      if (dot_at_s4_c && !hold_c) begin\n"
     "        dot_acc_r   <= '0;\n"
     "        dot_cnt_r   <= 2'd0;\n"
     "        dot_issue_r <= 2'd0;\n"
     "      end",
     "      if (dot_at_s4_c && !hold_c) begin\n"
     "        dot_acc_r   <= dot_acc_r;\n"
     "        dot_cnt_r   <= 2'd0;\n"
     "        dot_issue_r <= 2'd0;\n"
     "      end"),
    # The operand mux moved to S4 with the rest of the sequence. Every s4_*
    # register also feeds the ALU, so re-pointing the second product at the
    # third pair orphans nothing.
    ("X25 the second product is taken from the a2/b2 pair instead of a1/b1",
     "        2'd1:    begin mul_a_c = 33'(s4_a1_r); mul_b_c = 33'(s4_b1_r); end",
     "        2'd1:    begin mul_a_c = 33'(s4_a2_r); mul_b_c = 33'(s4_b2_r); end"),
    # Reshaped: dropping the term orphaned dot_inflight_c. ORing it keeps the
    # operand and keeps the defect -- a DOT no longer blocks issue, so another
    # instruction reaches S2 and drives the multiplier out from under it.
    # Re-pointed 2026-08-28: the issue line gained `&& !mul_denied_c` when the
    # executor moved onto the shared bank, because a refused request must stall
    # issue -- the register file holds operands for exactly one clock.
    ("X26 a DOT no longer freezes issue, so another op steals the multiplier",
     "    issue_c = |ready_c && !dot_inflight_c && !hold_c && !mul_denied_c;",
     "    issue_c = (|ready_c || dot_inflight_c) && !hold_c && !mul_denied_c;"),
    ("X27 only a DOT at S4 freezes issue, not one still upstream",
     "  assign dot_inflight_c = (s1_v_r && is_dot(s1_uop_r.op)) || (s2_v_r && is_dot(s2_op_r)) ||\n"
     "                          (s3_v_r && is_dot(s3_op_r))     || (s4_v_r && is_dot(s4_op_r));",
     "  assign dot_inflight_c = (s4_v_r && is_dot(s4_op_r));"),
    ("X28 the sum drops the product arriving on the final clock",
     "  assign dot_sum_c   = dot_acc_r + prod_ab;",
     "  assign dot_sum_c   = dot_acc_r;"),
    ("X29 DOT3 is treated as DOT2, so the third member never contributes",
     "    is_dot = (op == 8'h10) || (op == 8'h11);  // OP_DOT2, OP_DOT3",
     "    is_dot = (op == 8'h10);  // OP_DOT2, OP_DOT3"),

    # ---- the functional register file's group addressing -------------------
    # This is the defect that hid behind 440 passing programs: the fit probe
    # addresses every bank with the same row, which only reads a group
    # correctly when it does not cross a multiple of four.
    ("X30 every bank presents the base row, the fit probe's non-functional form",
     RF,
     "    row = base[RSEL-1:2] + {{(RSEL-3){1'b0}}, low_sum[2]};",
     "    row = base[RSEL-1:2];"),
    ("X31 the group carry is taken from the wrong bit",
     RF,
     "    low_sum = {1'b0, base[1:0]} + {1'b0, off};",
     "    low_sum = {1'b0, base[1:0]} + {1'b0, off} + 3'd1;"),
    ("X32 the member offset runs the wrong way round the banks",
     RF,
     "    off = bk - base[1:0];",
     "    off = base[1:0] - bk;"),
    # ---- issue order and commutativity ------------------------------------
    # These two were written under the heading "both expected to be
    # EQUIVALENT", with proofs, BEFORE any run. Both were then CAUGHT.
    #
    # That was a self-inflicted error and this file's own header says so:
    # "Nothing is declared until the first run says what actually survives --
    # writing a proof before the evidence is how a hole acquires a note."
    # I wrote the proofs predictively anyway, and predicting is exactly what
    # the rule forbids.
    #
    # X18's proof was wrong on the facts: contexts are STARTED one at a time,
    # so during the staggered start "highest ready" and "lowest ready" are
    # different contexts and the pinned cycle count sees it.
    #
    # X19 IS equivalent after all, and how that was established twice-wrongly
    # is the more useful record.
    #
    # Declared equivalent predictively (against the rule). Then a run reported
    # it CAUGHT, so the proof was retracted. Then a CLEAN run reported it
    # SURVIVING, restoring the proof below.
    #
    # The middle result was garbage, and its own run had already said so: that
    # sweep aborted with exit 4 and the message "restored model differs from
    # pristine -- the tree is NOT clean", because the driver was snapshotting
    # only one of the two files the table mutates. The register file had
    # X30/X31/X32 accumulated in it, so it was returning wrong operands and an
    # a0/b0 swap became observable. Commutativity never failed; the register
    # file underneath it did.
    #
    # THE LESSON IS NOT ABOUT COMMUTATIVITY. It is that I read per-mutant
    # verdicts out of a run that had ABORTED and declared itself unclean. A
    # run that fails its own integrity check has no verdicts, only noise --
    # and the guard said exactly that before I ignored it.
    ("X18 the ready scan picks the highest-numbered context rather than the lowest",
     "    for (int i = CTX - 1; i >= 0; i--) if (ready_c[i]) issue_ctx_c = CW'(i);",
     "    for (int i = 0; i < CTX; i++) if (ready_c[i]) issue_ctx_c = CW'(i);"),
    # Re-pointed 2026-08-28: the multiplier's ports are driven by a MUX now
    # that DOT sequencing shares them, so the swap moves to the mux's default
    # arm -- which is still the a0*b0 product, so the equivalence proof below
    # is unchanged.
    ("X19 the multiplier's operands are swapped",
     "    mul_a_c     = 33'(use_a0_c);\n"
     "    mul_b_c     = 33'(use_b0_c);",
     "    mul_a_c     = 33'(use_b0_c);\n"
     "    mul_b_c     = 33'(use_a0_c);"),
    # ---- the writeback port, which gained a GRANT on 2026-08-28 ------------
    # `wb_valid_o` was pure observation until then: a bare assign mirroring a
    # write already committed to this block's own register file. It is a
    # REQUEST now, because zhao_field_v3_svcpath's arbiter refuses the ALU by
    # design -- eight clocks per four-point group, measured -- and an
    # unrefusable write would lose every one of them.
    #
    # X33 IS A REGRESSION MUTANT FOR A REAL BUG. The write used to fire on
    # EVERY clock the instruction sat at S4, so a DOT wrote its destination
    # once per accumulation hold clock. The last write carried the right
    # value, so all 34 checks passed and nothing saw it -- until the port
    # could refuse and the transfer COUNTS diverged, 20 granted to 16 refused.
    ("X33 the write fires while the pipe is still HELD",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c;",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;"),
    # X35, X36 and X37 targeted the writeback HOLD and its grant, which was measured wrong and
    # removed the same day -- see zhao_probe_v3_exec.sv. They return with the
    # skid; against a block that no longer holds they are mutants that cannot
    # fail. X33 and X34 stay: `retire_hold_c` and `mul_denied_c` still gate the
    # write, and X33 is the regression mutant for the duplicate-write bug.
    # INVERTED on 2026-08-28. This used to remove `!mul_denied_c`; the term is
    # gone from the design because it was a LOST WRITE -- a denial does not
    # hold S4, so an instruction retires during one and suppressing its write
    # deletes it. The mutant now ADDS the term back, which is the defect.
    ("X34 the write is suppressed while a multiply is DENIED, and so is lost",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c;",
     "  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&\n"
     "                      !retire_hold_c && !mul_denied_c;"),
    # ---- operands held across a denial, 2026-08-28 -------------------------
    # The register file's read is a PIPELINE STAGE and a freeze does not freeze
    # it: the data arriving after a denial belongs to the instruction BEHIND the
    # stalled one. Without the hold, S3 pairs the stalled instruction's control
    # with its successor's operands -- 21 of 48 context-programs wrong, and
    # invisible with one context because the pipe is too empty for a denial to
    # land behind anything.
    # Reshaped: taking rf_a0 live orphans h_a0_r, read in exactly one place.
    # Inverting WHEN the hold applies keeps it live and states the same
    # defect -- the stalled instruction is paired with operands that are not
    # its own, just on the opposite clock.
    ("X38 the hold applies on the wrong clock, so the stale pair is used",
     "    use_a0_c = (opnd_held_r && !mul_denied_c) ? h_a0_r : rf_a0;",
     "    use_a0_c = (opnd_held_r && mul_denied_c) ? h_a0_r : rf_a0;"),
    ("X39 the hold is captured a clock late, after the file has moved on",
     "        if (!opnd_held_r) begin\n"
     "          opnd_held_r <= 1'b1;\n"
     "          h_a0_r <= rf_a0;",
     "        if (!opnd_held_r) begin\n"
     "          opnd_held_r <= 1'b1;\n"
     "          h_a0_r <= h_a1_r;"),
    ("X40 the hold is never released, so every later operand is stale",
     "      end else begin\n"
     "        opnd_held_r <= 1'b0;\n"
     "      end",
     "      end else begin\n"
     "        opnd_held_r <= opnd_held_r;\n"
     "      end"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique.

    MIXED LINE ENDINGS ARE REAL AND THEY DEFEAT A SINGLE GUESS. This used to
    pick one ending -- CRLF if the file contained any -- and translate the
    anchor to it. A file edited by a tool that writes LF into an otherwise
    CRLF file then has BOTH, and a multi-line anchor silently matches zero
    times in the region that differs. Two engine mutants failed exactly that
    way on 2026-08-28 while every single-line anchor in the same table worked.

    So both forms are tried. A multi-line anchor that matches under either is
    accepted; one that matches under neither still raises, and one that
    matches under both is still ambiguous and raises too.
    """
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
        "MULTIPLICATION IS COMMUTATIVE. The swap reaches only the operand mux's "
        "DEFAULT arm, which carries a0*b0; the DOT arms that supply a1*b1 and "
        "a2*b2 are untouched, and the ALU still receives a0_i and b0_i "
        "unswapped. Both operands are sign-extended to 33 bits by the same "
        "expression before the multiply, so the products are bit-identical "
        "rather than merely numerically equal. "
        "This proof was retracted once on the strength of a CONTAMINATED run "
        "-- see the note above the mutant -- and restored when a clean run "
        "showed it surviving. "
        "RE-SCORE THIS IF THE MULTIPLIER EVER TREATS ITS PORTS "
        "ASYMMETRICALLY: different widths, a signed/unsigned split, or an "
        "accumulate that reads one port twice.",    "X12":
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
