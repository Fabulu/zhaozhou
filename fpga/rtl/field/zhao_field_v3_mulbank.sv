// zhao_field_v3_mulbank.sv — the four-wide vector multiplier bank and its
// arbiter. Field v3 Phase 4.
//
// WHY THIS EXISTS
// ---------------
// `reports/FIELD_V3_SERVICE_ATTACH.md` found that the multiplier bank is not
// a lane's private property. `zhao_probe_curve_svc` contains no multiplier at
// all — it DRIVES one, and says so: "the vector multiplier bank (engine
// property, not probe silicon)". The distance service is the same shape. So
// one four-wide bank has three claimants:
//
//   * the four ALU lanes, one product each per MUL/MAD;
//   * the curve service, while stepping a lookup;
//   * the distance service, while squaring and summing.
//
// Nothing arbitrated that. This does.
//
// WHAT THE BANK IS
// ----------------
// Four `zhao_field_mul` lanes, which the brief prices at "four 33-bit lanes
// map to about 12 DSPs". Each is two clocks deep and FULLY PIPELINED — it
// accepts an issue every clock — so the bank sustains one four-wide request
// per clock and replies arrive in issue order, two clocks later.
//
// That pipelining is the whole reason this arbiter can be simple. There is no
// reorder buffer and no reply queue: a two-stage tag shadow carries the
// winner's identity alongside its operands, and the reply is routed by whose
// tag emerges. Issue order IS reply order.
//
// THE PRIORITY, and why it points this way
// -----------------------------------------
// Fixed priority, SERVICES ABOVE LANES. A service that is waiting holds a
// context hostage — that context has left the ready set entirely and cannot
// make progress until its reply lands. A lane that is waiting simply issues
// one clock later and its context is still in the barrel. So the claimant
// whose stall is expensive wins.
//
// CORRECTED 2026-08-28: THIS IS NOT A CHOICE. It is a requirement, and round
// robin would be actively wrong.
//
// Neither `zhao_probe_curve_svc` nor `zhao_probe_dist_svc` has a `mul_ready`
// input -- grepped both, zero matches. The curve service asserts
// `mul_issue_o = (f_state == F_ISSUE)` and advances on the next clock
// regardless. A refused service does not retry; it proceeds as though the
// multiply had been issued and later consumes a product that was never
// computed. Refusing a service is silently INCORRECT, not slow.
//
// AND SERVICES-FIRST IS STILL NOT SUFFICIENT. CURVE and DIST are both
// services. If both assert in one clock, one loses and has no way to know;
// fixed priority merely decides which of them is corrupted. The ways out are
// in reports/FIELD_V3_SERVICE_ATTACH.md, and the honest one is to give the
// services back-pressure -- a claimant that cannot be told "no" is not a
// claimant, it is an assumption.
//
// `PRIO_SERVICES_FIRST` stays a parameter because the LANES genuinely can be
// refused and their priority genuinely is a trade. It must not be read as
// licence to reorder the services.
//
// STARVATION IS POSSIBLE AND IS DECLARED, NOT DENIED. With fixed priority a
// permanently busy service starves the lanes. Two facts bound it: a service
// request occupies the bank for a bounded number of clocks (a curve step is
// not unbounded), and the services are themselves fed by contexts that the
// lanes must first execute. It is bounded in practice, not by construction,
// and `stall_lanes_o` counts exactly how long the lanes waited so the claim
// is measured rather than argued.
//
// Law:
//   reports/Fieldv3.md                     four-wide vector multiply/MAD bank
//   reports/FIELD_V3_SERVICE_ATTACH.md     the three claimants and this design
//   fpga/rtl/field/zhao_field_mul.sv       two clocks, pipelined, one per clock

