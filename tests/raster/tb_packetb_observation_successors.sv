`default_nettype none

// Bounded cycle differential for the two Packet-B observation-only successors.
// CMake registration is intentionally owned by the integration packet.
module tb_packetb_observation_successors (
    input var logic clk,
    input var logic rst_n,

    input var logic rcp_valid_i,
    input var logic [23:0] rcp_d_i,
    input var logic [13:0] rcp_tok_i,
    input var logic rcp_out_ready_i,

    output var logic rcp_v3_ready_o,
    output var logic rcp_v4_ready_o,
    output var logic rcp_v3_valid_o,
    output var logic rcp_v4_valid_o,
    output var logic [23:0] rcp_v3_r_o,
    output var logic [23:0] rcp_v4_r_o,
    output var logic [5:0] rcp_v3_k_o,
    output var logic [5:0] rcp_v4_k_o,
    output var logic rcp_v3_zero_o,
    output var logic rcp_v4_zero_o,
    output var logic [13:0] rcp_v3_tok_o,
    output var logic [13:0] rcp_v4_tok_o,
    output var logic [31:0] rcp_v3_accepted_o,
    output var logic [31:0] rcp_v4_accepted_o,
    output var logic [31:0] rcp_v3_completed_o,
    output var logic [31:0] rcp_v4_completed_o,
    output var logic [31:0] rcp_v3_mul_jobs_o,
    output var logic [31:0] rcp_v4_mul_jobs_o,
    output var logic [31:0] rcp_v3_zero_jobs_o,
    output var logic [31:0] rcp_v4_zero_jobs_o,
    output var logic [31:0] rcp_v3_phase_jobs_o,
    output var logic [31:0] rcp_v4_phase_jobs_o,
    output var logic [31:0] rcp_v3_negcorr_jobs_o,
    output var logic [31:0] rcp_v4_negcorr_jobs_o,
    output var logic [5:0] rcp_v3_occupancy_o,
    output var logic [5:0] rcp_v4_occupancy_o,
    output var logic rcp_v3_qerr_o,
    output var logic rcp_v4_qerr_o,
    output var logic rcp_v4_idle_o,

    input var logic persp_valid_i,
    input var logic signed [31:0] persp_u_over_w_i,
    input var logic signed [31:0] persp_v_over_w_i,
    input var logic [23:0] persp_mant_i,
    input var logic [5:0] persp_k_i,
    input var logic persp_depth_zero_i,
    input var logic [13:0] persp_tag_i,
    input var logic persp_out_ready_i,

    output var logic persp_old_ready_o,
    output var logic persp_v2_ready_o,
    output var logic persp_old_valid_o,
    output var logic persp_v2_valid_o,
    output var logic signed [31:0] persp_old_u_o,
    output var logic signed [31:0] persp_v2_u_o,
    output var logic signed [31:0] persp_old_v_o,
    output var logic signed [31:0] persp_v2_v_o,
    output var logic [13:0] persp_old_tag_o,
    output var logic [13:0] persp_v2_tag_o,
    output var logic persp_old_sat_o,
    output var logic persp_v2_sat_o,
    output var logic persp_old_depth_zero_o,
    output var logic persp_v2_depth_zero_o,
    output var logic [31:0] persp_old_fragments_o,
    output var logic [31:0] persp_v2_fragments_o,
    output var logic [31:0] persp_old_products_o,
    output var logic [31:0] persp_v2_products_o,
    output var logic [31:0] persp_old_zero_products_o,
    output var logic [31:0] persp_v2_zero_products_o,
    output var logic [4:0] persp_old_occupancy_o,
    output var logic [4:0] persp_v2_occupancy_o,
    output var logic persp_v2_idle_o
);

  zhao_raster_rcp24_v3 #(.NCTX(12), .TOKW(14)) u_rcp_v3 (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rcp_valid_i), .v_ready_o(rcp_v3_ready_o),
      .d_i(rcp_d_i), .v_tok_i(rcp_tok_i),
      .r_valid_o(rcp_v3_valid_o), .r_ready_i(rcp_out_ready_i),
      .r_o(rcp_v3_r_o), .k_o(rcp_v3_k_o), .d_zero_o(rcp_v3_zero_o),
      .r_tok_o(rcp_v3_tok_o), .accepted_o(rcp_v3_accepted_o),
      .completed_o(rcp_v3_completed_o), .mul_jobs_o(rcp_v3_mul_jobs_o),
      .zero_jobs_o(rcp_v3_zero_jobs_o), .phase_jobs_o(rcp_v3_phase_jobs_o),
      .negcorr_jobs_o(rcp_v3_negcorr_jobs_o), .occupancy_o(rcp_v3_occupancy_o),
      .qerr_o(rcp_v3_qerr_o)
  );

  zhao_raster_rcp24_v4 #(.NCTX(12), .TOKW(14)) u_rcp_v4 (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rcp_valid_i), .v_ready_o(rcp_v4_ready_o),
      .d_i(rcp_d_i), .v_tok_i(rcp_tok_i),
      .r_valid_o(rcp_v4_valid_o), .r_ready_i(rcp_out_ready_i),
      .r_o(rcp_v4_r_o), .k_o(rcp_v4_k_o), .d_zero_o(rcp_v4_zero_o),
      .r_tok_o(rcp_v4_tok_o), .accepted_o(rcp_v4_accepted_o),
      .completed_o(rcp_v4_completed_o), .mul_jobs_o(rcp_v4_mul_jobs_o),
      .zero_jobs_o(rcp_v4_zero_jobs_o), .phase_jobs_o(rcp_v4_phase_jobs_o),
      .negcorr_jobs_o(rcp_v4_negcorr_jobs_o), .occupancy_o(rcp_v4_occupancy_o),
      .qerr_o(rcp_v4_qerr_o), .idle_o(rcp_v4_idle_o)
  );

  zhao_raster_perspuv_pairpipe #(.NTOK(16), .TAGW(14)) u_persp_old (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(persp_valid_i), .v_ready_o(persp_old_ready_o),
      .u_over_w_i(persp_u_over_w_i), .v_over_w_i(persp_v_over_w_i),
      .r_mant_i(persp_mant_i), .r_k_i(persp_k_i),
      .depth_zero_i(persp_depth_zero_i), .tag_i(persp_tag_i),
      .r_valid_o(persp_old_valid_o), .r_ready_i(persp_out_ready_i),
      .u_o(persp_old_u_o), .v_o(persp_old_v_o), .tag_o(persp_old_tag_o),
      .sat_o(persp_old_sat_o), .depth_zero_o(persp_old_depth_zero_o),
      .fragments_o(persp_old_fragments_o), .products_o(persp_old_products_o),
      .zero_products_o(persp_old_zero_products_o),
      .occupancy_o(persp_old_occupancy_o)
  );

  zhao_raster_perspuv_pairpipe_v2 #(.NTOK(16), .TAGW(14)) u_persp_v2 (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(persp_valid_i), .v_ready_o(persp_v2_ready_o),
      .u_over_w_i(persp_u_over_w_i), .v_over_w_i(persp_v_over_w_i),
      .r_mant_i(persp_mant_i), .r_k_i(persp_k_i),
      .depth_zero_i(persp_depth_zero_i), .tag_i(persp_tag_i),
      .r_valid_o(persp_v2_valid_o), .r_ready_i(persp_out_ready_i),
      .u_o(persp_v2_u_o), .v_o(persp_v2_v_o), .tag_o(persp_v2_tag_o),
      .sat_o(persp_v2_sat_o), .depth_zero_o(persp_v2_depth_zero_o),
      .fragments_o(persp_v2_fragments_o), .products_o(persp_v2_products_o),
      .zero_products_o(persp_v2_zero_products_o),
      .occupancy_o(persp_v2_occupancy_o), .idle_o(persp_v2_idle_o)
  );

endmodule : tb_packetb_observation_successors

`default_nettype wire
