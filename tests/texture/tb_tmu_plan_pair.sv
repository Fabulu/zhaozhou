// tb_tmu_plan_pair.sv — the shipped TMU pipe and the elastic planner.
//
// The pipe plans AND samples: it issues a cache access, waits for the response,
// filters or decodes it, and retires through a ROB. The planner is the FRONT
// END only. To compare their addresses the pipe has to keep flowing, so this
// harness carries a one-deep cache MODEL that accepts every access and echoes a
// response of zeroes.
//
// The returned data is deliberately garbage: the four addresses do not depend
// on what came back, and inventing plausible texel data here would add a second
// thing that could be wrong without testing anything more.
`default_nettype none

module tb_tmu_plan_pair (
    input var logic clk,
    input var logic rst_n,

    // ---- the shipped pipe ----------------------------------------------------
    input  var logic         a_valid_i,
    output var logic         a_req_ready_o,
    input  var logic [31:0]  a_u_i,
    input  var logic [31:0]  a_v_i,
    input  var logic [31:0]  a_base_i,
    input  var logic [31:0]  a_mode_i,
    input  var logic [ 7:0]  a_lod_i,
    input  var logic [15:0]  a_src_i,
    input  var logic         a_acc_ready_i,
    output var logic         a_acc_valid_o,
    output var logic [  3:0] a_acc_en_o,
    output var logic [127:0] a_acc_addr_o,
    output var logic [ 15:0] a_acc_src_id_o,

    // ---- the elastic planner -------------------------------------------------
    input  var logic         b_valid_i,
    output var logic         b_req_ready_o,
    input  var logic [31:0]  b_u_i,
    input  var logic [31:0]  b_v_i,
    input  var logic [31:0]  b_base_i,
    input  var logic [31:0]  b_mode_i,
    input  var logic [ 7:0]  b_lod_i,
    input  var logic [15:0]  b_src_i,
    input  var logic         b_acc_ready_i,
    output var logic         b_acc_valid_o,
    output var logic [  3:0] b_acc_en_o,
    output var logic [127:0] b_acc_addr_o,
    output var logic [ 15:0] b_acc_src_id_o,
    output var logic [  3:0] b_occupancy_o
);

  // Outputs this harness does not examine are given NAMED sinks rather than
  // `()`. Empty pin connections are a lint class here, and naming them also
  // says which signals were deliberately ignored instead of forgotten.
  /* verilator lint_off UNUSEDSIGNAL */
  logic        nc_smp_valid, nc_mode_err, nc_idle;
  logic [23:0] nc_smp_rgb;
  logic [ 7:0] nc_smp_a, nc_smp_idx;
  logic [15:0] nc_smp_src;
  logic [31:0] nc_samples, nc_robfull;
  logic        nc_plan_filter, nc_plan_err;
  logic [ 7:0] nc_plan_fu, nc_plan_fv;
  logic [3:0] nc_plan_nib;
  logic [ 2:0] nc_plan_fmt;
  logic [31:0] nc_plan_accepted;
  logic [31:0] nc_cachewait, nc_filtbusy, nc_outstall;
  logic [31:0] nc_palfb;
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- a one-deep cache model for the shipped pipe -------------------------
  logic        cac_ready_w;
  logic        rsp_v_q;
  logic        pipe_cac_valid, pipe_cac_ready;

  assign cac_ready_w = a_acc_ready_i;   // the test controls access acceptance

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) rsp_v_q <= 1'b0;
    else begin
      // Accept one access, offer one response, hold it until the pipe takes it.
      if (rsp_v_q && pipe_cac_ready)         rsp_v_q <= 1'b0;
      if (pipe_cac_valid && cac_ready_w)     rsp_v_q <= 1'b1;
    end
  end

  zhao_texture_tmu_pipe u_ref (
      .clk               (clk),
      .rst_n             (rst_n),
      .req_valid_i       (a_valid_i),
      .pal_inv_valid_i   (1'b0),
      .req_ready_o       (a_req_ready_o),
      .req_u_i           (a_u_i),
      .req_v_i           (a_v_i),
      .req_base_i        (a_base_i),
      .req_pal_base_i    (32'd0),
      .req_mode_i        (a_mode_i),
      .req_lod_i         (a_lod_i),
      .req_src_id_i      (a_src_i),
      .cac_valid_o       (pipe_cac_valid),
      .cac_ready_i       (cac_ready_w),
      .cac_en_o          (a_acc_en_o),
      .cac_addr_o        (a_acc_addr_o),
      .cac_src_id_o      (a_acc_src_id_o),
      .cac_valid_i       (rsp_v_q),
      .cac_ready_o       (pipe_cac_ready),
      .cac_data_i        (64'd0),
      .smp_valid_o       (nc_smp_valid),
      .smp_ready_i       (1'b1),
      .smp_rgb_o         (nc_smp_rgb),
      .smp_a_o           (nc_smp_a),
      .smp_idx_o         (nc_smp_idx),
      .smp_src_id_o      (nc_smp_src),
      .mode_error_o      (nc_mode_err),
      .idle_o            (nc_idle),
      .texture_samples_o (nc_samples),
      .rob_full_clocks_o (nc_robfull),
      .cache_wait_clocks_o (nc_cachewait),
      .filter_busy_clocks_o(nc_filtbusy),
      .out_stall_clocks_o  (nc_outstall),
      .pal_fallback_o      (nc_palfb)
  );
  assign a_acc_valid_o = pipe_cac_valid;

  zhao_texture_tmu_plan #(
      .SRCW(16)
  ) u_plan (
      .clk         (clk),
      .rst_n       (rst_n),
      .req_valid_i (b_valid_i),
      .req_ready_o (b_req_ready_o),
      .req_u_i     (b_u_i),
      .req_v_i     (b_v_i),
      .req_base_i  (b_base_i),
      .req_mode_i  (b_mode_i),
      .req_lod_i   (b_lod_i),
      .req_src_id_i(b_src_i),
      .acc_valid_o (b_acc_valid_o),
      .acc_ready_i (b_acc_ready_i),
      .acc_en_o    (b_acc_en_o),
      .acc_addr_o  (b_acc_addr_o),
      .acc_src_id_o(b_acc_src_id_o),
      .acc_filter_o(nc_plan_filter),
      .acc_err_o   (nc_plan_err),
      .acc_nib_o   (nc_plan_nib),
      .acc_fu_o    (nc_plan_fu),
      .acc_fv_o    (nc_plan_fv),
      .acc_fmt_o   (nc_plan_fmt),
      .accepted_o  (nc_plan_accepted),
      .occupancy_o (b_occupancy_o)
  );

endmodule : tb_tmu_plan_pair

`default_nettype wire