`default_nettype none

module zhao_field_v3_mulbank #(
    // Claimant 0 is the ALU lanes; 1 and above are services. With
    // PRIO_SERVICES_FIRST the highest index wins, so services outrank lanes.
    parameter int CLAIMANTS = 3,
    parameter bit PRIO_SERVICES_FIRST = 1'b1,
    parameter int TAGW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- requests, one four-wide set per claimant --------------------------
    // UNPACKED arrays deliberately: a packed [C][4][33] port flattens to one
    // wide word, which the differential cannot index by claimant and lane.
    // The shape the test needs to drive is the shape the port should have.
    input  var logic [CLAIMANTS-1:0]  req_valid_i,
    output var logic [CLAIMANTS-1:0]  req_ready_o,
    input  var logic signed [32:0]    req_a_i   [CLAIMANTS][4],
    input  var logic signed [32:0]    req_b_i   [CLAIMANTS][4],
    input  var logic [TAGW-1:0]       req_tag_i [CLAIMANTS],

    // ---- replies, routed back to whoever won ------------------------------
    output var logic [CLAIMANTS-1:0]  rsp_valid_o,
    output var logic signed [65:0]    rsp_p_o [4],
    output var logic [TAGW-1:0]       rsp_tag_o,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] grants_o,
    output var logic [31:0] stall_lanes_o,  // clocks the lanes wanted and lost

    // The multiplier lanes' OWN valid must agree with the tag shadow every
    // clock. If it ever does not, a product is being routed to a claimant
    // that did not ask for it -- a wrong answer, not a slow one.
    //
    // This port exists because p_valid_lane came back from the linter as an
    // unused signal. The same choice arose in zhao_probe_v3_exec, where
    // making it evidence rather than deleting it caught a real pipeline bug
    // on the very first run.
    output var logic desync_o
);

  localparam int CW = (CLAIMANTS <= 1) ? 1 : $clog2(CLAIMANTS);

  // ---- the grant --------------------------------------------------------
  logic            grant_v_c;
  logic [CW-1:0]   grant_c;

  always_comb begin
    grant_v_c = |req_valid_i;
    grant_c   = '0;
    if (PRIO_SERVICES_FIRST) begin
      // Highest index wins: services outrank the lane group at index 0.
      for (int i = 0; i < CLAIMANTS; i++) if (req_valid_i[i]) grant_c = CW'(i);
    end else begin
      // Lowest index wins.
      for (int i = CLAIMANTS - 1; i >= 0; i--) if (req_valid_i[i]) grant_c = CW'(i);
    end
  end

  // One claimant is accepted per clock; the rest hold their request.
  always_comb begin
    for (int i = 0; i < CLAIMANTS; i++)
      req_ready_o[i] = grant_v_c && (grant_c == CW'(i));
  end

  // ---- the four multiplier lanes ----------------------------------------
  logic signed [65:0] p_lane [4];
  logic [3:0]         p_valid_lane;

  generate
    for (genvar l = 0; l < 4; l++) begin : g_lane
      zhao_field_mul u_mul (
          .clk      (clk),
          .rst_n    (rst_n),
          .issue_i  (grant_v_c),
          .a_i      (req_a_i[grant_c][l]),
          .b_i      (req_b_i[grant_c][l]),
          .p_o      (p_lane[l]),
          .p_valid_o(p_valid_lane[l])
      );
    end
  endgenerate

  // ---- the tag shadow ----------------------------------------------------
  // Two stages, matching the multiplier's two clocks. This is what makes the
  // arbiter simple: the reply that emerges belongs to whoever's tag emerges
  // with it, so nothing has to be matched up or reordered.
  logic            v_s1, v_s2;
  logic [CW-1:0]   who_s1, who_s2;
  logic [TAGW-1:0] tag_s1, tag_s2;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      v_s1 <= 1'b0;
      v_s2 <= 1'b0;
      who_s1 <= '0;
      who_s2 <= '0;
      tag_s1 <= '0;
      tag_s2 <= '0;
      grants_o <= 32'd0;
      stall_lanes_o <= 32'd0;
      desync_o <= 1'b0;
    end else begin
      v_s1   <= grant_v_c;
      who_s1 <= grant_c;
      tag_s1 <= req_tag_i[grant_c];
      v_s2   <= v_s1;
      who_s2 <= who_s1;
      tag_s2 <= tag_s1;

      // ALL FOUR lanes are issued together, so when the shadow says a reply
      // is due every lane must be valid, and when it says none, none may be.
      // Checking the whole vector rather than lane 0 is strictly stronger and
      // needs no lint waiver -- a lane that fell out of step would otherwise
      // be invisible behind lane 0 agreeing.
      if (v_s2 ? (p_valid_lane != 4'hF) : (p_valid_lane != 4'h0)) desync_o <= 1'b1;

      if (grant_v_c) grants_o <= grants_o + 32'd1;
      // The lanes wanted the bank and did not get it. Counted rather than
      // asserted away, because fixed priority CAN starve and the honest
      // answer is a number.
      if (req_valid_i[0] && !(grant_v_c && grant_c == CW'(0)))
        stall_lanes_o <= stall_lanes_o + 32'd1;
    end
  end

  always_comb begin
    for (int l = 0; l < 4; l++) rsp_p_o[l] = p_lane[l];
  end
  assign rsp_tag_o = tag_s2;

  always_comb begin
    for (int i = 0; i < CLAIMANTS; i++)
      rsp_valid_o[i] = v_s2 && (who_s2 == CW'(i));
  end

`ifdef FORMAL
  // The bank's own valids must agree with the shadow: if a product is coming
  // out, exactly one claimant is being told so.
  a_one_reply_at_a_time :
  assert property (@(posedge clk) disable iff (!rst_n) $onehot0(rsp_valid_o));

  a_reply_matches_mul_valid :
  assert property (@(posedge clk) disable iff (!rst_n) (|rsp_valid_o) == p_valid_lane[0]);

  // All four lanes are issued together, so their valids can never disagree.
  a_lanes_move_together :
  assert property (@(posedge clk) disable iff (!rst_n)
                   p_valid_lane == {4{p_valid_lane[0]}});
`endif

endmodule

`default_nettype wire
