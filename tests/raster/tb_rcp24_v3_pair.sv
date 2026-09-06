// tb_rcp24_v3_pair.sv — the shipped serial reciprocal, the V3 tile, and the V3
// tile's arithmetic core on its OWN port, side by side.
//
// THREE INSTANCES, AND THE THIRD ONE IS THE POINT.
//
// `zhao_raster_rcp24` is the oracle for BEHAVIOUR: same stimulus, same answers,
// bit for bit. That is what tb_rcp24_pair.sv already does for the V2 service and
// it is kept unchanged here.
//
// It is NOT sufficient for the V3 arithmetic. TEXTURE-ISLAND-V3 S10.5 replaces a
// 32-by-64 multiply with a 32x32 product plus a signed-wrap high-word
// correction, and the correction only engages when w > 2^31. Measured over the
// committed T24 table and all 16,777,215 nonzero denominators, both Newton steps
// included:
//
//     max w = 0x401F_EF88;  phases with w > 2^31 = 0.
//
// So a denominator-driven pair test CANNOT reach the correction. Delete the
// subtract entirely and every reciprocal still matches. `u_mul` is therefore
// exposed directly, with a/b/corr as ports, so the differential can drive the
// same (x, w) case families tools/rtl/architecture_numeric_checks.py drives --
// including the 124,679 negative-correction cases that script counts.
`default_nettype none

module tb_rcp24_v3_pair #(
    parameter int unsigned NCTX = 16,
    parameter int unsigned TOKW = 8,
    parameter int unsigned MTAGW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- A: the shipped serial reference -------------------------------------
    input  var logic        a_valid_i,
    output var logic        a_ready_o,
    input  var logic [23:0] a_d_i,
    output var logic        a_rvalid_o,
    input  var logic        a_rready_i,
    output var logic [23:0] a_r_o,
    output var logic [ 5:0] a_k_o,
    output var logic        a_zero_o,

    // ---- B: the V3 tile ------------------------------------------------------
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
    output var logic [31:0]     b_accepted_o,
    output var logic [31:0]     b_completed_o,
    output var logic [31:0]     b_mul_jobs_o,
    output var logic [31:0]     b_zero_jobs_o,
    output var logic [31:0]     b_phase_jobs_o,
    output var logic [31:0]     b_negcorr_jobs_o,
    output var logic [ 5:0]     b_occupancy_o,
    output var logic            b_qerr_o,

    // ---- C: the exact product unit, on its own port --------------------------
    input  var logic             c_valid_i,
    input  var logic [31:0]      c_a_i,
    input  var logic [31:0]      c_b_i,
    input  var logic [31:0]      c_corr_i,
    input  var logic [MTAGW-1:0] c_tag_i,
    output var logic             c_valid_o,
    output var logic [MTAGW-1:0] c_tag_o,
    output var logic [31:0]      c_phi_o,
    output var logic [31:0]      c_plo_o,
    output var logic [31:0]      c_wnext_o,
    output var logic [31:0]      c_xnext_o
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

  zhao_raster_rcp24_v3 #(
      .NCTX(NCTX),
      .TOKW(TOKW)
  ) u_v3 (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (b_valid_i),
      .v_ready_o     (b_ready_o),
      .d_i           (b_d_i),
      .v_tok_i       (b_tok_i),
      .r_valid_o     (b_rvalid_o),
      .r_ready_i     (b_rready_i),
      .r_o           (b_r_o),
      .k_o           (b_k_o),
      .d_zero_o      (b_zero_o),
      .r_tok_o       (b_tok_o),
      .accepted_o    (b_accepted_o),
      .completed_o   (b_completed_o),
      .mul_jobs_o    (b_mul_jobs_o),
      .zero_jobs_o   (b_zero_jobs_o),
      .phase_jobs_o  (b_phase_jobs_o),
      .negcorr_jobs_o(b_negcorr_jobs_o),
      .occupancy_o   (b_occupancy_o),
      .qerr_o        (b_qerr_o)
  );

  // The same module the tile instantiates, not a copy of it.
  zhao_raster_rcp24_mul #(
      .TAGW(MTAGW)
  ) u_mul (
      .clk     (clk),
      .rst_n   (rst_n),
      .valid_i (c_valid_i),
      .a_i     (c_a_i),
      .b_i     (c_b_i),
      .corr_i  (c_corr_i),
      .tag_i   (c_tag_i),
      .valid_o (c_valid_o),
      .tag_o   (c_tag_o),
      .p_hi_o  (c_phi_o),
      .p_lo_o  (c_plo_o),
      .w_next_o(c_wnext_o),
      .x_next_o(c_xnext_o)
  );

endmodule : tb_rcp24_v3_pair

`default_nettype wire
