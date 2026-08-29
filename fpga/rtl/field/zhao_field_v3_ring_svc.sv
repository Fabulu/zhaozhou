// zhao_field_v3_ring_svc.sv — THE PREPARED RING, JOINED TO ITS UNIFORMS.
//
// ENFORCED-BY: tests/differential/field_v3_ring_svc_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// `zhao_field_v3_ring` already computes the prepared ring and is closed at
// 23/23. It takes the four varying distances and FOUR UNIFORM SCALARS — r0,
// the midpoint m, and the two smoothstep reciprocals rA and rB — as direct
// inputs, because the reference computes those once per association on the
// ARM (`spec/form/cost-model.md`: uniform_ops are "executed ONCE per
// association ON THE ARM").
//
// So the arithmetic half exists and the storage half exists
// (`zhao_field_v3_sbank`). This is the piece between them: it fetches the four
// scalars out of the bank and hands them to the unit.
//
// THE OWNER CHOSE THIS SHAPE DELIBERATELY. Option (b) was to replicate each
// uniform across all four lanes and pass them as ordinary vector operands.
// That would have worked and it was rejected as a permanent architecture:
// "Do not make replicated uniform values in all four lanes the permanent
// architecture, and do not move RING back to cold." Four copies of one number
// burn register-file bandwidth and invent a special case that the next
// prepared operation would have to invent again.
//
// ---------------------------------------------------------------------------
// WHY THE SLOTS ARE PACKED INTO THE IMMEDIATE
// ---------------------------------------------------------------------------
// The planner allocates the four scalars NON-CONSECUTIVELY — `s_m`, then a
// temporary, then `s_rA`, then another temporary, then `s_rB`, with `s_r0`
// wherever the source register already lived. So one base index cannot
// address them and four indices have to travel with the instruction.
//
// `tools/field/measure_sreg_hwm.cpp` measured the bank at a worst case of 41
// slots, so 64 is the depth and 6 bits is the index. Four of them is 24 bits,
// which fits the existing 32-bit immediate with 8 to spare. The instruction
// word does not grow. That is why the depth was measured before this encoding
// was chosen rather than after.
//
//     imm[ 5: 0]  slot of r0
//     imm[11: 6]  slot of m
//     imm[17:12]  slot of rA
//     imm[23:18]  slot of rB
//     imm[31:24]  reserved, must be zero
//
// ---------------------------------------------------------------------------
// THE FETCH IS SIX CYCLES FOR FOUR VALUES, AND THAT IS NOT A MISTAKE
// ---------------------------------------------------------------------------
// The bank's read is REGISTERED: the datum for the address presented on cycle
// T lands on T+1, as a non-blocking assignment. So:
//
//     cycle 0   present r0's address
//     cycle 1   present m's  address, capture r0
//     cycle 2   present rA's address, capture m
//     cycle 3   present rB's address, capture rA
//     cycle 4                          capture rB
//     cycle 5   all four are readable  -> hand off
//
// **Handing off at cycle 4 would be the SPLINE bug again.** The curve
// service's neighbour phase declared itself finished on the cycle its last
// capture was still in flight and gave the arithmetic a value that had not
// been written yet: 96 failures out of 6930, visible only on the probes where
// the cubic actually ran, and passing outright when a probe was run in
// isolation because the stale register happened to hold the right number.
//
// One extra cycle, paid once per group, against a ring that already spends
// nine multiplier slots on that group. It is not on the critical path and it
// is the difference between right and subtly wrong.
module zhao_field_v3_ring_svc (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    input  var logic signed [31:0] req_d_0_i, req_d_1_i, req_d_2_i, req_d_3_i,
    input  var logic        [31:0] req_imm_i,   // packed slots, see above
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

    // Latches if an instruction ever sets the reserved immediate bits. They
    // are the only place a future encoding can grow, so a program that already
    // uses them is a program written against a different ABI, and that is a
    // fault to report rather than a field to ignore.
    output var logic               imm_bad_o
);

  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_FETCH = 3'd1;
  localparam logic [2:0] S_RUN   = 3'd2;
  localparam logic [2:0] S_HOLD  = 3'd3;

  logic [2:0] state_r;
  logic [2:0] fcyc_r;            // 0..5, the fetch phase

  logic signed [31:0] d_r [4];
  logic        [ 7:0] tag_r;
  logic        [ 5:0] slot_r [4];
  logic signed [31:0] uni_r [4];  // r0, m, rA, rB in that order

  // ---- the fetch schedule ---------------------------------------------------
  // Address on 0..3, capture on 1..4, complete at 5. `fcyc_r` names the cycle
  // and both halves derive from it, so the address and the capture cannot
  // disagree about which slot is in flight.
  assign sb_raddr_o = slot_r[fcyc_r[1:0]];

  logic fetch_done_c;
  assign fetch_done_c = (state_r == S_FETCH) && (fcyc_r == 3'd5);

  // ---- the arithmetic -------------------------------------------------------
  logic rg_v_valid, rg_v_ready, rg_r_valid;
  logic rg_offered_r;

  // Offered for exactly one clock. The unit latches on its own valid/ready and
  // re-offering a group it already took would send it twice. Defensive here
  // for the same reason the curve service's `f_spl_offered` is: it costs one
  // flop and it is correct the day the surrounding handshake changes.
  assign rg_v_valid = (state_r == S_RUN) && !rg_offered_r;

  zhao_field_v3_ring u_ring (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rg_v_valid), .v_ready_o(rg_v_ready),
      .d_0_i(d_r[0]), .d_1_i(d_r[1]), .d_2_i(d_r[2]), .d_3_i(d_r[3]),
      .r0_i(uni_r[0]), .m_i(uni_r[1]), .rA_i(uni_r[2]), .rB_i(uni_r[3]),
      .tag_i(tag_r),
      .r_valid_o(rg_r_valid), .r_ready_i(rsp_ready_i),
      .o0_0_o(rsp_r_0_o), .o0_1_o(rsp_r_1_o), .o0_2_o(rsp_r_2_o), .o0_3_o(rsp_r_3_o),
      .sat_add_o(rsp_sat_add_o), .sat_mul_o(rsp_sat_mul_o), .tag_o(rsp_tag_o),
      .mul_issue_o(mul_issue_o), .mul_ready_i(mul_ready_i),
      .mul_a_0_o(mul_a_0_o), .mul_a_1_o(mul_a_1_o),
      .mul_a_2_o(mul_a_2_o), .mul_a_3_o(mul_a_3_o),
      .mul_b_0_o(mul_b_0_o), .mul_b_1_o(mul_b_1_o),
      .mul_b_2_o(mul_b_2_o), .mul_b_3_o(mul_b_3_o),
      .mul_valid_i(mul_valid_i),
      .mul_p_0_i(mul_p_0_i), .mul_p_1_i(mul_p_1_i),
      .mul_p_2_i(mul_p_2_i), .mul_p_3_i(mul_p_3_i)
  );

  assign rsp_valid_o = rg_r_valid;
  assign req_ready_o = (state_r == S_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_r      <= S_IDLE;
      fcyc_r       <= 3'd0;
      tag_r        <= 8'd0;
      rg_offered_r <= 1'b0;
      imm_bad_o    <= 1'b0;
      for (int i = 0; i < 4; i++) begin
        d_r[i]    <= '0;
        slot_r[i] <= 6'd0;
        uni_r[i]  <= '0;
      end
    end else begin
      case (state_r)
        S_IDLE: begin
          if (req_valid_i) begin
            d_r[0] <= req_d_0_i;
            d_r[1] <= req_d_1_i;
            d_r[2] <= req_d_2_i;
            d_r[3] <= req_d_3_i;
            tag_r  <= req_tag_i;
            slot_r[0] <= req_imm_i[5:0];
            slot_r[1] <= req_imm_i[11:6];
            slot_r[2] <= req_imm_i[17:12];
            slot_r[3] <= req_imm_i[23:18];
            // LATCHED, not a pulse: one instruction with a stale encoding is
            // the whole finding.
            if (req_imm_i[31:24] != 8'd0) imm_bad_o <= 1'b1;
            fcyc_r       <= 3'd0;
            rg_offered_r <= 1'b0;
            state_r      <= S_FETCH;
          end
        end

        S_FETCH: begin
          if (fcyc_r != 3'd5) fcyc_r <= fcyc_r + 3'd1;

          // The capture lags the address by one cycle, so cycle c writes the
          // slot addressed on c-1. Deriving both from `fcyc_r` is what stops
          // the two drifting apart.
          if ((fcyc_r >= 3'd1) && (fcyc_r <= 3'd4)) begin
            // The cast is explicit because the index is 2 bits and the counter
            // is 3. Verilator refuses the implicit narrowing, and it is right
            // to: an off-by-one here writes the wrong uniform, which is the
            // same class of fault as the neighbour-phase bug this schedule is
            // shaped to avoid.
            automatic logic [1:0] cap_idx = 2'(fcyc_r - 3'd1);
            uni_r[cap_idx] <= sb_rdata_i;
          end

          if (fetch_done_c) state_r <= S_RUN;
        end

        S_RUN: begin
          if (rg_v_valid && rg_v_ready) rg_offered_r <= 1'b1;
          // THE HANDSHAKE CAN COMPLETE ON THE EDGE THE ANSWER APPEARS, and
          // going to S_HOLD unconditionally is a hang. A consumer that is
          // already ready takes the reply on this same clock: the unit sees
          // r_valid && r_ready, drops r_valid, and S_HOLD would then wait
          // forever for a valid that has already been consumed.
          //
          // Found by this block's own differential -- the first group answered
          // correctly and the SECOND never started, which is the signature of
          // a state machine that parks rather than one that computes wrongly.
          // Same shape as the neighbour phase completing a cycle early: a
          // state waiting on something that is already gone.
          if (rg_r_valid) state_r <= rsp_ready_i ? S_IDLE : S_HOLD;
        end

        // Reached only when the answer arrived while the consumer was NOT
        // ready. The unit holds it; this state stops a new group being
        // accepted over the top of a reply still on the wire.
        S_HOLD: begin
          if (rg_r_valid && rsp_ready_i) state_r <= S_IDLE;
        end

        default: state_r <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v3_ring_svc
