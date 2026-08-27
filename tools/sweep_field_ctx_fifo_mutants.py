#!/usr/bin/env python3
"""Mutation table for the FIELD v3 ready-context FIFO scheduler probe sweep
(tools/sweep_field_ctx_fifo.sh).

Target: fpga/rtl/synth/zhao_probe_ctx_fifo.sv ONLY (single consumer).

Classes from reports/Fieldv3.md: lost or duplicated context IDs, reply to
the wrong context, early context reuse, one-instruction-in-flight violations
— plus two REGRESSION PINS for defects this probe's own bring-up found:
F02 re-creates the service-completion starvation the first draft had (the
single-write-port wedge), and F07 re-creates the busy-context restart that
section 5 exists to refuse.
"""

RTL = "fpga/rtl/synth/zhao_probe_ctx_fifo.sv"

MUTANTS = [
    ("F01 the requeue enqueues the WRONG context (S0's, not S1's)",
     RTL,
     "  assign enq_ctx   = s1_ctx;",
     "  assign enq_ctx   = s0_ctx;"),

    ("F02 the service completion is never taken (duplicate flood — the starvation regression)",
     RTL,
     "  assign svc_take_done = svc_done_valid;",
     "  assign svc_take_done = 1'b0;"),

    # F03 first ran named "fetch/walk skew" and SURVIVED — correctly: see
    # EQUIVALENT. The REAL cross-context defect is F13.
    ("F03 the pc advances one stage later, at S1 (proven equivalent)",
     RTL,
     "      if (s0_valid) c_pc[s0_ctx] <= c_pc[s0_ctx] + 5'd1;",
     "      if (s1_valid) c_pc[s1_ctx] <= c_pc[s1_ctx] + 5'd1;"),

    ("F04 the last-instruction compare is off by one (END never fires)",
     RTL,
     "      s1_last  <= s0_valid && (c_pc[s0_ctx] == c_len[s0_ctx] - 5'd1);",
     "      s1_last  <= s0_valid && (c_pc[s0_ctx] == c_len[s0_ctx]);"),

    ("F05 a long op ALSO requeues immediately (duplicate + latency violation)",
     RTL,
     "  assign short_requeue = s1_valid && !s1_last && !is_long;",
     "  assign short_requeue = s1_valid && !s1_last;"),

    ("F06 a long LAST op never releases its context",
     RTL,
     "  assign done_valid_o = s1_valid && s1_last;",
     "  assign done_valid_o = s1_valid && s1_last && !is_long;"),

    ("F07 a BUSY context can be restarted (the section-5 regression)",
     RTL,
     """  assign start_ready_o = !svc_done_valid && (rq_count <= (CW + 1)'(CTX - 2)) &&
                         !c_busy[start_ctx_i];""",
     """  assign start_ready_o = !svc_done_valid && (rq_count <= (CW + 1)'(CTX - 2)) &&
                         (!c_busy[start_ctx_i] || 1'b1);"""),

    ("F08 the dequeue does not mark the context in flight",
     RTL,
     "        c_inflight[rq[rq_rd[CW-1:0]]] <= 1'b1;",
     "        c_inflight[rq[rq_rd[CW-1:0]]] <= 1'b0;"),

    ("F09 a dual-enqueue cycle advances the write pointer by ONE (lost context)",
     RTL,
     "          rq_wr <= rq_wr + (CW + 1)'(2);",
     "          rq_wr <= rq_wr + (CW + 1)'(1);"),

    ("F10 the reported pc comes from the WRONG context's walk",
     RTL,
     "      s1_pc    <= c_pc[s0_ctx];",
     "      s1_pc    <= c_pc[s1_ctx];"),

    ("F11 the service latency is halved (early context reuse)",
     RTL,
     "          svc_cnt[s1_ctx]  <= plan_q[4:0];",
     "          svc_cnt[s1_ctx]  <= plan_q[4:0] >> 1;"),

    ("F12 the last instruction issues invisibly (lost issue)",
     RTL,
     "  assign issue_valid_o = s1_valid;",
     "  assign issue_valid_o = s1_valid && !s1_last;"),

    ("F13 the pc of the context in S0 advances on behalf of the one in S1",
     RTL,
     "      if (s0_valid) c_pc[s0_ctx] <= c_pc[s0_ctx] + 5'd1;",
     "      if (s1_valid) c_pc[s0_ctx] <= c_pc[s0_ctx] + 5'd1;"),
]

# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {
    "F03": (
        "PROVEN EQUIVALENT. A context is never in S0 and S1 in the same "
        "cycle (it re-enters the ready queue only after its S2 requeue, so "
        "the earliest re-dequeue is two cycles after it left S0), and every "
        "reader of c_pc[x] - the plan fetch address, s1_pc, s1_last, and the "
        "next dequeue - either samples BEFORE the S0-exit increment or "
        "AFTER the S1-exit increment, which bracket no reader. A host "
        "restart cannot interleave either: start_ready requires the context "
        "FREE, which happens at S2, after both increment points. "
        "RE-SCORE THIS THE MOMENT a context can be re-dequeued one cycle "
        "after issue (e.g. a 0-cycle requeue path) or c_pc gains another "
        "reader between S0 and S1."
    ),
}


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


if __name__ == "__main__":
    import sys

    # LF-only stdout: bash command substitution must not capture CRs
    sys.stdout.reconfigure(newline="\n")

    if len(sys.argv) >= 2 and sys.argv[1] == "--count":
        print(len(MUTANTS))
    elif len(sys.argv) >= 3 and sys.argv[1] == "--name":
        print(MUTANTS[int(sys.argv[2])][0])
    elif len(sys.argv) >= 3 and sys.argv[1] == "--file":
        print(MUTANTS[int(sys.argv[2])][1])
    elif len(sys.argv) >= 3 and sys.argv[1] == "--apply":
        idx = int(sys.argv[2])
        name, path, old, new = MUTANTS[idx]
        with open(path, "r", encoding="utf-8", newline="") as f:
            gold = f.read()
        out = mutate(gold, old, new)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(out)
        print("applied %s to %s" % (name.split()[0], path))
    elif len(sys.argv) >= 3 and sys.argv[1] == "--equiv":
        tok = sys.argv[2]
        if tok in EQUIVALENT:
            print(EQUIVALENT[tok])
        else:
            sys.exit(1)
    else:
        print("usage: --count | --name N | --file N | --apply N | --equiv TOK")
        sys.exit(2)
