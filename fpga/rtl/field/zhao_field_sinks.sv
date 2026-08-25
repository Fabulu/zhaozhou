// zhao_field_sinks.sv -- the three Earth sink composition laws, in fabric.
//
// FIELD.WRITE.MATERIAL / NAV / HAZARD. The laws are the owner ruling of
// 2026-08-24, shipped as `zref::fieldir::compose_material`, `compose_nav` and
// `compose_hazard`. This block is their differential pen-mate: it consumes the
// same write stream in the same accepted-command order and must agree exactly.
//
// The three sinks are composed INDEPENDENTLY over one shared beat stream, which
// is why they live in one block: a beat carries at most one contribution to
// each layer, and the neutral element differs per layer.
//
//   MATERIAL  last ENABLED writer wins -- so it needs an explicit enable, and
//             the reference gives it one. Priority is command ORDER; there is
//             deliberately no material hierarchy in hardware.
//   NAV       deltas ADD. The additive identity is 0, so a beat that does not
//             touch nav writes 0 and needs no enable bit.
//   HAZARD    severities combine by MAX, and the reference states that zero is
//             neutral -- so again 0 is the no-contribution encoding.
//
// Adding enable bits to NAV and HAZARD would have been inventing interface
// where the laws already supply a neutral element.
//
// ENFORCED-BY: tests/differential/field_sinks_directed.cpp
module zhao_field_sinks (
    input  logic               clk,
    input  logic               rst_n,

    // Load authored layer E and restart the composition. The authored value is
    // never modified by the laws; it is the starting point.
    input  logic               load_i,
    input  logic [7:0]         auth_mat_a_i,
    input  logic [7:0]         auth_mat_b_i,
    input  logic [7:0]         auth_weight_i,
    input  logic signed [31:0] auth_nav_i,
    input  logic [7:0]         auth_hazard_i,

    // One write beat, in accepted command order.
    input  logic               wr_valid_i,
    input  logic               wr_mat_en_i,
    input  logic [7:0]         wr_mat_a_i,
    input  logic [7:0]         wr_mat_b_i,
    input  logic [7:0]         wr_mat_weight_i,
    input  logic signed [31:0] wr_nav_delta_i,
    input  logic signed [31:0] wr_haz_sev_i,

    output logic [7:0]         mat_a_o,
    output logic [7:0]         mat_b_o,
    output logic [7:0]         weight_o,
    output logic signed [31:0] nav_o,
    output logic [7:0]         hazard_o
);

  // ---- material -----------------------------------------------------------
  logic [7:0] mat_a_q, mat_b_q, weight_q;

  // ---- nav ----------------------------------------------------------------
  // The reference accumulates in int64 and clamps to the int32 range after
  // EVERY delta, but floors at zero only ONCE, on the way out. Those are not
  // the same thing: from cost 1, deltas {-10, +20} give 11, because the
  // intermediate -9 is carried rather than floored. Flooring per step would
  // give 20. The accumulator is therefore signed and may sit negative.
  logic signed [31:0] nav_q;

  wire signed [32:0] nav_sum = {nav_q[31], nav_q} + {wr_nav_delta_i[31], wr_nav_delta_i};
  wire signed [31:0] nav_sat = (nav_sum > 33'sh0_7FFFFFFF) ? 32'sh7FFFFFFF
                             : (nav_sum < -33'sh0_80000000) ? 32'sh80000000
                             : nav_sum[31:0];

  // ---- hazard -------------------------------------------------------------
  // Clamp to [0, 1.0] in Q16.16, convert to u8 with round-half-up, keep the
  // maximum. `s * 255` is a shift and a subtract, so this costs no DSP.
  logic [7:0] haz_q;

  // The conversion is a function so the rounding remainder stays a local, the
  // way every other rounded rescale in this tree is written.
  //
  //   s*255 peaks at 65536*255 = 0xFF0000, so the rounded sum peaks at
  //   0xFF8000 and bit 24 can never set. `s * 255` is a shift and a subtract,
  //   so this costs no DSP.
  function automatic logic [7:0] sev_to_u8(input logic signed [31:0] s);
    logic signed [31:0] lo;
    logic        [16:0] cl;
    begin
      lo = (s < 32'sd0) ? 32'sd0 : s;
      cl = (lo > 32'sd65536) ? 17'd65536 : lo[16:0];
      // The rounding remainder is discarded by the shift, so it is never named:
      // a named intermediate here would be sixteen bits nothing reads, which is
      // a lint waiver rather than a design.
      sev_to_u8 = 8'((({cl, 8'd0} - {8'd0, cl}) + 25'd32768) >> 16);
    end
  endfunction

  wire [7:0] haz_u8 = sev_to_u8(wr_haz_sev_i);

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      mat_a_q  <= 8'd0;
      mat_b_q  <= 8'd0;
      weight_q <= 8'd0;
      nav_q    <= 32'sd0;
      haz_q    <= 8'd0;
    end else if (load_i) begin
      mat_a_q  <= auth_mat_a_i;
      mat_b_q  <= auth_mat_b_i;
      weight_q <= auth_weight_i;
      nav_q    <= auth_nav_i;
      haz_q    <= auth_hazard_i;
    end else if (wr_valid_i) begin
      if (wr_mat_en_i) begin
        mat_a_q  <= wr_mat_a_i;
        mat_b_q  <= wr_mat_b_i;
        weight_q <= wr_mat_weight_i;
      end
      nav_q <= nav_sat;
      if (haz_u8 > haz_q) haz_q <= haz_u8;
    end
  end

  assign mat_a_o  = mat_a_q;
  assign mat_b_o  = mat_b_q;
  assign weight_o = weight_q;
  assign hazard_o = haz_q;

  // The zero floor is a property of the ANSWER, not of the accumulator.
  assign nav_o = (nav_q < 32'sd0) ? 32'sd0 : nav_q;

endmodule
