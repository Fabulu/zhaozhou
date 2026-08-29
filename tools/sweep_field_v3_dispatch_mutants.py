#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_dispatch.sv (FIELD.V3.DISPATCH).

WHAT THIS BLOCK CLAIMS
----------------------
It computes no answer, so there is nothing in the reference to differ against.
Every claim is about ROUTING, and every routing failure looks the same from a
distance: the machine keeps running and a value lands somewhere plausible.
That is what the mutants below are aimed at.

  * a context's operands reach ITS OWN lane, and that lane's results reach
    ITS OWN context's registers;
  * a partial group ISSUES rather than waiting for a fourth context that may
    never come;
  * a padded lane is visibly not data and is NEVER written;
  * a context is released after its LAST register, never before;
  * the drain is serial and lossless under backpressure;
  * an offer arriving with flush_i is refused rather than swallowed.

THE LAST ONE IS A REAL HOLE THAT EXISTED, for one lint cycle: without
`!flush_i` in long_ready_o, a context offered on the flush clock is accepted
into the group, lands outside the snapshot, and is then cleared -- handshaked
and gone. D14 is that exact defect.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_dispatch.sv"

MUTANTS = [
    # ---- the third issue term, added 2026-08-29 with the deadlock fix ------
    # D29 IS THE DEADLOCK ITSELF. Requiring flush_i alongside the new term makes
    # it add nothing the flush term did not already give, which is exactly the
    # pre-fix machine -- and it keeps every signal read, so it builds. Two
    # contexts on different long ops then wait on each other forever, so this
    # is caught by a TIMEOUT rather than a wrong value.
    ("D29 the group is not closed for a context that cannot join (the deadlock)",
     "                        (long_valid_i && !same_group_c));",
     "                        (flush_i && long_valid_i && !same_group_c));"),

    # And the other side of it: closing on a stale mismatch when nobody is
    # actually asking. same_group_c reads the OFFERED op, which is meaningless
    # while long_valid_i is low, so this issues short groups for no reason.
    ("D30 the group is closed on a mismatch nobody is offering",
     "                        (long_valid_i && !same_group_c));",
     "                        (!same_group_c));"),

    # ---- the gather ---------------------------------------------------------
    ("D01 a context lands in the wrong lane of the group",
     "        g_ctx_r[fill_r[1:0]] <= long_ctx_i;",
     "        g_ctx_r[3 - fill_r[1:0]] <= long_ctx_i;"),
    ("D02 the operands land in a different lane than the context",
     "        g_s0_r[fill_r[1:0]]  <= long_s0_i;",
     "        g_s0_r[3 - fill_r[1:0]]  <= long_s0_i;"),
    ("D03 a fifth context overwrites the fourth",
     "                        (fill_r < 3'd4) && !flush_i &&",
     "                        (fill_r <= 3'd4) && !flush_i &&"),
    # Reshaped: dropping the term orphans same_group_c, and the linter refuses
    # an unused signal. Inverting keeps the operand and is a real defect -- a
    # context can ONLY join a group it does not match.
    ("D04 a context with a different op or destination joins anyway",
     "                        (dst_width_of(long_op_i) != 2'd0) && same_group_c;",
     "                        (dst_width_of(long_op_i) != 2'd0) && !same_group_c;"),
    ("D05 a short op is accepted as a long one",
     "                        (dst_width_of(long_op_i) != 2'd0) && same_group_c;",
     "                        (dst_width_of(long_op_i) == 2'd0) && same_group_c;"),

    # ---- the instruction's immediate, added 2026-08-28 ---------------------
    # Found by trying to compose this with the noise unit: NOISE2's seed IS the
    # immediate, and two different NOISE2 instructions with different seeds
    # match on op AND destination. D26 lets them share a request, which hands
    # four points one seed and answers three of them for a different program
    # point -- values that are individually plausible and collectively wrong.
    ("D26 the immediate is not part of the group key",
     "                        ((long_op_i == g_op_r) && (long_dst_i == g_dst_r) &&\n"
     "                         (long_imm_i == g_imm_r));",
     "                        ((long_op_i == g_op_r) && (long_dst_i == g_dst_r));"),
    ("D27 the request carries the WRONG group's immediate",
     "            s_imm_r[tail_r]   <= g_imm_r;",
     "            s_imm_r[tail_r]   <= g_imm_r + 32'd1;"),
    ("D28 the gathered immediate is never captured",
     "        g_imm_r <= long_imm_i;",
     "        g_imm_r <= g_imm_r;"),

    # ---- the issue rule -----------------------------------------------------
    ("D06 a partial group is never issued, so one context alone DEADLOCKS",
     "                       ((fill_r == 3'd4) || (flush_i && (fill_r != 3'd0)) ||",
     "                       ((fill_r == 3'd4) || (1'b0 && flush_i && (fill_r != 3'd0)) ||"),
    ("D07 an EMPTY group is issued on flush",
     "                       ((fill_r == 3'd4) || (flush_i && (fill_r != 3'd0)) ||",
     "                       ((fill_r == 3'd4) || flush_i ||"),
    ("D08 the group issues one context early",
     "                       ((fill_r == 3'd4) || (flush_i && (fill_r != 3'd0)) ||",
     "                       ((fill_r == 3'd3) || (flush_i && (fill_r != 3'd0)) ||"),

    # ---- the pad ------------------------------------------------------------
    # Zero is a plausible coordinate, which is the whole reason the pad is
    # 3/5/7. Written as a change to the PARAMETER'S VALUE rather than to its
    # use: replacing `PAD_A` with a literal removes the only reference and the
    # linter refuses the orphaned parameter. That is the third time this class
    # has come up today -- C16, N01 and now D09 -- and the rule that falls out
    # is at the bottom of this file.
    ("D09 the pad is zero, which is a plausible coordinate",
     "  localparam logic signed [31:0] PAD_A = 32'sd3;",
     "  localparam logic signed [31:0] PAD_A = 32'sd0;"),
    ("D10 every lane is treated as real, so pads are sent as data",
     "      if (3'(l) < s_used_r[tail_r]) begin",
     "      if (3'(l) <= s_used_r[tail_r]) begin"),

    # ---- the slot snapshot --------------------------------------------------
    ("D11 the slot records the wrong destination base",
     "            s_dst_r[tail_r]   <= g_dst_r;",
     "            s_dst_r[tail_r]   <= g_dst_r + REGW'(1);"),
    ("D12 the slot records a full group however many joined",
     "            s_used_r[tail_r]  <= fill_r;",
     "            s_used_r[tail_r]  <= 3'd4;"),
    ("D13 the destination width is one register whatever the op",
     "            s_width_r[tail_r] <= dst_width_of(g_op_r);",
     "            s_width_r[tail_r] <= 2'd1;"),

    # ---- the lost-context hole, which was REAL -----------------------------
    ("D14 an offer arriving with flush is accepted and then thrown away",
     "                        (fill_r < 3'd4) && !flush_i &&",
     "                        (fill_r < 3'd4) &&"),

    # ---- the drain ----------------------------------------------------------
    ("D15 the drain writes every lane, including the padded ones",
     "              if (d_lane_r + 3'd1 >= s_used_r[head_r]) begin",
     "              if (d_lane_r + 3'd1 >= 3'd4) begin"),
    ("D16 the drain stops one lane early",
     "              if (d_lane_r + 3'd1 >= s_used_r[head_r]) begin",
     "              if (d_lane_r + 3'd1 >= s_used_r[head_r] - 3'd1) begin"),
    ("D17 the destination register does not advance with the member",
     "  assign wb_reg_o   = s_dst_r[head_r] + REGW'(d_memb_r);",
     "  assign wb_reg_o   = s_dst_r[head_r];"),
    ("D18 the writeback goes to a different context than the lane it came from",
     "  assign wb_ctx_o   = s_ctx_r[head_r][d_lane_r[1:0]];",
     "  assign wb_ctx_o   = s_ctx_r[head_r][3 - d_lane_r[1:0]];"),
    ("D19 the members are drained in the wrong order",
     "      2'd0:    wb_data_c = r0_r[head_r][d_lane_r[1:0]];\n"
     "      2'd1:    wb_data_c = r1_r[head_r][d_lane_r[1:0]];",
     "      2'd0:    wb_data_c = r1_r[head_r][d_lane_r[1:0]];\n"
     "      2'd1:    wb_data_c = r0_r[head_r][d_lane_r[1:0]];"),
    ("D20 the drain ignores backpressure and runs at one write per clock",
     "        DR_RUN: begin\n"
     "          if (wb_ready_i) begin",
     "        DR_RUN: begin\n"
     "          if (1'b1) begin"),

    # ---- the release --------------------------------------------------------
    ("D21 a context is released before its LAST register lands",
     "  assign rel_valid_o = wb_valid_o && wb_ready_i &&\n"
     "                       (d_memb_r == 2'(s_width_r[head_r] - 2'd1));",
     "  assign rel_valid_o = wb_valid_o && wb_ready_i &&\n"
     "                       (d_memb_r == 2'd0);"),
    ("D22 the release names a different context than the one written",
     "  assign rel_ctx_o   = s_ctx_r[head_r][d_lane_r[1:0]];",
     "  assign rel_ctx_o   = s_ctx_r[head_r][3 - d_lane_r[1:0]];"),
    ("D23 a context is released even when the write was refused",
     "  assign rel_valid_o = wb_valid_o && wb_ready_i &&\n"
     "                       (d_memb_r == 2'(s_width_r[head_r] - 2'd1));",
     "  assign rel_valid_o = wb_valid_o &&\n"
     "                       (d_memb_r == 2'(s_width_r[head_r] - 2'd1));"),

    # ---- the tag ------------------------------------------------------------
    ("D24 every group carries the same tag",
     "            next_tag_r <= next_tag_r + TAGW'(1);",
     "            next_tag_r <= next_tag_r;"),
    ("D25 the tag guard never reports a mismatch",
     "        if (!rsp_hit_c) begin",
     "        if (rsp_hit_c) begin"),
    # ---- the in-flight queue, added 2026-08-29 ---------------------------
    # Three of these are regressions for bugs that were really in this block
    # while it was being written. They are in the table so the next person
    # meets them as a failing check rather than as a hang.

    # D31 IS THE MODULO-BY-ZERO. SW'(OUTSTANDING) truncated the depth to the
    # pointer width -- 1'(2) == 0 -- so no pointer ever advanced and two groups
    # in flight deadlocked outright. It passed at depth 1 because modulo 1 is
    # 0, which is the right answer for the wrong reason.
    ("D31 the slot pointer never advances (the modulo-by-zero deadlock)",
     "    next_slot = (p == SW'(OUTSTANDING - 1)) ? SW'(0) : SW'(p + SW'(1));",
     "    next_slot = (p == SW'(OUTSTANDING - 1)) ? SW'(0) : p;"),

    # D32: the drain starting a clock late. Every consumer that expects the
    # write port live immediately then sees ZERO writes rather than wrong
    # ones, which reads as a dead block rather than a broken one.
    ("D32 the drain waits a clock instead of starting on the capture",
     "              (s_done_r[head_r] ||\n"
     "               (rsp_valid_i && rsp_ready_o && rsp_hit_c && (rsp_slot_c == head_r)))) begin",
     "              (s_done_r[head_r] ||\n"
     "               (1'b0 && rsp_valid_i && rsp_ready_o && rsp_hit_c && (rsp_slot_c == head_r)))) begin"),

    # D33: a response landing in the OLDEST slot rather than the one its tag
    # names. Harmless while one group is in flight and silent corruption the
    # moment two are -- the younger group's answer overwrites the older's.
    ("D33 a response is captured into the oldest slot, not the tagged one",
     "            r0_r[rsp_slot_c][l] <= rsp_r0_i[l];",
     "            r0_r[head_r][l] <= rsp_r0_i[l];"),

    # D34: accepting a context into a group that cannot be issued. The
    # executor has handshaked it away and parked it, so nothing will ever take
    # it -- a lost instruction, not a slow one.
    ("D34 ready is not gated on queue room, so a context is stranded",
     "  assign long_ready_o = (istate_r == I_GATHER) && (count_r != (SW+1)'(OUTSTANDING)) &&",
     "  assign long_ready_o = (istate_r == I_GATHER) &&"),

    # D35: draining the NEWEST group first. Write ordering and release timing
    # are the half of this block the queue was not allowed to change.
    ("D35 the drain runs newest-first, breaking write ordering",
     "          if ((count_r != '0) &&\n"
     "              (s_done_r[head_r] ||",
     "          if ((count_r != '0) &&\n"
     "              (s_done_r[tail_r] ||"),
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


# ---------------------------------------------------------------------------
# THE RULE THREE PREFLIGHT REFUSALS TAUGHT, ON ONE DAY
# ---------------------------------------------------------------------------
# PREFER MUTATING A VALUE OVER DELETING A USE.
#
# Three mutants were refused by their preflights on 2026-08-28, all for the
# same reason and all written the same way:
#
#   C16  dropped the only read of `mul_ready_i`      -> orphaned port
#   N01  replaced `s_mix[l]` with `s_reg[l]`         -> orphaned signal
#   D09  replaced `PAD_A` with a literal             -> orphaned parameter
#
# Verilator refuses an unused signal, port or parameter, so each was
# unbuildable -- and a mutant that cannot build is a DISCARD, not evidence. It
# scores nothing and it costs a whole rebuild to find out.
#
# The fix is the same every time and it usually produces a BETTER mutant:
# change what a thing IS rather than whether it is referenced. D09 now sets
# PAD_A to zero instead of bypassing it; N01 replays a neighbour's mix instead
# of no mix, which is sharper because it is wrong per point; C16 sends a
# refused issue onward instead of deleting the check, which fails by value
# rather than by timeout.
#
# The preflight catches all of these before anything is scored. The rule exists
# so the catch is rare rather than routine.

# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. NOTHING IS DECLARED PREDICTIVELY -- every entry is written only after
# a run has shown the mutant surviving.
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
