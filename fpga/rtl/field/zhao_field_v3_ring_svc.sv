// zhao_field_v3_ring_svc.sv — the prepared ring, joined to its uniforms, twice.
//
// ENFORCED-BY: tests/differential/field_v3_ring_svc_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// `zhao_field_v3_ring` computes the prepared ring and is closed at 23/23. It
// takes four varying distances and FOUR UNIFORM SCALARS -- r0, the midpoint m,
// and the two smoothstep reciprocals rA and rB -- because the reference
// computes those once per association on the ARM. `zhao_field_v3_sbank` holds
// them. This is the piece between: it fetches the four scalars by index and
// hands them to a unit.
//
// The slots are packed four-to-an-immediate, six bits each:
//
//     imm[ 5: 0] r0   imm[11: 6] m   imm[17:12] rA   imm[23:18] rB
//     imm[31:24] reserved, must be zero
//
// which fits because the bank depth was MEASURED at 64 slots before the
// encoding was chosen. The planner allocates those four non-consecutively, so
// one base index cannot address them.
//
// ---------------------------------------------------------------------------
// TWO UNITS, BECAUSE THE NINE PRODUCTS ARE A DEPENDENCY CHAIN
// ---------------------------------------------------------------------------
// This service first shipped with one unit and measured 50 clocks per group on
// the composed machine, against Earth's budget of 24.3 clocks per group for the
// WHOLE program. It was the largest blocking service left after the distance
// and trig rebuilds.
//
// The ring's nine products cannot be issued faster: p2 is t0*t0 and needs p1,
// p4 needs p2, and so on down the chain. Each product is an issue and a wait,
// so the unit spends most of its life stalled on a multiply it has to have.
//
// But the shared bank only owes NINE SLOTS per group. The stalls are therefore
// fillable by somebody else's work rather than shortenable, which is what two
// units buy: while group N waits for its product, group N+1 issues its own.
//
//     unit A   [p1]--wait--[p2]--wait--[p3]...
//     unit B        [p1]--wait--[p2]--wait--...
//     bank      A    B    A     B    A     B
//
// The bank port is arbitrated with the OLDER group winning, so a younger group
// can never starve an older one into missing its deadline -- and the reply
// order is kept by the same head/tail queue the distance service uses, so a
// unit that finishes early cannot hand a caller somebody else's ring.
module zhao_field_v3_ring_svc #(
    // RING UNITS. The nine products inside one unit are a dependency chain and
    // cannot be pipelined, so throughput here is bought only by having more
    // units -- the same trade the distance service makes with its root banks.
    //
    // TWO GIVES II 19 against an admission ceiling of 24.33 for the WHOLE
    // program, so on crater_ring -- the only program that uses the prepared
    // ring -- this service alone takes 78% of the budget.
    parameter int UNITS = 2
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    input  var logic signed [31:0] req_d_0_i, req_d_1_i, req_d_2_i, req_d_3_i,
    input  var logic        [31:0] req_imm_i,
    input  var logic        [ 7:0] req_tag_i,

    // ---- the uniform bank's single read port -------------------------------
    output var logic        [ 5:0] sb_raddr_o,
    input  var logic signed [31:0] sb_rdata_i,

    // ---- the shared four-wide multiplier bank ------------------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i,

    // ---- reply -------------------------------------------------------------
    output var logic               rsp_valid_o,
    input  var logic               rsp_ready_i,
    output var logic signed [31:0] rsp_r_0_o, rsp_r_1_o, rsp_r_2_o, rsp_r_3_o,
    output var logic        [ 3:0] rsp_sat_add_o,
    output var logic        [ 3:0] rsp_sat_mul_o,
    output var logic        [ 7:0] rsp_tag_o,

    output var logic               imm_bad_o
);

  // =========================================================================
  // THE FRONT END: fetch four uniforms, six cycles
  // =========================================================================
  //
  // The bank's read is REGISTERED, so the datum for an address presented on
  // cycle T lands on T+1: addresses on 0..3, captures on 1..4, done at 5.
  // Finishing at 4 would hand the arithmetic a value not yet written, which is
  // the bug the curve service's neighbour phase cost a session for.
  localparam logic [1:0] F_IDLE  = 2'd0;
  localparam logic [1:0] F_FETCH = 2'd1;
  localparam logic [1:0] F_HAND  = 2'd2;

  logic [1:0]         f_state_r;
  logic [2:0]         f_cyc_r;
  logic signed [31:0] f_d_r [4];
  logic        [ 7:0] f_tag_r;
  logic        [ 5:0] f_slot_r [4];
  logic signed [31:0] f_uni_r [4];

  assign sb_raddr_o = f_slot_r[f_cyc_r[1:0]];

  // =========================================================================
  // TWO RING UNITS
  // =========================================================================
  logic               u_busy_r [UNITS];
  logic               u_off_r  [UNITS];   // the group has been offered to the unit
  logic               rg_v_valid [UNITS], rg_v_ready [UNITS], rg_r_valid [UNITS];
  logic signed [31:0] rg_o [UNITS][4];
  logic        [ 3:0] rg_sat_add [UNITS], rg_sat_mul [UNITS];
  logic        [ 7:0] rg_tag_o [UNITS];
  logic               rg_mul_issue [UNITS], rg_mul_ready [UNITS], rg_mul_valid [UNITS];
  logic signed [32:0] rg_a [UNITS][4], rg_b [UNITS][4];

  logic signed [31:0] u_d_r [UNITS][4];
  logic signed [31:0] u_uni_r [UNITS][4];
  logic        [ 7:0] u_tag_r [UNITS];
  // Smooth mode travels with the group, not with the unit.
  logic               u_smooth_r [UNITS];
  logic               f_smooth_r;

  for (genvar u = 0; u < UNITS; u++) begin : gen_unit
    assign rg_v_valid[u] = u_busy_r[u] && !u_off_r[u];
    zhao_field_v3_ring u_ring (
        .clk(clk), .rst_n(rst_n),
        .v_valid_i(rg_v_valid[u]), .v_ready_o(rg_v_ready[u]),
        .d_0_i(u_d_r[u][0]), .d_1_i(u_d_r[u][1]),
        .d_2_i(u_d_r[u][2]), .d_3_i(u_d_r[u][3]),
        .smooth_i(u_smooth_r[u]),
        .r0_i(u_uni_r[u][0]), .m_i(u_uni_r[u][1]),
        .rA_i(u_uni_r[u][2]), .rB_i(u_uni_r[u][3]),
        .tag_i(u_tag_r[u]),
        .r_valid_o(rg_r_valid[u]), .r_ready_i(ret_fire_c && (head_u_c == UW'(u))),
        .o0_0_o(rg_o[u][0]), .o0_1_o(rg_o[u][1]),
        .o0_2_o(rg_o[u][2]), .o0_3_o(rg_o[u][3]),
        .sat_add_o(rg_sat_add[u]), .sat_mul_o(rg_sat_mul[u]), .tag_o(rg_tag_o[u]),
        .mul_issue_o(rg_mul_issue[u]), .mul_ready_i(rg_mul_ready[u]),
        .mul_a_0_o(rg_a[u][0]), .mul_a_1_o(rg_a[u][1]),
        .mul_a_2_o(rg_a[u][2]), .mul_a_3_o(rg_a[u][3]),
        .mul_b_0_o(rg_b[u][0]), .mul_b_1_o(rg_b[u][1]),
        .mul_b_2_o(rg_b[u][2]), .mul_b_3_o(rg_b[u][3]),
        .mul_valid_i(rg_mul_valid[u]),
        .mul_p_0_i(mul_p_0_i), .mul_p_1_i(mul_p_1_i),
        .mul_p_2_i(mul_p_2_i), .mul_p_3_i(mul_p_3_i)
    );
  end

  // ---- the order queue -----------------------------------------------------
  localparam int UW = (UNITS <= 2) ? 1 : ((UNITS <= 4) ? 2 : 3);
  logic [UW-1:0] oq_u_r [UNITS];
  logic [UW-1:0] oq_head_r, oq_tail_r;
  logic [UW:0]   oq_count_r;

  logic [UW-1:0] head_u_c;
  assign head_u_c = oq_u_r[oq_head_r];

  function automatic logic [UW-1:0] next_u(input logic [UW-1:0] p);
    // `UNITS - 1` is a CONSTANT, so this narrowing is exact rather than a
    // truncation waiting to happen. The wrap above is the one that bit.
    next_u = (p == UW'(UNITS - 1)) ? UW'(0) : UW'(p + UW'(1));
  endfunction

  logic [UW-1:0] free_u_c;
  logic          have_free_c;
  always_comb begin
    have_free_c = 1'b0;
    free_u_c    = '0;
    // Lowest free unit, scanned downwards so unit 0 wins -- arbitrary but FIXED.
    for (int u = UNITS - 1; u >= 0; u--)
      if (!u_busy_r[u]) begin
        have_free_c = 1'b1;
        free_u_c    = UW'(u);
      end
  end

  // ---- the bank port, OLDER GROUP FIRST ------------------------------------
  // A younger group must never starve an older one: the deadline belongs to the
  // group that was accepted first, and fairness here is not a preference.
  logic [UW-1:0] bank_u_c;
  always_comb begin
    // The oldest unit that wants the bank, walking the accept order from the
    // head. A younger group must never starve an older one.
    // INT ARITHMETIC ON PURPOSE. `UW'(UNITS)` truncates the count to the
    // POINTER's width -- at UNITS=4 that is 2 bits and UW'(4) is ZERO, so the
    // wrap became a modulo by zero. The dispatcher paid an hour for exactly
    // this with `SW'(OUTSTANDING)`; the sum must not be narrowed before the
    // wrap is taken.
    //
    // Only the `oq_count_r` entries starting at the head hold a group. Reading
    // past them would arbitrate for a unit that was never enqueued.
    bank_u_c = head_u_c;
    for (int k = UNITS - 1; k >= 0; k--)
      if ((UW+1)'(k) < oq_count_r) begin
        automatic logic [UW-1:0] cand = oq_u_r[(int'(oq_head_r) + k) % UNITS];
        if (rg_mul_issue[cand]) bank_u_c = cand;
      end
  end

  // NO SERIALISATION. `zhao_field_v3_mulbank` is FULLY PIPELINED -- two clocks
  // deep, a new pair accepted every clock, with its own two-stage tag shadow.
  // An earlier version here allowed one product in flight and measured II 27;
  // that was this file's limit, not the bank's, and shipping it would have been
  // a number describing my own scaffolding.
  assign mul_issue_o = rg_mul_issue[bank_u_c];
  assign mul_a_0_o = rg_a[bank_u_c][0];
  assign mul_a_1_o = rg_a[bank_u_c][1];
  assign mul_a_2_o = rg_a[bank_u_c][2];
  assign mul_a_3_o = rg_a[bank_u_c][3];
  assign mul_b_0_o = rg_b[bank_u_c][0];
  assign mul_b_1_o = rg_b[bank_u_c][1];
  assign mul_b_2_o = rg_b[bank_u_c][2];
  assign mul_b_3_o = rg_b[bank_u_c][3];

  // A TWO-DEEP OWNER SHADOW, matching the bank's own two stages. The bank
  // routes products back per CLAIMANT and this service is one claimant with two
  // units behind it, so the split between them is this block's job.
  //
  // THE GRANT MUST CARRY THE SAME CONDITION THE ISSUE DOES. An earlier version
  // gated the issue and left the grant ungated, which told a unit it was served
  // when its request never reached the bank -- it then advanced and waited
  // forever for a product nobody started. That is the OPEN-LOOP CLAIMANT defect
  // this engine's blockers document describes at length, recreated here by me
  // while fixing something else, and it showed as multiplies issuing every
  // three clocks forever with no reply ever arriving.
  logic          sh_v_r [2];
  logic [UW-1:0] sh_u_r [2];
  for (genvar u = 0; u < UNITS; u++) begin : gen_grant
    assign rg_mul_ready[u] = mul_ready_i && (bank_u_c == UW'(u));
    assign rg_mul_valid[u] = mul_valid_i && sh_v_r[1] && (sh_u_r[1] == UW'(u));
  end

  assign req_ready_o = (f_state_r == F_IDLE) && (oq_count_r != (UW+1)'(UNITS));

  logic ret_fire_c;
  assign rsp_valid_o   = (oq_count_r != '0) && u_busy_r[head_u_c] && rg_r_valid[head_u_c];
  assign ret_fire_c    = rsp_valid_o && rsp_ready_i;
  assign rsp_r_0_o     = rg_o[head_u_c][0];
  assign rsp_r_1_o     = rg_o[head_u_c][1];
  assign rsp_r_2_o     = rg_o[head_u_c][2];
  assign rsp_r_3_o     = rg_o[head_u_c][3];
  assign rsp_sat_add_o = rg_sat_add[head_u_c];
  assign rsp_sat_mul_o = rg_sat_mul[head_u_c];
  assign rsp_tag_o     = rg_tag_o[head_u_c];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_state_r   <= F_IDLE;
      f_cyc_r     <= 3'd0;
      f_tag_r     <= 8'd0;
      imm_bad_o   <= 1'b0;
      f_smooth_r  <= 1'b0;
      oq_head_r   <= '0;
      oq_tail_r   <= '0;
      oq_count_r  <= '0;
      sh_v_r[0]   <= 1'b0;
      sh_v_r[1]   <= 1'b0;
      sh_u_r[0]   <= '0;
      sh_u_r[1]   <= '0;
      for (int i = 0; i < 4; i++) begin
        f_d_r[i]    <= '0;
        f_slot_r[i] <= 6'd0;
        f_uni_r[i]  <= '0;
      end
      for (int u = 0; u < 2; u++) begin
        u_busy_r[u] <= 1'b0;
        u_off_r[u]  <= 1'b0;
        u_tag_r[u]  <= 8'd0;
        oq_u_r[u]   <= '0;
        for (int i = 0; i < 4; i++) begin
          u_d_r[u][i]   <= '0;
          u_uni_r[u][i] <= '0;
        end
      end
    end else begin
      // ---- the owner shadow, one stage per bank stage ---------------------
      sh_v_r[1] <= sh_v_r[0];
      sh_u_r[1] <= sh_u_r[0];
      sh_v_r[0] <= mul_issue_o && mul_ready_i;
      sh_u_r[0] <= bank_u_c;

      // ---- the front end ---------------------------------------------------
      case (f_state_r)
        F_IDLE: begin
          if (req_valid_i && req_ready_o) begin
            f_d_r[0] <= req_d_0_i;
            f_d_r[1] <= req_d_1_i;
            f_d_r[2] <= req_d_2_i;
            f_d_r[3] <= req_d_3_i;
            f_tag_r  <= req_tag_i;
            f_slot_r[0] <= req_imm_i[5:0];
            f_slot_r[1] <= req_imm_i[11:6];
            f_slot_r[2] <= req_imm_i[17:12];
            f_slot_r[3] <= req_imm_i[23:18];
            // BIT 24 IS SMOOTH MODE: answer with the first smoothstep alone and
            // stop after four products instead of nine. It comes out of what
            // used to be reserved space, so [31:25] stays a FAULT rather than
            // padding -- a caller with rubbish in the top bits is still told.
            f_smooth_r  <= req_imm_i[24];
            if (req_imm_i[31:25] != 7'd0) imm_bad_o <= 1'b1;
            f_cyc_r   <= 3'd0;
            f_state_r <= F_FETCH;
          end
        end

        F_FETCH: begin
          if (f_cyc_r != 3'd5) f_cyc_r <= f_cyc_r + 3'd1;
          if ((f_cyc_r >= 3'd1) && (f_cyc_r <= 3'd4)) begin
            automatic logic [1:0] cap = 2'(f_cyc_r - 3'd1);
            f_uni_r[cap] <= sb_rdata_i;
          end
          if (f_cyc_r == 3'd5) f_state_r <= F_HAND;
        end

        // Hand to a free unit. If both are busy the front WAITS rather than
        // dropping the group: back-pressure, not loss.
        F_HAND: begin
          if (have_free_c) begin
            for (int i = 0; i < 4; i++) begin
              u_d_r[free_u_c][i]   <= f_d_r[i];
              u_uni_r[free_u_c][i] <= f_uni_r[i];
            end
            u_tag_r[free_u_c]    <= f_tag_r;
            u_smooth_r[free_u_c] <= f_smooth_r;
            u_busy_r[free_u_c] <= 1'b1;
            u_off_r[free_u_c]  <= 1'b0;
            oq_u_r[oq_tail_r]  <= free_u_c;
            oq_tail_r          <= next_u(oq_tail_r);
            f_state_r          <= F_IDLE;
          end
        end

        default: f_state_r <= F_IDLE;
      endcase

      // ---- each unit is offered its group exactly once ---------------------
      for (int u = 0; u < 2; u++)
        if (rg_v_valid[u] && rg_v_ready[u]) u_off_r[u] <= 1'b1;

      // ---- retire the oldest -----------------------------------------------
      if (ret_fire_c) begin
        u_busy_r[head_u_c] <= 1'b0;
        u_off_r[head_u_c]  <= 1'b0;
        oq_head_r          <= next_u(oq_head_r);
      end

      // ---- occupancy, decided in ONE place ---------------------------------
      begin
        automatic logic push = (f_state_r == F_HAND) && have_free_c;
        automatic logic pop  = ret_fire_c;
        if (push && !pop)      oq_count_r <= oq_count_r + (UW+1)'(1);
        else if (pop && !push) oq_count_r <= oq_count_r - (UW+1)'(1);
      end
    end
  end

endmodule : zhao_field_v3_ring_svc
