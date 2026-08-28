#!/usr/bin/env python3
"""The mutant table for zhao_probe_v3_engine.sv (FIELD.V3.ENGINE).

WHY THIS SWEEP EXISTS SEPARATELY FROM THE EXECUTOR'S AND THE BANK'S
-------------------------------------------------------------------
Both halves are already swept: the executor at 31/31 and the bank at 14
mutants of its own. This file exists for the thing NEITHER can test, because
it does not exist in either one alone -- REFUSAL.

Standing by itself the executor's multiplier always answered. Behind an
arbiter it can be denied, and the register file holds an instruction's
operands for exactly ONE clock. So an instruction whose product the bank
declined to start has lost its operands by the next clock, and issue must
stall rather than carry on. That path is created by the composition and is
reachable only here.

WHAT ITS FAILURES LOOK LIKE
---------------------------
The engine keeps running. Every context still retires, the counters still
count, and a program that never contends still produces the right answer --
which is most programs in a microbenchmark. The wrongness appears only when
the bank is busy, and then it appears as a value computed from operands that
belonged to a different instruction. That is why the engine carries a rival
claimant at all: without one, every mutant below is unreachable and the sweep
would score a clean sheet on a block whose central claim was never exercised.

The three claims worth attacking:

  * A REFUSED REQUEST STALLS ISSUE. Not "retries later" -- the operands are
    gone. Not "proceeds with what it has" -- what it has is nothing.
  * THE EXECUTOR IS CLAIMANT 0 AND THE RIVAL IS CLAIMANT 1. Swap them and
    the declared priority silently reverses, which a test that only watches
    values will not notice.
  * THE UNUSED BANK LANES ARE NOT ZERO. Zero is a plausible product. They are
    tied to recognisable constants so a routing bug that reaches them looks
    wrong rather than convincing.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/synth/zhao_probe_v3_engine.sv"
EXEC = "fpga/rtl/synth/zhao_probe_v3_exec.sv"

# Entries are (name, old, new) against RTL, or (name, path, old, new) when the
# mutation lands in another file of the cone. The refusal STALL lives in the
# executor but is only reachable through this composition, so it is swept from
# here rather than from a sweep that cannot exercise it.
MUTANTS = [
    # ---- the refusal path, which exists only in the composition ------------
    # Reshaped: dropping the term orphaned mul_denied_c, and a tautology is a
    # constant expression the linter also refuses. INVERTING it keeps the
    # operand and is a real defect -- issue happens only when the bank has
    # just refused, which is the exact opposite of the rule.
    ("E01 a refused request does not stall issue, so operands are lost",
     EXEC,
     "    issue_c = |ready_c && !dot_inflight_c && !hold_c && !mul_denied_c;",
     "    issue_c = |ready_c && !dot_inflight_c && !hold_c && mul_denied_c;"),
    ("E02 the denial is read from the wrong direction",
     EXEC,
     "  assign mul_denied_c = mul_req_valid_o && !mul_req_ready_i;",
     "  assign mul_denied_c = !mul_req_valid_o && mul_req_ready_i;"),
    ("E03 a denial is only noticed when nothing was requested",
     EXEC,
     "  assign mul_denied_c = mul_req_valid_o && !mul_req_ready_i;",
     "  assign mul_denied_c = mul_req_valid_o && mul_req_ready_i;"),

    # ---- who is which claimant --------------------------------------------
    # Reordering two independent assignments inside an always_comb is
    # SEMANTICALLY IDENTICAL and would come back byte-identical. The sources
    # are what must swap.
    ("E04 the executor and the rival swap claimant slots",
     "    bank_req_valid[0] = ex_req_valid;\n"
     "    bank_req_valid[1] = rival_req_i;",
     "    bank_req_valid[0] = rival_req_i;\n"
     "    bank_req_valid[1] = ex_req_valid;"),
    # Reshaped: reading slot 1 alone orphaned slot 0. ANDing keeps both and
    # keeps the defect -- the executor is only granted when the rival is too.
    ("E05 the executor is granted only when the rival is granted as well",
     "  assign ex_req_ready = bank_req_ready[0];",
     "  assign ex_req_ready = bank_req_ready[0] && bank_req_ready[1];"),
    # Reshaped for the same reason as E05.
    ("E06 the executor accepts a reply meant for either claimant",
     "  assign ex_rsp_valid = bank_rsp_valid[0];",
     "  assign ex_rsp_valid = bank_rsp_valid[0] || bank_rsp_valid[1];"),
    # Reshaped: a constant orphaned the whole ready vector.
    ("E07 the executor is told it was granted whenever ANYONE was",
     "  assign ex_req_ready = bank_req_ready[0];",
     "  assign ex_req_ready = |bank_req_ready;"),

    # ---- the operands that reach the bank ----------------------------------
    ("E08 the executor's operands are swapped on the way to the bank",
     "        bank_a[c][l] = (c == 0) ? ex_req_a : 33'sd3;\n"
     "        bank_b[c][l] = (c == 0) ? ex_req_b : 33'sd5;",
     "        bank_a[c][l] = (c == 0) ? ex_req_b : 33'sd3;\n"
     "        bank_b[c][l] = (c == 0) ? ex_req_a : 33'sd5;"),
    ("E09 the rival's operands are zero, which is a plausible product",
     "        bank_b[c][l] = (c == 0) ? ex_req_b : 33'sd5;",
     "        bank_b[c][l] = (c == 0) ? ex_req_b : 33'sd0;"),
    ("E10 the claimants share one tag, so replies cannot be told apart",
     "      bank_tag[c] = 8'(c);",
     "      bank_tag[c] = 8'd0;"),

    # ---- the reply the executor consumes -----------------------------------
    ("E11 the executor consumes a lane it does not own",
     "  assign ex_rsp_p     = bank_p[0];",
     "  assign ex_rsp_p     = bank_p[1];"),
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
# sweep. Nothing is declared until the first run says what actually survives,
# and NOTHING IS DECLARED PREDICTIVELY -- that rule was broken twice on
# FIELD.V3.EXEC, with two proofs written before any evidence and both then
# contradicted by a run.
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
