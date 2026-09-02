// tb_perspuv_pair.sv — the serial perspective block and the scheduled lane.
//
// `zhao_raster_perspuv` contains its own `zhao_raster_rcp24` and takes invw24.
// `zhao_raster_perspuv_svc` is the MULTIPLY LANE only and takes a reciprocal
// that has already returned. So the harness carries a THIRD instance -- a
// standalone rcp24 -- purely to derive the same mantissa and exponent the
// reference computes internally, and feeds those to the lane.
//
// That extra instance is the honest way to compare two blocks with different
// scopes. Approximating the mantissa in C++ instead would compare the lane
// against a guess at the reference's input, which is not the same test at all.
`default_nettype none

module tb_perspuv_pair #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the serial reference (rcp + multiply, one fragment at a time) -------
    input  var logic               a_valid_i,
    output var logic               a_ready_o,
    input  var logic signed [31:0] a_uow_i,
    input  var logic signed [31:0] a_vow_i,
    input  var logic        [23:0] a_invw_i,
    input  var logic        [15:0] a_tag_i,
    output var logic               a_rvalid_o,
    input  var logic               a_rready_i,
    output var logic signed [31:0] a_u_o,
    output var logic signed [31:0] a_v_o,
    output var logic        [15:0] a_tag_o,
    output var logic               a_sat_o,
    output var logic               a_dz_o,

    // ---- a standalone reciprocal, to feed the lane the same mant/k ----------
    input  var logic               c_valid_i,
    output var logic               c_ready_o,
    input  var logic        [23:0] c_d_i,
    output var logic               c_rvalid_o,
    input  var logic               c_rready_i,
    output var logic        [23:0] c_r_o,
    output var logic        [ 5:0] c_k_o,
    output var logic               c_zero_o,

    // ---- the scheduled multiply lane ----------------------------------------
    input  var logic               b_valid_i,
    output var logic               b_ready_o,
    input  var logic signed [31:0] b_uow_i,
    input  var logic signed [31:0] b_vow_i,
    input  var logic        [23:0] b_mant_i,
    input  var logic        [ 5:0] b_k_i,
    input  var logic               b_dz_i,
    input  var logic        [15:0] b_tag_i,
    output var logic               b_rvalid_o,
    input  var logic               b_rready_i,
    output var logic signed [31:0] b_u_o,
    output var logic signed [31:0] b_v_o,
    output var logic        [15:0] b_tag_o,
    output var logic               b_sat_o,
    output var logic               b_dz_o,
    output var logic [31:0]        b_products_o,
    output var logic [3:0]         b_occupancy_o
);

  zhao_raster_perspuv u_ref (
      .clk              (clk),
      .rst_n            (rst_n),
      .v_valid_i        (a_valid_i),
      .v_ready_o        (a_ready_o),
      .u_over_w_i       (a_uow_i),
      .v_over_w_i       (a_vow_i),
      .invw24_i         (a_invw_i),
      .tag_i            (a_tag_i),
      .r_valid_o        (a_rvalid_o),
      .r_ready_i        (a_rready_i),
      .u_o              (a_u_o),
      .v_o              (a_v_o),
      .tag_o            (a_tag_o),
      .sat_o            (a_sat_o),
      .depth_zero_o     (a_dz_o),
      .fragments_o      (),
      .sat_fragments_o  (),
      .rcp_recips_o     (),
      .rcp_busy_clocks_o()
  );

  zhao_raster_rcp24 u_rcp (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (c_valid_i),
      .v_ready_o    (c_ready_o),
      .d_i          (c_d_i),
      .r_valid_o    (c_rvalid_o),
      .r_ready_i    (c_rready_i),
      .r_o          (c_r_o),
      .k_o          (c_k_o),
      .d_zero_o     (c_zero_o),
      .recips_o     (),
      .busy_clocks_o()
  );

  zhao_raster_perspuv_svc #(
      .NTOK(NTOK),
      .TAGW(TAGW)
  ) u_svc (
      .clk         (clk),
      .rst_n       (rst_n),
      .v_valid_i   (b_valid_i),
      .v_ready_o   (b_ready_o),
      .u_over_w_i  (b_uow_i),
      .v_over_w_i  (b_vow_i),
      .r_mant_i    (b_mant_i),
      .r_k_i       (b_k_i),
      .depth_zero_i(b_dz_i),
      .tag_i       (b_tag_i),
      .r_valid_o   (b_rvalid_o),
      .r_ready_i   (b_rready_i),
      .u_o         (b_u_o),
      .v_o         (b_v_o),
      .tag_o       (b_tag_o),
      .sat_o       (b_sat_o),
      .depth_zero_o(b_dz_o),
      .fragments_o (),
      .products_o  (b_products_o),
      .occupancy_o (b_occupancy_o)
  );

endmodule : tb_perspuv_pair

`default_nettype wire
