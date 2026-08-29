// zhao_field_v3_trig.sv — OP_SIN and OP_COS, four points at a time.
//
// ENFORCED-BY: tests/differential/field_v3_trig_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS AND WHY IT IS SMALL
// ---------------------------------------------------------------------------
// `tools/field/measure_earth_ops.cpp` planned the three shipped Earth programs
// and found exactly three canonical opcodes they use that the hardware did not
// serve: DIST2, SIN and COS. This is two of the three, and it is the cheap
// two: the arithmetic already exists as `zhao_field_sin`, which the rotation
// service has been using all along.
//
// So this block is a SEQUENCER, not arithmetic. Its whole job is to walk four
// points through one lookup unit and hand the answers back in lane order.
//
// The oracle is one line each:
//
//     OP_SIN   dst[0] = fx_sin(angle16{(uint16_t)src[0]})
//     OP_COS   dst[0] = fx_cos(angle16{(uint16_t)src[0]})
//
// THE LOW SIXTEEN BITS ARE THE ANGLE and the upper half is ignored rather than
// being an error — the same law the rotation service states for its own angle
// port, and for the same reason: a caller that leaves rubbish in the top half
// gets a defined answer, and the same one the software gives.
//
// ---------------------------------------------------------------------------
// ONE LOOKUP UNIT, WALKED — AND THE LATENCY IS TWO, NOT ONE
// ---------------------------------------------------------------------------
// Four copies of `zhao_field_sin` would answer in one clock and cost four
// tables. Walking one costs four clocks and one table, which is the same trade
// the rotation service already makes, and this engine is short of area rather
// than of clocks.
//
// `zhao_field_sin` has TWO registered stages. So the answer arriving while the
// counter reads `k` belongs to issue `k - 2`:
//
//     cycle 0   present lane 0's angle
//     cycle 1   present lane 1's angle
//     cycle 2   present lane 2's angle, capture lane 0
//     cycle 3   present lane 3's angle, capture lane 1
//     cycle 4                           capture lane 2
//     cycle 5                           capture lane 3
//     cycle 6   all four readable  -> reply
//
// **Completing at cycle 5 would be the bug this engine has now made twice** —
// the curve service's neighbour phase and the ring service's uniform fetch both
// declared themselves finished on the clock their last capture was still a
// non-blocking assignment in flight. Seven cycles for four lookups is correct
// and the extra one is not slack.
module zhao_field_v3_trig #(
    parameter int LANES = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic               is_cos_i,   // 0 = OP_SIN, 1 = OP_COS
    // The angle per point. Only [15:0] is read; the top half is ignored by
    // law, not by accident.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [31:0] a0_0_i, a0_1_i, a0_2_i, a0_3_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    output var logic        [ 7:0] tag_o
);

  localparam logic [1:0] T_IDLE = 2'd0;
  localparam logic [1:0] T_WALK = 2'd1;
  localparam logic [1:0] T_HOLD = 2'd2;

  logic [1:0]         state_r;
  logic [2:0]         k_r;              // 0..6
  logic        [15:0] ang_r [LANES];
  logic               cos_r;
  logic        [ 7:0] tag_r;
  logic signed [31:0] res_r [LANES];

  // The angle presented this cycle. Lanes 0..3 go out on cycles 0..3; the
  // index is masked so cycles 4..6 present lane 0 again, harmlessly, rather
  // than indexing out of range.
  logic [15:0] ang_c;
  assign ang_c = ang_r[k_r[1:0]];

  logic signed [31:0] sin_result;
  zhao_field_sin u_sin (
      .clk(clk),
      .angle_i(ang_c),
      .is_cos_i(cos_r),
      .result_o(sin_result)
  );

  assign v_ready_o = (state_r == T_IDLE);
  assign r_valid_o = (state_r == T_HOLD);
  assign tag_o     = tag_r;
  assign o0_0_o    = res_r[0];
  assign o0_1_o    = res_r[1];
  assign o0_2_o    = res_r[2];
  assign o0_3_o    = res_r[3];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_r <= T_IDLE;
      k_r     <= 3'd0;
      cos_r   <= 1'b0;
      tag_r   <= 8'd0;
      for (int l = 0; l < LANES; l++) begin
        ang_r[l] <= 16'd0;
        res_r[l] <= '0;
      end
    end else begin
      case (state_r)
        T_IDLE: begin
          if (v_valid_i) begin
            ang_r[0] <= a0_0_i[15:0];
            ang_r[1] <= a0_1_i[15:0];
            ang_r[2] <= a0_2_i[15:0];
            ang_r[3] <= a0_3_i[15:0];
            cos_r    <= is_cos_i;
            tag_r    <= tag_i;
            k_r      <= 3'd0;
            state_r  <= T_WALK;
          end
        end

        T_WALK: begin
          if (k_r != 3'd6) k_r <= k_r + 3'd1;

          // The answer on the port now belongs to issue k-2. Deriving the
          // capture index from the SAME counter that drives the address is
          // what stops the two drifting apart.
          if ((k_r >= 3'd2) && (k_r <= 3'd5)) begin
            automatic logic [1:0] cap = 2'(k_r - 3'd2);
            res_r[cap] <= sin_result;
          end

          // Cycle 6, not 5: lane 3's capture is still in flight at 5.
          if (k_r == 3'd6) state_r <= T_HOLD;
        end

        T_HOLD: begin
          if (r_ready_i) state_r <= T_IDLE;
        end

        default: state_r <= T_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v3_trig
