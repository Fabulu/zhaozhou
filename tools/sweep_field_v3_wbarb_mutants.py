#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_wbarb.sv (FIELD.V3.WBARB).

WHAT THIS BLOCK CLAIMS
----------------------
It computes nothing. It decides who gets the register file's single write port,
and every failure looks the same from a distance: a write happens, to a
plausible register, with a plausible value.

  * each policy picks the claimant it says it does;
  * ready is ONE-HOT, and never fires when nobody asked;
  * the port carries the WINNER's context, register and value -- all three,
    independently, because a mux crossed in one field only is the kind of
    defect that reads as a data bug somewhere else entirely;
  * round robin starves nobody and costs no throughput;
  * `served_o` counts grants and `stalled_o` counts LOSSES, not requests.

THE LAST ONE HAS ALREADY BITTEN THIS PROJECT. The multiplier bank's M08 mutant
-- counting requests instead of losses -- survived its first sweep, because
every test there had the claimant losing on every clock, where the two counters
read identically. The distinguishing case is a claimant that asks and WINS, and
it is section 2 of this block's differential.

PREFER MUTATING A VALUE OVER DELETING A USE (the rule three preflight refusals
taught on 2026-08-28, written up in sweep_field_v3_dispatch_mutants.py):
deleting a use orphans a signal, Verilator refuses it, and an unbuildable
mutant is a discard rather than evidence.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_wbarb.sv"

MUTANTS = [
    # ---- the fixed policies -------------------------------------------------
    # The scan runs so the LAST assignment wins, so the direction IS the
    # policy. Reversing one is the whole defect and needs no other change.
    ("W01 policy 0 picks the highest claimant instead of the lowest",
     "      for (int c = CLAIMANTS - 1; c >= 0; c--) begin\n"
     "        if (req_valid_i[c]) begin\n"
     "          winner_c = CW'(c);\n"
     "          any_c    = 1'b1;\n"
     "        end\n"
     "      end\n"
     "    end else if (policy_i == 2'd1) begin",
     "      for (int c = 0; c < CLAIMANTS; c++) begin\n"
     "        if (req_valid_i[c]) begin\n"
     "          winner_c = CW'(c);\n"
     "          any_c    = 1'b1;\n"
     "        end\n"
     "      end\n"
     "    end else if (policy_i == 2'd1) begin"),
    ("W02 policy 1 picks the lowest claimant instead of the highest",
     "    end else if (policy_i == 2'd1) begin\n"
     "      for (int c = 0; c < CLAIMANTS; c++) begin",
     "    end else if (policy_i == 2'd1) begin\n"
     "      for (int c = CLAIMANTS - 1; c >= 0; c--) begin"),
    ("W03 policy 0 is decoded as policy 1, so the lowest-first arm is dead",
     "    if (policy_i == 2'd0) begin",
     "    if (policy_i == 2'd1) begin"),

    # ---- round robin --------------------------------------------------------
    ("W04 round robin never advances, so it is a fixed priority in disguise",
     "        rr_r <= (int'(winner_c) + 1 >= CLAIMANTS) ? '0 : CW'(int'(winner_c) + 1);",
     "        rr_r <= rr_r;"),
    ("W05 round robin advances by two, so one claimant is skipped every turn",
     "        rr_r <= (int'(winner_c) + 1 >= CLAIMANTS) ? '0 : CW'(int'(winner_c) + 1);",
     "        rr_r <= (int'(winner_c) + 2 >= CLAIMANTS) ? '0 : CW'(int'(winner_c) + 2);"),
    ("W06 the rotation runs under EVERY policy, so a fixed priority drifts",
     "      if ((policy_i == 2'd2) && any_c) begin",
     "      if (any_c) begin"),
    ("W07 round robin starts one claimant late",
     "        automatic logic [CW-1:0] c = CW'((int'(rr_r) + k) % CLAIMANTS);",
     "        automatic logic [CW-1:0] c = CW'((int'(rr_r) + k + 1) % CLAIMANTS);"),

    # ---- the port -----------------------------------------------------------
    # Each field separately: a mux crossed in ONE field is the defect that
    # reads as a data bug somewhere else entirely.
    ("W08 the context comes from claimant 0 whoever won",
     "  assign wr_ctx_o  = req_ctx_i[winner_c];",
     "  assign wr_ctx_o  = req_ctx_i[0];"),
    ("W09 the register number comes from claimant 0 whoever won",
     "  assign wr_reg_o  = req_reg_i[winner_c];",
     "  assign wr_reg_o  = req_reg_i[0];"),
    ("W10 the data comes from claimant 0 whoever won",
     "  assign wr_data_o = req_data_i[winner_c];",
     "  assign wr_data_o = req_data_i[0];"),
    ("W11 a write is asserted every clock, including when nobody asked",
     "  assign wr_en_o   = any_c;",
     "  assign wr_en_o   = 1'b1;"),

    # ---- ready --------------------------------------------------------------
    ("W12 every asking claimant is told it won, so ready is not one-hot",
     "      req_ready_o[c] = any_c && (winner_c == CW'(c));",
     "      req_ready_o[c] = any_c && req_valid_i[c];"),
    ("W13 ready fires with no request outstanding",
     "      req_ready_o[c] = any_c && (winner_c == CW'(c));",
     "      req_ready_o[c] = (winner_c == CW'(c));"),

    # ---- the counters -------------------------------------------------------
    # W15 is the M08 defect, moved to this block. Section 2 -- a lone claimant
    # that asks and WINS -- is the only shape that separates it from the truth.
    ("W14 served_o counts requests rather than grants",
     "        if (req_valid_i[c] && req_ready_o[c]) served_o[c] <= served_o[c] + 32'd1;",
     "        if (req_valid_i[c]) served_o[c] <= served_o[c] + 32'd1;"),
    ("W15 stalled_o counts REQUESTS rather than losses -- the M08 defect",
     "        if (req_valid_i[c] && !req_ready_o[c]) stalled_o[c] <= stalled_o[c] + 32'd1;",
     "        if (req_valid_i[c]) stalled_o[c] <= stalled_o[c] + 32'd1;"),
    ("W16 stalled_o counts every claimant that did not win, asking or not",
     "        if (req_valid_i[c] && !req_ready_o[c]) stalled_o[c] <= stalled_o[c] + 32'd1;",
     "        if (!req_ready_o[c]) stalled_o[c] <= stalled_o[c] + 32'd1;"),
    ("W17 the two counters are swapped",
     "        if (req_valid_i[c] && req_ready_o[c]) served_o[c] <= served_o[c] + 32'd1;",
     "        if (req_valid_i[c] && !req_ready_o[c]) served_o[c] <= served_o[c] + 32'd1;"),
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
