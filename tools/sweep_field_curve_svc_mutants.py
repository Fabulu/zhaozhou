#!/usr/bin/env python3
"""Mutation table for the FIELD v3 barrel curve service probe sweep
(tools/sweep_field_curve_svc.sh).

Target: fpga/rtl/synth/zhao_probe_curve_svc.sv ONLY — the probe has exactly
one consumer (guard 7).

Classes from reports/Fieldv3.md where they exist in this seam: wrong segment
(search depth, guard off-by-one, raw-vs-clamped, port slaving), stale entry
capture, stale table-cache meta (reload, wrong slot, swapped entry-0 lanes),
value law (mad y term, rounding, DCURVE lane), saturation-lane collapse,
reply clobber under backpressure, capacity violation, staging loss — plus
"the barrel de-pipelines", which proves the II gate in the directed test
actually bites.
"""

RTL = "fpga/rtl/synth/zhao_probe_curve_svc.sv"

MUTANTS = [
    ("C01 lanes 0/2 search only five steps",
     RTL,
     "    consume[0] = s_busy && !s_done && !n_busy && cyc[0] && (cyc <= 4'd11);",
     "    consume[0] = s_busy && !s_done && !n_busy && cyc[0] && (cyc <= 4'd9);"),

    # Reshaped for lint: deleting the whole clamp orphans meta_xn1
    # (UNUSEDSIGNAL); dropping only the LOWER bound keeps every signal read
    # and is the exact "search runs on raw a" defect below the table.
    ("C02 the lower clamp bound is dropped (search on raw a below x[0])",
     RTL,
     "      req_clamped[l] = (req_a[l] < meta_x0[req_tbl_i]) ? meta_x0[req_tbl_i]\n"
     "                     : ((req_a[l] > meta_xn1[req_tbl_i]) ? meta_xn1[req_tbl_i] : req_a[l]);",
     "      req_clamped[l] = (req_a[l] > meta_xn1[req_tbl_i]) ? meta_xn1[req_tbl_i] : req_a[l];"),

    ("C03 the mid guard is off by one (top knot never landed)",
     RTL,
     "      if (consume[l] && (s_mid[l] <= s_nm1) && (rd_x[l] <= s_clamped[l])) begin",
     "      if (consume[l] && (s_mid[l] < s_nm1) && (rd_x[l] <= s_clamped[l])) begin"),

    ("C04 the entry x is not captured on a taken step (stale x_ent)",
     RTL,
     "        upd_xe[l]  = rd_x[l];",
     "        upd_xe[l]  = s_xe[l];"),

    ("C05 entry-0 init reads dy0 into y (swapped meta lanes)",
     RTL,
     "            s_ye[l]      <= meta_y0[req_tbl_i];",
     "            s_ye[l]      <= meta_dy0[req_tbl_i];"),

    ("C06 port B is slaved to port A (lanes 2/3 search lane 0/1 addresses)",
     RTL,
     "    rb_addr = {s_tbl, cyc[0] ? s_mid[3][5:0] : s_mid[2][5:0]};",
     "    rb_addr = ra_addr;"),

    ("C07 the barrel de-pipelines (II gate must bite)",
     RTL,
     "  assign req_ready_o = !st_valid;",
     "  assign req_ready_o = !st_valid && !s_busy && (f_state == F_IDLE);"),

    ("C08 the add flags collapse onto lane 0",
     RTL,
     "          f_sat_add[l] <= sub_fired(s_clamped[l], upd_xe[l]);",
     "          f_sat_add[l] <= sub_fired(s_clamped[0], upd_xe[0]);"),

    # Reshaped for lint: dropping the y term entirely orphans f_ye
    # (UNUSEDSIGNAL); halving its shift keeps it read and is the same class
    # of mad-law defect.
    ("C09 the mad's y term is halved",
     RTL,
     "    for (int l = 0; l < LANES; l++) curve_p[l] = mul_p[l] + (sx(f_ye[l]) <<< 16);",
     "    for (int l = 0; l < LANES; l++) curve_p[l] = mul_p[l] + (sx(f_ye[l]) <<< 15);"),

    ("C10 the result rescale truncates instead of rounding half-up",
     RTL,
     "    logic signed [64:0] r;\n"
     "    begin\n"
     "      r = (65'(v) + (65'sd1 <<< 15)) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;",
     "    logic signed [64:0] r;\n"
     "    begin\n"
     "      r = 65'(v) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;"),

    ("C11 DCURVE returns y instead of dy",
     RTL,
     "          f_res[l] <= upd_dye[l];",
     "          f_res[l] <= upd_ye[l];"),

    ("C12 a held reply is clobbered by the next push (early reuse)",
     RTL,
     "        F_PUSH: begin\n"
     "          if (!rsp_valid_o || rsp_ready_i) begin",
     "        F_PUSH: begin\n"
     "          if (1'b1) begin"),

    ("C13 a fifth group is accepted while replies are blocked",
     RTL,
     "  assign req_ready_o = !st_valid;",
     "  assign req_ready_o = 1'b1;"),

    ("C14 the table-cache commit writes the wrong slot's entry count",
     RTL,
     "        meta_n[tl_tbl_i]   <= tl_n_i;",
     "        meta_n[~tl_tbl_i]  <= tl_n_i;"),

    ("C15 the staging register is never popped (group re-runs forever)",
     RTL,
     "      end else if (start_fire && st_valid) begin\n"
     "        st_valid <= 1'b0;",
     "      end else if (start_fire && st_valid) begin\n"
     "        st_valid <= st_valid;"),

    # ---- the refusal loop, added 2026-08-28 with mul_ready_i ---------------
    # The bank is shared and can say no. Before this port the finish stage
    # advanced out of F_ISSUE regardless and then waited for a product that a
    # refusal had never started -- a HANG, and one that could only appear
    # after the service was attached to the engine. Section 7 of the
    # differential refuses on a pseudo-random schedule; these attack the three
    # ways the hold can be got wrong.
    # Reshaped after the preflight refused it: dropping the condition removes
    # the ONLY read of mul_ready_i, so the port is orphaned and Verilator will
    # not build it. That is the preflight doing its job -- a mutant that cannot
    # build is a discard, not evidence.
    #
    # Sending the refused issue STRAIGHT TO F_PUSH keeps the port live and is
    # the same claim from the other side: the stage advances on a refused
    # issue. The pre-fix RTL hung waiting for a product nobody started; this
    # publishes the finish registers without one, so it fails by VALUE rather
    # than by timeout. Either way it is "a refusal is not noticed".
    ("C16 a refused issue advances anyway, publishing without its product",
     RTL,
     "        F_ISSUE: if (mul_ready_i) f_state <= F_WAIT;",
     "        F_ISSUE: if (mul_ready_i) f_state <= F_WAIT; else f_state <= F_PUSH;"),
    ("C17 the grant is read inverted, so it advances only when refused",
     RTL,
     "        F_ISSUE: if (mul_ready_i) f_state <= F_WAIT;",
     "        F_ISSUE: if (!mul_ready_i) f_state <= F_WAIT;"),
    # The request must stay asserted across a refusal or the retry never
    # happens. Gating the issue with the grant is the plausible-looking
    # version of that mistake, and it deadlocks: ready is only offered to a
    # claimant that is asking.
    ("C18 the request is withdrawn while it is being refused",
     RTL,
     "  assign mul_issue_o = (f_state == F_ISSUE) || ((f_state == F_SPL) && spl_mul_issue);",
     "  assign mul_issue_o = ((f_state == F_ISSUE) && mul_ready_i) || ((f_state == F_SPL) && spl_mul_issue);"),
    # C01 AND C18 WENT STALE ON 2026-08-29 and the preflight refused both.
    # Their anchors named lines the neighbour phase then edited: consume[0]
    # gained `!n_busy`, and mul_issue_o gained the SPLINE half of its mux.
    #
    # THIS IS THE ARGUMENT FOR RE-SCORING rather than carrying a score
    # forward. Both mutants were CAUGHT in the 18/18 run and both would have
    # stayed "caught" in any summary that trusted that number -- while
    # neither could be applied to the RTL at all. A stale anchor is not a
    # weaker test, it is NO test, and from the outside it looks exactly like
    # a passing one.
    # ---- the neighbour phase, added 2026-08-29 with SPLINE -----------------
    # The eighteen above were scored against a service that had no such phase.
    # A NEW PHASE IS NEW LOGIC AND THE OLD SCORE DOES NOT COVER IT -- these
    # attack it directly, and S01 and S10 are regression mutants for the two
    # defects that were actually in the shipped RTL during this run.
    ("S01 the neighbour phase completes one cycle early (stale p3)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """  assign lookup_complete = (s_mode == M_SPLINE) ? (n_busy && (ncyc == 3'd7))""",
     """  assign lookup_complete = (s_mode == M_SPLINE) ? (n_busy && (ncyc == 3'd6))"""),
    # THE REAL DEFECT. The table read is registered, so the datum addressed on
    # cycle 5 lands on cycle 6 as a non-blocking assignment -- after a handoff
    # on that same edge has sampled it. Completing at 6 hands the spline unit
    # the PREVIOUS group's p3. It showed as 96 of 6930, only on midpoints
    # (t != 0), only on the 64-knot table, and the same probe PASSED when run
    # alone because the stale register happened to hold the right number.
    ("S02 the low end wraps instead of replicating (i-1 reads the far end)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """      if (want < 9'sd0)                nb_idx = 6'd0;""",
     """      if (want < 9'sd0)                nb_idx = s_nm1[5:0];"""),
    ("S03 the high clamp is one short (i+1/i+2 stop at n-2)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """      else if (want > 9'(s_nm1))       nb_idx = s_nm1[5:0];""",
     """      else if (want > 9'(s_nm1))       nb_idx = s_nm1[5:0] - 6'd1;"""),
    ("S04 the capture reads the current cycle's address, not the previous",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """          automatic logic [2:0] prev  = ncyc - 3'd1;""",
     """          automatic logic [2:0] prev  = ncyc;"""),
    ("S05 t is not clamped at the top of the unit interval",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """      else if (r > 32'sd65536)   spline_t = 32'sd65536;""",
     """      else if (r > 32'sd65536)   spline_t = r;"""),
    ("S06 the search keeps consuming through the neighbour phase",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """    consume[0] = s_busy && !s_done && !n_busy && cyc[0] && (cyc <= 4'd11);""",
     """    consume[0] = s_busy && !s_done && cyc[0] && (cyc <= 4'd11);"""),
    # ALSO A REAL DEFECT, found earlier the same run: the neighbour reads went
    # through the same compare-and-step logic and walked the segment down to
    # n-1. 173 failures.
    ("S07 p1 takes a neighbour instead of the search's own entry",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """          f_p1[l]   <= upd_ye[l];""",
     """          f_p1[l]   <= s_p0[l];"""),
    ("S08 the group is offered to the spline unit every cycle (sent twice)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """          if (spl_v_valid && spl_v_ready) f_spl_offered <= 1'b1;""",
     """          if (spl_v_valid && spl_v_ready) f_spl_offered <= 1'b0;"""),
    ("S09 the spline unit is never granted the multiplier (hang)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """  assign spl_mul_ready = mul_ready_i && (f_state == F_SPL);""",
     """  assign spl_mul_ready = mul_ready_i && (f_state == F_WAIT);"""),
    ("S10 the phase runs one cycle too long (p3 overwritten by a re-read)",
     "fpga/rtl/synth/zhao_probe_curve_svc.sv",
     """        if (ncyc != 3'd7) ncyc <= ncyc + 3'd1;""",
     """        if (ncyc != 3'd6) ncyc <= ncyc + 3'd1;"""),
]

# Machine-readable, so a survivor is either PROVEN equivalent here or fails
# the sweep. Nothing is declared until the first run says what survives.
EQUIVALENT = {}


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
