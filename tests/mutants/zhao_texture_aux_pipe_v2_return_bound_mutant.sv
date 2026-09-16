// zhao_texture_aux_pipe_v2_return_bound_mutant.sv
// COMMITTED POSITIVE CONTROL — NEVER ADD TO PRODUCTION SOURCES.
// Exact assertion/counter control: RETURN_BOUND.
// One substantive mutation: reset return occupancy above its legal bound.
// All shipped fault predicates, credit_fault_o logic, and assertions below
// are byte-for-byte production code apart from the required module rename.
// zhao_texture_aux_pipe_v2.sv — Packet-B Surface Sheet AUX adapter.
//
// This is not a behavioral edit of the oracle-only zhao_texture_aux_pipe.  It is
// the versioned adapter owned by TEXTURE.AUX.V2 and it imports the complete READ
// transaction from SURFACE.SHEET:
//
//   request  {op=READ, handle32, texel12={v6,u6}, src_id16={2'b00,owner14}}
//   response {op2, status2, tag8, strength8, src_id16}, ready/valid in both ways
//
// Every accepted logical AUX job pulses issue_valid_o on its acceptance edge.
// A local refusal is structurally downstream of that edge and cannot be offered
// earlier than the following cycle.  Every accepted Sheet READ is recorded in an
// in-order identity FIFO until exactly one response is consumed.  Malformed owed
// responses refuse the FIFO HEAD; unsolicited responses drain into a fault sink
// and can never invent an owner.
//
// One credit spans the whole accepted lifetime: input arithmetic, the fixed six-
// stage divider, held Sheet offer, issued identity, owed response reservation,
// local refusal, and held typed return.  It is released only when the owner takes
// out_valid_o.  idle_o names every stateful part explicitly; sheet_rsp_owed_o is
// exactly issued_identity_fifo_occupancy != 0.
//
// The typed owner plane is exactly 48 bits:
//   [47:40] status, [39:32] tag, [31:24] strength, [23:0] zero
// AUX is never RGB, alpha, raw index, TMU sample 2, or a substitute for sample 2.
`default_nettype none

`ifndef ZHAO_AUX_T4_DEGENERATE_CAPTURE
`define ZHAO_AUX_T4_DEGENERATE_CAPTURE(value) (value)
`endif
`ifndef ZHAO_AUX_T4_INPUT_FAULT_CAPTURE
`define ZHAO_AUX_T4_INPUT_FAULT_CAPTURE(value) (value)
`endif

module zhao_texture_aux_pipe_v2_return_bound_mutant #(
    parameter int unsigned NUM_W   = 40,
    parameter int unsigned DEN_W   = 32,
    parameter int unsigned REM_W   = 39,
    parameter int unsigned OWNERW  = 14,
    parameter int unsigned CREDIT  = 16
) (
    input  var logic                       clk,
    input  var logic                       rst_n,
    input  var logic                       frame_fault_clear_i,

    // Logical owner job. force_refuse is admission-frozen malformed-material
    // disposition; a degenerate envelope is detected locally.
    input  var logic                       job_valid_i,
    output var logic                       job_ready_o,
    input  var logic signed [31:0]         job_wx_i,
    input  var logic signed [31:0]         job_wz_i,
    input  var logic signed [31:0]         job_env_x0_i,
    input  var logic signed [31:0]         job_env_x1_i,
    input  var logic signed [31:0]         job_env_z0_i,
    input  var logic signed [31:0]         job_env_z1_i,
    input  var logic        [31:0]         job_sheet_handle_i,
    input  var logic        [OWNERW-1:0]   job_owner_i,
    input  var logic                       job_force_refuse_i,

    // Logical issue notification to zhao_texture_v3own.  No ready exists on the
    // owner issue port; this pulse is exactly the accepted job edge.
    output var logic                       issue_valid_o,
    output var logic        [OWNERW-1:0]   issue_owner_o,

    // Complete Surface Sheet READ request.
    output var logic                       req_valid_o,
    input  var logic                       req_ready_i,
    output var logic        [1:0]          req_op_o,
    output var logic        [31:0]         req_handle_o,
    output var logic        [11:0]         req_texel_o,
    output var logic        [15:0]         req_src_id_o,

    // Complete Surface Sheet response.  Unsolicited traffic is always drained;
    // an owed response is accepted only with its reserved terminal slot.
    input  var logic                       pg_valid_i,
    output var logic                       pg_ready_o,
    input  var logic        [1:0]          pg_op_i,
    input  var logic        [1:0]          pg_status_i,
    input  var logic        [7:0]          pg_tag_i,
    input  var logic        [7:0]          pg_strength_i,
    input  var logic        [15:0]         pg_src_id_i,

    // Typed AUX return to zhao_texture_v3own.
    output var logic                       out_valid_o,
    input  var logic                       out_ready_i,
    output var logic        [OWNERW-1:0]   out_owner_o,
    output var logic        [47:0]         out_result_o,

    // Structural observations. refuse_valid_o is the held local-refusal offer at
    // the offer FIFO head, not a reconstructed pulse from a counter.
    output var logic                       refuse_valid_o,
    output var logic                       sheet_rsp_owed_o,
    output var logic                       idle_o,

    // Accepted/completed accounting and independently typed protocol evidence.
    output var logic        [31:0]         accepted_o,
    output var logic        [31:0]         sheet_reads_o,
    output var logic        [31:0]         local_refused_o,
    output var logic        [31:0]         completed_o,
    output var logic        [31:0]         degenerate_o,
    output var logic        [31:0]         sheet_hits_o,
    output var logic        [31:0]         sheet_misses_o,
    output var logic        [31:0]         sheet_rsp_wrong_op_o,
    output var logic        [31:0]         sheet_rsp_wrong_status_o,
    output var logic        [31:0]         sheet_rsp_wrong_src_o,
    output var logic        [31:0]         sheet_rsp_unsolicited_o,
    output var logic        [31:0]         credit_fault_o,
    output var logic                       frame_fault_o,
    output var logic        [$clog2(CREDIT+1)-1:0] credit_in_use_o
);

  localparam logic [1:0] SHEET_OP_READ       = 2'd1;
  localparam logic [1:0] SHEET_STATUS_HIT    = 2'd0;
  localparam logic [1:0] SHEET_STATUS_MISS   = 2'd3;
  localparam logic [7:0] SOURCE_REFUSED      = 8'h01;
  localparam int unsigned CREDIT_W           = $clog2(CREDIT + 1);
  localparam int unsigned CREDIT_AW          = $clog2(CREDIT);

  initial begin : p_parameter_contract
    if ((CREDIT < 16) || ((1 << CREDIT_AW) != CREDIT))
      $fatal(1, "aux-v2 CREDIT must be a power of two and at least 16");
    if (OWNERW != 14)
      $fatal(1, "aux-v2 Surface Sheet source echo requires exact owner14");
    if (NUM_W != (DEN_W + 8))
      $fatal(1, "aux-v2 clamp width requires NUM_W == DEN_W + 8");
    if (REM_W < (DEN_W + 6))
      $fatal(1, "aux-v2 divider remainder is too narrow");
  end

  // ==========================================================================
  // Lifetime credit and logical issue
  // ==========================================================================
  logic [CREDIT_W-1:0] credit_q;
  logic                job_fire_c;
  logic                out_fire_c;

  always_comb begin
    // Conservative full-to-free fence: a same-cycle terminal pop is not a
    // combinational admission bypass.
    job_ready_o  = rst_n && (credit_q != CREDIT_W'(CREDIT));
    job_fire_c   = job_valid_i && job_ready_o;
    issue_valid_o = job_fire_c;
    issue_owner_o = job_owner_i;
    credit_in_use_o = credit_q;
  end

  // ==========================================================================
  // A0 input arithmetic, then the existing six-stage restoring divider
  // ==========================================================================
  logic                    envelope_degenerate_c;
  logic [DEN_W-1:0]        du_c;
  logic [DEN_W-1:0]        dv_c;
  logic signed [NUM_W-1:0] nu_c;
  logic signed [NUM_W-1:0] nv_c;

  always_comb begin
    envelope_degenerate_c = (job_env_x1_i <= job_env_x0_i)
                         || (job_env_z1_i <= job_env_z0_i);
    du_c = $unsigned(job_env_x1_i) - $unsigned(job_env_x0_i);
    dv_c = $unsigned(job_env_z1_i) - $unsigned(job_env_z0_i);
    nu_c = (NUM_W'($signed(job_wx_i)) - NUM_W'($signed(job_env_x0_i))) <<< 6;
    nv_c = (NUM_W'($signed(job_wz_i)) - NUM_W'($signed(job_env_z0_i))) <<< 6;
  end

  logic                    a0_valid_q;
  logic                    a0_refuse_q;
  logic                    a0_degenerate_q;
  logic                    a0_input_fault_q;
  logic [DEN_W-1:0]        a0_du_q;
  logic [DEN_W-1:0]        a0_dv_q;
  logic signed [NUM_W-1:0] a0_nu_q;
  logic signed [NUM_W-1:0] a0_nv_q;
  logic [31:0]             a0_handle_q;
  logic [OWNERW-1:0]       a0_owner_q;

  logic neg_u_c;
  logic neg_v_c;
  logic sat_u_c;
  logic sat_v_c;

  always_comb begin
    neg_u_c = a0_nu_q[NUM_W-1];
    neg_v_c = a0_nv_q[NUM_W-1];
    sat_u_c = !neg_u_c
           && ($unsigned(a0_nu_q) >= {2'b00, a0_du_q, 6'b000000});
    sat_v_c = !neg_v_c
           && ($unsigned(a0_nv_q) >= {2'b00, a0_dv_q, 6'b000000});
  end

  // The divider carries a side-table index. A slot is read seven cycles after it
  // is written: one registered clamp edge plus the fixed six-cycle divider. It
  // cannot be reused for CREDIT clocks; CREDIT>=16 is the margin.
  logic                    side_refuse_q    [CREDIT];
  logic                    side_sat_u_q     [CREDIT];
  logic                    side_sat_v_q     [CREDIT];
  logic [31:0]             side_handle_q    [CREDIT];
  logic [OWNERW-1:0]       side_owner_q     [CREDIT];
  logic [CREDIT_AW-1:0]    side_write_q;

  logic                    div_in_valid_q;
  logic [REM_W-1:0]        div_in_ru_q;
  logic [DEN_W-1:0]        div_in_du_q;
  logic [REM_W-1:0]        div_in_rv_q;
  logic [DEN_W-1:0]        div_in_dv_q;
  logic [CREDIT_AW-1:0]    div_in_tag_q;

  logic                    div_valid_w;
  logic [5:0]              div_u_w;
  logic [5:0]              div_v_w;
  logic [CREDIT_AW-1:0]    div_tag_w;
  logic [31:0]             div_issued_unused_w;
  logic [3:0]              div_occupancy_w;

  zhao_texture_aux_div6 #(
      .REM_W(REM_W),
      .DEN_W(DEN_W),
      .TAGW(CREDIT_AW)
  ) u_div (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid_i(div_in_valid_q),
      .in_ru_i(div_in_ru_q),
      .in_du_i(div_in_du_q),
      .in_rv_i(div_in_rv_q),
      .in_dv_i(div_in_dv_q),
      .in_tag_i(div_in_tag_q),
      .out_valid_o(div_valid_w),
      .out_qu_o(div_u_w),
      .out_qv_o(div_v_w),
      .out_tag_o(div_tag_w),
      .issued_o(div_issued_unused_w),
      .occupancy_o(div_occupancy_w)
  );

  // ==========================================================================
  // Divider-result / Sheet-offer FIFO
  // ==========================================================================
  logic [5:0]            offer_u_q      [CREDIT];
  logic [5:0]            offer_v_q      [CREDIT];
  logic [31:0]           offer_handle_q [CREDIT];
  logic [OWNERW-1:0]     offer_owner_q  [CREDIT];
  logic                  offer_refuse_q [CREDIT];
  logic [CREDIT_AW-1:0]  offer_write_q;
  logic [CREDIT_AW-1:0]  offer_read_q;
  logic [CREDIT_W-1:0]   offer_count_q;

  logic                  offer_head_valid_c;
  logic                  offer_head_refuse_c;
  logic [OWNERW-1:0]     offer_head_owner_c;
  logic                  req_fire_c;
  logic                  local_push_c;
  logic                  offer_pop_c;

  assign offer_head_valid_c  = (offer_count_q != CREDIT_W'(0));
  assign offer_head_refuse_c = offer_refuse_q[offer_read_q];
  assign offer_head_owner_c  = offer_owner_q[offer_read_q];

  // ==========================================================================
  // Issued Sheet identity FIFO: the authority for every owed response
  // ==========================================================================
  logic [OWNERW-1:0]     issued_owner_q [CREDIT];
  logic [CREDIT_AW-1:0]  issued_write_q;
  logic [CREDIT_AW-1:0]  issued_read_q;
  logic [CREDIT_W-1:0]   issued_count_q;

  logic                  issued_head_valid_c;
  logic [OWNERW-1:0]     issued_head_owner_c;
  logic [15:0]           issued_expected_src_c;
  logic                  pg_fire_c;
  logic                  owed_response_c;
  logic                  owed_response_push_c;
  logic                  issued_pop_c;

  assign issued_head_valid_c = (issued_count_q != CREDIT_W'(0));
  assign issued_head_owner_c = issued_owner_q[issued_read_q];
  assign issued_expected_src_c = {{(16-OWNERW){1'b0}}, issued_head_owner_c};
  assign sheet_rsp_owed_o = issued_head_valid_c;

  // ==========================================================================
  // Typed return FIFO.  Two producers may land in one cycle: the owed Sheet
  // response has priority at write_q, then a local refusal uses write_q+1.
  // ==========================================================================
  logic [OWNERW-1:0]     return_owner_q [CREDIT];
  logic [47:0]           return_result_q[CREDIT];
  logic                  return_local_q [CREDIT];
  logic [CREDIT_AW-1:0]  return_write_q;
  logic [CREDIT_AW-1:0]  return_read_q;
  logic [CREDIT_W-1:0]   return_count_q;

  logic [CREDIT_W-1:0]   return_free_c;
  logic [1:0]            return_pushes_c;
  logic [7:0]            response_status_c;
  logic [7:0]            response_tag_c;
  logic [7:0]            response_strength_c;
  logic                  response_ok_c;
  logic                  response_hit_c;
  logic                  response_miss_c;
  logic                  response_wrong_op_c;
  logic                  response_wrong_status_c;
  logic                  response_wrong_src_c;

  always_comb begin
    return_free_c = CREDIT_W'(CREDIT) - return_count_q;

    // With no issued head the response is an always-draining fault sink and
    // consumes no terminal slot.  Credit proves an owed head always has room;
    // the explicit gate is retained as a structural guard.
    pg_ready_o = !issued_head_valid_c || (return_free_c != CREDIT_W'(0));
    pg_fire_c  = pg_valid_i && pg_ready_o;
    owed_response_c      = pg_fire_c && issued_head_valid_c;
    owed_response_push_c = owed_response_c;
    issued_pop_c         = owed_response_c;

    response_wrong_op_c     = (pg_op_i != SHEET_OP_READ);
    response_wrong_status_c = (pg_status_i != SHEET_STATUS_HIT)
                           && (pg_status_i != SHEET_STATUS_MISS);
    response_wrong_src_c    = (pg_src_id_i != issued_expected_src_c);
    response_hit_c = owed_response_c && !response_wrong_op_c
                   && !response_wrong_status_c && !response_wrong_src_c
                   && (pg_status_i == SHEET_STATUS_HIT);
    response_miss_c = owed_response_c && !response_wrong_op_c
                    && !response_wrong_status_c && !response_wrong_src_c
                    && (pg_status_i == SHEET_STATUS_MISS);
    response_ok_c = response_hit_c;

    response_status_c   = response_ok_c ? 8'd0 : SOURCE_REFUSED;
    response_tag_c      = response_ok_c ? pg_tag_i : 8'd0;
    response_strength_c = response_ok_c ? pg_strength_i : 8'd0;

    // A local refusal waits if the owed response takes the final free slot.
    refuse_valid_o = offer_head_valid_c && offer_head_refuse_c;
    local_push_c = refuse_valid_o
                && (return_free_c >= (owed_response_push_c
                                      ? CREDIT_W'(2) : CREDIT_W'(1)));

    // A non-refused head may be offered only with an issued-identity slot.  The
    // credit bound makes the full case unreachable, but the full guard is real.
    req_valid_o  = offer_head_valid_c && !offer_head_refuse_c
                && (issued_count_q != CREDIT_W'(CREDIT));
    req_op_o     = SHEET_OP_READ;
    req_handle_o = offer_handle_q[offer_read_q];
    req_texel_o  = {offer_v_q[offer_read_q], offer_u_q[offer_read_q]};
    req_src_id_o = {{(16-OWNERW){1'b0}}, offer_head_owner_c};
    req_fire_c   = req_valid_o && req_ready_i;

    offer_pop_c = req_fire_c || local_push_c;

    out_valid_o  = (return_count_q != CREDIT_W'(0));
    out_owner_o  = return_owner_q[return_read_q];
    out_result_o = return_result_q[return_read_q];
    out_fire_c   = out_valid_o && out_ready_i;

    return_pushes_c = 2'(owed_response_push_c) + 2'(local_push_c);

    idle_o = (credit_q == CREDIT_W'(0))
          && !a0_valid_q
          && !div_in_valid_q
          && (div_occupancy_w == 4'd0)
          && (offer_count_q == CREDIT_W'(0))
          && (issued_count_q == CREDIT_W'(0))
          && (return_count_q == CREDIT_W'(0));
  end

  // Synthesizable positive-control instrument.  Each predicate is independently
  // named so a mutant can force exactly one fault without moving both sides of a
  // comparison together.  The shipped counter advances once per faulty cycle,
  // even if several predicates are simultaneously true, and wraps modulo 2^32.
  logic credit_bound_fault_c;
  logic offer_bound_fault_c;
  logic issued_bound_fault_c;
  logic return_bound_fault_c;
  logic fixed_producer_room_fault_c;
  logic owed_response_room_fault_c;
  logic owed_response_room_seen_q;
  logic owed_response_room_event_c;
  logic issue_before_local_return_fault_c;
  logic credit_fault_event_c;

  always_comb begin
    credit_bound_fault_c = credit_q > CREDIT_W'(CREDIT);
    offer_bound_fault_c = offer_count_q > CREDIT_W'(CREDIT);
    issued_bound_fault_c = issued_count_q > CREDIT_W'(CREDIT);
    return_bound_fault_c = return_count_q > CREDIT_W'(CREDIT);
    fixed_producer_room_fault_c = div_valid_w
                               && (offer_count_q == CREDIT_W'(CREDIT))
                               && !offer_pop_c;
    owed_response_room_fault_c = issued_head_valid_c
                              && (return_free_c == CREDIT_W'(0));
    owed_response_room_event_c = owed_response_room_fault_c
                              && !owed_response_room_seen_q;
    issue_before_local_return_fault_c = local_push_c
                                     && (credit_q == CREDIT_W'(0));
    credit_fault_event_c = credit_bound_fault_c
                        || offer_bound_fault_c
                        || issued_bound_fault_c
                        || return_bound_fault_c
                        || fixed_producer_room_fault_c
                        || owed_response_room_event_c
                        || issue_before_local_return_fault_c;
  end

  // ==========================================================================
  // State transitions
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      credit_q                    <= CREDIT_W'(0);
      a0_valid_q                  <= 1'b0;
      a0_refuse_q                 <= 1'b0;
      a0_degenerate_q             <= 1'b0;
      a0_input_fault_q            <= 1'b0;
      div_in_valid_q              <= 1'b0;
      side_write_q                <= '0;
      offer_write_q               <= '0;
      offer_read_q                <= '0;
      offer_count_q               <= CREDIT_W'(0);
      issued_write_q              <= '0;
      issued_read_q               <= '0;
      issued_count_q              <= CREDIT_W'(0);
      return_write_q              <= '0;
      return_read_q               <= '0;
      return_count_q              <= CREDIT_W'(CREDIT + 1);
      owed_response_room_seen_q  <= 1'b0;
      accepted_o                  <= 32'd0;
      sheet_reads_o               <= 32'd0;
      local_refused_o             <= 32'd0;
      completed_o                 <= 32'd0;
      degenerate_o                <= 32'd0;
      sheet_hits_o                <= 32'd0;
      sheet_misses_o              <= 32'd0;
      sheet_rsp_wrong_op_o        <= 32'd0;
      sheet_rsp_wrong_status_o    <= 32'd0;
      sheet_rsp_wrong_src_o       <= 32'd0;
      sheet_rsp_unsolicited_o     <= 32'd0;
      credit_fault_o               <= 32'd0;
      frame_fault_o               <= 1'b0;
    end else begin
      // Frame-boundary clear affects only the sticky frame summary.  Every
      // event-specific assignment below appears later in this process, so a
      // same-edge fault has set priority.  No counter, credit, valid or payload
      // register shares this clear.
      if (frame_fault_clear_i)
        frame_fault_o <= 1'b0;

      // Edge-qualify the independent owed/no-room level: a held corrupt state
      // is one fault occurrence, not a fresh event on every stalled cycle.
      owed_response_room_seen_q <= owed_response_room_fault_c;

      // The lifetime credit is released only at typed owner-return acceptance.
      credit_q <= credit_q + CREDIT_W'(job_fire_c) - CREDIT_W'(out_fire_c);

      // A0 is the issue-before-local-return fence. Even force_refuse enters this
      // registered stage after its logical issue pulse. The two diagnostic facts
      // are captured independently so the sticky frame/counter fanout starts only
      // from A0 registers, never from live fragment/envelope inputs.
      a0_valid_q <= job_fire_c;
      if (job_fire_c) begin
        a0_refuse_q     <= job_force_refuse_i || envelope_degenerate_c;
        a0_degenerate_q <= `ZHAO_AUX_T4_DEGENERATE_CAPTURE(
                               envelope_degenerate_c);
        a0_input_fault_q <= `ZHAO_AUX_T4_INPUT_FAULT_CAPTURE(
                                job_force_refuse_i || envelope_degenerate_c);
        a0_du_q         <= du_c;
        a0_dv_q         <= dv_c;
        a0_nu_q         <= nu_c;
        a0_nv_q         <= nv_c;
        a0_handle_q     <= job_sheet_handle_i;
        a0_owner_q      <= job_owner_i;
        accepted_o      <= accepted_o + 32'd1;
      end
      if (a0_valid_q) begin
        if (a0_degenerate_q)
          degenerate_o <= degenerate_o + 32'd1;
        if (a0_input_fault_q)
          frame_fault_o <= 1'b1;
      end

      // Register the complete clamp/divider identity bundle. Partial retiming
      // would pair one job's numerator with another job's denominator or side
      // index under back-to-back traffic.
      div_in_valid_q <= a0_valid_q;
      if (a0_valid_q) begin
        div_in_ru_q <= (neg_u_c || sat_u_c) ? {REM_W{1'b0}}
                                             : REM_W'($unsigned(a0_nu_q));
        div_in_du_q <= a0_du_q;
        div_in_rv_q <= (neg_v_c || sat_v_c) ? {REM_W{1'b0}}
                                             : REM_W'($unsigned(a0_nv_q));
        div_in_dv_q <= a0_dv_q;
        div_in_tag_q <= side_write_q;
      end

      // Side data is written on the exact registered-clamp edge.
      if (a0_valid_q) begin
        side_refuse_q[side_write_q]     <= a0_refuse_q;
        side_sat_u_q[side_write_q]      <= sat_u_c;
        side_sat_v_q[side_write_q]      <= sat_v_c;
        side_handle_q[side_write_q]     <= a0_handle_q;
        side_owner_q[side_write_q]      <= a0_owner_q;
        side_write_q                    <= side_write_q + CREDIT_AW'(1);
      end

      // Every fixed-latency divider result has a credit-reserved offer slot.
      if (div_valid_w) begin
        offer_u_q[offer_write_q] <= side_sat_u_q[div_tag_w] ? 6'd63 : div_u_w;
        offer_v_q[offer_write_q] <= side_sat_v_q[div_tag_w] ? 6'd63 : div_v_w;
        offer_handle_q[offer_write_q] <= side_handle_q[div_tag_w];
        offer_owner_q[offer_write_q]  <= side_owner_q[div_tag_w];
        offer_refuse_q[offer_write_q] <= side_refuse_q[div_tag_w];
        offer_write_q <= offer_write_q + CREDIT_AW'(1);
      end
      if (offer_pop_c)
        offer_read_q <= offer_read_q + CREDIT_AW'(1);
      offer_count_q <= offer_count_q + CREDIT_W'(div_valid_w)
                                    - CREDIT_W'(offer_pop_c);

      // Request acceptance creates the owed identity entry.  Moving a response
      // to the return FIFO consumes exactly the head entry.
      if (req_fire_c) begin
        issued_owner_q[issued_write_q] <= offer_head_owner_c;
        issued_write_q <= issued_write_q + CREDIT_AW'(1);
        sheet_reads_o <= sheet_reads_o + 32'd1;
      end
      if (issued_pop_c)
        issued_read_q <= issued_read_q + CREDIT_AW'(1);
      issued_count_q <= issued_count_q + CREDIT_W'(req_fire_c)
                                      - CREDIT_W'(issued_pop_c);

      // Owed response first.  Its owner comes from the issued FIFO head, never
      // from the response's claimed source id.
      if (owed_response_push_c) begin
        return_owner_q[return_write_q] <= issued_head_owner_c;
        return_result_q[return_write_q] <= {
          response_status_c, response_tag_c, response_strength_c, 24'd0
        };
        return_local_q[return_write_q] <= 1'b0;

        if (response_hit_c)
          sheet_hits_o <= sheet_hits_o + 32'd1;
        if (response_miss_c) begin
          sheet_misses_o <= sheet_misses_o + 32'd1;
          frame_fault_o  <= 1'b1;
        end
        if (response_wrong_op_c) begin
          sheet_rsp_wrong_op_o <= sheet_rsp_wrong_op_o + 32'd1;
          frame_fault_o <= 1'b1;
        end
        if (response_wrong_status_c) begin
          sheet_rsp_wrong_status_o <= sheet_rsp_wrong_status_o + 32'd1;
          frame_fault_o <= 1'b1;
        end
        if (response_wrong_src_c) begin
          sheet_rsp_wrong_src_o <= sheet_rsp_wrong_src_o + 32'd1;
          frame_fault_o <= 1'b1;
        end
      end else if (pg_fire_c && !issued_head_valid_c) begin
        // Always-draining fault sink: no owner and no return entry are created.
        sheet_rsp_unsolicited_o <= sheet_rsp_unsolicited_o + 32'd1;
        frame_fault_o <= 1'b1;
      end

      if (local_push_c) begin
        automatic logic [CREDIT_AW-1:0] local_write;
        local_write = owed_response_push_c
                    ? (return_write_q + CREDIT_AW'(1)) : return_write_q;
        return_owner_q[local_write] <= offer_head_owner_c;
        return_result_q[local_write] <= {SOURCE_REFUSED, 8'd0, 8'd0, 24'd0};
        return_local_q[local_write] <= 1'b1;
      end

      return_write_q <= return_write_q + CREDIT_AW'(return_pushes_c);
      if (out_fire_c)
        return_read_q <= return_read_q + CREDIT_AW'(1);
      return_count_q <= return_count_q + CREDIT_W'(return_pushes_c)
                                      - CREDIT_W'(out_fire_c);

      if (out_fire_c) begin
        completed_o <= completed_o + 32'd1;
        if (return_local_q[return_read_q])
          local_refused_o <= local_refused_o + 32'd1;
      end

      // This is a durable instrument, not the sticky summary.  Frame clear never
      // touches it, and this final assignment gives same-edge fault set priority.
      if (credit_fault_event_c) begin
        credit_fault_o <= credit_fault_o + 32'd1;
        frame_fault_o  <= 1'b1;
      end
    end
  end

`ifndef QUARTUS_SYNTHESIS
  // Structural bounds are also checked from the directed bench.  These local
  // assertions make accidental edits loud in any --assert elaboration.
  logic past_valid_q;
  logic past_req_valid_q;
  logic past_req_ready_q;
  logic [1:0] past_req_op_q;
  logic [31:0] past_req_handle_q;
  logic [11:0] past_req_texel_q;
  logic [15:0] past_req_src_q;
  logic past_out_valid_q;
  logic past_out_ready_q;
  logic [OWNERW-1:0] past_out_owner_q;
  logic [47:0] past_out_result_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      past_valid_q      <= 1'b0;
      past_req_valid_q  <= 1'b0;
      past_req_ready_q  <= 1'b0;
      past_out_valid_q  <= 1'b0;
      past_out_ready_q  <= 1'b0;
    end else begin
      past_valid_q      <= 1'b1;
      past_req_valid_q  <= req_valid_o;
      past_req_ready_q  <= req_ready_i;
      past_req_op_q     <= req_op_o;
      past_req_handle_q <= req_handle_o;
      past_req_texel_q  <= req_texel_o;
      past_req_src_q    <= req_src_id_o;
      past_out_valid_q  <= out_valid_o;
      past_out_ready_q  <= out_ready_i;
      past_out_owner_q  <= out_owner_o;
      past_out_result_q <= out_result_o;
    end
  end

  always_ff @(posedge clk) begin
    if (past_valid_q) begin
      a_credit_bound: assert (!credit_bound_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[CREDIT_BOUND]");
      a_offer_bound: assert (!offer_bound_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[OFFER_BOUND]");
      a_issued_bound: assert (!issued_bound_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[ISSUED_BOUND]");
      a_return_bound: assert (!return_bound_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[RETURN_BOUND]");
      a_fixed_producer_has_room: assert (!fixed_producer_room_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[FIXED_PRODUCER_ROOM]");
      a_owed_response_has_room: assert (!owed_response_room_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[OWED_RESPONSE_ROOM]");
      a_issue_precedes_local_return: assert (!issue_before_local_return_fault_c)
        else $error("ZHAO_AUX_V2_ASSERT_FIRE[ISSUE_BEFORE_LOCAL_RETURN]");

      if (past_req_valid_q && !past_req_ready_q) begin
        a_req_valid_held: assert (req_valid_o);
        a_req_op_held: assert (req_op_o == past_req_op_q);
        a_req_handle_held: assert (req_handle_o == past_req_handle_q);
        a_req_texel_held: assert (req_texel_o == past_req_texel_q);
        a_req_src_held: assert (req_src_id_o == past_req_src_q);
      end
      if (past_out_valid_q && !past_out_ready_q) begin
        a_out_valid_held: assert (out_valid_o);
        a_out_owner_held: assert (out_owner_o == past_out_owner_q);
        a_out_result_held: assert (out_result_o == past_out_result_q);
      end
    end
  end
`endif

  // The divider's own accepted-count witness is intentionally not lifecycle
  // authority; accepted_o and completed_o are the Packet-B accounting edges.
  logic unused_divider_evidence;
  always_comb unused_divider_evidence = |div_issued_unused_w & 1'b0;

endmodule : zhao_texture_aux_pipe_v2_return_bound_mutant

`undef ZHAO_AUX_T4_DEGENERATE_CAPTURE
`undef ZHAO_AUX_T4_INPUT_FAULT_CAPTURE
`default_nettype wire
