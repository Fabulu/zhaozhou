// tb_rcp24_pair.sv — the serial reciprocal and the scheduled one, side by side.
//
// The brief says "Keep the exact existing arithmetic and ROM". The only way to
// hold that claim honestly is to run BOTH blocks on the same stimulus and
// compare, rather than re-implementing the Newton iteration in C++ and checking
// the new block against a third copy of the same idea.
//
// Both instances see identical `d_i`. The serial block accepts one at a time
// and the scheduled one accepts up to NCTX, so the harness drives each at its
// own pace and matches results BY TOKEN -- which is also the point of the
// rebuild: the scheduled block returns in completion order, not request order.
`default_nettype none

module tb_rcp24_pair #(
    parameter int unsigned NCTX = 8,
    parameter int unsigned TOKW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the serial reference ------------------------------------------------
    input  var logic        a_valid_i,
    output var logic        a_ready_o,
    input  var logic [23:0] a_d_i,
    output var logic        a_rvalid_o,
    input  var logic        a_rready_i,
    output var logic [23:0] a_r_o,
    output var logic [ 5:0] a_k_o,
    output var logic        a_zero_o,

    // ---- the scheduled candidate ---------------------------------------------
    input  var logic            b_valid_i,
    output var logic            b_ready_o,
    input  var logic [23:0]     b_d_i,
    input  var logic [TOKW-1:0] b_tok_i,
    output var logic            b_rvalid_o,
    input  var logic            b_rready_i,
    output var logic [23:0]     b_r_o,
    output var logic [ 5:0]     b_k_o,
    output var logic            b_zero_o,
    output var logic [TOKW-1:0] b_tok_o,
    output var logic [31:0]     b_mul_busy_o,
    output var logic [3:0]      b_occupancy_o
);

  zhao_raster_rcp24 u_ref (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (a_valid_i),
      .v_ready_o    (a_ready_o),
      .d_i          (a_d_i),
      .r_valid_o    (a_rvalid_o),
      .r_ready_i    (a_rready_i),
      .r_o          (a_r_o),
      .k_o          (a_k_o),
      .d_zero_o     (a_zero_o),
      .recips_o     (),
      .busy_clocks_o()
  );

  zhao_raster_rcp24_svc #(
      .NCTX(NCTX),
      .TOKW(TOKW)
  ) u_svc (
      .clk        (clk),
      .rst_n      (rst_n),
      .v_valid_i  (b_valid_i),
      .v_ready_o  (b_ready_o),
      .d_i        (b_d_i),
      .v_tok_i    (b_tok_i),
      .r_valid_o  (b_rvalid_o),
      .r_ready_i  (b_rready_i),
      .r_o        (b_r_o),
      .k_o        (b_k_o),
      .d_zero_o   (b_zero_o),
      .r_tok_o    (b_tok_o),
      .accepted_o (),
      .completed_o(),
      .mul_busy_o (b_mul_busy_o),
      .occupancy_o(b_occupancy_o)
  );

endmodule : tb_rcp24_pair

`default_nettype wire
